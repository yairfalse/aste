# Cycle 46 — Blind Density macro ranking

## What changed

Added five deterministic anonymous A/B/C/D trials for ranking 0%, 33%, 67%,
and 100% Density. Four musical fixtures use the production graph; a fifth
dry-path trial is an exact null control. Answer, response, measurement, and
instruction files remain separate.

## Why

The internal macro mappings are mathematically monotonic, but release gate 17
requires evidence that listeners perceive the complete interaction as
increasing density. Level-matched blind ranking isolates that question from
loudness and label bias.

## Evidence

- The pack contains 20 four-second, 48 kHz stereo WAV files covering transient,
  bass, dense, ambient, and dry-control trials.
- Maximum gain reduction rises at every Density step for all four musical
  fixtures: 0.000–3.873 dB at 0%, 6.270–11.598 dB at 33%, 13.327–18.975 dB at
  67%, and 20.225–26.036 dB at 100%.
- Every trial is RMS-matched within 0.000001 dB and normalized to a common
  -1 dBFS sample peak within 0.000001 dB.
- Adjacent musical renders remain distinct, nulling between -46.352 and
  -24.727 dBFS. All three adjacent dry-control comparisons null below
  -300 dBFS and produce byte-identical WAVs.
- Repeated generation with seed `852294` is byte-identical. Release and sanitizer
  answer keys share SHA-256 `89b64886b13f24b353adcdbd8359271539acc0d822915dae2ca3918e021c2e6b`;
  measurements share `588bbf335408111b364106082eb05216906459fe16c215469eb9d2fbf27061c4`.
- Release and Address/UndefinedBehavior sanitizer matrices pass 29/29.
- Production DSP, golden output, latency, and parameter behavior remain
  unchanged.

## Risks

Synthetic fixtures at one production setting do not represent every mastering
context. RMS matching intentionally removes loudness as a cue, while loudness
may be part of the macro's normal musical effect. Listening responses remain
pending, so release gate 17 stays open.

## Next step

Complete `build-plugin/density-macro-auditions/responses.csv` blind on monitors
and headphones, then compare the submitted rankings with the answer key.
