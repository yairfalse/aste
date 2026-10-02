//! Unprivileged, opt-in cleanup of older per-user Aste plugin copies.
use std::{
    ffi::CString,
    fs, io,
    os::unix::fs::{DirBuilderExt, MetadataExt},
    path::{Path, PathBuf},
    process::Command,
};

unsafe extern "C" {
    fn geteuid() -> u32;
    fn renamex_np(from: *const std::ffi::c_char, to: *const std::ffi::c_char, flags: u32) -> i32;
}

const PRODUCTS: [(&str, &str); 6] = [
    ("Density D-01", "density-d01"),
    ("Harmonic H-01", "harmonic-h01"),
    ("Sequence S-01", "sequence-s01"),
    ("Loop L-01", "loop-l01"),
    ("Impulse I-01", "impulse-i01"),
    ("Field F-01", "field-f01"),
];
type Version = [u32; 3];

fn version(text: &str) -> Option<Version> {
    let parts: Vec<_> = text.split('.').collect();
    if parts.len() != 3
        || parts
            .iter()
            .any(|p| p.is_empty() || !p.bytes().all(|c| c.is_ascii_digit()))
    {
        return None;
    }
    Some([
        parts[0].parse().ok()?,
        parts[1].parse().ok()?,
        parts[2].parse().ok()?,
    ])
}

fn plain_path(path: &Path) -> bool {
    path.ancestors()
        .all(|p| fs::symlink_metadata(p).is_ok_and(|m| !m.file_type().is_symlink()))
}

fn property(bundle: &Path, key: &str) -> Option<String> {
    let plist = bundle.join("Contents/Info.plist");
    if !plain_path(&plist) {
        return None;
    }
    let result = Command::new("/usr/bin/plutil")
        .args(["-extract", key, "raw", "-o", "-"])
        .arg(plist)
        .output()
        .ok()?;
    if !result.status.success() {
        return None;
    }
    Some(String::from_utf8(result.stdout).ok()?.trim().to_owned())
}

fn info(bundle: &Path, id: &str) -> Option<Version> {
    if property(bundle, "CFBundleIdentifier")? != id {
        return None;
    }
    version(&property(bundle, "CFBundleShortVersionString")?)
}

fn known_name(name: &str, expected: &str) -> bool {
    if name == expected {
        return true;
    }
    name.strip_prefix(&format!("{expected}.backup-"))
        .is_some_and(|suffix| {
            !suffix.is_empty()
                && suffix.len() <= 64
                && suffix.bytes().all(|c| c.is_ascii_digit() || c == b'-')
        })
}

fn signed(bundle: &Path) -> bool {
    Command::new("/usr/bin/codesign")
        .args(["--verify", "--strict", "--deep"])
        .arg(bundle)
        .output()
        .is_ok_and(|r| r.status.success())
}

#[derive(Debug)]
struct Candidate {
    source: PathBuf,
    replacement: PathBuf,
    id: String,
    format: &'static str,
    old_version: Version,
    new_version: Version,
    identity: (u64, u64),
}

fn plan(
    user: &Path,
    system: &Path,
    verify: impl Fn(&Path) -> bool,
) -> io::Result<(Vec<Candidate>, Vec<String>)> {
    let mut candidates = Vec::new();
    let mut skipped = Vec::new();
    for (format, extension) in [("VST3", "vst3"), ("Components", "component")] {
        let directory = user.join(format);
        if !directory.exists() {
            continue;
        }
        if !plain_path(&directory) {
            skipped.push(format!("{}: symlinked path", directory.display()));
            continue;
        }
        for entry in fs::read_dir(&directory)? {
            let entry = entry?;
            let name = entry.file_name();
            let Some(name) = name.to_str() else {
                continue;
            };
            for (product, slug) in PRODUCTS {
                let expected = format!("{product}.{extension}");
                if !known_name(name, &expected) {
                    continue;
                }
                let source = entry.path();
                let replacement = system.join(format).join(&expected);
                let id = format!("invalid.aste.{slug}");
                let versions = info(&source, &id).zip(info(&replacement, &id));
                let Some((old_version, new_version)) = versions else {
                    skipped.push(format!("{format}/{name}: missing replacement, unknown identity/version, or symlink"));
                    continue;
                };
                if old_version >= new_version {
                    skipped.push(format!("{format}/{name}: same or newer version; kept"));
                    continue;
                }
                if !verify(&replacement) {
                    skipped.push(format!(
                        "{format}/{name}: replacement signature failed; kept"
                    ));
                    continue;
                }
                let metadata = fs::symlink_metadata(&source)?;
                candidates.push(Candidate {
                    source,
                    replacement,
                    id,
                    format,
                    old_version,
                    new_version,
                    identity: (metadata.dev(), metadata.ino()),
                });
            }
        }
    }
    candidates.sort_by(|a, b| a.source.cmp(&b.source));
    Ok((candidates, skipped))
}

