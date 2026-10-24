# microSD Music Player

**microSD karttaki müzik paketini veya `~/Music` kütüphanesini otomatik algılayan,
açılışta çalmaya başlayan ve toplamı `45:00` hedefiyle en fazla `45:45`'e kadar
sınırlayan GTK4 + Rust oynatıcı.**

Python yok, harici medya sunucusu yok: tek bir yerel uygulama, basit bir arayüz
ve gerçek dosya sürelerine göre kurulan bir çalma bloğu.

---

## Öne çıkanlar

- **Otomatik kaynak algılama** — `data.yaml` paketi, `.m3u` / `.m3u8` listesi
  veya düz klasör ağacı.
- **45:00 hedefi + 45 sn esneme** — şarkı süreleri `ffprobe` ile okunur, yukarı
  yuvarlanır ve 45:00'a en yakın alt küme seçilir; toplam asla `45:45`'i geçmez.
  Şarkı ortasından kesilmez.
- **Kısa kaynakta akıllı döngü** — kaynak 45 dakikadan kısaysa liste döngüye
  alınır, en geç `45:45`'te 6 saniyelik fade-out ile durur.
- **Geniş biçim desteği** — Opus, Ogg/Vorbis, MP3, AAC/M4A, FLAC, WAV, AIFF,
  WMA, AC3, DTS, APE, WavPack, TTA ve daha fazlası.
- **microSD'ye dışa aktarma** — seçilen blok, `data.yaml` + kapak + ses dosyaları
  olarak; FAT/exFAT'te yasak karakterler temizlenerek yazılır.
- **Sade GTK4 arayüzü** — kapak, şu an çalan, ilerleme, ses seviyesi ve kuyruk.

---

## Nasıl çalışır?

```
kaynak algılama → ffprobe süreleri → 45:00 hedefli plan (üst sınır 45:45) → GTK4 / GStreamer oynatma
```

1. **Algıla:** Verilen yol (veya otomatik seçim) paket, çalma listesi ya da
   klasör olarak yorumlanır.
2. **Ölç:** Her parçanın gerçek süresi `ffprobe` ile paralel olarak alınır.
3. **Planla:** Süreler yukarı yuvarlanır; toplamı `45:45`'i aşmayan ve 45:00'a
   en yakın alt küme dinamik programlama ile seçilir.
4. **Çal:** Arayüz açılır ve (varsayılan olarak) ilk parça hemen başlar.

---

## Hızlı başlangıç

```bash
cd ~/Documents/Projects/timi
cargo build --release

# Bağlı microSD paketi varsa onu, yoksa ~/Music'i otomatik seç
./target/release/timi
```

Belirli bir kaynakla:

```bash
# microSD paketi
./target/release/timi -s /run/media/$USER/C450-14F5

# Çalma listesi
./target/release/timi -s ~/Music/Aşure.m3u8

# Sadece 45 dakikalık planı listele
./target/release/timi -s ~/Music --list
```

---

## Gereksinimler

| Bileşen | Neden? |
| --- | --- |
| Rust / Cargo | Derleme |
| GTK4 geliştirme paketleri (≥ 4.8) | Arayüz ve `GtkMediaFile` |
| `ffprobe` (FFmpeg) | Gerçek süre tespiti |
| GStreamer + codec eklentileri | Ses çözme/oynatma |

Kontrol:

```bash
pkg-config --modversion gtk4   # örn. 4.22.5
ffprobe -version               # örn. 8.1.3
gst-inspect-1.0 --version      # örn. 1.28.8
```

Kurulum örnekleri (dağıtıma göre paket adları değişebilir):

