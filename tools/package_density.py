#!/usr/bin/env python3
"""Create and inspect deterministic internal Density development packages."""

import argparse
import hashlib
import io
import json
from pathlib import Path, PurePosixPath
import plistlib
import re
import stat
import subprocess
import sys
import tempfile
import zipfile


PRODUCT = "Density D-01"
BUNDLE = f"{PRODUCT}.vst3"
BUNDLE_ID = "invalid.aste.density-d01"
ARCHITECTURES = ["arm64", "x86_64"]
FIXED_TIME = (1980, 1, 1, 0, 0, 0)
DEVELOPMENT_NOTICE = """DENSITY D-01 — INTERNAL DEVELOPMENT BUILD

Not for distribution or production use.

This bundle uses a placeholder identity, an ad-hoc code signature, and has not
been notarized. Creating this archive does not close the product's licensing,
host-compatibility, signing, notarization, or release gates.
"""


def fail(message):
    raise ValueError(message)


def load_metadata(data, require_clean=False):
    metadata = json.loads(data)
    if metadata.get("schema") != 1 or metadata.get("project") != "aste_signal_instruments":
        fail("unsupported build metadata")
    if not re.fullmatch(r"[0-9a-f]{40}", metadata.get("commit", "")):
        fail("build metadata has no immutable commit")
    if metadata.get("source_dirty") not in {"true", "false"}:
        fail("invalid source_dirty value")
    if require_clean and metadata["source_dirty"] != "false":
        fail("package was built from a dirty source tree")
    if metadata.get("build_type") != "Release":
        fail("package must use a Release build")
    if metadata.get("vst3") != "ON" or metadata.get("sanitizers") != "OFF":
        fail("package must be a non-sanitized VST3 build")
    if set(metadata.get("architecture", "").split(";")) != set(ARCHITECTURES):
        fail("package must contain arm64 and x86_64")
    if not re.fullmatch(r"\d+\.\d+\.\d+", metadata.get("version", "")):
        fail("invalid product version")
    return metadata


def run_checked(command):
    result = subprocess.run(command, text=True, capture_output=True, check=False)
    if result.returncode:
        fail(f"command failed: {' '.join(command)}\n{result.stdout}{result.stderr}")
    return result.stdout + result.stderr


def inspect_bundle(bundle, metadata):
    if bundle.name != BUNDLE or not bundle.is_dir():
        fail(f"expected bundle named {BUNDLE}")
    for path in bundle.rglob("*"):
        if path.is_symlink():
            fail(f"bundle contains a symlink: {path.relative_to(bundle)}")

    plist_path = bundle / "Contents" / "Info.plist"
    info = plistlib.loads(plist_path.read_bytes())
    expected = {
        "CFBundleDisplayName": PRODUCT,
        "CFBundleExecutable": PRODUCT,
        "CFBundleIdentifier": BUNDLE_ID,
        "CFBundleShortVersionString": metadata["version"],
        "CFBundleVersion": metadata["version"],
    }
    for key, value in expected.items():
        if info.get(key) != value:
            fail(f"unexpected {key}: {info.get(key)!r}")

    binary = bundle / "Contents" / "MacOS" / PRODUCT
    if set(run_checked(["lipo", str(binary), "-archs"]).split()) != set(ARCHITECTURES):
        fail("bundle binary is not universal arm64+x86_64")
    run_checked(["codesign", "--verify", "--strict", "--deep", str(bundle)])
    signature = run_checked(["codesign", "-dv", "--verbose=4", str(bundle)])
    if "Signature=adhoc" not in signature or "TeamIdentifier=not set" not in signature:
        fail("development bundle must have an ad-hoc signature and no team identity")

    module_info = bundle / "Contents" / "Resources" / "moduleinfo.json"
    if not module_info.is_file():
        fail("bundle has no VST3 moduleinfo.json")


def package_record(metadata):
    return {
        "architectures": ARCHITECTURES,
        "bundle_identifier": BUNDLE_ID,
        "bundle_path": BUNDLE,
        "code_signature": "ad-hoc",
        "distribution_allowed": False,
        "notarized": False,
        "product": PRODUCT,
        "release_status": "internal-development-only",
        "schema": 1,
        "source_commit": metadata["commit"],
        "source_dirty": metadata["source_dirty"] == "true",
        "version": metadata["version"],
    }


def json_bytes(value):
    return (json.dumps(value, indent=2, sort_keys=True) + "\n").encode()


def collect_files(bundle, metadata_bytes, notices_bytes, record):
    files = {
        "BUILD-METADATA.json": (metadata_bytes, 0o644),
        "DEVELOPMENT_BUILD.txt": (DEVELOPMENT_NOTICE.encode(), 0o644),
        "PACKAGE.json": (json_bytes(record), 0o644),
        "THIRD_PARTY_NOTICES.md": (notices_bytes, 0o644),
    }
    executable = PurePosixPath(BUNDLE) / "Contents" / "MacOS" / PRODUCT
    for path in sorted(bundle.rglob("*")):
        if path.is_dir():
            continue
        relative = PurePosixPath(BUNDLE) / path.relative_to(bundle).as_posix()
        mode = 0o755 if relative == executable else 0o644
        files[str(relative)] = (path.read_bytes(), mode)

    manifest = "".join(
        f"{hashlib.sha256(data).hexdigest()}  {name}\n"
        for name, (data, _) in sorted(files.items())
    ).encode()
    files["CONTENTS.sha256"] = (manifest, 0o644)
    return files


