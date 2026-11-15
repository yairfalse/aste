//! Offline evidence inspection, not a DSP implementation or listening judgment.
//! Supports only the repository's little-endian IEEE float32 RIFF WAV exports.
use std::{env, fs, path::Path, process::ExitCode};

type Result<T> = std::result::Result<T, Box<dyn std::error::Error>>;

struct Audio {
    rate: u32,
    channels: usize,
    samples: Vec<f64>,
}

fn parse(bytes: &[u8]) -> Result<Audio> {
    if bytes.len() < 12 || &bytes[..4] != b"RIFF" || &bytes[8..12] != b"WAVE" {
        return Err("expected RIFF WAVE".into());
    }
    let size = u32::from_le_bytes(bytes[4..8].try_into()?) as usize + 8;
    if size != bytes.len() {
        return Err("RIFF size mismatch".into());
    }
    let mut format = None;
    let mut data = None;
    let mut at = 12;
    while at < bytes.len() {
        if bytes.len() - at < 8 {
            return Err("truncated chunk header".into());
        }
        let n = u32::from_le_bytes(bytes[at + 4..at + 8].try_into()?) as usize;
        let end = at
            .checked_add(8)
            .and_then(|x| x.checked_add(n))
            .ok_or("chunk overflow")?;
        let body = bytes.get(at + 8..end).ok_or("truncated chunk")?;
        match &bytes[at..at + 4] {
            b"fmt " => {
                if format.is_some() || n < 16 {
                    return Err("invalid or duplicate format".into());
                }
                let tag = u16::from_le_bytes(body[0..2].try_into()?);
                let channels = u16::from_le_bytes(body[2..4].try_into()?) as usize;
                let rate = u32::from_le_bytes(body[4..8].try_into()?);
                let byte_rate = u32::from_le_bytes(body[8..12].try_into()?) as u64;
                let align = u16::from_le_bytes(body[12..14].try_into()?) as usize;
                let bits = u16::from_le_bytes(body[14..16].try_into()?);
                if tag != 3
                    || !(1..=2).contains(&channels)
                    || bits != 32
                    || rate == 0
                    || align != channels * 4
                    || byte_rate != rate as u64 * align as u64
                {
                    return Err("expected consistent mono/stereo IEEE float32 format".into());
                }
                format = Some((rate, channels));
            }
            b"data" => {
                if data.replace(body).is_some() {
                    return Err("duplicate audio chunk".into());
                }
            }
            _ => {}
        }
        at = end.checked_add(n % 2).ok_or("padding overflow")?;
        if at > bytes.len() {
            return Err("missing chunk padding".into());
        }
    }
    let (rate, channels) = format.ok_or("missing format")?;
    let data = data.ok_or("missing audio")?;
    if data.is_empty() || data.len() % (4 * channels) != 0 {
        return Err("empty or incomplete frames".into());
    }
    let samples: Vec<f64> = data
        .chunks_exact(4)
        .map(|x| f32::from_le_bytes(x.try_into().expect("four-byte chunk")) as f64)
        .collect();
    if samples.iter().any(|x| !x.is_finite()) {
        return Err("non-finite audio".into());
    }
    Ok(Audio {
        rate,
        channels,
        samples,
    })
}

fn read(path: &Path) -> Result<Audio> {
    parse(&fs::read(path)?)
}
fn rms(samples: &[f64]) -> f64 {
    (samples.iter().map(|x| x * x).sum::<f64>() / samples.len() as f64).sqrt()
}
fn db(value: f64) -> String {
    if value == 0.0 {
        "-inf".into()
    } else {
        format!("{:.9}", 20.0 * value.log10())
    }
}
fn quoted(value: &str) -> String {
    format!("\"{}\"", value.replace('"', "\"\""))
}

