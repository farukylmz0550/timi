//! Kaynak yapısı algılama: microSD paketi (`data.yaml`), m3u/m3u8 listesi
//! veya düz klasör ağacı.

use std::collections::HashMap;
use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;
use std::sync::{Mutex, OnceLock};
use std::time::UNIX_EPOCH;

use anyhow::{bail, Context, Result};
use serde::Deserialize;

/// Klasör taramasında ses dosyası sayılan uzantılar.
///
/// `GtkMediaFile` (GStreamer) bu biçimlerin çoğunu doğrudan çözer. `data.yaml`
/// ve m3u/m3u8 listeleri uzantıya bakmaz; oradaki her dosya denenir.
const AUDIO_EXTS: &[&str] = &[
    // Ogg / Opus / Vorbis / Speex
    "opus", "ogg", "oga", "ogv", "spx",
    // MPEG ses
    "mp3", "mp2", "mp1", "mpga",
    // AAC / Apple
    "m4a", "m4b", "m4r", "m4v", "aac",
    // Kayıpsız
    "flac", "wav", "wave", "aiff", "aif", "aifc", "au", "snd", "ape", "wv", "wvp",
    "tta", "mpc", "mpp", "mp+",
    // Windows / surround
    "wma", "asf", "ac3", "eac3", "dts", "dca",
    // RealAudio
    "ra", "rm", "ram",
    // Mobil
    "amr", "3gp", "3g2",
    // Konteynerler (ses parçası çalınır)
    "mp4", "mka", "mkv", "webm",
];

const COVER_NAMES: &[&str] = &[
    "cover.png", "cover.jpg", "cover.jpeg", "folder.png", "folder.jpg", "front.png",
    "front.jpg", "albumart.png", "albumart.jpg",
];

#[derive(Clone, Debug)]
pub struct Track {
    pub path: PathBuf,
    pub title: String,
    pub duration: f64,
    pub artist: Option<String>,
    pub album: Option<String>,
    pub year: Option<i32>,
    pub genre: Option<String>,
    pub track_no: Option<u32>,
    pub disc_no: Option<u32>,
}

impl Track {
    /// Başlık zaten sanatçı adıyla başlamıyorsa "Sanatçı - Başlık" üretir.
    pub fn display_title(&self) -> String {
        match &self.artist {
            Some(artist) if !artist.is_empty() && !self.title.starts_with(artist) => {
                format!("{artist} - {}", self.title)
            }
            _ => self.title.clone(),
        }
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Kind {
    Bundle,
    Playlist,
    Folder,
}

impl Kind {
    pub fn label(self) -> &'static str {
        match self {
            Kind::Bundle => "bundle (data.yaml)",
            Kind::Playlist => "playlist (m3u8)",
            Kind::Folder => "klasör taraması",
        }
    }
}

#[derive(Clone, Debug)]
pub struct Source {
    pub kind: Kind,
    pub root: PathBuf,
    pub name: String,
    pub title: Option<String>,
    pub artist: Option<String>,
    pub year: Option<i32>,
    pub genre: Option<String>,
    pub description: Option<String>,
    /// `data.yaml` içindeki `type`: `mixed` veya `album`.
    pub collection_type: Option<String>,
    pub tracks: Vec<Track>,
    pub cover: Option<PathBuf>,
}

impl Source {
    pub fn total(&self) -> f64 {
        self.tracks.iter().map(|t| t.duration).sum()
    }
}

// ---------------------------------------------------------------------------
// Süre önbelleği (ffprobe)
// ---------------------------------------------------------------------------

type CacheMap = HashMap<String, f64>;

fn cache() -> &'static Mutex<CacheMap> {
    static CACHE: OnceLock<Mutex<CacheMap>> = OnceLock::new();
    CACHE.get_or_init(|| {
        let mut map = CacheMap::new();
        if let Ok(text) = fs::read_to_string(cache_path()) {
            for line in text.lines() {
                if let Some((key, value)) = line.rsplit_once('\t') {
                    if let Ok(dur) = value.parse::<f64>() {
                        map.insert(key.to_string(), dur);
                    }
                }
            }
        }
        Mutex::new(map)
    })
}

fn cache_path() -> PathBuf {
    let base = std::env::var_os("XDG_CACHE_HOME")
        .map(PathBuf::from)
        .unwrap_or_else(|| PathBuf::from(std::env::var_os("HOME").unwrap_or_default()).join(".cache"));
    base.join("microsd-music-player").join("durations.tsv")
}

