# ADR 0003 — Independent VST3 validator gate

Status: accepted, 2026-08-09.

## Context

Density already has linked adapter tests, a repo-owned binary-loading host, and
local pluginval and Steinberg validator evidence. Hosted CI did not reproduce an
independent standards validator, so a wrapper regression could pass every
repo-owned test and still merge.

The gate must not add code to the product binary or introduce a copyleft or
closed-source release dependency.

## Decision

The universal VST3 CI job fetches Steinberg VST3 SDK 3.8.0 at immutable commit
`9fad9770f2ae8542ab1a548a68c1ad1ac690abe0`, builds only its official arm64
command-line validator, and runs the extensive suite against the signed
universal Density bundle.

The SDK is MIT-licensed. Its source and validator remain CI-only and are not
linked into or packaged with Density. The existing local pluginval protocol is
retained as complementary evidence but is not added to CI because this one
source-built authoritative validator closes the standards gate without adding
a second external tool or GPL-governed build path.

## Consequences

- Every VST3 CI run independently checks the built bundle after internal tests,
  code-signature verification, and architecture verification.
- The validator source identity and licence are reviewable and deterministic.
- CI requires network access to fetch the pinned SDK and its four validator
  submodules; VSTGUI, examples, tutorials, and documentation are not fetched.
- Hosted validation executes the arm64 slice. Existing local arm64 and Rosetta
  x86_64 537/537 results remain architecture evidence; native Intel and DAW
  testing remain separate release gates.
