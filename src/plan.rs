//! 45 dakikalık blok planlama (+45 sn esneme payı).
//!
//! Blok, gerçek (ffprobe) sürelerin yukarı yuvarlanmış toplamıyla ve hedef
//! 45:00'a en yakın olacak şekilde seçilir. Üst sınır 45:45'tir; toplam süre
//! bu değeri asla geçmez.

use std::time::{SystemTime, UNIX_EPOCH};

use anyhow::{bail, Result};

use crate::source::{probe_duration, Source, Track};

pub const TARGET_SEC: i32 = 45 * 60; // hedef: 45:00
pub const SLACK_SEC: i32 = 45; // esneme payı: +45 sn
pub const CAP_SEC: i32 = TARGET_SEC + SLACK_SEC; // kesin üst sınır: 45:45
const PLAN_FLOOR: i32 = TARGET_SEC - SLACK_SEC; // 44:15 alt sınır

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Mode {
    Block,
    Loop,
}

#[derive(Clone, Debug)]
pub struct Plan {
    pub items: Vec<Track>,
    pub mode: Mode,
    pub stop_at: Option<f64>,
    pub fade: f64,
    pub source_name: String,
}

impl Plan {
    pub fn total(&self) -> f64 {
        self.stop_at
            .unwrap_or_else(|| self.items.iter().map(|t| t.duration).sum())
    }
}

pub fn fmt_time(sec: f64) -> String {
    let sec = sec.max(0.0) as i64;
    format!("{}:{:02}", sec / 60, sec % 60)
}

/// Şarkıları bölmeden, toplamı [floor, cap] aralığında ve `ideal` süreye en
/// yakın olan bir alt küme seçer. Eşit uzaklıkta üst tarafı (esneme payını)
/// tercih eder.
fn select_block(tracks: &[Track], floor: i32, ideal: i32, cap: i32) -> Option<Vec<usize>> {
    let cap = cap as usize;
    let target = floor.max(0) as usize;
    let ideal = ideal as i64;

    let mut reachable = vec![false; cap + 1];
    let mut parent_sum = vec![-1i32; cap + 1];
    let mut parent_idx = vec![-1i32; cap + 1];
    reachable[0] = true;

    for (idx, track) in tracks.iter().enumerate() {
        let dur = track.duration.ceil().max(1.0) as i64 as usize;
        if dur > cap {
            continue;
        }
        let mut s = cap - dur;
        loop {
            if reachable[s] && !reachable[s + dur] {
                reachable[s + dur] = true;
                parent_sum[s + dur] = s as i32;
                parent_idx[s + dur] = idx as i32;
            }
            if s == 0 {
                break;
            }
            s -= 1;
        }
    }

    // 45:00'a en yakın toplamı seç; 45:45'i asla geçme.
    let goal = (target..=cap)
        .filter(|s| reachable[*s])
        .min_by_key(|s| ((*s as i64 - ideal).abs(), std::cmp::Reverse(*s)))?;
    let mut picked = Vec::new();
    let mut s = goal;
    while parent_sum[s] >= 0 {
        picked.push(parent_idx[s] as usize);
        s = parent_sum[s] as usize;
    }
    picked.reverse();
    picked.sort_unstable();
    Some(picked)
}

fn shuffle_tracks(tracks: &mut [Track]) {
    let mut seed = SystemTime::now()
        .duration_since(UNIX_EPOCH)
        .map(|d| d.subsec_nanos() as u64)
        .unwrap_or(0x9e3779b97f4a7c15)
        | 1;
    for i in (1..tracks.len()).rev() {
        seed = seed
            .wrapping_mul(6364136223846793005)
            .wrapping_add(1442695040888963407);
        let j = ((seed >> 33) as usize) % (i + 1);
        tracks.swap(i, j);
    }
}

fn resolve_actual_durations(tracks: &mut [Track]) {
    let n = tracks.len();
    if n == 0 {
        return;
    }
    let workers = std::thread::available_parallelism()
        .map(|v| v.get())
        .unwrap_or(4)
        .clamp(1, 8)
        .min(n);
    let chunk_size = n.div_ceil(workers);
    let mut durations = vec![0.0f64; n];

    std::thread::scope(|scope| {
        for (track_chunk, dur_chunk) in tracks
            .chunks(chunk_size)
            .zip(durations.chunks_mut(chunk_size))
        {
            scope.spawn(move || {
                for (track, dur) in track_chunk.iter().zip(dur_chunk.iter_mut()) {
                    *dur = probe_duration(&track.path);
                }
            });
        }
    });

    for (track, dur) in tracks.iter_mut().zip(durations) {
        if dur > 0.0 {
            track.duration = dur;
        }
    }
}

pub fn build_plan(source: &Source, shuffle: bool) -> Result<Plan> {
    let mut tracks: Vec<Track> = source
        .tracks
        .iter()
        .filter(|t| t.path.is_file())
        .cloned()
        .collect();

    if tracks.is_empty() {
        bail!("Kaynakta oynatılabilir ses dosyası yok.");
    }

    resolve_actual_durations(&mut tracks);

    if shuffle {
        shuffle_tracks(&mut tracks);
    }

    let picked = select_block(&tracks, PLAN_FLOOR, TARGET_SEC, CAP_SEC)
        .or_else(|| select_block(&tracks, TARGET_SEC - 120, TARGET_SEC, CAP_SEC));

    if let Some(indices) = picked {
        let items = indices.into_iter().map(|i| tracks[i].clone()).collect();
        return Ok(Plan {
            items,
            mode: Mode::Block,
            stop_at: None,
            fade: 0.0,
            source_name: source.name.clone(),
        });
    }

    // Kaynak 45 dakikadan kısa: döngüye al, hedefte fade-out ile dur.
    let total: f64 = tracks.iter().map(|t| t.duration).sum();
    let total = total.max(1.0);
    let repeats = (((CAP_SEC as f64 + 1.0) / total).ceil() as usize + 1).max(2);
    let mut items = Vec::with_capacity(tracks.len() * repeats);
    for _ in 0..repeats {
        items.extend(tracks.iter().cloned());
    }

    Ok(Plan {
        items,
        mode: Mode::Loop,
        stop_at: Some(CAP_SEC as f64),
        fade: 6.0,
        source_name: source.name.clone(),
    })
}

pub fn summary(plan: &Plan) -> String {
    match plan.mode {
        Mode::Block => format!(
            "Blok: {} / {} şarkı (hedef {}, üst sınır {})",
            fmt_time(plan.total()),
            plan.items.len(),
            fmt_time(TARGET_SEC as f64),
            fmt_time(CAP_SEC as f64)
        ),
        Mode::Loop => format!(
            "Döngü: {}'da fade-out ({}, hedef {})",
            fmt_time(plan.total()),
            plan.source_name,
            fmt_time(TARGET_SEC as f64)
        ),
    }
}
