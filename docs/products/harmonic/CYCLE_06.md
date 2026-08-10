# Cycle 6 — audible Harmonic participation

## What changed

Raised the bounded state-variable drive from 1–3 to 1–6 and replaced linear
positive-gain participation with a square-root law. The full contribution
coefficient is 1.1. Cuts, neutral gain, host parameters, schema 1, zero latency,
and the serial four-band topology are unchanged.

## Why

Music-machine use found the EQ's nonlinear identity too restrained and small
parameter moves too easy to miss. Square-root participation makes a +3 dB boost
engage half of the gain-dependent character range instead of one quarter,
without coloring cuts or adding another mode.

## Evidence

- At +12 dB Presence and a 0.5-peak 1 kHz sine, H3 ratios at Harmonic
  0/25/50/100% are `4.71e-8`, `0.00202`, `0.00595`, and `0.01279`; motion is
  monotonic and full scale is about -37.9 dBc.
- At +3 dB and full Harmonic, the same test measures a `0.01980` H3 ratio,
  about -34.1 dBc, so modest boosts enter an unmistakable character range.
- A -12/-6 dB two-band cut remains sample-identical at Harmonic 0 and 100%.
- Six-rate broad/sculpt reports remain finite, zero-latency, and active. Peak
  output stays below 0.61 in the tracked fixtures.
- The 48 kHz / 128-sample worst-case automation benchmark measures 0.718134%
  median across five runs (0.711658–0.743789%) on one M4 Pro core.
- Core tests retain deterministic variable-block output and zero callback
  allocation.

## Risks

The production mapping is now intentionally much stronger than lab Candidate
3's original advance gate. Its differential alias and multiband fatigue costs
need fresh measurement and listening. At strong settings nonlinear compression
can reduce net RMS gain, so all preference tests must be output-level matched.

## Next step

Install the rebuilt Harmonic bundle and repeat the music-machine checklist with
special attention to +3 dB boost sensitivity, Presence/Air fatigue, serial
multi-band interaction, and useful Harmonic taper.
