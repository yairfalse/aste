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
drive continuously from 1 to 3 and the state-difference contribution from zero
to the measured full amount. Positive gain scales nonlinear participation;
zero and negative gain remain linear. No parameter changes topology or latency.
