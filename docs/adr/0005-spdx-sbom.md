# ADR 0005 — Minimal SPDX package bill of materials

Status: accepted, 2026-08-09.

## Context

The internal Density package identifies its files but did not describe which
third-party software contributes to the binary. Dependency provenance must be
machine-readable without introducing a scanner, package manager, network
service, or unreviewed licensing conclusion.

## Decision

The packager emits `SBOM.spdx.json` using SPDX 2.3 JSON. It describes exactly
three packages:

1. Density D-01 0.1.0;
2. JUCE 8.0.13 at pinned commit
   `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`;
3. the MIT-licensed Steinberg VST3 SDK 3.8.0 sources bundled inside that JUCE
   commit.

Relationships record that Density statically links JUCE and the bundled VST3
SDK, and that JUCE contains those SDK sources. The independent Steinberg
validator checkout is CI tooling and is intentionally absent from the product
dependency graph.

The project and JUCE concluded licences remain `NOASSERTION` until distribution
terms are selected and reviewed. The SBOM records that uncertainty instead of
guessing. Its creation time is the source commit time, and its unique namespace
is derived from the version, commit, build metadata, and deterministic bundle
fingerprint. Compiler, system, and architecture provenance are recorded in the
document comment.

The package inspector reconstructs the expected SPDX document from build
metadata and bundle contents and requires an exact match. SPDX file analysis is
`false`; the separate `CONTENTS.sha256` inventory covers every packaged file.

## Consequences

- Every internal package carries deterministic component and relationship data.
- Dependency identity cannot drift away from CMake provenance without failing
  package inspection.
- The SBOM is not a vulnerability scan, licence approval, or claim that all
  optional source dependencies in the JUCE repository are linked into Density.
- Add another component only when it contributes to the product artifact.

Reference: https://spdx.github.io/spdx-spec/v2.3/
