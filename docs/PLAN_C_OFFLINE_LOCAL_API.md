# C'ye Geçiş Planı: Tamamen Çevrimdışı, Yerel API, Katman Ayrımı

Tarih: 2026-10-24  
Proje: microsd-music-player  
Hedef: Raspberry Pi Zero ve benzeri çok düşük donanımlarda çalışabilecek, GUI katmanı silinip CLI ile her şey yapılabilen, tamamen çevrimdışı bir yapı.

## 1) Temel İlkeler

- **Tamamen çevrimdışı**: Ağ isteği, API anahtarı, token yok. Sadece dosya sistemi + ALSA/miniaudio + yerel önbellek.
- **Yerel kütüphane API**: `include/msp/*.h` üzerinden direkt fonksiyon çağrısıyla erişim. HTTP değil, link-time API.
- **Katman ayrımı**: `core` GUI'ye bağımlı değil. `ui/gtk` ve `ui/ncurses` tamamen opsiyonel.
- **GUI silinse bile çalışsın**: `core + cli + backend` ile tüm işlevler (`list`, `export`, `play`, tarama, planlama) yapılabilir.
- **Hafiflik**: Mümkün olduğunca az bağımlılık, Pi Zero odaklı.
- **Minimal kullanıcı arayüzü**: GUI'deki teknik detaylar kaldırılacak, sadece kullanıcıya lazım olan bilgiler gösterilecek.

## 2) Hedef Mimari

```text
microsd-mp/
├── core/        # Çekirdek mantık (libmspcore) - GUI bağımsız
├── backend/     # Oynatma (ALSA / miniaudio)
├── cli/         # CLI arayüzü (GUI'sız çalışabilir)
├── ui/
│   ├── ncurses/ # Hafif TUI (opsiyonel)
│   └── gtk/     # GTK4 UI (opsiyonel, kaldırılamaz değil ama ayrı)
├── include/msp/ # Public header'lar
├── docs/        # Dokümantasyon
├── utils/       # Ortak yardımcılar
├── build/       # Derleme çıktıları
└── Makefile     # Modül bazlı derleme
```

## 3) Katman Sorumlulukları

| Katman | Sorumluluk | Bağımlılık |
| --- | --- | --- |
| `core` | Tarama, `data.yaml`/`m3u/m3u8`, 45:00+45sn DP, süre ölçümü/cache, kapak bulma, export, FAT ad temizleme | Sadece libc (+ isteğe bağlı `ffprobe` harici çağrı) |
| `backend` | Oynatma, duraklatma, next/prev, shuffle, repeat, crossfade, automix, gapless | ALSA veya `miniaudio.h` + decode kütüphaneleri |
| `cli` | Komut işleme, çıktı formatı, play/list/export/info | `core` + `backend` |
| `ui/ncurses` | Minimal TUI | `core` + `backend` + ncurses |
| `ui/gtk` | Sade GUI (teknik detaylar kaldırılmış) | `core` + `backend` + gtk4 + libadwaita (opsiyonel) |

## 4) Public API Taslağı

Tüm erişim `include/msp/` üzerinden, doğrudan fonksiyon çağrısı.

### `include/msp/types.h`
```c
#ifndef MSP_TYPES_H
#define MSP_TYPES_H

#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msp_track {
    char *path;
    char *title;
    char *artist;
    char *album;
    char *genre;
    int year;
    int track_no;
    int disc_no;
    double duration_sec;
} msp_track_t;

typedef struct msp_source {
    char *root;
    char *name;
    char *title;
    char *artist;
    char *genre;
    char *cover;
    int type; /* 0=folder,1=playlist,2=bundle */
    msp_track_t *tracks;
    size_t track_count;
} msp_source_t;

typedef enum {
    MSP_MODE_BLOCK = 0,
    MSP_MODE_LOOP  = 1
} msp_mode_t;

typedef struct msp_plan {
    msp_track_t *items;
    size_t item_count;
    msp_mode_t mode;
    double stop_at;
    double fade_sec;
    double total_sec;
    int cap_exceeded;
} msp_plan_t;

typedef struct msp_opts {
    bool shuffle;
    bool fewer_shuffle;
    int repeat_mode; /* 0=off,1=all,2=one */
    bool crossfade;
    bool automix;
    bool gapless;
    double crossfade_sec;
} msp_opts_t;

#ifdef __cplusplus
}
#endif
#endif
```

### `include/msp/core.h`
```c
#ifndef MSP_CORE_H
#define MSP_CORE_H

#include "msp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Kaynak algılama */
int msp_source_auto(msp_source_t **out);
int msp_source_detect(const char *path, msp_source_t **out);
void msp_source_free(msp_source_t *s);

/* Planlama (45:00 hedef + 45sn üst sınır) */
int msp_plan_build(msp_source_t *s, const msp_opts_t *opts, msp_plan_t **out);
double msp_plan_total(const msp_plan_t *p);
void msp_plan_free(msp_plan_t *p);

/* Dışa aktarma */
int msp_export_bundle(const msp_plan_t *p, const msp_source_t *s, const char *dest_dir, char **out_path);

/* Yardımcılar */
const char *msp_format_time(double sec, char *buf, size_t len);
void msp_cache_flush(void);

#ifdef __cplusplus
}
#endif
#endif
```

