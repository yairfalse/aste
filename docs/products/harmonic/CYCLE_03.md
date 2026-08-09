# Cycle 3 — pre-emphasis candidate gate

## What changed

Added Harmonic's second nonlinear lab topology and froze its advance gate before
rendering. The new command emits separate tone and two-tone CSVs across all six
sample rates. The analyzer measures contour gain, phase, H2/H3, cut cleanliness,
macro monotonicity, folded odd-harmonic energy, and third-order SMPTE/CCIF-style
products. Harmonic bins that alias exactly onto the fundamental are explicitly
marked unobservable.

## Why

Candidate 1 proved that obvious harmonic growth can destroy the user's EQ
gesture. Candidate 2 tests whether frequency-selective drive can retain that
gesture while producing controlled boost-only nonlinear participation. Freezing
the gate first prevents a favorable-looking render from redefining success.

## Evidence

- `harmonic_preemphasis_compare` passes under CTest at 44.1, 48, 88.2, 96,
  176.4, and 192 kHz.
- All 174 tone rows and 24 two-tone rows are finite.
- Worst gated center-gain error is 0.295 dB; worst center-phase error is 0.0112
  degrees.
- The 0.9-peak overload rows remain finite; worst gain error is 0.361 dB.
- Canonical full-macro H3 is approximately -51.8 dBc and rises monotonically
  across the macro sweep.
- Observable equal-cut H3 stays below -206.5 dBc.
- The worst folded odd-harmonic proxy is -50.29 dBc; active third-order
  two-tone products range from -47.09 to -41.38 dBc.
- Two independent renders compare byte-for-byte equal for both CSV files.
- Candidate result: `valid=true`, `advances=true`.

## Risks

The five-percent colored-branch blend is a research mapping, not a selected
control law. The alias result is a coherent-bin proxy, not a differential
high-rate reference. Strong two-tone products may be musically excessive. Only
one band is active, no even-bias behavior exists, no automation or block test
exists, and no listening preference has been established.

## Next step

Implement and gate the stateful nonlinear-filter candidate. Then compare both
survivors under two-band and four-band interaction, differential alias, overload
recovery, deterministic block scheduling, CPU, and level-matched listening
tests before selecting production DSP or reserving parameter IDs.
