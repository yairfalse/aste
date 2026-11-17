#!/usr/bin/env python3
"""Reproduce isolated N/R research into a new directory; never install plugins."""
import argparse
import hashlib
import json
from pathlib import Path
import platform
import shlex
import shutil
import struct
import subprocess
import sys
import zipfile


ROOT = Path(__file__).resolve().parents[2]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def inputs():
    paths = []
    for folder in ("research/n01", "research/r01", "apps/impulse", "apps/loop",
                   "apps/sequence", "apps/field", "libs/parameters"):
        paths.extend(p for p in (ROOT / folder).rglob("*")
                     if p.is_file() and (p.suffix in (".cpp", ".hpp") or p.name == "CMakeLists.txt"))
    paths += [Path(__file__).resolve(), ROOT / "tests/realtime_audit_mac.cpp", ROOT / "LICENSE"]
    return {str(p.relative_to(ROOT)): digest(p) for p in sorted(set(paths))}


def pack_hashes(path):
    return {str(p.relative_to(path)): digest(p) for p in sorted(path.rglob("*")) if p.is_file()}


def check_wav(path, seconds):
    data = path.read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise RuntimeError(f"Not a WAV: {path}")
    fmt = None
    frames = None
    at = 12
    while at + 8 <= len(data):
        kind, length = struct.unpack_from("<4sI", data, at)
        start = at + 8
        if start + length > len(data):
            raise RuntimeError(f"Truncated WAV: {path}")
        if kind == b"fmt ":
            fmt = struct.unpack_from("<HHIIHH", data, start)
        if kind == b"data":
            frames = length // 8
        at = start + length + length % 2
    if not fmt or (fmt[0], fmt[1], fmt[2], fmt[4], fmt[5]) != (3, 2, 48000, 8, 32):
        raise RuntimeError(f"Unexpected WAV format: {path}")
    if frames != seconds * 48000:
        raise RuntimeError(f"Unexpected WAV duration: {path}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--cmake", default=shutil.which("cmake"))
    parser.add_argument("--sdk")
    parser.add_argument("--cxx-include")
    parser.add_argument("--mutations", action="store_true")
    args = parser.parse_args()
    if not args.cmake:
        parser.error("CMake 3.25+ is required; provide --cmake")
    output = args.output.resolve()
    # Protect existing listening responses, historical evidence and build trees.
    if output.exists():
        parser.error("Output already exists; choose a new directory")
    source = inputs()
    output.mkdir(parents=True)
    logs = output / "logs"
    logs.mkdir()
    commands = []

    def run(name, command, capture=None):
        print(name, flush=True)
        logfile = logs / (name + ".log")
        commands.append({"name": name, "argv": [str(x) for x in command]})
        with logfile.open("w") as log:
            subprocess.run([str(x) for x in command], cwd=ROOT, stdout=log,
                           stderr=subprocess.STDOUT, check=True)
        if capture:
            shutil.copyfile(logfile, capture)

    configure = []
    if args.sdk:
        configure += [f"-DCMAKE_OSX_SYSROOT={args.sdk}"]
    if args.cxx_include:
        configure += [f"-DCMAKE_CXX_FLAGS=-isystem {args.cxx_include}"]
    provenance = {
        "head": subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=ROOT, text=True).strip(),
        "tracked_patch_sha256": hashlib.sha256(subprocess.check_output(["git", "diff", "HEAD"], cwd=ROOT)).hexdigest(),
        "source_sha256": source,
        "platform": platform.platform(), "machine": platform.machine(),
        "musical_listening_assessment": "not performed",
    }
    summary = {}
    for product in ("n01", "r01"):
        folder = output / product
        release = folder / "release"
        run(product + "-configure", [args.cmake, "-S", ROOT / "research" / product,
                                    "-B", release, "-DCMAKE_BUILD_TYPE=Release", *configure])
        run(product + "-build", [args.cmake, "--build", release, "--parallel", "3"])
        ctest = Path(args.cmake).with_name("ctest")
        run(product + "-tests", [ctest, "--test-dir", release, "--output-on-failure"])
        lab = release / (product + "_lab")
        a, b = folder / "listening-a", folder / "listening-b"
        run(product + "-pack-a", [lab, "--pack", a])
        run(product + "-pack-b", [lab, "--pack", b])
        hashes = pack_hashes(a)
        if hashes != pack_hashes(b):
            raise RuntimeError(product + " independently rendered packs differ")
        waves = list(a.rglob("*.wav"))
        expected = 40 if product == "n01" else 64
        if len(waves) != expected:
            raise RuntimeError(f"{product}: expected {expected} WAVs, got {len(waves)}")
        for wave in waves:
            check_wav(wave, 16 if product == "n01" else 8)
        for pack in (a, b):
            (pack / "SOURCE_INPUTS.json").write_text(json.dumps(provenance, indent=2) + "\n")
            files = pack_hashes(pack)
            (pack / "SHA256SUMS").write_text("".join(f"{value}  {name}\n" for name, value in files.items()))
        summary[product] = {"wav_count": len(waves), "pack_byte_reproducible": True,
                            "pack": str(a.relative_to(output))}
        if product == "r01":
            for measure in ("tuning", "alias", "stress"):
                run(product + "-" + measure, [lab, "--" + measure, folder / (measure + ".csv")])
        sanitized = folder / "sanitized"
        run(product + "-sanitize-configure", [args.cmake, "-S", ROOT / "research" / product,
            "-B", sanitized, "-DCMAKE_BUILD_TYPE=Debug", f"-D{product.upper()}_SANITIZERS=ON", *configure])
        run(product + "-sanitize-build", [args.cmake, "--build", sanitized, "--parallel", "3"])
        run(product + "-sanitize-tests", [ctest, "--test-dir", sanitized, "--output-on-failure"])
        if args.mutations:
            compiler = ["/usr/bin/c++"]
            if args.sdk:
                compiler += ["-isysroot", args.sdk]
            if args.cxx_include:
                compiler += ["-isystem", args.cxx_include]
            wrapper = folder / "research-cxx"
            wrapper.write_text("#!/bin/sh\nexec " + shlex.join(compiler) + ' "$@"\n')
            wrapper.chmod(0o755)
            run(product + "-mutations", [release / (product + "_mutations"),
                                         ROOT / "research" / product, folder / "mutations", wrapper])
    # Timing is deliberately last and sequential: no build/render competes with it.
    for product in ("n01", "r01"):
        folder = output / product
        lab = folder / "release" / (product + "_lab")
        if product == "n01":
            run(product + "-benchmark", [lab, "--benchmark"], folder / "benchmark.csv")
        else:
            run(product + "-benchmark", [lab, "--benchmark", folder / "benchmark.csv"])
    if inputs() != source:
        raise RuntimeError("Source inputs changed during the run; evidence is not a consistent snapshot")
    source_archive = output / "research-source.zip"
    with zipfile.ZipFile(source_archive, "w", zipfile.ZIP_DEFLATED) as archive:
        for name in source:
            archive.write(ROOT / name, name)
    provenance["source_archive_sha256"] = digest(source_archive)
    for product in ("n01", "r01"):
        pack = output / product / "listening-a"
        archive = output / (product.upper() + "-listening.zip")
        with zipfile.ZipFile(archive, "w", zipfile.ZIP_DEFLATED) as z:
            for path in sorted(pack.rglob("*")):
                if path.is_file():
                    z.write(path, path.relative_to(pack))
        summary[product]["zip_sha256"] = digest(archive)
    receipt = {**provenance, "invocation": sys.argv, "commands": commands, "results": summary}
    (output / "receipt.json").write_text(json.dumps(receipt, indent=2) + "\n")
    print(json.dumps(summary, indent=2), flush=True)


if __name__ == "__main__":
    main()
