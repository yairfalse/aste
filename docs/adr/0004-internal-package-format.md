# ADR 0004 — Deterministic internal package rehearsal

Status: accepted, 2026-08-09.

## Context

Density needs repeatable artifact inspection before final company identity,
distribution terms, Developer ID signing, and notarization exist. Producing a
downloadable release or installer now would misrepresent the prototype and
could violate unresolved licensing requirements.

## Decision

The `density_package` target creates one uncompressed ZIP for internal CI use.
It contains the signed universal VST3, build provenance, machine-readable
package policy, third-party notices, an explicit non-distribution warning, and
a SHA-256 inventory.

The standard-library packager fixes entry order, timestamps, permissions, and
compression method; rejects symlinks and unsafe paths; verifies bundle identity,
version, arm64/x86_64 slices, and ad-hoc signing; and renders twice before
writing. CI independently reopens the archive, verifies every checksum, and
requires a clean source commit. The archive is discarded after CI and is not
uploaded as a release artifact.

## Consequences

- Packaging structure and provenance can regress only by failing CI.
- The package honestly records `internal-development-only`, ad-hoc signing,
  placeholder identity, and absent notarization.
- Stored ZIP entries favor reproducibility over download size.
- This does not select an installer, public bundle identifier, company name,
  distribution licence, signing identity, or release channel.
- A distributable package remains blocked until those decisions and all release
  gates are complete.
