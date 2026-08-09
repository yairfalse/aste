# Cycle 62 — expiring dependency-security gate

## What changed

Added a machine-readable security review for the two packaged third-party
components and an offline CTest gate that binds the review to their exact
versions and commits. The gate expires 90 days after review.

## Why

An SBOM identifies dependencies but does not say whether published security
advisories were reviewed. Density's C++ sources do not have a package-manager
lockfile suitable for a conventional ecosystem scanner, and a live query in
every build would make CI nondeterministic.

## Evidence

- The official JUCE `8.0.13` tag resolves to the packaged commit
  `7c9d3783b127263d72bb65fe0a7e2dc8a02a7ac2`.
- The official VST3 SDK `v3.8.0_build_66` tag resolves to
  `9fad9770f2ae8542ab1a548a68c1ad1ac690abe0`.
- OSV exact-commit queries returned no advisories for either commit.
- Both publishers' public GitHub repository advisory endpoints returned no
  published repository advisories.
- NVD returned two relevant JUCE records, CVE-2021-23520 and CVE-2021-23521;
  both affect versions before 6.1.5, so JUCE 8.0.13 is outside the ranges.
- NVD's other two JUCE keyword matches were reviewed and recorded as unrelated;
  its VST3 SDK query returned no matches.
- Six negative cases reject expiry, changed identity, incomplete NVD
  accounting, malformed advisory identity, malformed lists, and a non-object
  document root.
- The universal local CTest suite passes 33/33 tests.
- Two package rebuilds produced the same 11-file archive at SHA-256
  `971be4e69ddb5d550d20b1287116714f9b3f973286f28fa1ba7ad8697fdf52d8`.

## Risks

No-known-affected-advisory is not a security guarantee. OSV and NVD coverage of
C++ source dependencies may be incomplete, and keyword matching requires human
classification. CI tooling and operating-system libraries are outside this
packaged-runtime review. The evidence expires on 2026-11-07.

## Next step

Require the new gate on the hosted macOS matrix, then carry the reviewed date,
expiry, and ledger digest into the deterministic internal package.