pub fn save_duration_cache() {
    let path = cache_path();
    if let Some(dir) = path.parent() {
        let _ = fs::create_dir_all(dir);
    }
    let map = cache().lock().unwrap_or_else(|e| e.into_inner());
    let mut out = String::new();
    for (key, value) in map.iter() {
        out.push_str(key);
        out.push('\t');
        out.push_str(&format!("{value:.3}"));
        out.push('\n');
    }
    let _ = fs::write(path, out);
}

pub fn probe_duration(path: &Path) -> f64 {
    let meta = fs::metadata(path).ok();
    let key = meta
        .as_ref()
        .map(|m| {
            let mtime = m
                .modified()
                .ok()
                .and_then(|t| t.duration_since(UNIX_EPOCH).ok())
                .map(|d| d.as_nanos())
                .unwrap_or(0);
            format!("{}\t{}\t{}", path.display(), mtime, m.len())
        })
        .unwrap_or_else(|| path.display().to_string());

    if let Some(d) = cache()
        .lock()
        .unwrap_or_else(|e| e.into_inner())
        .get(&key)
        .copied()
    {
        return d;
    }

    let mut duration = 180.0;
    if let Ok(output) = Command::new("ffprobe")
        .args([
            "-v",
            "error",
            "-show_entries",
            "format=duration",
            "-of",
            "default=noprint_wrappers=1:nokey=1",
        ])
        .arg(path)
        .output()
    {
        if output.status.success() {
            if let Ok(text) = String::from_utf8(output.stdout) {
                if let Ok(value) = text.trim().parse::<f64>() {
                    if value.is_finite() && value > 0.0 {
                        duration = value;
                    }
                }
            }
        }
    }

    cache()
        .lock()
        .unwrap_or_else(|e| e.into_inner())
        .insert(key, duration);
    duration
}

// ---------------------------------------------------------------------------
// Yardımcılar
// ---------------------------------------------------------------------------

pub fn is_audio(path: &Path) -> bool {
    path.extension()
        .and_then(|e| e.to_str())
        .map(|e| AUDIO_EXTS.contains(&e.to_ascii_lowercase().as_str()))
        .unwrap_or(false)
}

pub fn find_cover(start: &Path) -> Option<PathBuf> {
    let mut cur = if start.is_dir() {
        start.to_path_buf()
    } else {
        start.parent()?.to_path_buf()
    };
    for _ in 0..3 {
        for name in COVER_NAMES {
            let candidate = cur.join(name);
            if candidate.is_file() {
                return Some(candidate);
            }
        }
        if !cur.pop() {
            break;
        }
    }
    None
}

fn stem(path: &Path) -> String {
    path.file_stem()
        .map(|s| s.to_string_lossy().to_string())
        .unwrap_or_default()
}

fn push_track(tracks: &mut Vec<Track>, path: PathBuf, title: Option<&str>, duration: Option<f64>) {
    let title = title
        .map(|s| s.trim())
        .filter(|s| !s.is_empty())
        .map(|s| s.to_string())
        .unwrap_or_else(|| stem(&path));
    tracks.push(Track {
        path,
        title,
        duration: duration.filter(|d| *d > 0.0).unwrap_or(0.0),
        artist: None,
        album: None,
        year: None,
        genre: None,
        track_no: None,
        disc_no: None,
    });
}

fn clean_opt(value: Option<String>) -> Option<String> {
    value
        .map(|s| s.trim().to_string())
        .filter(|s| !s.is_empty())
}

// ---------------------------------------------------------------------------
// data.yaml paketi
// ---------------------------------------------------------------------------

#[derive(Deserialize)]
struct BundleYaml {
    #[serde(default)]
    cover: Option<String>,
    #[serde(default, alias = "name", alias = "album", alias = "collection")]
    title: Option<String>,
    #[serde(default, alias = "album_artist", alias = "albumartist", alias = "artist")]
    artist: Option<String>,
    #[serde(default, alias = "album_title")]
    album: Option<String>,
    #[serde(default)]
    year: Option<i32>,
    #[serde(default)]
    genre: Option<String>,
    #[serde(default)]
    description: Option<String>,
    #[serde(default, rename = "type", alias = "kind", alias = "collection_type")]
    collection_type: Option<String>,
    #[serde(default, alias = "tracks")]
    playlist: Vec<BundleEntry>,
}

#[derive(Deserialize)]
struct BundleEntry {
    file: String,
    #[serde(default)]
    title: Option<String>,
    #[serde(default, alias = "performer")]
    artist: Option<String>,
    #[serde(default)]
    album: Option<String>,
    #[serde(default)]
    year: Option<i32>,
    #[serde(default)]
    genre: Option<String>,
    #[serde(default, alias = "track_number", alias = "tracknumber", alias = "number")]
    track: Option<u32>,
    #[serde(default, alias = "disc_number", alias = "discnumber")]
    disc: Option<u32>,
    #[serde(default)]
    duration_sec: Option<f64>,
}

