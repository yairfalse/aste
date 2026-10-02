# ADR 0017: Free open-source macOS development packaging

Status: accepted, 2026-10-02.

## Decision

At the owner's request, license Aste's original code under AGPL-3.0-only and
use the AGPLv3 option in pinned JUCE 8.0.13. Preserve third-party licenses.
No paid account, commercial JUCE license, purchase, publication, or Apple
enrollment is part of this change.

Use native CMake/CPack and Apple's pkgbuild/productbuild/hdiutil rather than a
custom installer. The opt-in installer target requires universal arm64/x86_64
AU and VST3 builds, offers twelve plugin components grouped by instrument, and
installs license notices. It has no install scripts or background services.
The disk image includes the matching first-party source and full pinned JUCE
source so recipients can rebuild without fetching JUCE. Source staging selects
explicit project roots and excludes Git history and generated build trees.

Retain existing plugin IDs and the provisional development identity. Local
ad-hoc signatures validate binary integrity but do not identify a trusted
publisher. The package and disk image are unsigned and unnotarized. Document
macOS's normal approval path without disabling security protections. Target
macOS 11 at compile time; testing on the oldest OS and physical Intel hardware
remains required before claiming compatibility.

A dependency-free Rust check verifies universal slices, signatures and system-
only runtime dependencies, instantiates each AU through Apple's in-process
registration API, and repeats checks on extracted installer payloads. No test
installs plugins into the user's or system's plugin directories.

Version 0.1.1 adds a universal Rust cleanup utility, installed in /Applications
and supplied on the disk image. It runs only when explicitly opened (or invoked
with `--apply`), never as a privileged installer script. Default launch previews
eligible older per-user copies and asks before moving them into a dated backup
outside plugin scan folders. It matches known filenames and bundle identities,
requires a strictly newer signature-verified system replacement of the same
format, rejects symlinked paths, rechecks the preview, and uses exclusive atomic
moves. Same/newer copies, other users' folders, projects and presets are not
modified. The utility refuses root execution and never permanently deletes files.

## Relationship to previous decisions

This permits a local six-product development installer beyond the Density-only
internal ZIP in [ADR 0004](0004-internal-package-format.md). The existing strict
Density rehearsal remains unchanged. Neither packaging path establishes DAW
compatibility, notarization, or a supported public release. Public publication
remains a separate action; no upload is performed by the build target.