```bash
# Fedora
sudo dnf install gtk4-devel ffmpeg-free \
  gstreamer1-plugins-base gstreamer1-plugins-good \
  gstreamer1-plugins-bad-free gstreamer1-plugins-ugly-free \
  gstreamer1-plugin-libav

# Debian / Ubuntu
sudo apt install libgtk-4-dev ffmpeg \
  gstreamer1.0-plugins-base gstreamer1.0-plugins-good \
  gstreamer1.0-plugins-bad gstreamer1.0-plugins-ugly gstreamer1.0-libav

# Arch Linux
sudo pacman -S gtk4 ffmpeg gst-plugins-base gst-plugins-good \
  gst-plugins-bad gst-plugins-ugly gst-plugins-libav
```

> Opus ve MP3 gibi biçimler için GStreamer'ın `good`/`bad`/`ugly`/`libav`
> eklentilerinden en az biri gerekir. Eksik codec'ler yalnızca o dosyayı
> etkiler; oynatıcı diğer parçalarla devam eder.

---

## Kullanım

```text
timi [seçenekler]

  -s, --source <YOL>   Paket klasörü, .m3u/.m3u8 ya da müzik klasörü
                       (verilmezse: bağlı microSD paketi, yoksa ~/Music)
      --list           45 dakikalık bloğu listele ve çık
  -e, --export <DEST>  Bloğu data.yaml + kapak + dosyalar olarak yaz
      --shuffle        Blok seçiminden önce listeyi karıştır
      --no-autoplay    Arayüz açılırken çalma
  -h, --help           Yardımı göster
```

### Örnekler

```bash
# Varsayılan: SD paketi → yoksa ~/Music, hemen çalar
./target/release/timi

# Karışık 45 dakikalık blok, otomatik çalma kapalı
./target/release/timi -s ~/Music --shuffle --no-autoplay

# Planı gör, çalmadan çık
./target/release/timi -s ~/Music --list

# 45 dakikalık bloğu SD kartın köküne yaz (alt klasör oluşturmaz)
./target/release/timi -s ~/Music --export /run/media/$USER/C450-14F5
```

`--list` çıktısı:

```text
Kaynak : playlist (m3u8) · /home/farukylmz/Music
Plan   : Blok: 44:53 / 11 şarkı (hedef 45:00, üst sınır 45:45)
Toplam : 44:53
  01.   4:05  Pentagram - Bir
  02.   4:11  OGÜN SANLISOY - Saydim
  03.   5:33  Haluk Levent - Elfida
  ...
```

---

## Desteklenen kaynak yapıları

### 1. `data.yaml` paketi (microSD)

Düz veya alt klasörlü olabilir; `file` yolları `data.yaml`'ın bulunduğu klasöre
görelidir.

#### Resmi albüm örneği

```yaml
version: 2
type: album        # album | mixed
title: "Bir"
artist: "Pentagram"
year: 2002
genre: "Rock"
cover: cover.png
playlist:
  - file: "01-Bir.opus"
    title: "Bir"
    artist: "Pentagram"
    album: "Bir"
    track: 1
    disc: 1
  - file: "02-Seytan-Bunun-Neresinde.opus"
    title: "Şeytan Bunun Neresinde"
    artist: "Pentagram"
    album: "Bir"
    track: 2
```

#### Karma / derleme örneği

```yaml
version: 2
type: mixed
title: "Aşure - 45 dk"
artist: "Çeşitli Sanatçılar"
genre: "Karışık"
description: "45 dakikalık karışık seçki"
playlist:
  - file: "Pentagram/Bir/Bir.opus"
    title: "Bir"
    artist: "Pentagram"
    album: "Bir"
    track: 1
  - file: "Baris-Akarsu/Islak-Islak/Islak-Islak.opus"
    title: "Islak Islak"
    artist: "Barış Akarsu"
    album: "Islak Islak"
    track: 2
```

#### Alt alanlar

