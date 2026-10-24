//! microSD Music Player
//!
//! İki kaynak yapısını otomatik algılar:
//!
//! 1. Paket (microSD) yapısı:
//!        <klasör>/
//!          cover.png
//!          data.yaml        # cover + playlist (file/title/duration_sec)
//!          *.opus / *.mp3 ...
//!
//! 2. Kütüphane yapısı (~/Music gibi):
//!        <klasör>/
//!          Asure.m3u8       # #EXTINF süre,başlık + göreli dosya yolu
//!          Sanatçı/Albüm/Şarkı.opus ...
//!
//! Uygulama açılır, kaynağı algılar, 45:00 hedefli bir blok kurar ve doğrudan
//! çalmaya başlar. Blok toplamı en fazla 45:45 olabilir (+45 sn esneme payı).
//! Kaynak 45 dakikadan kısaysa liste döngüye alınır ve 45:45'te fade-out ile
//! durur.

mod export;
mod plan;
mod source;

use std::path::PathBuf;

use anyhow::{bail, Result};

use crate::plan::fmt_time;

fn print_help() {
    println!(
        "microSD Music Player

Kullanım:
  microsd-music-player [seçenekler]

Seçenekler:
  -s, --source <YOL>   Paket klasörü, .m3u8 ya da müzik klasörü
                       (verilmezse: bağlı microSD paketi, yoksa ~/Music)
      --list           45 dakikalık bloğu listele ve çık
  -e, --export <DEST>  Bloğu data.yaml + kapak + dosyalar olarak yaz
      --shuffle        Blok seçiminden önce listeyi karıştır
      --no-autoplay    Arayüz açılırken çalma
  -h, --help           Bu yardımı göster"
    );
}

fn print_plan(source: &source::Source, plan: &plan::Plan) {
    println!("Kaynak : {} · {}", source.kind.label(), source.root.display());
    println!("Plan   : {}", plan::summary(plan));
    println!("Toplam : {}", fmt_time(plan.total()));
    for (i, item) in plan.items.iter().enumerate() {
        println!(
            "  {:02}. {:>6}  {}",
            i + 1,
            fmt_time(item.duration),
            item.display_title()
        );
    }
}

fn main() -> Result<()> {
    let mut source_path: Option<PathBuf> = None;
    let mut list = false;
    let mut export_dir: Option<PathBuf> = None;
    let mut shuffle = false;
    let mut autoplay = true;

    let args: Vec<String> = std::env::args().skip(1).collect();
    let mut i = 0;
    while i < args.len() {
        match args[i].as_str() {
            "-s" | "--source" => {
                i += 1;
                match args.get(i) {
                    Some(value) => source_path = Some(PathBuf::from(value)),
                    None => bail!("--source için bir yol gerekli"),
                }
            }
            "--list" => list = true,
            "-e" | "--export" => {
                i += 1;
                match args.get(i) {
                    Some(value) => export_dir = Some(PathBuf::from(value)),
                    None => bail!("--export için bir hedef gerekli"),
                }
            }
            "--shuffle" => shuffle = true,
            "--no-autoplay" => autoplay = false,
            "-h" | "--help" => {
                print_help();
                return Ok(());
            }
            other => {
                eprintln!("Bilinmeyen argüman: {other}");
                print_help();
                std::process::exit(2);
            }
        }
        i += 1;
    }

    let path = match source_path {
        Some(path) => path,
        None => source::auto_detect()?,
    };
    let src = source::detect_source(&path)?;
    if src.tracks.is_empty() {
        bail!("Kaynakta oynatılabilir ses dosyası yok: {}", path.display());
    }

    let pl = plan::build_plan(&src, shuffle)?;
    source::save_duration_cache();

    if list {
        print_plan(&src, &pl);
        return Ok(());
    }

    if let Some(dest) = export_dir {
        let out = export::export_bundle(&pl, &src, &dest)?;
        println!("Yazıldı: {}", out.display());
        println!("{}", plan::summary(&pl));
        return Ok(());
    }

    println!("GUI devre dışı bırakıldı.");
    Ok(())
}
