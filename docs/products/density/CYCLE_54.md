# Cycle 54 — Standalone VST3 host

## What changed

Added a repo-owned headless standalone host using JUCE's existing VST3 loader.
It discovers and instantiates the built Density bundle, checks its public
contract, exchanges state, and processes deterministic stereo signals without
linking product code.

## Why

The linked adapter tests and external validators did not satisfy the brief's
standalone-host requirement. Loading the binary through VST3 catches packaging,
factory, and host-wrapper failures that direct construction cannot reach.

## Evidence

- Arm64 Release, arm64 ASan/UBSan, and x86_64 Release matrices pass 32/32.
- The host sees exactly one `Density D-01` component, 11 parameters, and zero
  reported latency.
- State mutation changes serialized data; restore reproduces the prior bytes
  within the same host instance.
- Stereo processing remains finite at block sizes 1, 2, 7, 127, 511, and 2048.
- Native arm64 and Rosetta x86_64 host binaries each load the matching slice of
  the universal VST3.
- A state carrying Drive at normalized value 0.8125 restores arm64-to-x86_64
  and x86_64-to-arm64. The architecture-specific wrapper encodings need not be
  byte-identical to restore the same product state.

## Risks

The smoke host uses JUCE, as does the product wrapper, so it is not an
independent conformance authority. Steinberg and pluginval remain the
independent validators. Rosetta does not replace native Intel hardware or DAW
testing.

## Next step

Run the documented Cubase, Ableton, and additional-host matrix on target
machines, including cross-architecture session transfer and offline rendering.