fn changed(candidate: &Candidate) -> bool {
    !plain_path(&candidate.source)
        || !plain_path(&candidate.replacement)
        || fs::symlink_metadata(&candidate.source)
            .map(|m| (m.dev(), m.ino()))
            .ok()
            != Some(candidate.identity)
        || info(&candidate.source, &candidate.id) != Some(candidate.old_version)
        || info(&candidate.replacement, &candidate.id) != Some(candidate.new_version)
}

fn make_directory(path: &Path) -> io::Result<()> {
    if path.exists() {
        if plain_path(path) && path.is_dir() {
            return Ok(());
        }
        return Err(io::Error::other(
            "backup path contains a symlink or is not a directory",
        ));
    }
    let parent = path
        .parent()
        .ok_or_else(|| io::Error::other("missing parent"))?;
    make_directory(parent)?;
    fs::DirBuilder::new().mode(0o700).create(path)
}

fn move_exclusive(source: &Path, destination: &Path) -> io::Result<()> {
    let from = CString::new(source.as_os_str().as_encoded_bytes())?;
    let to = CString::new(destination.as_os_str().as_encoded_bytes())?;
    // RENAME_EXCL: atomic, no overwrite, no copy-and-delete fallback.
    if unsafe { renamex_np(from.as_ptr(), to.as_ptr(), 4) } == 0 {
        Ok(())
    } else {
        Err(io::Error::last_os_error())
    }
}

fn apply(
    candidates: &[Candidate],
    backup: &Path,
    verify: impl Fn(&Path) -> bool,
) -> io::Result<()> {
    if candidates
        .iter()
        .any(|c| changed(c) || !verify(&c.replacement))
    {
        return Err(io::Error::other(
            "plugins changed since preview; nothing moved. Run cleanup again",
        ));
    }
    if backup.exists() {
        return Err(io::Error::other("backup session already exists"));
    }
    make_directory(
        backup
            .parent()
            .ok_or_else(|| io::Error::other("missing backup parent"))?,
    )?;
    fs::DirBuilder::new().mode(0o700).create(backup)?;
    fs::write(backup.join("RESTORE.txt"), "Quit your DAW before restoring. Move the desired bundle from VST3 or Components back to the matching folder in ~/Library/Audio/Plug-Ins. Do not overwrite a newer copy without backing it up. Projects, presets and DAW caches were not modified.\n")?;
    for candidate in candidates {
        let destination = backup.join(candidate.format);
        make_directory(&destination)?;
        if changed(candidate) {
            return Err(io::Error::other(format!(
                "plugins changed during cleanup; earlier moves are safe in {}",
                backup.display()
            )));
        }
        move_exclusive(
            &candidate.source,
            &destination.join(candidate.source.file_name().unwrap()),
        )
        .map_err(|error| {
            io::Error::other(format!(
                "{error}; earlier moves are safe in {}",
                backup.display()
            ))
        })?;
    }
    Ok(())
}