fn scan(path: &Path) -> Result<usize> {
    if path.is_dir() {
        let mut children = fs::read_dir(path)?
            .map(|e| e.map(|e| e.path()))
            .collect::<std::io::Result<Vec<_>>>()?;
        children.sort();
        let mut count = 0;
        for child in children {
            count += scan(&child)?;
        }
        return Ok(count);
    } else if path.extension().is_some_and(|e| e == "wav") {
        let a = read(path)?;
        let frames = a.samples.len() / a.channels;
        let peak = a.samples.iter().fold(0.0_f64, |p, x| p.max(x.abs()));
        let mut max_step = 0.0_f64;
        let mut quiet_windows = 0;
        let mut windows = 0;
        // Complete nonoverlapping 10 ms windows, combined channel RMS.
        let window_frames = (a.rate as usize / 100).max(1);
        for window in a.samples.chunks_exact(window_frames * a.channels) {
            windows += 1;
            if rms(window) < 0.001 {
                quiet_windows += 1;
            }
        }
        for i in a.channels..a.samples.len() {
            max_step = max_step.max((a.samples[i] - a.samples[i - a.channels]).abs());
        }
        let near_peak = a
            .samples
            .iter()
            .filter(|x| peak > 0.0 && x.abs() >= peak * 0.99)
            .count();
        let over_unity = a.samples.iter().filter(|x| x.abs() > 1.0).count();
        let exact_mono = a.channels == 1 || a.samples.chunks_exact(2).all(|s| s[0] == s[1]);
        println!(
            "{},{},{},{},{},{},{},{:.9},{},{},{},{}",
            quoted(&path.display().to_string()),
            a.rate,
            a.channels,
            frames,
            db(peak),
            db(rms(&a.samples)),
            db(max_step),
            near_peak as f64 / a.samples.len() as f64,
            over_unity,
            quiet_windows,
            windows,
            exact_mono
        );
        return Ok(1);
    }
    Ok(0)
}

// Least-squares scalar match removes gain only, not EQ, timing or phase changes.
fn residual(a: &[f64], b: &[f64]) -> Result<(f64, f64, f64)> {
    if a.len() != b.len() || a.is_empty() {
        return Err("incompatible sample counts".into());
    }
    let aa = a.iter().map(|x| x * x).sum::<f64>();
    let bb = b.iter().map(|x| x * x).sum::<f64>();
    if aa == 0.0 || bb == 0.0 {
        return Err("silent comparison: ratio unknown".into());
    }
    let scale = a.iter().zip(b).map(|(x, y)| x * y).sum::<f64>() / aa;
    let difference = a
        .iter()
        .zip(b)
        .map(|(x, y)| (y - scale * x).powi(2))
        .sum::<f64>();
    Ok((
        scale,
        (difference / bb).sqrt(),
        a.iter()
            .zip(b)
            .map(|(x, y)| (x - y).abs())
            .fold(0.0, f64::max),
    ))
}

fn compare(a: &Path, b: &Path) -> Result<()> {
    let left = read(a)?;
    let right = read(b)?;
    if left.rate != right.rate || left.channels != right.channels {
        return Err("incompatible WAV formats".into());
    }
    let (scale, ratio, delta) = residual(&left.samples, &right.samples)?;
    println!(
        "{},{},{:.12},{},{:.12}",
        quoted(&a.display().to_string()),
        quoted(&b.display().to_string()),
        scale,
        db(ratio),
        delta
    );
    Ok(())
}

fn suite(root: &Path) -> Result<()> {
    println!("reference,candidate,least_squares_scale,residual_relative_db,max_sample_difference");
    for product in [
        "density", "harmonic", "sequence", "loop", "impulse", "field",
    ] {
        let directory = root.join(product).join("audio");
        let mut files = fs::read_dir(&directory)?
            .map(|e| e.map(|e| e.path()))
            .collect::<std::io::Result<Vec<_>>>()?;
        files.sort();
        let mut comparisons = 0;
        for file in files {
            let name = file
                .file_name()
                .and_then(|n| n.to_str())
                .ok_or("invalid file name")?;
            if let Some(fixture) = name.strip_suffix("_after_start.wav") {
                for variant in ["before_start", "same_controls_legacy", "current_extreme"] {
                    compare(&directory.join(format!("{fixture}_{variant}.wav")), &file)?;
                    comparisons += 1;
                }
            }
        }
        if comparisons == 0 {
            return Err(format!("no comparison audio for {product}").into());
        }
    }
    Ok(())
}

