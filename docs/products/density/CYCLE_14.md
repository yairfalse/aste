# Cycle 14 — Full processing-boundary allocation check

## What changed

Extended allocation counting from the framework-independent core across the
full JUCE processor adapter. Added repeated prepare/process/release coverage for
mono, stereo, bypass, abrupt parameter endpoints, required rates and block
sizes, and zero-length processing.

## Why

A no-allocation core does not prove that its plugin wrapper is safe. The host
calls `processBlock`, so the automated boundary must match that entry point.

## Evidence

- Sixty-four lifecycle cycles execute 896 full adapter processing calls.
- Coverage spans six required sample rates, thirteen nonzero block sizes plus
  zero, mono and stereo buffers, active and bypass paths, and abrupt Drive,
  Density, and Stereo endpoints.
- All 896 calls report zero ordinary C++ heap allocations.
- Every processed sample remains finite.
- Release and ASan/UBSan plugin builds each pass all six CTest checks.

## Risks

The counter detects ordinary C++ heap allocation in this executable. It does
not prove absence of locks, system calls, aligned allocation, Objective-C
runtime allocation, priority inversion, or host behavior outside the callback.

## Next step

Add a targeted lock/system-call trap only if a platform-supported test can
produce reliable evidence; do not infer those properties from allocation data.
