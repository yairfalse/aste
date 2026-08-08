# Parameter model

Stable IDs use product-local ASCII names. Normalized host values map to the
physical ranges below; ranges are provisional until musical validation, but IDs
are treated as permanent from the first external build.

| ID | Name | Unit/range | Default | Mapping | Smoothing |
|---|---|---|---:|---|---|
| `drive` | Drive | -12..24 dB | 0 | linear dB | cascaded 3+3 ms |
| `crush` | Crush | 0..100 % | 65 | linear | 10 ms |
| `attack` | Attack | 0.02..30 ms | 1 | logarithmic | 5 ms in log-time space |
| `release` | Release | 20..1200 ms | 180 | logarithmic | coefficient per block |
| `density` | Density | 0..100 % | 50 | linear | 10 ms |
| `blend` | Blend | 0..100 % | 50 | linear | cascaded 3+3 ms |
| `stereo` | Stereo link | 0..100 % | 100 | linear | 10 ms |
| `output` | Output | -24..12 dB | 0 | linear dB | cascaded 3+3 ms |
| `detector_hpf` | Detector HPF | 20..300 Hz | 90 | logarithmic | coefficient per block |
| `protection` | Protection | off/on | on | boolean | deliberate transition |
| `bypass` | Host bypass | off/on | off | boolean | host transition |

Drag, fine adjustment, reset, typed entry, and text conversion belong to the
JUCE adapter. Exact parsing rejects trailing junk and non-finite values.

Density continuously lowers threshold while increasing ratio, saturation drive,
program-dependent release curvature, and crushed-path make-up. There are no
topology switches. `mapDensity()` is the maintainer-visible mapping and has a
monotonicity test.