// Independent arithmetic witness for the expression in Impulse::process.
// This does NOT invoke production DSP, measure audio, or reproduce host behavior.
fn clocks() {
    println!("rate,bpm,ratchet,absolute_sample,block_a,block_b,tick_a,tick_b");
    for rate in [44100., 48000., 88200., 96000., 176400., 192000.] {
        for bpm in [80., 100., 120., 137.] {
            for ratchet in [1., 2., 3., 4.] {
                for sample in 0..(rate as usize * 8) {
                    let tick = |block| {
                        let start = sample / block * block;
                        let ppq = start as f64 * bpm / (60. * rate);
                        ((ppq + bpm / (60. * rate) * (sample - start) as f64) / (0.25 / ratchet))
                            .floor() as i64
                    };
                    let a = tick(127);
                    let b = tick(511);
                    if a != b {
                        println!("{rate},{bpm},{ratchet},{sample},127,511,{a},{b}");
                        break;
                    }
                }
            }
        }
    }
}

fn run() -> Result<()> {
    let args: Vec<_> = env::args().collect();
    match args.get(1).map(String::as_str) {
        Some("scan") if args.len() == 3 => {
            println!("file,rate,channels,frames,peak_dbfs,rms_dbfs,max_adjacent_step_dbfs,fraction_within_1pct_peak,samples_over_unity,quiet_10ms_windows,total_10ms_windows,exact_mono");
            if scan(Path::new(&args[2]))? == 0 {
                return Err("no WAV evidence found; coverage unknown".into());
            }
            Ok(())
        }
        Some("compare") if args.len() == 4 => compare(Path::new(&args[2]), Path::new(&args[3])),
        Some("suite") if args.len() == 3 => suite(Path::new(&args[2])),
        Some("clocks") if args.len() == 2 => {
            clocks();
            Ok(())
        }
        _ => {
            Err("usage: inspect scan DIRECTORY | compare A.wav B.wav | suite PACK | clocks".into())
        }
    }
}

fn main() -> ExitCode {
    match run() {
        Ok(()) => ExitCode::SUCCESS,
        Err(e) => {
            eprintln!("audit inspection failed: {e}");
            ExitCode::FAILURE
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    fn wave(value: f32) -> Vec<u8> {
        let mut b = b"RIFF".to_vec();
        b.extend(40_u32.to_le_bytes());
        b.extend(b"WAVEfmt ");
        b.extend(16_u32.to_le_bytes());
        b.extend(3_u16.to_le_bytes());
        b.extend(1_u16.to_le_bytes());
        b.extend(48000_u32.to_le_bytes());
        b.extend(192000_u32.to_le_bytes());
        b.extend(4_u16.to_le_bytes());
        b.extend(32_u16.to_le_bytes());
        b.extend(b"data");
        b.extend(4_u32.to_le_bytes());
        b.extend(value.to_le_bytes());
        b
    }
    #[test]
    fn float_wave_and_fail_closed() {
        let bytes = wave(0.25);
        let a = parse(&bytes).unwrap();
        assert_eq!(a.samples, [0.25]);
        assert_eq!(a.rate, 48000);
        assert_eq!(a.channels, 1);
        for n in 0..bytes.len() {
            assert!(parse(&bytes[..n]).is_err());
        }
        assert!(parse(&wave(f32::NAN)).is_err());
        let mut wrong = bytes;
        wrong[20] = 1;
        assert!(parse(&wrong).is_err());
    }
    #[test]
    fn gain_is_not_shape_change() {
        let (scale, difference, _) = residual(&[1., -1.], &[2., -2.]).unwrap();
        assert_eq!(scale, 2.);
        assert_eq!(difference, 0.);
        assert_eq!(residual(&[1., 0.], &[0., 1.]).unwrap().1, 1.);
        assert!(residual(&[0.], &[1.]).is_err());
        assert!(residual(&[1.], &[1., 2.]).is_err());
    }
}
