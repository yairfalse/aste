# Cycle 02 — VST3 boundary

## What changed

Pinned JUCE 8.0.13 behind `ASTE_BUILD_VST3`; added an arm64 VST3 adapter, ten
stable parameters, schema-1 deterministic state, mono/stereo processing,
lock-free meter snapshots, a scalable industrial editor, and plugin tests.

## Why

This is the smallest host-loadable Density artifact. DSP stays independent of
JUCE, while the framework handles VST3 ABI, automation, text entry,
accessibility primitives, editor lifetime, and state transport.

## Evidence

- Release and ASan/UBSan core/plugin tests pass.
- pluginval 1.0.4 strictness 10 passes with all requested rates and block sizes.
- Steinberg VST3 SDK 3.8.0 validator passes all 47 tests.
- State bytes are deterministic after round-trip; malformed state is ignored.
- Plugin reports and processes zero latency; final bundle signature verifies.

## Risks

Only arm64 is built. Cubase, Ableton, true UI visual regression, audio-thread
system-call trapping, and musical listening remain. The target DAWs are not
installed on the current machine. The saturator is still un-oversampled and
the detector is fully stereo-linked.

## Next step

Implement and measure continuous stereo linking, then run Cubase/Ableton smoke
matrices when those hosts are available.