| Alan | Zorunlu | Açıklama |
| --- | --- | --- |
| `version` | Hayır | Şema sürümü; dışa aktarmalar `2` yazar. |
| `type` / `kind` / `collection_type` | Hayır | `album` veya `mixed`. Belirtilmezse parça sanatçılarından çıkarılır. |
| `title` / `name` / `album` | Hayır | Paket/ad/ albüm adı. Yoksa klasör adı kullanılır. |
| `artist` / `album_artist` | Hayır | Albüm sanatçısı. Birden çok sanatçı varsa otomatik `Çeşitli Sanatçılar`. |
| `year` | Hayır | Yayın yılı. |
| `genre` | Hayır | Tarz. |
| `description` | Hayır | Açıklama (arayüzde gösterilmez ama saklanır). |
| `cover` | Hayır | Kapak dosyası; `data.yaml`'a göreli yol. |
| `playlist` / `tracks` | Evet | Parça listesi. |
| `playlist[].file` | Evet | Ses dosyası; göreli veya mutlak yol. |
| `playlist[].title` | Hayır | Görünen ad; yoksa dosya adı kullanılır. |
| `playlist[].artist` / `performer` | Hayır | Parça sanatçısı. |
| `playlist[].album` | Hayır | Albüm adı. |
| `playlist[].year` | Hayır | Parça yılı. |
| `playlist[].genre` | Hayır | Parça tarzı. |
| `playlist[].track` / `track_number` / `number` | Hayır | Parça numarası. |
| `playlist[].disc` / `disc_number` | Hayır | Disk numarası. |
| `playlist[].duration_sec` | Hayır | Eski biçimlerle uyumluluk; plan yine gerçek süreyi `ffprobe` ile ölçer. |

Eski, minimal `data.yaml` biçimleri geriye dönük olarak hâlâ desteklenir. Kapak
verilmezse ilk parçanın yakınındaki `cover.png`, `cover.jpg`, `folder.jpg`,
`front.jpg`, `albumart.jpg` aranır. Dışa aktarma artık daha zengin bir `data.yaml`
(v2) üretir; klasör yapısı korunur, aynı dosyalar tek sefer kopyalanır.

- `cover` verilmezse ilk parçanın klasöründe ve üst klasörlerinde
  `cover.png`, `cover.jpg`, `folder.jpg`, `front.jpg`, `albumart.jpg` gibi
  adlar aranır.
- Listede olup diskte bulunmayan dosyalar uyarı verilerek atlanır.
- Dışa aktarma `duration_sec` / `total_duration_sec` yazmaz; gerek yoktur.

### 2. `.m3u` / `.m3u8` çalma listesi

```m3u8
#EXTM3U
#PLAYLIST:Aşure
#EXTINF:245,Pentagram - Bir
Pentagram/Bir/Bir.opus
#EXTINF:251,OGÜN SANLISOY - Saydim
Ogun Sanlisoy/O Gun/Saydim.opus
```

- `#EXTINF` süresi ve başlığı opsiyoneldir; eksikse dosyadan ölçülür.
- Yollar liste dosyasına göre çözülür; mutlak yollar da desteklenir.
- `#PLAYLIST:` başlığı kaynak adı olarak kullanılır.

### 3. Düz klasör taraması

```
~/Music/
├── Pentagram/Bir/Bir.opus
├── Pentagram/Bir/Seytan Bunun Neresinde.opus
└── Baris Akarsu/Islak Islak/Kimdir O.opus
```

- Alt klasörler dahil **özyinelemeli** taranır.
- Dosyalar ad sırasına göre eklenir.
- Yol en az üç bileşenliyse başlık `Sanatçı - Şarkı` biçiminde üretilir
  (`Pentagram - Bir`); değilse dosya adı kullanılır.

### Algılama sırası

`-s` verilmezse:

1. `/run/media/$USER/*/data.yaml` içeren ilk bağlı disk (alfabetik),
2. `~/Music`.

`-s <yol>` verilirse:

1. Yol dosya ise ve `.m3u`/`.m3u8` ise → çalma listesi,
2. Klasörde `data.yaml` varsa → paket,
3. Klasörde `.m3u`/`.m3u8` varsa → alfabetik ilk liste,
4. Aksi halde → özyinelemeli klasör taraması.

---

## Desteklenen ses biçimleri