fn parse_bundle(root: &Path) -> Result<Source> {
    let yaml_path = root.join("data.yaml");
    let text = fs::read_to_string(&yaml_path)
        .with_context(|| format!("data.yaml okunamadı: {}", yaml_path.display()))?;
    let meta: BundleYaml =
        serde_yaml::from_str(&text).with_context(|| "data.yaml ayrıştırılamadı")?;

    let mut tracks = Vec::new();
    for entry in &meta.playlist {
        let path = if Path::new(&entry.file).is_absolute() {
            PathBuf::from(&entry.file)
        } else {
            root.join(&entry.file)
        };
        if !path.is_file() {
            eprintln!("[uyarı] bulunamadı, atlandı: {}", path.display());
            continue;
        }
        push_track(&mut tracks, path, entry.title.as_deref(), entry.duration_sec);
        if let Some(track) = tracks.last_mut() {
            track.artist = clean_opt(entry.artist.clone());
            track.album = clean_opt(entry.album.clone());
            track.year = entry.year;
            track.genre = clean_opt(entry.genre.clone());
            track.track_no = entry.track;
            track.disc_no = entry.disc;
        }
    }

    for track in &mut tracks {
        if track.duration <= 0.0 {
            track.duration = probe_duration(&track.path);
        }
    }

    let cover = meta
        .cover
        .as_deref()
        .map(|c| root.join(c))
        .filter(|p| p.is_file())
        .or_else(|| tracks.first().and_then(|t| find_cover(&t.path)));

    let bundle_artist = clean_opt(meta.artist).or_else(|| {
        let mut artists: std::collections::HashSet<String> = std::collections::HashSet::new();
        for t in &tracks {
            if let Some(a) = &t.artist {
                artists.insert(a.clone());
            }
        }
        if artists.len() == 1 {
            artists.into_iter().next()
        } else if artists.is_empty() {
            None
        } else {
            Some("Çeşitli Sanatçılar".to_string())
        }
    });

    let title = clean_opt(meta.title).or_else(|| clean_opt(meta.album));
    let name = title.clone().unwrap_or_else(|| {
        bundle_artist
            .clone()
            .filter(|a| a != "Çeşitli Sanatçılar")
            .or_else(|| {
                root.file_name()
                    .map(|s| s.to_string_lossy().to_string())
            })
            .unwrap_or_else(|| root.display().to_string())
    });

    let mut collection_type = clean_opt(meta.collection_type);
    if collection_type.is_none() {
        let mut artists: std::collections::HashSet<String> = std::collections::HashSet::new();
        for t in &tracks {
            if let Some(a) = &t.artist {
                artists.insert(a.clone());
            }
        }
        collection_type = Some(if artists.len() > 1 { "mixed" } else { "album" }.to_string());
    }

    Ok(Source {
        kind: Kind::Bundle,
        root: root.to_path_buf(),
        name,
        title,
        artist: bundle_artist,
        year: meta.year,
        genre: clean_opt(meta.genre),
        description: clean_opt(meta.description),
        collection_type,
        tracks,
        cover,
    })
}

// ---------------------------------------------------------------------------
// m3u / m3u8 listesi
// ---------------------------------------------------------------------------

fn parse_playlist(path: &Path) -> Result<Source> {
    let root = path.parent().unwrap_or(Path::new(".")).to_path_buf();
    let text = fs::read_to_string(path)
        .with_context(|| format!("çalma listesi okunamadı: {}", path.display()))?;

    let mut name = stem(path);
    let mut tracks = Vec::new();
    let mut pending_duration: Option<f64> = None;
    let mut pending_title: Option<String> = None;

    for raw in text.lines() {
        let line = raw.trim();
        if line.is_empty() {
            continue;
        }
        if line.starts_with("#EXTM3U") {
            continue;
        }
        if let Some(rest) = line.strip_prefix("#PLAYLIST:") {
            let candidate = rest.trim();
            if !candidate.is_empty() {
                name = candidate.to_string();
            }
            continue;
        }
        if let Some(rest) = line.strip_prefix("#EXTINF:") {
            let (dur, title) = rest.split_once(',').unwrap_or((rest, ""));
            pending_duration = dur.trim().parse::<f64>().ok();
            pending_title = Some(title.trim().to_string()).filter(|s| !s.is_empty());
            continue;
        }
        if line.starts_with('#') {
            continue;
        }

        let candidate = Path::new(line);
        let file_path = if candidate.is_absolute() {
            candidate.to_path_buf()
        } else {
            root.join(candidate)
        };

        if !file_path.is_file() {
            eprintln!("[uyarı] bulunamadı, atlandı: {}", file_path.display());
            pending_duration = None;
            pending_title = None;
            continue;
        }

        let duration = pending_duration.filter(|d| *d > 0.0);
        push_track(&mut tracks, file_path, pending_title.as_deref(), duration);
        pending_duration = None;
        pending_title = None;
    }

    for track in &mut tracks {
        if track.duration <= 0.0 {
            track.duration = probe_duration(&track.path);
        }
    }

    let cover = tracks.first().and_then(|t| find_cover(&t.path));
    Ok(Source {
        kind: Kind::Playlist,
        root,
        name: name.clone(),
        title: Some(name),
        artist: None,
        year: None,
        genre: None,
        description: None,
        collection_type: None,
        tracks,
        cover,
    })
}

