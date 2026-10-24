# timi

CLI-first, offline, local-API music player for microSD bundles and folders. Runs on low-power devices (Raspberry Pi Zero class). GUI is optional and fully removable.

## Özellikler
- Tamamen çevrimdışı, yerel fonksiyon API (`include/msp/*.h`)
- 45:00 hedef + 45 sn üst sınır (DP planlama)
- Otomatik kaynak algılama (microSD, ~/Music)
- `data.yaml` v1/v2, `.m3u/.m3u8` desteği
- FAT dosya adı temizleme ile dışa aktarma
- Süre ölçümü (ffprobe) + önbellek (`~/.cache/microsd-music-player/durations.tsv`)
- Temel oynatma (miniaudio) - CLI üzerinden
- Hafif TUI iskeleti (`ui/ncurses`)
- GTK kaldırıldı, arayüz tamamen CLI'ye taşındı

## Derleme

```bash
make core      # sadece libmspcore
make backend   # oynatma backend
make cli       # CLI (GTK/libadwaita linklemez)
make all-min   # core + cli
make tui       # ncurses TUI (opsiyonel)
make clean
```

## Kullanım

```bash
# Kaynağı otomatik algıla ve listele
./build/msmp-cli --list

# Belirli kaynaktan listele
./build/msmp-cli --list -s /run/media/$USER/KARTADI

# Bloğu hedefe dışa aktar (GUI yok)
./build/msmp-cli --export /hedef/klasor -s /run/media/$USER/KARTADI

# Bloku çal
./build/msmp-cli --play -s /run/media/$USER/KARTADI

# Karıştırarak planla
./build/msmp-cli --list --shuffle
```

## Mimari
- `core/` - Çekirdek mantık (GUI bağımsız)
- `backend/` - Oynatma (miniaudio)
- `cli/` - Komut satırı arayüzü
- `include/msp/` - Yerel kütüphane API (types/core/backend)
- `ui/ncurses/` - Hafif TUI (opsiyonel)
- `docs/PLAN_C_OFFLINE_LOCAL_API.md` - Mimari planı

## Notlar
- `make cli` çıktısı GTK, Adwaita, glib ile linklenmez
- `ui/gtk/` klasörü silinse bile `make cli` çalışır
- Hiçbir ağ çağrısı, API anahtarı yok - tamamen çevrimdışı
