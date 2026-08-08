# Cycle 13 — Rebuilt VST3 conformance

## What changed

Rebuilt the actual Density VST3 after the editor baseline and negative-zero
display correction. Repeated pluginval and Steinberg validation against that
bundle, verified its final ad-hoc signature, and audited installed DAWs.

## Why

Passing the internal editor executable does not prove that the changed VST3
bundle still loads and behaves correctly at its host boundary. Automated
conformance and real-host compatibility are recorded separately.

## Evidence

- pluginval 1.0.4 passes strictness 10 with fixed seed `0xd01`.
- The run covers all 78 combinations of six required sample rates and thirteen
  required block sizes, plus editor automation, state, buses, thread safety,
  processing, automation, and parameter fuzzing.
- Steinberg VST3 SDK 3.8.0 validator passes 47/47 tests.
- Strict code-signature verification passes after manifest generation.
- The project Release build passes all six CTest checks.
- The machine audit finds no Cubase, Ableton Live, or additional VST3 DAW.

## Risks

The artifact remains arm64-only. Neither validator substitutes for Cubase or
Ableton session recall, freeze, bounce, bypass, scaling, Retina, or repaint
testing. Double-precision audio processing is not implemented.

## Next step

Run the documented host matrix when a target DAW is available. Until then,
continue with host-independent release gates rather than claiming compatibility.
