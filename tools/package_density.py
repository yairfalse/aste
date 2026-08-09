#!/usr/bin/env python3
"""Create and inspect deterministic internal Density development packages."""

import argparse
from datetime import date, datetime, timezone
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

sys.dont_write_bytecode = True
from check_dependency_security import validate as validate_security_audit


PRODUCT = "Density D-01"
BUNDLE = f"{PRODUCT}.vst3"
BUNDLE_ID = "invalid.aste.density-d01"
ARCHITECTURES = ["arm64", "x86_64"]
JUCE_VERSION = "8.0.13"
JUCE_COMMIT = "7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2"
VST3_SDK_VERSION = "3.8.0"
VST3_SDK_UPSTREAM_COMMIT = "9fad9770f2ae8542ab1a548a68c1ad1ac690abe0"
SECURITY_AUDIT_SHA256 = "9423a8e39443095417f73b0b1f180dd7202b45ded5c8387ffa846fe476d6c55d"
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
    if not str(metadata.get("commit_timestamp", "")).isdigit():
        fail("build metadata has no commit timestamp")
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
    if metadata.get("juce_commit") != JUCE_COMMIT:
        fail("package does not use the reviewed JUCE commit")
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


def load_security_audit(data, as_of):
    if hashlib.sha256(data).hexdigest() != SECURITY_AUDIT_SHA256:
        fail("dependency security audit does not match the reviewed digest")
    audit = json.loads(data)
    errors = validate_security_audit(audit, as_of)
    if errors:
        fail("dependency security audit invalid: " + "; ".join(errors))
    return audit


def security_record(audit):
    return {
        "file": "DEPENDENCY-SECURITY.json",
        "next_review_due": audit["next_review_due"],
        "reviewed_on": audit["reviewed_on"],
        "sha256": SECURITY_AUDIT_SHA256,
        "status": "no_known_affected_advisories",
    }


