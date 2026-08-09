# Security policy

Density D-01 is an internal prototype and has no supported public release yet.
Report suspected vulnerabilities privately to the repository owner; do not put
session files, crash dumps, personal audio, or unpublished plugin binaries in a
public issue. A public security contact will be added when the company identity
and release channel are selected.

Security-relevant areas include malformed plugin state, preset parsing, host
lifecycle calls, integer or buffer bounds, dependency provenance, package
signing, and unintended filesystem or network access. Reports should include
the commit, macOS version, architecture, host, sample rate, block size, minimal
reproduction, and whether crafted data is required.

The project does not currently provide a compatibility or embargo guarantee.
Do not distribute a build until dependency licences, signing identity, package
contents, and release checks have been reviewed.

Packaged third-party dependencies have a reviewed advisory ledger at
[`docs/security/dependency-audit.json`](docs/security/dependency-audit.json).
The offline gate binds that evidence to the exact JUCE and bundled VST3 SDK
pins and expires it after 90 days. "No known affected advisories" is a dated
search result, not a claim that the dependencies are vulnerability-free. The
review method is recorded in
[ADR 0006](docs/adr/0006-expiring-dependency-security-review.md).
The internal package embeds that exact ledger and binds its digest, review
date, expiry, and disposition into `PACKAGE.json`. Rewriting the archive's own
checksum inventory cannot replace the repository-reviewed digest.
