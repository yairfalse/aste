# Cycle 44 — Stereo-link stability measurements

## What changed

Added a deterministic production report for centered kick, hard-panned
percussion, correlated program, decorrelated ambience, mono-in-stereo, and
polarity-inverted fixtures at 0%, 50%, and 100% detector link.

## Why

Endpoint unit tests proved detector behavior but did not quantify image balance,
correlation, or width through the complete nonlinear parallel graph.

## Evidence

- All 18 renders remain finite under 23.254–27.356 dB gain reduction.
- Centered and mono-in-stereo fixtures remain sample-identical between channels
  at every link setting; maximum error is exactly zero.
- Polarity-inverted fixtures preserve exact inversion at every link setting;
  maximum summed-channel error is exactly zero.
- Increasing link monotonically improves balance preservation for every
  asymmetric fixture.
- Full link reduces absolute balance movement versus independent detection by
  18.846 dB for hard-panned material, 0.733 dB for correlated program, and
  5.268 dB for decorrelated ambience.
- Worst fully linked balance movement is 3.952 dB and worst correlation movement
  is 0.006960 in these deliberately severe cases. No perceptual pass threshold
  is claimed.
- Release and Address/UndefinedBehavior sanitizer matrices pass 27/27.
- Release and sanitizer CSVs are byte-identical with SHA-256
  `b1c4912d83e1c8f1efaf674c3f865653df8349a600beee3ebe60858a82056431`.
- Production golden and six-rate variable-block evidence remain unchanged.

## Risks

These are synthetic fixtures at one aggressive operating point. A 3.952 dB
fully linked balance change may still be musically objectionable, and objective
balance/correlation metrics cannot identify perceived image motion alone.

## Next step

Render an anonymous level-matched stereo-link audition pack from the measured
fixtures, then collect headphone, monitor, and mono listening responses.
