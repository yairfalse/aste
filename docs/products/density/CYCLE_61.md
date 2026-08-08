# Cycle 61 — deterministic SPDX dependency evidence

## What changed

Embedded a deterministic SPDX 2.3 JSON SBOM in the internal Density package.
Added the source commit timestamp to build metadata and made package inspection
require the exact Density, JUCE, and bundled VST3 SDK identities, licensing
status, and relationships.

## Why

The SHA-256 inventory proves which files are present but not which upstream
software contributes to the binary. Release engineering needs both kinds of
evidence, while unresolved distribution terms must remain explicit rather than
being converted into an unsupported licence conclusion.

## Evidence

- The SBOM contains three packages and four relationships.
- JUCE is fixed at version 8.0.13 and commit
  `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`.
- The bundled Steinberg VST3 SDK is identified as version 3.8.0 under MIT;
  the separate CI validator checkout is excluded from product dependencies.
- The SPDX creation time is derived from the immutable source commit timestamp,
  not wall-clock package time.
- The document records compiler, system, and architecture; its namespace is
  derived from build metadata and the deterministic bundle fingerprint.
- The generated document passes the official SPDX 2.3 draft-07 JSON schema.
- The local dirty-tree package contains 11 inventoried files and verifies at
  SHA-256 `3a5ff839ffff7c37b9e2036fea24ce3816aa561b8c0ff3ef25ccd8a4ab7b4643`.

## Risks

This is a deliberately narrow build-time SBOM, not a source-repository scan or
vulnerability report. Density and JUCE licence conclusions remain
`NOASSERTION` until the applicable distribution terms are selected. System
libraries supplied by macOS are not enumerated in this first document.

## Next step

Require the clean hosted SBOM gate to pass, then add a dependency audit that
checks the pinned components against current published security advisories.
