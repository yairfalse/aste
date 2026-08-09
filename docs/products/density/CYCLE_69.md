# Cycle 69 — local macOS VST3 installation

## What changed

Added one user-scoped macOS installer for the universal Density development
bundle. It validates the signature and both binary architectures, stages a
copy under the user's VST3 directory, preserves an existing Density bundle as
a timestamped backup, and verifies the installed copy.

## Why

A built or packaged VST3 is not automatically discoverable by Cubase or
Ableton. The host needs the bundle under the standard per-user VST3 location
before it can scan and instantiate it.

## Evidence

- Shell syntax validation is the 34th CTest gate and passes in CI.
- A temporary-directory installation preserves bundle signature and arm64 plus
  x86_64 slices.
- The standalone smoke host loads the installed temporary copy through VST3.
- The real user installation reports its final path and retains any previous
  copy instead of deleting it.

## Risks

The bundle remains ad-hoc signed, unnotarized, and marked internal-only with an
invalid placeholder identifier. Installation proves file placement, not Cubase
or Ableton compatibility. A running DAW must be reopened before relying on its
plugin inventory.

## Next step

Open Cubase 14, rescan VST3 plugins, instantiate Density on mono and stereo
tracks, and record the host matrix in `HOST_COMPATIBILITY.md`.
