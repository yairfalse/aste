# Harmonic H-01 DSP research

## References

The linear peaking reference uses the normalized peaking-EQ biquad equations
documented in the W3C Audio Working Group's
[Audio EQ Cookbook](https://www.w3.org/TR/audio-eq-cookbook/), adapted there
from Robert Bristow-Johnson's original work. Harmonic's provisional
gain-to-Q law is project-owned and is not claimed to come from that reference.

## Candidate 0 — linear proportional-Q reference

Status: retained as the scientific and neutral reference, not a product graph.

At ±12 dB and 1 kHz, the double-precision reference reaches the requested center
gain within `1e-9` dB at 44.1, 48, 88.2, 96, 176.4, and 192 kHz. Center phase is
within numerical noise of zero. The provisional proportional-Q law produces a
half-gain bandwidth of 0.586–0.590 octaves over those rates. A zero-gain impulse
null is exactly zero because neutral is an explicit bypass state.

## Candidate 1 — nonlinear band-residual excitation

Status: rejected in its first form.

The candidate derives the linear filter residual, applies a bounded odd `tanh`
transfer to boosted residual only, and recombines it with the linear output. Cut
therefore remains linear. At +12 dB, a 0.5-peak 1 kHz input, and Harmonic amounts
0.25, 0.5, and 1.0, H3 rises monotonically from the numerical floor to roughly
-35.37, -30.77, and -22.85 dBc. H2 remains at the numerical floor, as expected
for the symmetric transfer. The equal -12 dB cut remains near the numerical
harmonic floor.

The same sweep exposes the rejection reason: measured center gain falls from
+12 dB to approximately +10.74, +8.77, and +2.16 dB as Harmonic increases. The
macro therefore changes the meaning of the band gain by as much as 9.84 dB.
That violates the product contract even though the harmonic motion is monotonic.

The tracked test requires six-rate finiteness, exact neutral null, exact linear
center gain, clean cut behavior, nontrivial boosted H3, and monotonic H3 across
the Harmonic sweep. The CSV reports the gain collapse rather than compensating
or hiding it.

## Candidate 2 — pre-emphasis, bounded stage, and de-emphasis

Status: advances through the frozen lab gate; retained for comparison, not
selected for production.

The candidate gives boosted material a +6 dB, Q 0.9 pre-emphasis around the
selected center, applies an odd-symmetric bounded `tanh` stage with drive from
1 to 3, applies the reciprocal -6 dB contour, and then sends that result through
the requested proportional-Q boost. Five percent of the difference between
this colored branch and the linear reference is blended back at full Harmonic.
Cuts and Harmonic zero take an explicit linear path. This is an original lab
graph, not a circuit-emulation claim.

The frozen matrix contains 174 tone observations: five centers, four levels,
equal cuts, and a canonical four-position macro sweep at all six sample rates.
It also contains 24 two-tone observations. Through 0.5 peak input, worst center
gain error is 0.295 dB and worst center phase error is 0.0112 degrees. At 0.9
peak, the worst reported gain error is 0.361 dB. Full-macro canonical H3 is
-51.83 to -51.82 dBc across rates; the 0, 0.25, 0.5, and 1.0 macro sweep rises
monotonically from the numerical floor through approximately -65.4, -58.1, and
-51.8 dBc. Observable equal-cut H3 remains below -206.5 dBc.

The cost is not hidden. The folded odd-harmonic proxy reaches -50.29 dBc at the
44.1 kHz / 12 kHz / 0.5-peak case. Active third-order SMPTE-style products at
7 kHz +/- 120 Hz measure about -41.38 dBc. Active CCIF-style third-order
products at 18 and 21 kHz measure -47.09 to -44.01 dBc. These metrics do not yet
have a selection ceiling: they are comparison evidence for later anti-alias and
listening work.

At 48 kHz, 12 kHz H3 folds exactly onto the fundamental. The analyzer records
those five rows as `h3_observable=0`, excludes the collision from the H3 gate
and folded proxy, and preserves the raw bin value. This prevents a linear cut
from being falsely classified as 0 dBc distortion. Alias figures remain a
coherent-bin proxy rather than a high-rate differential measurement.

## Candidate 3 — bounded state-variable filter

Status: advances through the frozen lab gate; retained beside Candidate 2, not
selected for production.

The candidate runs matching linear and nonlinear topology-preserving
state-variable band sections at Q 0.9. Harmonic continuously interpolates the
nonlinear section's band-integrator update from its linear value to a bounded
`tanh` value while drive moves from 1 to 3. At full Harmonic, eight percent of
the nonlinear-minus-linear band state is added over the unchanged
proportional-Q contour. Cuts remain explicitly linear. This is a deliberately
small behavioral experiment, not a circuit model or a formal zero-delay
nonlinear solve.

The shared Candidate 2 matrix gives a direct comparison. Through 0.5 peak,
Candidate 3's worst center-gain error is 0.138 dB; its worst error at 0.9 peak
is 0.145 dB. Worst phase error is 0.0316 degrees. Canonical full-macro H3 spans
-62.47 to -61.24 dBc across rates and rises monotonically. Observable cut H3
again remains below -206.5 dBc.

Its worst folded odd-harmonic proxy is -57.42 dBc, 7.13 dB below Candidate 2's
worst result. Active two-tone products span -119.60 to -55.12 dBc; the worst is
13.75 dB below Candidate 2's -41.38 dBc result. These are coherent-bin proxy
comparisons, not a production anti-alias decision.

All 120 state rows are finite. Maximum impulse peak is 1.513 and maximum
0.9-peak overload output is 3.542. After two seconds of silence, impulse, step,
and overload tails fall below the -300 dBFS reporting floor. A 0.5-level step
settles with at most `8.1e-12` absolute steady-state error. Neutral silence is
exact. Two complete renders of all three CSV files are byte-identical.

## Decision

Keep the linear filter as the reference and Candidates 2 and 3 as live lab
comparisons. Do not promote or tune the first residual topology. Candidate 3
has the stronger initial objective result, but objective cleanliness alone does
not establish the product's useful nonlinear identity. Both survivors now
require serial and parallel multi-band interaction, differential alias
measurement, automation and block-schedule tests, CPU measurement, and
level-matched listening before selection.
