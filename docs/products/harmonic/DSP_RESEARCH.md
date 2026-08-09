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

## Decision

Keep the linear filter as the reference. Do not promote or tune the first
residual topology. The next candidate should test pre-emphasis around a bounded
stage with explicit fundamental-gain measurement; it advances only if Harmonic
motion preserves the contour within a frozen tolerance.