def package_record(metadata, audit):
    return {
        "architectures": ARCHITECTURES,
        "bundle_identifier": BUNDLE_ID,
        "bundle_path": BUNDLE,
        "code_signature": "ad-hoc",
        "dependency_security": security_record(audit),
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


def bundle_fingerprint(files):
    digest = hashlib.sha256()
    for name, (data, _) in sorted(files.items()):
        if name.startswith(BUNDLE + "/"):
            digest.update(name.encode())
            digest.update(b"\0")
            digest.update(hashlib.sha256(data).digest())
    return digest.hexdigest()


def sbom_record(metadata, fingerprint):
    created = datetime.fromtimestamp(
        int(metadata["commit_timestamp"]), timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    document_fingerprint = hashlib.sha256(
        json_bytes(metadata) + fingerprint.encode()).hexdigest()
    juce_location = f"git+https://github.com/juce-framework/JUCE.git@{JUCE_COMMIT}"
    return {
        "SPDXID": "SPDXRef-DOCUMENT",
        "comment": (
            f"Internal build: {metadata['compiler']}; {metadata['system']}; "
            f"architectures {metadata['architecture']}. Complete packaged-file "
            "checksums are in CONTENTS.sha256."
        ),
        "creationInfo": {
            "created": created,
            "creators": ["Tool: aste-package-density-1"],
        },
        "dataLicense": "CC0-1.0",
        "documentNamespace": (
            "https://spdx.org/spdxdocs/density-d01/"
            f"{metadata['version']}/{metadata['commit']}/{document_fingerprint}"
        ),
        "name": f"Density-D01-{metadata['version']}-internal-SBOM",
        "packages": [
            {
                "SPDXID": "SPDXRef-Package-DensityD01",
                "copyrightText": "NOASSERTION",
                "downloadLocation": "NONE",
                "filesAnalyzed": False,
                "licenseComments": (
                    "Project and distribution licences are not selected; this "
                    "artifact is internal-development-only."
                ),
                "licenseConcluded": "NOASSERTION",
                "licenseDeclared": "NOASSERTION",
                "name": PRODUCT,
                "packageFileName": BUNDLE,
                "primaryPackagePurpose": "APPLICATION",
                "sourceInfo": f"Built from source commit {metadata['commit']}.",
                "supplier": "NOASSERTION",
                "versionInfo": metadata["version"],
            },
            {
                "SPDXID": "SPDXRef-Package-JUCE",
                "copyrightText": "Copyright Raw Material Software Limited",
                "downloadLocation": juce_location,
                "filesAnalyzed": False,
                "licenseComments": (
                    "JUCE declares AGPLv3 or JUCE 8 commercial licensing; the "
                    "applicable distribution licence is unresolved."
                ),
                "licenseConcluded": "NOASSERTION",
                "licenseDeclared": "NOASSERTION",
                "name": "JUCE",
                "primaryPackagePurpose": "LIBRARY",
                "supplier": "Organization: Raw Material Software Limited",
                "versionInfo": JUCE_VERSION,
            },
            {
                "SPDXID": "SPDXRef-Package-VST3SDK",
                "copyrightText": (
                    "Copyright (c) 2025, Steinberg Media Technologies GmbH"
                ),
                "downloadLocation": (
                    f"{juce_location}#modules/juce_audio_processors_headless/"
                    "format_types/VST3_SDK"
                ),
                "filesAnalyzed": False,
                "licenseConcluded": "MIT",
                "licenseDeclared": "MIT",
                "name": "Steinberg VST 3 SDK bundled by JUCE",
                "primaryPackagePurpose": "LIBRARY",
                "supplier": "Organization: Steinberg Media Technologies GmbH",
                "versionInfo": VST3_SDK_VERSION,
            },
        ],
        "relationships": [
            {
                "relatedSpdxElement": "SPDXRef-Package-DensityD01",
                "relationshipType": "DESCRIBES",
                "spdxElementId": "SPDXRef-DOCUMENT",
            },
            {
                "relatedSpdxElement": "SPDXRef-Package-JUCE",
                "relationshipType": "STATIC_LINK",
                "spdxElementId": "SPDXRef-Package-DensityD01",
            },
            {
                "relatedSpdxElement": "SPDXRef-Package-VST3SDK",
                "relationshipType": "STATIC_LINK",
                "spdxElementId": "SPDXRef-Package-DensityD01",
            },
            {
                "relatedSpdxElement": "SPDXRef-Package-VST3SDK",
                "relationshipType": "CONTAINS",
                "spdxElementId": "SPDXRef-Package-JUCE",
            },
        ],
        "spdxVersion": "SPDX-2.3",
    }


def collect_files(bundle, metadata_bytes, notices_bytes, security_bytes,
                  metadata, audit):
    files = {
        "BUILD-METADATA.json": (metadata_bytes, 0o644),
        "DEPENDENCY-SECURITY.json": (security_bytes, 0o644),
        "DEVELOPMENT_BUILD.txt": (DEVELOPMENT_NOTICE.encode(), 0o644),
        "PACKAGE.json": (json_bytes(package_record(metadata, audit)), 0o644),
        "THIRD_PARTY_NOTICES.md": (notices_bytes, 0o644),
    }
    executable = PurePosixPath(BUNDLE) / "Contents" / "MacOS" / PRODUCT
    for path in sorted(bundle.rglob("*")):
        if path.is_dir():
            continue
        relative = PurePosixPath(BUNDLE) / path.relative_to(bundle).as_posix()
        mode = 0o755 if relative == executable else 0o644
        files[str(relative)] = (path.read_bytes(), mode)

    files["SBOM.spdx.json"] = (
        json_bytes(sbom_record(metadata, bundle_fingerprint(files))), 0o644)

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


def verify_archive(path, require_clean=False, as_of=None):
    as_of = as_of or date.today()
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
            "DEPENDENCY-SECURITY.json",
            "DEVELOPMENT_BUILD.txt",
            "SBOM.spdx.json",
            "THIRD_PARTY_NOTICES.md",
            f"{bundle_prefix}Info.plist",
            f"{bundle_prefix}MacOS/{PRODUCT}",
            f"{bundle_prefix}Resources/moduleinfo.json",
            f"{bundle_prefix}_CodeSignature/CodeResources",
        }
        if not required.issubset(relative_files):
            fail(f"package is missing required files: {sorted(required - relative_files.keys())}")
        audit = load_security_audit(relative_files["DEPENDENCY-SECURITY.json"], as_of)
        if record != package_record(metadata, audit):
            fail("PACKAGE.json does not match build metadata or internal policy")
        expected_sbom = sbom_record(metadata, bundle_fingerprint({
            name: (data, 0) for name, data in relative_files.items()
        }))
        if json.loads(relative_files["SBOM.spdx.json"]) != expected_sbom:
            fail("SBOM does not match build provenance or dependency policy")
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
    security_bytes = args.security.read_bytes()
    audit = load_security_audit(security_bytes, date.today())
    inspect_bundle(args.bundle, metadata)
    root = f"Density-D01-{metadata['version']}-internal-macos-universal"
    files = collect_files(
        args.bundle, metadata_bytes, args.notices.read_bytes(), security_bytes,
        metadata, audit)
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
    create_parser.add_argument("--security", type=Path, required=True)
    create_parser.add_argument("--output", type=Path, required=True)
    verify_parser = subparsers.add_parser("verify")
    verify_parser.add_argument("--archive", type=Path, required=True)
    verify_parser.add_argument("--require-clean", action="store_true")
    verify_parser.add_argument("--as-of", type=date.fromisoformat, default=date.today())
    args = parser.parse_args()
    if args.command == "create":
        create(args)
    else:
        verify_archive(args.archive, require_clean=args.require_clean, as_of=args.as_of)


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, KeyError, json.JSONDecodeError,
            plistlib.InvalidFileException, zipfile.BadZipFile) as error:
        print(f"density package: {error}", file=sys.stderr)
        raise SystemExit(1)