def render_archive(root, files):
    directories = {root + "/"}
    for name in files:
        parts = PurePosixPath(name).parts[:-1]
        for length in range(1, len(parts) + 1):
            directories.add(root + "/" + "/".join(parts[:length]) + "/")

    output = io.BytesIO()
    entries = [(name, None) for name in directories]
    entries.extend((f"{root}/{name}", value) for name, value in files.items())
    with zipfile.ZipFile(output, "w", compression=zipfile.ZIP_STORED) as archive:
        for name, value in sorted(entries):
            if value is not None:
                data, mode = value
                info = zipfile.ZipInfo(name, FIXED_TIME)
                info.create_system = 3
                info.external_attr = (stat.S_IFREG | mode) << 16
                archive.writestr(info, data)
                continue
            info = zipfile.ZipInfo(name, FIXED_TIME)
            info.create_system = 3
            info.external_attr = (stat.S_IFDIR | 0o755) << 16 | 0x10
            archive.writestr(info, b"")
    return output.getvalue()


def safe_archive_name(name):
    path = PurePosixPath(name)
    return bool(name) and not name.startswith("/") and "\\" not in name and ".." not in path.parts


def verify_archive(path, require_clean=False):
    with zipfile.ZipFile(path) as archive:
        infos = archive.infolist()
        names = [info.filename for info in infos]
        if len(names) != len(set(names)) or names != sorted(names):
            fail("archive entries must be unique and sorted")
        if archive.testzip() is not None:
            fail("archive CRC check failed")
        for info in infos:
            mode = info.external_attr >> 16
            if (not safe_archive_name(info.filename) or info.date_time != FIXED_TIME
                    or info.compress_type != zipfile.ZIP_STORED or stat.S_ISLNK(mode)):
                fail(f"unsafe or non-deterministic archive entry: {info.filename}")

        package_names = [name for name in names if name.endswith("/PACKAGE.json")]
        if len(package_names) != 1:
            fail("archive must contain one PACKAGE.json")
        root = package_names[0].split("/", 1)[0]
        record = json.loads(archive.read(package_names[0]))
        expected_root = f"Density-D01-{record.get('version')}-internal-macos-universal"
        if root != expected_root or any(not name.startswith(root + "/") for name in names):
            fail("archive has an unexpected root directory")

        metadata = load_metadata(
            archive.read(f"{root}/BUILD-METADATA.json"), require_clean=require_clean)
        expected_record = package_record(metadata)
        if record != expected_record:
            fail("PACKAGE.json does not match build metadata or internal policy")

        relative_files = {
            name[len(root) + 1:]: archive.read(name)
            for name in names if not name.endswith("/")
        }
        manifest = relative_files.pop("CONTENTS.sha256", None)
        if manifest is None:
            fail("archive has no CONTENTS.sha256")
        recorded = {}
        for line in manifest.decode().splitlines():
            digest, separator, name = line.partition("  ")
            if not separator or name in recorded:
                fail("malformed checksum manifest")
            recorded[name] = digest
        calculated = {
            name: hashlib.sha256(data).hexdigest()
            for name, data in relative_files.items()
        }
        if recorded != calculated:
            fail("checksum manifest does not match archive contents")

        bundle_prefix = f"{BUNDLE}/Contents/"
        required = {
            "DEVELOPMENT_BUILD.txt",
            "THIRD_PARTY_NOTICES.md",
            f"{bundle_prefix}Info.plist",
            f"{bundle_prefix}MacOS/{PRODUCT}",
            f"{bundle_prefix}Resources/moduleinfo.json",
            f"{bundle_prefix}_CodeSignature/CodeResources",
        }
        if not required.issubset(relative_files):
            fail(f"package is missing required files: {sorted(required - relative_files.keys())}")
        binary_name = f"{root}/{bundle_prefix}MacOS/{PRODUCT}"
        binary_info = archive.getinfo(binary_name)
        if not binary_info.external_attr >> 16 & 0o111:
            fail("packaged plug-in binary is not executable")
        info = plistlib.loads(relative_files[f"{bundle_prefix}Info.plist"])
        if info.get("CFBundleIdentifier") != BUNDLE_ID:
            fail("packaged bundle identifier does not match internal identity")

        with tempfile.TemporaryDirectory(prefix="density-package-") as temporary:
            archive.extractall(temporary)
            inspect_bundle(Path(temporary) / root / BUNDLE, metadata)

    digest = hashlib.sha256(path.read_bytes()).hexdigest()
    print(f"density package: ok, {len(relative_files) + 1} files, sha256={digest}")


def create(args):
    metadata_bytes = args.metadata.read_bytes()
    metadata = load_metadata(metadata_bytes)
    inspect_bundle(args.bundle, metadata)
    root = f"Density-D01-{metadata['version']}-internal-macos-universal"
    files = collect_files(args.bundle, metadata_bytes, args.notices.read_bytes(),
                          package_record(metadata))
    first = render_archive(root, files)
    if first != render_archive(root, files):
        fail("archive construction is not deterministic")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    args.output.write_bytes(first)
    verify_archive(args.output)


def main():
    parser = argparse.ArgumentParser()
    subparsers = parser.add_subparsers(dest="command", required=True)
    create_parser = subparsers.add_parser("create")
    create_parser.add_argument("--bundle", type=Path, required=True)
    create_parser.add_argument("--metadata", type=Path, required=True)
    create_parser.add_argument("--notices", type=Path, required=True)
    create_parser.add_argument("--output", type=Path, required=True)
    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("--archive", type=Path, required=True)
    verify_parser.add_argument("--require-clean", action="store_true")
    args = parser.parse_args()
    if args.command == "create":
        create(args)
    else:
        verify_archive(args.archive, require_clean=args.require_clean)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, json.JSONDecodeError,
            plistlib.InvalidFileException, zipfile.BadZipFile) as error:
        print(f"density package: {error}", file=sys.stderr)
        raise SystemExit(1)
