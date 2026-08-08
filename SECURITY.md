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