// ---------------------------------------------------------------------------
// Klasör taraması
// ---------------------------------------------------------------------------

fn collect_audio(dir: &Path, out: &mut Vec<PathBuf>) {
    let Ok(read_dir) = fs::read_dir(dir) else {
        return;
    };
    let mut entries: Vec<_> = read_dir.filter_map(|e| e.ok()).collect();
    entries.sort_by_key(|e| e.file_name());
    for entry in entries {
        let path = entry.path();
        if path.is_dir() {
            collect_audio(&path, out);
        } else if is_audio(&path) {
            out.push(path);
        }
    }
}

fn scan_folder(root: &Path) -> Source {
    let mut paths = Vec::new();
    collect_audio(root, &mut paths);

    let mut tracks = Vec::new();
    for path in paths {
        let title = path
            .strip_prefix(root)
            .ok()
            .filter(|rel| rel.components().count() >= 3)
            .and_then(|rel| rel.components().next())
            .map(|first| format!("{} - {}", first.as_os_str().to_string_lossy(), stem(&path)));
        push_track(&mut tracks, path, title.as_deref(), None);
    }

    for track in &mut tracks {
        track.duration = probe_duration(&track.path);
    }

    let cover = tracks.first().and_then(|t| find_cover(&t.path));
    let name = root
        .file_name()
        .map(|s| s.to_string_lossy().to_string())
        .unwrap_or_else(|| root.display().to_string());
    Source {
        kind: Kind::Folder,
        root: root.to_path_buf(),
        name: name.clone(),
        title: Some(name),
        artist: None,
        year: None,
        genre: None,
        description: None,
        collection_type: None,
        tracks,
        cover,
    }
}

// ---------------------------------------------------------------------------
// Algılama
// ---------------------------------------------------------------------------

pub fn detect_source(path: &Path) -> Result<Source> {
    if !path.exists() {
        bail!("Kaynak bulunamadı: {}", path.display());
    }

    if path.is_file() {
        let ext = path
            .extension()
            .and_then(|e| e.to_str())
            .unwrap_or_default()
            .to_ascii_lowercase();
        if ext == "m3u" || ext == "m3u8" {
            return parse_playlist(path);
        }
        bail!("Dosya kaynağı için .m3u/.m3u8 bekleniyor.");
    }

    if path.join("data.yaml").is_file() {
        return parse_bundle(path);
    }

    let mut playlists: Vec<PathBuf> = fs::read_dir(path)
        .map(|rd| {
            rd.filter_map(|e| e.ok())
                .map(|e| e.path())
                .filter(|p| {
                    p.is_file()
                        && p.extension()
                            .and_then(|e| e.to_str())
                            .map(|e| matches!(e.to_ascii_lowercase().as_str(), "m3u" | "m3u8"))
                            .unwrap_or(false)
                })
                .collect()
        })
        .unwrap_or_default();
    playlists.sort();
    if let Some(first) = playlists.first() {
        return parse_playlist(first);
    }

    Ok(scan_folder(path))
}

pub fn auto_detect() -> Result<PathBuf> {
    let user = std::env::var("USER").unwrap_or_default();
    let media = Path::new("/run/media").join(user);
    if media.is_dir() {
        let mut candidates: Vec<PathBuf> = fs::read_dir(&media)
            .map(|rd| rd.filter_map(|e| e.ok()).map(|e| e.path()).collect())
            .unwrap_or_default();
        candidates.sort();
        for candidate in candidates {
            if candidate.join("data.yaml").is_file() {
                return Ok(candidate);
            }
        }
    }

    let music = PathBuf::from(std::env::var_os("HOME").unwrap_or_default()).join("Music");
    if music.is_dir() {
        return Ok(music);
    }

    bail!("Kaynak bulunamadı; --source ile bir klasör verin.");
}
