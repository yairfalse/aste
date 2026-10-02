# ADR 0019: Tone shelves and a more direct compressor blend

- Status: development; real-music listening pending
- Date: 2026-10-03

## Decision

Further listening feedback requested more decisive tools after 0.2.0. Instead
of increasing distortion or master level again, make Harmonic's outer bands
low/high shelves and reduce Density's dry dilution in its starting points.

Foundation and Air use the standard Audio EQ Cookbook low/high shelving biquad
equations with slope S=1. They retain double coefficient/state precision,
20 ms coefficient smoothing, exact neutral bypass and zero latency. No new
parameters, modes or dependencies are added. Body/Presence bells and the
nonlinear stage are unchanged. The gain labels specify shelf plateau gain;
the frequency knobs mark the half-gain corner. Tooltips explain that distinction.

Density's default is 90% Blend and -7 dB Output. Its existing coloured presets
use 50–80% Blend with lower Output trims. The compressor graph, threshold,
ratio, saturation and protection remain unchanged. Full dry and fully wet
remain available, and saved parameters are not reset.

## Verification

The spectral regression exercises all four bands, +/-6 and +/-12 dB, four
frequency positions, and all six sample rates, automating from neutral. It
checks half-gain shelf corners within 0.1 dB, shelf plateaus within 0.3 dB and
less than 0.3 dB influence at the opposite spectral end. Bell-centre gain and
wide boosts remain checked. The host-boundary test distinguishes bell centres
from shelf corners rather than misreporting the latter as full gain.

At 48 kHz, Density's -24 to -18 dBFS-peak sine input step now produces a 0.365 dB
RMS output increase, versus 0.588 dB in 0.2.0. The regression requires under
0.4 dB at each tested rate; gain-reduction targets remain unchanged. The v2
Density golden uses explicit parameters and must continue to match unchanged.

These are response measurements, not a substitute for level-matched listening
on music. Existing finite-output, realtime, neutral, stereo, state-recall,
variable-block, alias-report and host checks remain required. No limiter,
smoothing or validation is removed to produce more apparent impact.

All 70 local regression checks pass, including the unchanged Density v2 golden.
Both changed plugins pass 537 Steinberg extensive checks. DSP ASan/UBSan,
Clang static analysis, formatting and the existing local CPU budgets pass.
These results do not establish native Intel/oldest-macOS or real-DAW support.

## Compatibility

Version 0.3.0 intentionally changes existing Harmonic outer-band renders.
Parameter IDs, ranges and schema 1 remain stable. Retain 0.2.0 for exact old
renders. Density's new defaults/presets are opt-in for existing instances;
their saved settings render as before. Other four tools are unchanged.
