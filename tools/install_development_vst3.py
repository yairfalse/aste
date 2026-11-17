#!/usr/bin/env python3
"""Install the eleven local development VST3s, preserving a rollback set."""
import argparse
import datetime
import hashlib
import json
import os
from pathlib import Path
import plistlib
import pwd
import re
import shutil
import subprocess
import tempfile

PRODUCTS = [
    ("DensityD01", "Density D-01", "density-d01"),
    ("HarmonicH01", "Harmonic H-01", "harmonic-h01"),
    ("SequenceS01", "Sequence S-01", "sequence-s01"),
    ("LoopL01", "Loop L-01", "loop-l01"),
    ("ImpulseI01", "Impulse I-01", "impulse-i01"),
    ("FieldF01", "Field F-01", "field-f01"),
    ("NonMachineN01", "Non-Machine N-01", "non-machine-n01"),
    ("ResonatorR01", "Resonator R-01", "resonator-r01"),
    ("GrainG01", "Grain G-01", "grain-g01"),
    ("ContaminationC01", "Contamination C-01", "contamination-c01"),
    ("ClickK01", "Click K-01", "click-k01"),
]


def info(bundle):
    return plistlib.loads((bundle / "Contents/Info.plist").read_bytes())


def verify(bundle, name):
    subprocess.run(["codesign", "--verify", "--strict", "--deep", str(bundle)], check=True)
    subprocess.run(["lipo", str(bundle / "Contents/MacOS" / name), "-verify_arch", "arm64", "x86_64"], check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-root", required=True, type=Path)
    parser.add_argument("--check-only", action="store_true")
    parser.add_argument("--only", nargs="+", choices=[p[2] for p in PRODUCTS])
    args = parser.parse_args()
    build = args.build_root.resolve()
    metadata = json.loads((build / "build-metadata.json").read_text())
    version = metadata.get("version", "")
    if metadata.get("project") != "aste_signal_instruments" or not re.fullmatch(r"[0-9]+\.[0-9]+\.[0-9]+", version):
        raise SystemExit("Invalid build metadata")
    destination = Path("/Library/Audio/Plug-Ins/VST3")
    prepared = []
    for target, name, identity in PRODUCTS:
        if args.only and identity not in args.only:
            continue
        src = build / (target + "_artefacts") / "Release/VST3" / (name + ".vst3")
        data = info(src)
        if data.get("CFBundleIdentifier") != "invalid.aste." + identity or data.get("CFBundleShortVersionString") != version:
            raise SystemExit("Wrong bundle identity/version: " + str(src))
        verify(src, name)
        dst = destination / src.name
        if dst.is_symlink():
            raise SystemExit("Refusing a symlinked installed bundle: " + str(dst))
        old = info(dst) if dst.exists() else None
        if old and old.get("CFBundleIdentifier") != data["CFBundleIdentifier"]:
            raise SystemExit("Installed identity differs: " + str(dst))
        prepared.append((src, dst, name, old))
        print("Verified", name, version, flush=True)
    if args.check_only:
        return
    processes = subprocess.check_output(["ps", "-axo", "comm="], text=True)
    if any("/Cubase" in line for line in processes.splitlines()):
        raise SystemExit("Save and quit Cubase first. No plugins changed.")
    owner = pwd.getpwuid(Path(__file__).resolve().parents[1].stat().st_uid)
    backups = Path(owner.pw_dir) / "Library/Application Support/Aste/Plugin Backups"
    backups.mkdir(parents=True, exist_ok=True)
    backup = Path(tempfile.mkdtemp(prefix=datetime.datetime.now().strftime("%Y%m%d-%H%M%S-") + version + "-", dir=backups))
    stage = Path(tempfile.mkdtemp(prefix=".aste-stage-", dir=destination))
    journal = []
    try:
        # Copy and verify the entire set before replacing any installed copy.
        for src, dst, name, old in prepared:
            subprocess.run(["ditto", str(src), str(stage / dst.name)], check=True)
            verify(stage / dst.name, name)
        (backup / "RESTORE.txt").write_text(
            "Quit Cubase. With administrator approval, move the old VST3 bundles in this folder "
            "back to /Library/Audio/Plug-Ins/VST3, replacing the new versions. "
            "N-01/R-01 may have no earlier copy; remove their installed bundles to uninstall them. "
            "Projects, presets and Cubase preferences were not modified.\n")
        for src, dst, name, old in prepared:
            prior = backup / dst.name if dst.exists() else None
            if prior:
                shutil.move(str(dst), str(prior))
            journal.append((dst, prior))
            os.replace(stage / dst.name, dst)
            verify(dst, name)
            print("Installed", name, version, flush=True)
        receipt = [{"name": name, "version": version, "previous_version": old.get("CFBundleShortVersionString") if old else None,
                    "path": str(dst), "sha256": hashlib.sha256((dst / "Contents/MacOS" / name).read_bytes()).hexdigest()}
                   for src, dst, name, old in prepared]
        (backup / "installed.json").write_text(json.dumps(receipt, indent=2) + "\n")
    except BaseException:
        for dst, prior in reversed(journal):
            if dst.exists():
                shutil.rmtree(dst)
            if prior and prior.exists():
                shutil.move(str(prior), str(dst))
        raise
    finally:
        shutil.rmtree(stage)
    print("Previous versions and receipt:", backup, flush=True)


if __name__ == "__main__":
    main()
