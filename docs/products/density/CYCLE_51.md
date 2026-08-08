# Cycle 51 — Build provenance

## What changed

Every CMake configure now emits validated JSON metadata containing project and
schema versions, commit and dirty state, system, architecture, compiler,
generator, build type, C++ standard, VST3 and sanitizer flags, pinned JUCE
revision, and JUnit result location.

## Why

Compiler and dependency versions in console logs are easy to lose. A small
configured artifact makes every local or CI result attributable without adding
a packaging system.

## Evidence

- `tools/check_build_metadata.py` validates required fields through CTest.
- The current arm64 Release metadata records AppleClang 21.0.0.21000101,
  C++20, commit `922dae61e27e526e47af3c0c71d1590e6b10fc56`, and pinned JUCE commit
  `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`.
- Local dirty state is explicit rather than silently attributed to the commit.
- CI writes its CTest JUnit summary to the recorded path.

## Risks

The metadata describes a build directory, not a notarized release artifact.
Release identity, signing certificate, installer, hashes, and final licence
inventory remain future packaging gates.

## Next step

Carry the metadata and JUnit summary into release artifacts only after all
release gates pass.