fn dialog(message: &str, confirm: bool) -> io::Result<bool> {
    let script = if confirm {
        "on run argv\nreturn button returned of (display dialog (item 1 of argv) with title \"Aste Plugin Cleanup\" buttons {\"Cancel\", \"Move to Backup\"} default button \"Cancel\")\nend run"
    } else {
        "on run argv\ndisplay dialog (item 1 of argv) with title \"Aste Plugin Cleanup\" buttons {\"OK\"} default button \"OK\"\nend run"
    };
    let result = Command::new("/usr/bin/osascript")
        .args(["-e", script, message])
        .output()?;
    if !result.status.success() {
        if String::from_utf8_lossy(&result.stderr).contains("(-128)") {
            return Ok(false);
        }
        return Err(io::Error::other(
            String::from_utf8_lossy(&result.stderr).into_owned(),
        ));
    }
    Ok(String::from_utf8_lossy(&result.stdout).trim() == "Move to Backup")
}

fn run(gui: bool, apply_now: bool) -> io::Result<()> {
    if unsafe { geteuid() } == 0 {
        return Err(io::Error::other(
            "run as your normal user, not root or sudo",
        ));
    }
    let home = fs::canonicalize(
        std::env::var_os("HOME").ok_or_else(|| io::Error::other("HOME is missing"))?,
    )?;
    let (candidates, skipped) = plan(
        &home.join("Library/Audio/Plug-Ins"),
        Path::new("/Library/Audio/Plug-Ins"),
        signed,
    )?;
    let mut report = String::from("Quit your DAW before continuing. Only older per-user Aste copies with an installed, signature-verified newer system copy are eligible. Nothing is deleted.\n\n");
    for c in &candidates {
        report.push_str(&format!(
            "{} ({}.{}.{})\n",
            c.source.display(),
            c.old_version[0],
            c.old_version[1],
            c.old_version[2]
        ));
    }
    if candidates.is_empty() {
        report.push_str("No eligible older copies found.\n");
    }
    if !skipped.is_empty() {
        report.push_str(&format!("\nLeft unchanged:\n{}\n", skipped.join("\n")));
    }
    println!("{report}");
    let confirmed = if gui {
        dialog(&report, !candidates.is_empty())?
    } else {
        apply_now
    };
    if candidates.is_empty() || !confirmed {
        return Ok(());
    }
    let date = Command::new("/bin/date")
        .args(["-u", "+%Y%m%dT%H%M%SZ"])
        .output()?;
    if !date.status.success() {
        return Err(io::Error::other("cannot create backup timestamp"));
    }
    let stamp = String::from_utf8(date.stdout).map_err(io::Error::other)?;
    let backup = home
        .join("Library/Application Support/Aste/Plugin Backups")
        .join(format!("{}-{}", stamp.trim(), std::process::id()));
    apply(&candidates, &backup, signed).map_err(|error| {
        io::Error::other(format!("{error}\nBackup location: {}", backup.display()))
    })?;
    let done = format!("Moved {} older copies to:\n{}\n\nRestart your DAW and rescan plugins if needed. Restore instructions are in the backup folder.", candidates.len(), backup.display());
    println!("{done}");
    if gui {
        dialog(&done, false)?;
    }
    Ok(())
}

