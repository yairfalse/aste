# Cycle 70 — assertive Density range

## What changed

Expanded the Density macro's threshold, ratio, saturation, and nonlinear
release ranges. Crush now scales saturation participation, while positive Drive
adds up to 75% more saturation drive. Parameter IDs, ranges, schema 1, routing,
latency, and UI remain unchanged.

## Why

Music-machine use found the compressor conservative and several control moves
less audible than their panel prominence implied. The issue was the internal
mapping, not a missing control or alternate compressor topology.

## Evidence

- The 0/33/67/100% macro audition is monotonic on all four musical fixtures,
  level matched within 0.000001 dB, and leaves the dry control bit-identical.
- Maximum gain reduction reaches 26.317–32.340 dB at Density 100%, versus
  20.225–26.036 dB in the preceding pack.
- The production golden fixtures gain 0.29–0.75 dB more RMS and reach
  18.50–24.41 dB maximum gain reduction, with zero reported latency.
- Unit tests prove monotonic saturation, near-linear saturation at Crush 0,
  positive-Drive participation, finiteness, and allocation-free processing.
- Five 48 kHz / 128-sample Release benchmarks measure 0.275783–0.288113% of
  one M4 Pro core, with a 0.283874% median.

## Risks

The stronger 1x saturator can expose more aliasing and hardness, especially on
high-frequency material. Output protection remains sample-peak only. Synthetic
fixtures prove direction and determinism, not a mastering preference.

## Next step

Replace the installed Density bundle and repeat level-matched tests on full
mixes, percussion, bass-heavy, ambient, and sparse acoustic material. Record
whether the top third is useful or should be tapered before release candidate.
