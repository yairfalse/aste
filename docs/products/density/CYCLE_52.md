# Cycle 52 — x86_64 and universal VST3 validation

## What changed

Built and executed a complete x86_64 Density VST3/test tree under Rosetta, then
built one ad-hoc-signed universal VST3 containing native arm64 and x86_64 slices.
Built an x86_64 Steinberg validator from the pinned 3.8.0 source to validate the
matching slice.

## Why

An arm64-only build cannot support the requested Intel workflow, and a universal
file claim is insufficient unless both slices load and validate.

## Evidence

- `file` identifies the x86_64 plugin, lab, and adapter test executables as thin
  x86_64 Mach-O binaries.
- The x86_64 tree passes 31/31 checks under Rosetta, including state fuzzing,
  callback audits, editor artifacts, documentation, and provenance.
- `lipo` reports exactly `x86_64 arm64` for the universal VST3 executable.
- The universal bundle passes strict deep code-signature verification.
- The arm64 and x86_64 Steinberg 3.8.0 validators each pass the universal bundle
  at extensive level: 537/537.
- pluginval 1.0.4 passes the universal bundle at strictness 10 with seed
  `0xd01` on arm64.

## Risks

Rosetta proves executable compatibility, not native Intel CPU performance or
DAW behavior. The bundle is ad-hoc signed, not Developer ID signed or notarized.
No distributable artifact is claimed.

## Next step

Run the same x86_64 binary on a native Intel Mac and measure the production CPU
budget inside the target hosts.
