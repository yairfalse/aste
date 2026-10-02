# ADR 0018: Audible EQ and compression at track levels

- Status: accepted for development; musical listening pending
- Date: 2026-10-02

## Problem

User listening found Harmonic and Density barely audible in music. The old
Density default's effective threshold was -11.8 dBFS with a 50% dry blend.
Deterministic 1 kHz signals peaking at -24 and -18 dBFS produced no compression.
Harmonic's band controls were connected and delivered their labelled centre
gain, but boosts became narrower as gain increased, contrary to the intended
broad musical contour. A +6 dB Body boost gave only +2.03 dB one octave below.

## Decision

Keep the existing algorithms, parameter IDs/ranges, state schemas, latency and
protection. Lower Density's threshold mapping by 16 dB (-18 to -46 dB), add
12 dB to its Crush-scaled branch make-up mapping (12 to 22 dB), and start new
instances at 75% processed Blend with -6 dB Output trim. Make-up restores branch
presence; final Output is reduced so the default is not simply louder. Crush
zero still removes compression and saturation participation; Blend zero with
unity Output retains the aligned dry path.

Harmonic's positive Q law is now `0.7 / (1 + 0.7 * gain / 12)`; the existing
tighter cut law is unchanged. Centre gain is not multiplied. Five existing
coloured presets receive stronger boost/cut contrasts; Default remains exactly
neutral. The Harmonic nonlinearity itself is unchanged.

## Evidence and limits

The regression tests first failed against 0.1.1. At 48 kHz the revised Density
default measures 3.37 dB gain reduction for a -24 dBFS-peak 1 kHz sine and
9.09 dB for -18 dBFS; -30 dBFS remains below threshold. Tests cover multiple
sample rates and the JUCE processing boundary, not just the mapping formula.
The same 6 dB input increase changes the actual default blended output by only
0.59 dB, confirming compression rather than merely a more active meter.

With Harmonic zero, all four EQ bands retain their requested +/-6 dB centre
gain within 0.1 dB at quiet input. A +6 dB Body boost now gives +3.71 dB one
octave below, versus +2.03 dB before. Air gives +2.89 dB at 6 kHz versus +1.36 dB
before (12 kHz centre, 48 kHz sample rate). Frequency warping near Nyquist is
not misrepresented as constant octave bandwidth. Host-parameter tests check
each band's measured audio gain after automation from a neutral instance.

That host-level test also exposed float coefficient smoothing stopping short:
an automated +6 dB Foundation boost measured +5.838 dB at 80 Hz/48 kHz.
The peaking filter now retains double-precision coefficients and delay state,
and coefficient smoothing uses double arithmetic. This fixes convergence
rather than widening the 0.1 dB test tolerance. Audio buffers remain float,
parameter ranges and the 20 ms smoothing time are unchanged. The quiet-signal
multirate tests now start neutral and automate into both boosts and cuts.

The detector research fixture's feedback candidate was calibrated to the old
threshold. Its calibration now accounts for the threshold shift; the bounded
reference solve uses twelve iterations to keep the same residual limit. Longer
silence between isolated events allows the slow reference to recover at the
lower threshold. The dual detector's slow tail now remains active after the
short burst: release to 1 dB takes 1566.9 ms versus peak's 1111.6 ms. Its old
"faster than peak" comparison is intentionally reversed for this operating
point, not removed. Other release-order checks, sustain matching, pre-event
silence and solver residual limits are unchanged. None of these research
candidates is used by the shipped compressor.

These measurements establish changed response, not listener preference.
Level-matched real-music listening is still required. Nonlinear aliasing remains
an existing limitation; no oversampling, limiter or realtime safeguard is
removed. Density's golden metric changes must be reviewed separately from
regenerating artifacts; the 0.1.x manifest is retained.

The reviewed v2 golden keeps the same four fixtures and parameter values.
Maximum reduction rises by 15.56 dB on each fixture, as expected from the
16 dB threshold change and unchanged ratio. Final RMS decreases by 1.47 to
3.06 dB rather than hiding the change with increased master level. Peaks stay
below -2.34 dBFS; stereo correlations change by at most 0.025 and latency stays
zero. Crest factor rises by 1.03 to 2.48 dB because the parallel path retains
attacks while the compressed body changes. The old baseline is preserved in
`tests/golden/density-production-v1.csv`; CTest checks the explicitly reviewed
`density-production-v2.csv` with the same tolerances. This is a numeric review,
not a claim to have auditioned or preferred the renders.

Local verification passed all 70 CTest checks and 537 Steinberg extensive
checks for each changed plugin. ASan/UBSan DSP tests, Clang static analysis and
formatting passed. The local 48 kHz/128-frame Harmonic benchmark remains under
its 1% core budget (five-run median 0.85%); Density measured 0.31% of one core.
These are local measurements, not a supported-hardware performance guarantee.

## Compatibility

Use minor development version 0.2.0 because existing sessions intentionally
render differently despite exact parameter recall. Existing Blend values are
and Output values are not overwritten. Keep the 0.1.1 download for exact old renders. No automatic
installation, cleanup, tagging or publication is part of this change.
