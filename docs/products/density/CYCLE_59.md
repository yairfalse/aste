# Cycle 59 — independent VST3 validation gate

## What changed

Added Steinberg VST3 SDK 3.8.0 extensive validation to the hosted universal
VST3 job. CI fetches immutable commit
`9fad9770f2ae8542ab1a548a68c1ad1ac690abe0`, builds only the arm64 validator
target, and runs it against the already tested and signature-verified bundle.
Recorded the MIT licence and accepted the dependency boundary in ADR 0003.

## Why

The repository had strong local validator history but the hosted gate stopped
at repo-owned tests and a repo-owned smoke host. Independent validation must be
reproducible on every change rather than preserved only as workstation history.

## Evidence

- Official tag `v3.8.0_build_66` resolves to the pinned commit.
- The pinned SDK and validator source are MIT-licensed.
- A clean validator-only Release build succeeds with VSTGUI, plug-in examples,
  utilities, and universal-validator output disabled.
- CI fetches only `base`, `cmake`, `pluginterfaces`, and `public.sdk`; unrelated
  VSTGUI, tutorial, and documentation submodules are excluded.
- The validator configure pins the hosted runner's `/usr/bin/clang` tools to
  avoid the SDK's pre-`project()` compiler-detection failure on a pristine tree.
- The resulting arm64 validator passes the current signed universal Density
  bundle at extensive level: 537 passed, 0 failed.
- The universal build tree passes all 32 CTest checks after the documentation
  and workflow changes.
- Strict bundle-signature verification passes, and `lipo -verify_arch` confirms
  both arm64 and x86_64 slices.
- Density continues to expose one audio component, one controller, 11 stable
  parameters, mono/stereo processing, and zero reported latency.

## Risks

The new CI gate downloads the SDK and submodules, increasing network exposure
and VST3-job duration. The hosted validator executes the universal bundle's
arm64 slice; native Intel, pluginval, and real DAWs remain separate evidence.
No claim is made that a validator substitutes for Cubase or Ableton.

## Next step

Require the hosted validator gate to pass, then move to reproducible packaging
metadata and artifact inspection without claiming a signed release.