### `include/msp/backend.h`
```c
#ifndef MSP_BACKEND_H
#define MSP_BACKEND_H

#include "msp/types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct msp_player msp_player_t;

int msp_player_create(const msp_plan_t *plan, const msp_opts_t *opts, msp_player_t **out);
int msp_player_play(msp_player_t *p);
int msp_player_pause(msp_player_t *p);
int msp_player_next(msp_player_t *p);
int msp_player_prev(msp_player_t *p);
int msp_player_stop(msp_player_t *p);
int msp_player_seek(msp_player_t *p, double sec);
void msp_player_free(msp_player_t *p);

#ifdef __cplusplus
}
#endif
#endif
```

**Not:** Hiçbir HTTP, JSON-RPC, socket sunucusu, API key yok. Tamamen lokal fonksiyon çağrısı.

## 5) GUI Sadeleştirme Planı

Mevcut GUI'deki gereksiz teknik bilgiler kaldırılacak.

| Kaldır/Değiştir | Açıklama |
|---|---|
| Kaynak yolu gösterimi | Kaldırılacak |
| "Algılanan yapı: bundle/playlist/folder ..." | Kaldırılacak |
| "Hedef 45:00 · üst sınır 45:45" rozeti | Kaldırılacak |
| Teknik durum metinleri | "Çalıyor/Duraklatıldı" gibi minimal |
| Yol tabanlı alt başlık | Sadece "Sanatçı - Şarkı" / "Albüm adı" |
| Liste satırlarında gereksiz meta | Sadece sıra no, başlık, süre |

**Hedef görünüm (Spotify benzeri):** Sol kapak, sağ üstte şarkı+sanatçı, ilerleme, kontrol butonları, alt/kuyruk sade liste.

## 6) Aşamalı Uygulama Takvimi

| Aşama | Durum | Açıklama |
|---|---|---|
| 1 | **YAPILIYOR** | `docs/PLAN_C_OFFLINE_LOCAL_API.md` oluştur |
| 2 | Beklemede | Klasör iskeleti ve `include/msp/` header'ları |
| 3 | Beklemede | `core/plan.c` - DP portu (Rust ile kıyaslanarak test) |
| 4 | Beklemede | `core/source.c` - tarama + `data.yaml` + `m3u8` (minimal parser) |
| 5 | Beklemede | `core/export.c` - kopyalama + FAT temizleme + v2 yaml |
| 6 | Beklemede | `core/cache.c`, `core/probe.c`, `core/util.c` |
| 7 | Beklemede | `cli/main.c` - `--list`, `--export` GUI'sız çalışsın |
| 8 | Beklemede | `backend/` - temel çalma (miniaudio) |
| 9 | Beklemede | `ui/ncurses/` hafif TUI |
| 10 | Beklemede | `ui/gtk/` sadeleştirme + core API'e geçiş |
| 11 | Beklemede | Makefile modül bazlı hedefler |
| 12 | Beklemede | Test & doğrulama (GUI silinince CLI çalışsın) |

## 7) Derleme Hedefleri

```makefile
make core     # sadece libmspcore
make cli      # core + backend + cli (GTK yok)
make tui      # core + backend + ncurses
make gui      # core + backend + ui/gtk
make all-min  # cli+tui minimum
make clean
```

**Kural:** `make cli` çıktısı `pkg-config gtk4` bağımlılığı **içermemeli**.

## 8) Bağımlılıklar

| Derleme | Gerekli | Açıklama |
|---|---|---|
| `core` | gcc/clang, make | Mümkünse sadece libc |
| `cli` | + alsa-lib veya miniaudio, + decode (mpg123/libvorbisfile/libopusfile/flac) | Pi Zero uygun |
| `tui` | + ncurses | Çok hafif |
| `gui` | + gtk4, libadwaita (opsiyonel) | Masaüstü için ayrı |

## 9) Doğrulama Kriterleri

- [ ] `make core` bağımsız derlenebiliyor
- [ ] `make cli` GTK/libadwaita linklemeden derlenebiliyor
- [ ] `ui/gtk/` klasörü silinse bile `make cli` çalışıyor
- [ ] `--list` ve `--export` GUI açmadan tam çalışıyor
- [ ] `data.yaml` (v1 ve v2) okunabiliyor
- [ ] 45:00 + 45sn kuralı korunuyor
- [ ] FAT ad temizleme çalışıyor
- [ ] Tamamen çevrimdışı çalışıyor (hiçbir ağ çağrısı yok)
- [ ] GUI'de teknik metinler temizlenmiş
- [ ] Raspberry Pi Zero benzeri ortamda CLI kullanımı mümkün

## 10) Notlar

- "API" = **yerel kütüphane API** (fonksiyon çağrısı). Web API değil.
- İlk portta `ffprobe` kullanılmaya devam edilebilir, zamanla minimize edilebilir.
- `data.yaml` için **minimal parser** yazılacak (kütüphane bağımlılığını azaltmak için).
- Tüm geliştirme adımları bu belgeye işlenecek.
