# ADR 0014: Density and Harmonic character range

- Status: accepted for internal beta
- Date: 2026-08-10

## Decision

Keep both product topologies and stable host parameter contracts, but widen
their internal musical mappings.

Density's hero macro now spans -2 to -30 dB threshold, 3:1 to 60:1 ratio,
1-to-9 saturation drive, 1-to-5 release curvature, and 0-to-10 dB crush-path
make-up. Crush scales saturation participation, and positive Drive increases
the resulting saturation drive by up to 75%.

Harmonic's bounded state-variable drive now spans 1 to 6. A boosted band's
state-difference contribution uses square-root gain participation and a 1.1
full-scale coefficient. Cuts and zero gain retain the exact linear path.

## Why

Music-machine feedback found both instruments too conservative and parameter
motion insufficiently audible. New public parameters, modes, or alternate
topologies would enlarge the interface without addressing the mapping problem.
The revised laws use the existing musical controls and remain continuous.

## Evidence

Density's four-fixture 0/33/67/100% macro pack remains level matched within
0.000001 dB and monotonic. Maximum gain reduction now spans 0–2.763 dB at
0%, 7.648–13.219 dB at 33%, 17.101–22.982 dB at 67%, and 26.317–32.340 dB at
100%. Adjacent musical renders null between -38.96 and -20.86 dBFS; the dry
control remains sample-identical.

At +12 dB Presence and a 0.5-peak 1 kHz sine, Harmonic's product H3 ratio rises
monotonically from `4.71e-8` through `0.00202`, `0.00595`, and `0.01279` at
0/25/50/100%, reaching about -37.9 dBc. Equal cuts remain sample-identical
between Harmonic 0 and 100%. At +3 dB and full Harmonic, the same test measures
a `0.01980` H3 ratio, about -34.1 dBc, proving that a modest boost participates
clearly. The revised graph remains below the 1% local CPU budget.

## Consequences

No parameter ID, range, state schema, topology, or reported latency changes.
Existing sessions recall their values exactly but intentionally render with the
new internal character law. Density golden audio is therefore reviewed and
updated. Harmonic's stronger production mapping needs renewed alias, fatigue,
and level-matched listening checks before release-candidate status.
