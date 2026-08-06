# Cycle 12 — Editor render baseline

## What changed

Added one plugin-test mode that renders the active 980×540 editor at 2× scale
and measures full-panel software paint time over 120 idle and active frames.
Visual review exposed and corrected a `-0.00 dB` Output display at its zero
default. CTest generates the PNG in the build directory.

## Why

The editor contract now has structural coverage. A deterministic visual
artifact and measured paint baseline provide evidence for repaint work without
adding screenshot infrastructure to production code.

## Evidence

- Five Release runs measured a median 0.500 ms per idle full-panel paint and
  0.455 ms with active meters at 980×540.
- The reviewed 1960×1080 PNG preserves the intended Density-first hierarchy,
  boundary meters, restrained oxblood accent, and legible control grouping.
- The corrected Output now displays canonical `0.00 dB` at its default.
- Repeated renders produced SHA-256
  `325440f9982010a78f84453ccd130e2b741f1bd7cb322fa090008d40299271cd`.
- Release and ASan/UBSan plugin builds each pass all six CTest checks.

## Risks

Headless software rendering excludes native host compositing, Retina display
transfer, GPU behavior, window resize activity, and actual 30 Hz timer wake-up
cost. Timing is a local baseline, not a cross-machine release threshold.

## Next step

Keep the approved image as the local baseline. Validate Retina transfer and
meter-only repaint cost in an actual VST3 host; do not optimize the current
paint path without evidence from that run.
