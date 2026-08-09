# Cycle 4 — bounded state-variable candidate

## What changed

Added Candidate 3: matching linear and bounded topology-preserving
state-variable band sections whose state difference is mixed over the unchanged
linear contour. Candidate 2 and Candidate 3 now share one measurement matrix.
The stateful command adds neutral, impulse, step, overload, steady-state, and
two-second recovery CSV evidence.

## Why

Candidate 2 proved that selective drive can preserve the EQ gesture, but its
IMD and folded-harmonic costs are strong. Candidate 3 tests whether placing the
bounded behavior inside filter state produces a more controlled interaction.
Its topology and gate were frozen before the first render.

## Evidence

- `harmonic_stateful_compare` passes at all six supported sample rates.
- All 174 tone, 24 two-tone, and 120 state rows are finite.
- Worst gated center-gain error is 0.138 dB; worst 0.9-peak error is 0.145 dB.
- Worst center-phase error is 0.0316 degrees.
- Full-macro canonical H3 spans -62.47 to -61.24 dBc and rises monotonically.
- Observable cut H3 remains below -206.5 dBc.
- Worst folded odd-harmonic proxy is -57.42 dBc, 7.13 dB below Candidate 2.
- Worst active two-tone result is -55.12 dBc, 13.75 dB below Candidate 2.
- Maximum impulse and overload peaks are 1.513 and 3.542.
- Step steady-state error is at most `8.1e-12`; all recovery tails are below the
  -300 dBFS reporting floor after two seconds.
- Two renders of every output CSV compare byte-for-byte equal.
- Candidate result: `valid=true`, `advances=true`.

## Risks

This is a behavioral state update, not a solved nonlinear circuit equation.
Its eight-percent mapping and Q are research constants. The objective result is
cleaner than Candidate 2, but it may also be less musically distinctive. No
multi-band ordering, automation, variable-block, CPU, differential alias, or
listening preference exists yet.

## Next step

Build serial and parallel two-band/four-band interaction fixtures for both
survivors. Measure unexpected gain, order dependence, phase/recombination,
differential alias energy, deterministic automation and block schedules, and
CPU before generating level-matched listening packs.