fn main() {
    let args: Vec<_> = std::env::args().skip(1).collect();
    if args == ["--help"] {
        println!("Usage: aste-plugin-cleanup [--preview | --apply]\nNo arguments opens a preview and confirmation dialog. Older copies are moved to backup, never deleted.");
        return;
    }
    let gui = args.is_empty();
    if !gui && args != ["--preview"] && args != ["--apply"] {
        eprintln!("Usage: aste-plugin-cleanup [--preview | --apply]\nNo arguments opens a preview and confirmation dialog.");
        std::process::exit(2);
    }
    if let Err(error) = run(gui, args == ["--apply"]) {
        eprintln!("{error}");
        if gui {
            let _ = dialog(&error.to_string(), false);
        }
        std::process::exit(1);
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use std::os::unix::fs::symlink;

    fn bundle(path: &Path, id: &str, version: &str) {
        fs::create_dir_all(path.join("Contents")).unwrap();
        fs::write(path.join("Contents/Info.plist"), format!("<?xml version=\"1.0\"?><plist version=\"1.0\"><dict><key>CFBundleIdentifier</key><string>{id}</string><key>CFBundleShortVersionString</key><string>{version}</string></dict></plist>")).unwrap();
    }

    #[test]
    fn only_moves_older_known_copies_with_replacements() {
        let root = std::env::temp_dir()
            .canonicalize()
            .unwrap()
            .join(format!("aste-cleanup-test-{}", std::process::id()));
        fs::create_dir(&root).unwrap();
        let user = root.join("user");
        let system = root.join("system");
        for (product, slug) in PRODUCTS {
            bundle(
                &system.join(format!("VST3/{product}.vst3")),
                &format!("invalid.aste.{slug}"),
                "0.1.1",
            );
        }
        let old = user.join("VST3/Density D-01.vst3");
        bundle(&old, "invalid.aste.density-d01", "0.1.0");
        let legacy = user.join("VST3/Density D-01.vst3.backup-20261001-120000-1");
        bundle(&legacy, "invalid.aste.density-d01", "0.1.0");
        bundle(
            &user.join("VST3/Harmonic H-01.vst3"),
            "invalid.aste.harmonic-h01",
            "0.1.1",
        );
        bundle(
            &user.join("VST3/Sequence S-01.vst3"),
            "invalid.aste.sequence-s01",
            "0.2.0",
        );
        bundle(&user.join("VST3/Loop L-01.vst3"), "someone.else", "0.1.0");
        bundle(
            &user.join("Components/Field F-01.component"),
            "invalid.aste.field-f01",
            "0.1.0",
        ); // No AU replacement.
        let linked = root.join("linked-impulse");
        bundle(&linked, "invalid.aste.impulse-i01", "0.1.0");
        symlink(&linked, user.join("VST3/Impulse I-01.vst3")).unwrap();
        let (candidates, _) = plan(&user, &system, |_| true).unwrap();
        assert_eq!(candidates.len(), 2);
        assert!(old.exists() && legacy.exists(), "preview never moves files");
        assert!(
            plan(&user, &system, |_| false).unwrap().0.is_empty(),
            "unverified replacement is never used"
        );
        let au = system.join("Components/Field F-01.component");
        bundle(&au, "invalid.aste.field-f01", "0.1.1");
        let (candidates, _) = plan(&user, &system, |_| true).unwrap();
        assert_eq!(
            candidates.len(),
            3,
            "AU becomes eligible only after its replacement exists"
        );
        let backup = root.join("backup/session");
        bundle(&old, "invalid.aste.density-d01", "0.3.0");
        assert!(apply(&candidates, &backup, |_| true).is_err());
        assert!(
            !backup.exists() && legacy.exists(),
            "changed preview aborts before moving anything"
        );
        bundle(&old, "invalid.aste.density-d01", "0.1.0");
        let redirect = root.join("redirect");
        symlink(root.join("backup"), &redirect).unwrap();
        assert!(apply(&candidates, &redirect.join("session"), |_| true).is_err());
        apply(&candidates, &backup, |_| true).unwrap();
        assert!(!old.exists() && !legacy.exists());
        assert!(backup
            .join("VST3/Density D-01.vst3/Contents/Info.plist")
            .exists());
        assert!(backup
            .join("Components/Field F-01.component/Contents/Info.plist")
            .exists());
        assert!(
            linked.join("Contents/Info.plist").exists(),
            "symlink target is untouched"
        );
        assert!(user.join("VST3/Harmonic H-01.vst3").exists());
        assert!(user.join("VST3/Loop L-01.vst3").exists());
        let first = root.join("first");
        let second = root.join("second");
        fs::write(&first, "first").unwrap();
        fs::write(&second, "second").unwrap();
        assert!(
            move_exclusive(&first, &second).is_err(),
            "never overwrite a backup"
        );
        assert_eq!(fs::read_to_string(second).unwrap(), "second");
        fs::remove_dir_all(root).unwrap();
    }
}