Klasör taraması aşağıdaki uzantıları ses dosyası sayar. `data.yaml` ve
m3u/m3u8 listelerinde **uzantı filtresi yoktur**; GStreamer'ın çözebildiği her
dosya çalınır.

| Grup | Uzantılar |
| --- | --- |
| Ogg / Opus / Vorbis / Speex | `opus`, `ogg`, `oga`, `ogv`, `spx` |
| MPEG ses | `mp3`, `mp2`, `mp1`, `mpga` |
| AAC / Apple | `m4a`, `m4b`, `m4r`, `m4v`, `aac` |
| Kayıpsız | `flac`, `wav`, `wave`, `aiff`, `aif`, `aifc`, `au`, `snd`, `ape`, `wv`, `wvp`, `tta`, `mpc`, `mpp`, `mp+` |
| Windows / surround | `wma`, `asf`, `ac3`, `eac3`, `dts`, `dca` |
| RealAudio | `ra`, `rm`, `ram` |
| Mobil | `amr`, `3gp`, `3g2` |
| Konteyner (ses parçası) | `mp4`, `mka`, `mkv`, `webm` |

Bu sistemde GStreamer ile doğrulananlar: Opus, Ogg/Vorbis, Speex, MP3, MP2, M4A,
AAC, FLAC, WAV, AIFF, AU, WMA, AC3, DTS, WavPack, TTA, MKA, M4B.

> Oynatma tamamen kurulu GStreamer eklentilerine bağlıdır. Örneğin bu sistemde
> EAC3 ve CAF için çözücü/demuxer yoktur; CAF taranmaz, EAC3 listede görünse bile
> çalınamazsa arayüz hata gösterir. Video konteynerlerinde yalnızca ses çalınır;
> video penceresi yoktur.

---

## 45:00 + 45 sn kuralı

- Süreler `ffprobe` ile **gerçek dosyadan** alınır ve `ceil()` ile yukarı
  yuvarlanır. Böylece yuvarlama hatası toplamı sınırın üstüne taşırmaz.
- Seçim, şarkıları bölmeden yapılan bir **alt küme** problemidir (0/1 DP).
  Hedef `45:00`, esneme payı `+45 sn`dir. Toplam, `44:15` ile `45:45` arasında
  ve 45:00'a en yakın olacak şekilde seçilir. Bu aralıkta çözüm yoksa alt sınır
  `43:00`'a indirilir; yine de toplam asla `45:45`'i geçmez.
- Uygun alt küme bulunamazsa (kaynak kısa ya da tüm parçalar çok uzun) liste
  **döngüye** alınır. Oynatma en geç `45:45`'te durur; son 6 saniyede ses kısılır.
- `--shuffle` yalnızca seçim sırasını değiştirir; süre sınırı aynıdır.

---

## Arayüz

- Kapak görseli (yoksa yer tutucu), parça adı ve kaynak bilgisi
- İlerleme çubuğu, geçen / toplam süre
- Önceki, çal/duraklat, sonraki
- Ses seviyesi kaydırıcısı
- Kuyruk listesi ve durum satırı (hata mesajları burada görünür)

| Tuş | İşlev |
| --- | --- |
| `Boşluk` | Çal / duraklat |
| `→` | Sonraki parça |
| `←` | Önceki parça |
| `Esc` | Uygulamayı kapat |

---

## Dışa aktarma

```bash
./target/release/timi -s ~/Music --export /run/media/$USER/C450-14F5
```

- Seçilen blok **doğrudan hedef klasöre** yazılır; ek bir `... - 45 dk`
  sarmalayıcı klasör oluşturulmaz.
- Kaynağa göreli klasör yapısı korunur (`Pentagram/Bir/Bir.opus` gibi).
- Aynı adlı dosyalar üzerine yazılır; hedefteki ilgisiz dosyalar silinmez.
  Önemli veriler için önceden yedek alın.
