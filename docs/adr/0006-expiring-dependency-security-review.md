# ADR 0006 — expiring dependency-security review

Status: Accepted — 2026-08-09

## Context

Density's package has two third-party runtime components: pinned JUCE and the
VST3 SDK sources bundled inside that JUCE tree. Neither is represented by a
package-manager lockfile that a normal ecosystem scanner can evaluate. A live
network lookup in every build would make CI depend on mutable services without
removing the need to review version ranges and false-positive keyword matches.

## Decision

Keep one reviewed JSON ledger at `docs/security/dependency-audit.json`. Record
the exact packaged identities, the OSV exact-commit query, each publisher's
public GitHub advisory endpoint, and an NVD keyword query. Retain relevant
historical advisories with an explicit disposition and account for unrelated
keyword matches instead of silently discarding them.

The standard-library checker cross-checks the ledger against the CMake and
package pins and rejects incomplete source coverage, an OSV match on an exact
audited commit, missing historical advisories, future dates, or a review older
than 90 days. CTest runs the checker offline. Updating a dependency or reaching
the expiry date therefore requires a new network review and committed evidence.

The package embeds the exact ledger bytes. Its policy record carries the
ledger SHA-256, review date, expiry, and disposition, while the verifier pins
the reviewed digest in repository code. Recomputing an archive's internal
checksums therefore cannot substitute altered security evidence.

This ledger covers packaged runtime dependencies only. The standalone
validator checkout, GitHub Actions, build tools, and macOS system libraries are
separate operational dependencies and are not represented as product contents.

## Consequences

Ordinary builds remain deterministic and do not transmit dependency data. The
repository cannot silently carry an indefinitely stale "no known issues"
statement. The result still means only that no published advisory in the
reviewed sources is known to affect the pins; it is not proof that the code has
no vulnerability and it does not replace source review, fuzzing, or signing.
