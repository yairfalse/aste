# Cycle 11 — Editor interaction contract

## What changed

Added a headless plugin-editor contract test covering exact Density value
parsing, keyboard focus, default reset, accessibility exposure, documented
resize limits, and painting at minimum size.

## Why

The editor already implemented these interactions through JUCE, but they were
unverified. One focused test now protects the required operating contract
without duplicating framework behavior or changing the interface.

## Evidence

- The Density control parses `73.25 %` exactly within 0.001.
- Density and Protection expose keyboard focus and accessibility state.
- Density reports its 50% double-click default.
- Editor limits report 760×420 through 1520×840.
- A complete minimum-size editor paint produces an opaque panel.
- Release and ASan/UBSan plugin builds each pass all five CTest checks.

## Risks

This is structural coverage, not a visual-regression image comparison or a
substitute for Retina, host-background, keyboard-navigation, and assistive
technology testing in actual hosts.

## Next step

Measure editor idle and active repaint cost, then capture a reviewed visual
baseline on the target macOS display scale.
