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

## Decision

Keep the linear filter as the reference and the second candidate as a live lab
comparison. Do not promote or tune the first residual topology. Candidate 2
passes its predeclared gate without post-measurement threshold changes, but its
strong IMD and folded-harmonic results prevent a production decision. The next
candidate is the stateful nonlinear-filter graph; both survivors then require
multi-band interaction, differential alias measurement, and level-matched
listening before selection.
