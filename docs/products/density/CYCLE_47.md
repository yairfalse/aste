# Cycle 47 — Deterministic keyboard navigation

## What changed

Assigned an explicit keyboard focus order to all ten visible controls: Density,
Drive, Crush, Attack, Release, Blend, Stereo, Detector HPF, Output, and
Protection. Extended the existing headless editor contract test to verify every
control is focusable and occupies exactly one documented position.

## Why

Keyboard traversal previously depended on JUCE's layout inference. Explicit
ordering keeps the hero control first and makes navigation stable when the panel
is resized or rearranged.

## Evidence

- The editor contract resolves all ten controls by their accessibility titles,
  verifies keyboard focus is enabled, and checks consecutive orders 1–10.
- Exact value entry, double-click reset, accessibility exposure, resize bounds,
  headless painting, and audio-thread audits continue to pass in the same test.
- Release and Address/UndefinedBehavior sanitizer matrices pass 29/29.
- The rebuilt VST3 passes pluginval 1.0.4 at strictness 10 with seed `0xd01`.
- Steinberg VST3 SDK 3.8.0 extensive validation passes 537/537.
- No DSP, parameter, state, latency, or visual-layout behavior changed.

## Risks

The structural test does not reproduce host-specific Tab handling, macOS
VoiceOver, or a first-time user's understanding of the controls. Release gate
21 therefore remains pending independent observation in a real host.

## Next step

Verify the focus sequence and control announcements in a target DAW with
keyboard-only operation and VoiceOver, then record any navigation failures.
