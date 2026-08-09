# Cycle 63 — package-bound security evidence

## What changed

Embedded the exact reviewed dependency-security ledger in every internal
Density package. `PACKAGE.json` now records its file name, SHA-256, review date,
expiry, and disposition. Package inspection requires the repository-known
ledger digest and re-runs the complete security validation.

## Why

Repository CI evidence alone does not travel with an archive. Copying the
ledger without a trusted digest would also be weak because an archive could
rewrite the ledger, package metadata, and checksum inventory together.

## Evidence

- The embedded `DEPENDENCY-SECURITY.json` digest is
  `9423a8e39443095417f73b0b1f180dd7202b45ded5c8387ffa846fe476d6c55d`.
- `PACKAGE.json` repeats that digest, the 2026-08-09 review date, the
  2026-11-07 expiry, and `no_known_affected_advisories` disposition.
- Two rebuilds produced the same 12-file archive at SHA-256
  `8f2cad6d6b4678e02544dd6bddf41909c76ec57f7b4089baee57847427e0f0e6`.
- Verification as of 2026-11-08 rejects the otherwise intact package because
  the review expired on 2026-11-07.
- A tampered archive was given a rewritten ledger, matching `PACKAGE.json`
  digest, and fully recomputed `CONTENTS.sha256`; verification still rejected
  it against the repository-known ledger digest.
- A separately recomputed archive with only the duplicated review date altered
  was rejected because `PACKAGE.json` no longer reconstructed exactly.
- The package verifier suppresses imported-module bytecode output and leaves a
  clean source tree unchanged.
- The universal local CTest suite passes 33/33 tests.

## Risks

The digest is a repository review anchor, not a cryptographic publisher
signature; anyone authorized to change policy code can update it. The normal
verifier deliberately rejects an expired package, while `--as-of` exists for
deterministic testing and historical inspection and must not be used to waive a
release gate. The archive remains ad-hoc signed and internal-only.

## Next step

Audit the operational supply chain separately: pin GitHub Actions to immutable
commits, inventory CI/build dependencies, and keep them out of the product SBOM.