- `data.yaml` yalnızca `file` + `title` alanlarıyla yazılır; süreler tekrar
  `ffprobe` ile ölçülür.
- FAT/exFAT'te yasak olan `< > : " / \ | ? *` karakterleri dosya ve klasör
  adlarında `_` yapılır (ör. `Haberin Var mı?.opus` → `Haberin Var mi_.opus`).
- Kaynakta kapak varsa `cover.<uzantı>` olarak kopyalanır.

---

## Önbellek ve ortam değişkenleri

| Değişken / dosya | Etki |
| --- | --- |
| `PLAYER_MUTE=1` | Sesi kapatır (testlerde faydalı) |
| `XDG_CACHE_HOME` | Süre önbelleğinin üst klasörü |
| `USER` | `/run/media/$USER` otomatik algılaması |
| `HOME` | `~/Music` ve varsayılan önbellek yolu |

Süre önbelleği:

```
${XDG_CACHE_HOME:-~/.cache}/timi/durations.tsv
```

Anahtar; dosya yolu + değişiklik zamanı + boyuttur. Dosya değişirse süre yeniden
ölçülür. `ffprobe` çalışmazsa süre 180 saniye varsayılır.

---

## Proje yapısı

```
timi/
├── Cargo.toml
├── README.md
└── src/
    ├── main.rs      # CLI, kaynak seçimi, --list / --export
    ├── source.rs    # paket / m3u8 / klasör algılama, ffprobe + önbellek
    ├── plan.rs      # 45:00 (+45 sn) DP planı, döngü modu, süre biçimleme
    ├── export.rs    # data.yaml + dosya kopyalama, FAT ad temizliği
    └── gui.rs       # GTK4 arayüzü ve GtkMediaFile oynatma
```

---

## Sorun giderme

**"Kaynak bulunamadı"**
: `-s` ile bir klasör/lista verin. Otomatik algılama yalnızca
`/run/media/$USER/*/data.yaml` ve `~/Music` yollarına bakar.

**microSD otomatik seçilmiyor**
: Kart birden fazla bağlıysa alfabetik ilk `data.yaml` paketi seçilir; istediğiniz
paketi `-s` ile açıkça verin.

**"Kaynakta oynatılabilir ses dosyası yok"**
: Dosyaların gerçekten var olduğunu, `data.yaml` yollarının doğru olduğunu ve
uzantının desteklendiğini kontrol edin.

**Süreler yanlış görünüyor**
: `ffprobe` kurulu mu? Eksikse 180 sn tahmini kullanılır. Önbelleği silmek için
`~/.cache/timi/durations.tsv` dosyasını kaldırabilirsiniz.

**Bir biçim çalınmıyor**
: GStreamer eklentileri eksik olabilir. `gst-inspect-1.0 | grep -i <codec>` ile
kontrol edin; `gstreamer1-plugins-*` ve `gstreamer1-plugin-libav` paketlerini
kurun.

**Arayüz açılmıyor**
: Uygulamayı bir masaüstü oturumunda (X11/Wayland) çalıştırın; SSH/headless
ortamda GTK penceresi açılamaz. `--list` ve `--export` GUI gerektirmez.

**Dosya adları SD karta yazılamıyor**
: FAT/exFAT `? : " < > | *` karakterlerini kabul etmez. Dışa aktarma bunları
otomatik olarak `_` yapar; elle kopyalarken siz de temizleyin.

---

## Sınırlar

- Linux / GTK4 odaklıdır; otomatik SD algılaması `/run/media` düzenini varsayar.
- Yalnızca yerel dosyalar; ağ akışı (HTTP) ve radyo desteklenmez.
- Video oynatılmaz; video konteynerlerinde yalnızca ses parçası çalınır.
- Etiket (ID3/Vorbis comment) okunmaz; başlıklar `data.yaml`, m3u veya dosya
  adından gelir.
- Arayüz bilinçli olarak sade tutulmuştur; kuyruk listesi görüntüleme amaçlıdır.
