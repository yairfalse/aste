# Cycle 60 — deterministic internal package rehearsal

## What changed

Added a macOS `density_package` target and a standard-library package inspector.
The target creates a deterministic internal ZIP containing the universal VST3,
build and package metadata, third-party notices, an explicit development-only
warning, and a SHA-256 inventory. Hosted CI now creates and inspects the archive
after VST3 validation without publishing it.

## Why

The bundle needs reproducible provenance and content inspection before release
engineering begins. The final company identity, distribution terms, Developer
ID certificate, and notarization credentials are unresolved, so this cycle
must exercise packaging without creating a false release artifact.

## Evidence

- The source bundle passes strict code-signature verification and contains
  exactly the arm64 and x86_64 architectures before packaging.
- All archive entries have a fixed 1980 timestamp, normalized permissions,
  sorted names, and stored compression.
- Two in-process renders are byte-identical.
- Two separate target invocations produce SHA-256
  `1b88dbe5d057731f6c481bc5003b7933f78442285bcef72c2302dae4b195ed55`
  from the same local inputs.
- The inspector validates ten files and their complete checksum inventory.
- The inspector safely extracts the packaged bundle and repeats strict
  signature, placeholder identity, version, and architecture verification.
- The clean-source gate deliberately rejects the local dirty-tree rehearsal.
- A malformed non-ZIP fixture fails safely with exit status 1.

## Risks

The ZIP is intentionally uncompressed and is not suitable for public delivery.
The local hash includes workstation build provenance and is evidence of
same-input repeatability, not a promised cross-toolchain release hash. Final
identity, licensing, Developer ID signing, notarization, host validation, and
release gates remain open.

## Next step

Require the clean hosted packaging gate to pass, then add a generated artifact
bill of materials without introducing a release installer.
