# Harmonic H-01 parameter contract

These schema-1 identifiers are permanent from the first music-machine build.
All continuous controls support host automation, typed entry, fine adjustment,
and reset.

| ID | Name | Range | Default | Mapping | Smoothing |
|---|---|---:|---:|---|---|
| `input` | Input | -18..18 dB | 0 dB | linear dB | 5 ms |
| `foundation_gain` | Foundation | -12..12 dB | 0 dB | linear dB | 20 ms coefficients |
| `foundation_frequency` | Foundation Frequency | 35..160 Hz | 80 Hz | logarithmic | 20 ms coefficients |
| `body_gain` | Body | -12..12 dB | 0 dB | linear dB | 20 ms coefficients |
| `body_frequency` | Body Frequency | 160..1000 Hz | 400 Hz | logarithmic | 20 ms coefficients |
| `presence_gain` | Presence | -12..12 dB | 0 dB | linear dB | 20 ms coefficients |
| `presence_frequency` | Presence Frequency | 800..7000 Hz | 2500 Hz | logarithmic | 20 ms coefficients |
| `air_gain` | Air | -12..12 dB | 0 dB | linear dB | 20 ms coefficients |
| `air_frequency` | Air Frequency | 6000..20000 Hz | 12000 Hz | logarithmic | 20 ms coefficients |
| `harmonic` | Harmonic | 0..100 % | 35% | linear macro | 10 ms |
| `output` | Output | -18..18 dB | 0 dB | linear dB | 5 ms |
| `bypass` | Host bypass | off/on | off | boolean | host transition |

Band order is Foundation, Body, Presence, Air. Harmonic moves the bounded-stage
drive continuously from 1 to 6. At each boosted band the nonlinear-state
contribution is `1.1 * Harmonic * sqrt(positive gain / 12 dB)`. The square-root
participation law makes small positive boosts responsive instead of reserving
most character for the final few decibels. Zero and negative gain remain
linear. No parameter changes topology or latency.

From 0.3.0, Foundation is a low shelf and Air a high shelf, with slope S=1.
Their gain controls specify the plateau below/above the frequency control;
the frequency is the half-gain transition. Body and Presence remain bells:
positive contours use `Q = 0.7 / (1 + 0.7 * gain / 12)`, and cuts use
`Q = 0.7 * (1 + 0.7 * abs(gain) / 12)`. Bell gain labels remain actual
centre-frequency dB with Harmonic at zero, not scaled intensity values.
Default stays neutral. Existing saved values recall unchanged but outer bands
now render as shelves. See [ADR 0019](../../adr/0019-tone-shelves-and-direct-blend.md).
