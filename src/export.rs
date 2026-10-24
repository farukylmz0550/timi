//! Seçilen bloğu `data.yaml` + kapak + ses dosyaları olarak, kaynağın göreli
//! klasör yapısını koruyarak diske yazar.

use std::collections::HashSet;
use std::fs;
use std::path::{Component, Path, PathBuf};

use anyhow::{Context, Result};
use serde::Serialize;

use crate::plan::Plan;
use crate::source::{find_cover, Source};

#[derive(Serialize)]
struct OutBundle {
    version: u32,
    #[serde(rename = "type", skip_serializing_if = "Option::is_none")]
    collection_type: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    title: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    artist: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    year: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    genre: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    description: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    cover: Option<String>,
    playlist: Vec<OutEntry>,
}

#[derive(Serialize)]
struct OutEntry {
    file: String,
    title: String,
    #[serde(skip_serializing_if = "Option::is_none")]
    artist: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    album: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    year: Option<i32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    genre: Option<String>,
    #[serde(skip_serializing_if = "Option::is_none")]
    track: Option<u32>,
    #[serde(skip_serializing_if = "Option::is_none")]
    disc: Option<u32>,
}

fn relative_parts(path: &Path, root: &Path) -> Vec<String> {
    path.strip_prefix(root)
        .map(|rel| {
            rel.components()
                .filter_map(|c| match c {
                    Component::Normal(s) => Some(s.to_string_lossy().to_string()),
                    _ => None,
                })
                .collect()
        })
        .unwrap_or_default()
}

/// Etiket yoksa klasör yapısından sanatçı tahmin eder:
/// `Sanatçı/Albüm/Şarkı.opus` → `Sanatçı`.
fn infer_artist(source: &Source, track: &crate::source::Track) -> Option<String> {
    if let Some(artist) = &track.artist {
        return Some(artist.clone());
    }
    let parts = relative_parts(&track.path, &source.root);
    (parts.len() >= 3).then(|| parts[0].clone())
}

/// Etiket yoksa klasör yapısından albüm tahmin eder:
/// `Sanatçı/Albüm/Şarkı.opus` → `Albüm`.
fn infer_album(source: &Source, track: &crate::source::Track) -> Option<String> {
    if let Some(album) = &track.album {
        return Some(album.clone());
    }
    let parts = relative_parts(&track.path, &source.root);
    (parts.len() >= 2).then(|| parts[parts.len() - 2].clone())
}

fn relative_to_source(path: &Path, root: &Path) -> PathBuf {
    path.strip_prefix(root)
        .map(|p| p.to_path_buf())
        .ok()
        .filter(|p| !p.as_os_str().is_empty())
        .unwrap_or_else(|| {
            path.file_name()
                .map(PathBuf::from)
                .unwrap_or_else(|| PathBuf::from("track"))
        })
}

/// FAT/exFAT dosya adlarında yasak olan karakterleri temizler.
fn sanitize_component(name: &str) -> String {
    let mut out: String = name
        .chars()
        .map(|c| match c {
            '<' | '>' | ':' | '"' | '/' | '\\' | '|' | '?' | '*' => '_',
            c if (c as u32) < 0x20 => '_',
            c => c,
        })
        .collect();
    while out.ends_with(' ') || out.ends_with('.') {
        out.pop();
    }
    if out.is_empty() {
        out.push('_');
    }
    out
}

fn sanitize_relative(rel: &Path) -> PathBuf {
    rel.components()
        .map(|c| PathBuf::from(sanitize_component(&c.as_os_str().to_string_lossy())))
        .collect()
}

pub fn export_bundle(plan: &Plan, source: &Source, dest: &Path) -> Result<PathBuf> {
    // Doğrudan hedefe yaz: ek bir "<ad> - 45 dk" sarmalayıcı klasörü oluşturma.
    let dest = dest.to_path_buf();
    fs::create_dir_all(&dest)
        .with_context(|| format!("hedef klasör oluşturulamadı: {}", dest.display()))?;

    let mut seen: HashSet<PathBuf> = HashSet::new();
    let mut entries: Vec<OutEntry> = Vec::new();

    for track in &plan.items {
        // Döngü modunda aynı dosya birden çok kez gelebilir; YAML'a bir kez yaz.
        if !seen.insert(track.path.clone()) {
            continue;
        }
        let rel = sanitize_relative(&relative_to_source(&track.path, &source.root));
        let target = dest.join(&rel);
        if let Some(parent) = target.parent() {
            fs::create_dir_all(parent)?;
        }
        fs::copy(&track.path, &target).with_context(|| {
            format!(
                "kopyalanamadı: {} -> {}",
                track.path.display(),
                target.display()
            )
        })?;

        let track_no = track.track_no.unwrap_or(entries.len() as u32 + 1);
        entries.push(OutEntry {
            file: rel.to_string_lossy().replace('\\', "/"),
            title: track.title.clone(),
            artist: infer_artist(source, track),
            album: infer_album(source, track),
            year: track.year,
            genre: track.genre.clone(),
            track: Some(track_no),
            disc: track.disc_no,
        });
    }

    let artists: HashSet<&str> = entries.iter().filter_map(|e| e.artist.as_deref()).collect();
    let collection_type = source.collection_type.clone().or_else(|| {
        if artists.len() > 1 {
            Some("mixed".to_string())
        } else if artists.len() == 1 {
            Some("album".to_string())
        } else {
            None
        }
    });
    let artist = source.artist.clone().or_else(|| {
        if artists.len() == 1 {
            artists.iter().next().map(|a| a.to_string())
        } else if artists.len() > 1 {
            Some("Çeşitli Sanatçılar".to_string())
        } else {
            None
        }
    });
    let title = source.title.clone().or_else(|| Some(source.name.clone()));

    let cover = source
        .cover
        .clone()
        .or_else(|| plan.items.first().and_then(|t| find_cover(&t.path)));
    let mut cover_name = None;
    if let Some(cover_path) = cover {
        let ext = cover_path
            .extension()
            .map(|e| e.to_string_lossy().to_lowercase())
            .unwrap_or_else(|| "png".to_string());
        let name = format!("cover.{ext}");
        fs::copy(&cover_path, dest.join(&name)).with_context(|| {
            format!("kapak kopyalanamadı: {}", cover_path.display())
        })?;
        cover_name = Some(name);
    }

    let bundle = OutBundle {
        version: 2,
        collection_type,
        title,
        artist,
        year: source.year,
        genre: source.genre.clone(),
        description: source.description.clone(),
        cover: cover_name,
        playlist: entries,
    };
    let yaml = serde_yaml::to_string(&bundle).context("data.yaml üretilemedi")?;
    fs::write(dest.join("data.yaml"), yaml)?;

    Ok(dest)
}
