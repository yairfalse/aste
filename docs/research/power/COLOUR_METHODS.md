# Density and Harmonic measurement methods

These diagnostics use the unchanged production processors from baseline
`8a603dc`, through [colour_diagnostics.hpp](../../../research/power/colour_diagnostics.hpp).
`power::colourChecks()` is a short analytical/negative-control check;
`power::colourDiagnostics(directory)` writes the seven CSVs below after those
checks pass. Filesystem operations and analysis occur offline. There is no new
plugin parameter, altered production code, changed state interpretation, or
promoted algorithm.

## Reproducible scope

The module was compiled with C++20, `-O2 -Wall -Wextra -Wpedantic -Werror` and
executed successfully in the isolated audit worktree. Its first complete run
with all seven reports took approximately 8.8 seconds wall time. That is a
diagnostic runtime observation, **not** a processor CPU benchmark. The parent
audit owns final compiler/commit provenance, sanitizer gates and listening pack.
Executed raw outputs were initially retained under `build-colour/final/`.
A second complete run under `build-colour/repeat/` produced byte-identical
copies of all seven CSVs (`diff -rq` exited successfully).

| File | Data rows | What is measured |
|---|---:|---|
| `harmonic-contours.csv` | 288 | Four bands, ±6/±12 dB, three probe frequencies, six rates; Harmonic=0, quiet input |
| `harmonic-products.csv` | 176 | Eleven individual/all boost/cut profiles, four input levels, macro 0/1, ordinary versus +12/−12 gain gesture |
| `harmonic-imd.csv` | 352 | The same settings with two selected two-tone product measurements |
| `density-interactions.csv` | 182 | Twenty-six single/joint control settings on three generated musical fixtures and four sine levels |
| `density-stage-folds.csv` | 432 | Current production transfer functions/mappings, two coherent high tones, four levels, six rates |
| `harmonic-folds.csv` | 8 | Current full graph at one high tone, two levels, macro 0/1, ordinary versus linked gain |
| `harmonic-headroom.csv` | 8 | All +12 dB contours with independently extreme Input/Output and macro 0/1 |

No subjective assessment is implied. Musical sources here are deterministic
generated fixtures, not the creator's recordings or complex commercial excerpts.

## Harmonic contour, phase and nonlinear products

Tone and two-tone renders are 0.75 seconds, with processing in 127-frame blocks.
Complex sinusoidal projection measures the last 0.25 seconds after 0.5 seconds
of settling. Frequencies have integer cycles in that measurement interval;
gain and phase are measured from the output/input complex ratio, not inferred
from parameter labels. Peak, RMS, output-headroom counts and correction peak
describe the entire 0.75-second render, including its initial transient.

The contour probes use 44.1, 48, 88.2, 96, 176.4 and 192 kHz at −36 dBFS peak.
The three probe frequencies per band are:

- Foundation: 20, 80, 2000 Hz, with its frequency control at 80 Hz.
- Body: 100, 400, 1600 Hz, control at 400 Hz.
- Presence: 624, 2500, 10000 Hz, control at 2500 Hz.
- Air: 1000, 12000, 20000 Hz, control at 12000 Hz.

These are selected low-side/control/high-side observations, not a continuous
response sweep. Outer-band control frequency means the **half-gain shelf
corner**, not a bell peak. The side probes characterize approach to the shelf
plateau; they must not be called mathematically infinite-frequency plateaus.
The measurements are linear-reference response because Harmonic is zero;
nonlinear settings do not possess a single level-independent frequency response.

Nonlinear profiles are each individual ±6 dB band, all bands −6 dB, all +6 dB,
and all +12 dB. Individual tones use 80/400/2500/6004 Hz; the Air control is
6004 Hz in that individual case. All-band profiles use 1 kHz. Four source
levels are −36, −24, −12 and −6 dBFS peak. Macro positions are 0 and 1.
The selected candidate gesture uses existing **Input +12 dB / Output −12 dB**
rather than an altered nonlinear model. The gain gesture changes the operating
point deliberately; it is not the external listening-pack level match.

H2/H3/H5 ratios use actual projected output fundamental magnitude. Products
above Nyquist are marked `not_observable`, never reported as their folded
frequency under a natural-harmonic label. Air's 6004 Hz rather than exactly
6000 Hz avoids low-order fold collisions. Values below the declared −300 dB
display floor are displayed at that floor; missing/invalid reference energy
is `unknown`. Near-floor values represent numerical resolution/leakage, not
proof of physically absent distortion.

The SMPTE-style fixture combines 60 Hz and 7 kHz at an amplitude ratio of 4:1.
Its reported selected third-order power is the sum at 6880 and 7120 Hz relative
to the measured 7 kHz carrier power. The CCIF-style fixture combines 18 and
19 kHz equally; its selected products at 17 and 20 kHz are compared with the
mean of the two measured carrier powers. The sum of source amplitudes is the
reported input peak **bound**. These are explicitly defined two-tone probes,
not a claim of full SMPTE/CCIF standards conformance or total IMD.

The `correction_peak` field is the processor's maximum nonlinear-minus-linear
state correction across its bands before final Output. It changes with the
internal Input gain and is not a perceived loudness or harmonic-generation
meter. Do not compare that scalar alone to rank the linked gain gesture.

## Density's actual output versus its detector

The table keeps the normal graph and Protection enabled. Single-control sweeps
cover Density, Crush, Drive, Blend and Output, with joint Density/Crush endpoint
settings at Drive +24 dB and full wet. All other controls retain current defaults.
The input classes are percussion, bass, generated mixed music, and 1 kHz tones
at the same four peak levels. Eight-second musical fixtures include their two
seconds of final source silence; both input and output RMS cover the same
whole interval. Tone fixtures use the 0.75-second interval above.

Each row reports **both** maximum detector gain reduction and actual output
RMS gain, peak and crest factor. More gain reduction is not asserted to imply
more output density. The post-compressor makeup, saturation, fixed crush clip,
parallel blend and optional final Protection all affect that relationship.
The Output sweep is a musical control experiment, not a matching procedure:
Output precedes Protection and may change its clipping behavior.

Crush zero is not a whole-path bypass. It removes compression and almost all
tanh contribution but leaves the fixed crush clip. With a sufficiently driven
input that first clip reaches 1; enabled Protection then applies its knee again,
giving a 0.97037037 peak at unity Output. An initial diagnostic incorrectly
expected a peak near 1 with both clips active and failed. The corrected oracle
uses the analytical second-knee value and separately verifies the existing
Protection-off control's fixed-clip peak of exactly 1. Production safeguards
were not changed.

## Restricted folded-product probes

Density stage measurements call the **current** `mapCrushSaturation`,
`saturateSample` and `controlledClipSample` functions. They do not recycle old
drive coefficients or historical alias tables. Three laws are exercised:
Density/Crush/Drive = 0/0/0, 0.5/0.65/0 and 1/1/+24 dB. The separate Protection
stage does not depend on these laws; its repeated rows are controls, not
independent replications.

For each of the six rates, targets near 7 and 11 kHz are moved to an odd bin of
an 8192-sample coherent record. Source peaks are 0.01, 0.1, 0.5 and 1.1. A Hann
FFT measures central-bin power. Fold bins are the distinct predicted aliases
of odd orders 3…63 whose unaliased frequency exceeds Nyquist; the fundamental
is excluded. Odd coherent bins avoid collisions among these selected natural
and folded products. Both channels are identical, so the shared routine's
explicitly mid-channel spectrum is appropriate here. Fundamental amplitude
uses the Hann coherent-gain correction. A clipped waveform's fundamental
amplitude can exceed its sample peak; that is not an analyzer failure.

The stage report isolates saturation, saturation followed by the crush clip,
and the final Protection transfer. It **does not** include the compressor's
detector, makeup, dynamics or actual stage-input distribution. Its selected
fold-power ratio is not total broadband alias energy, and it is not a blanket
measurement of the whole plugin on music.

The full Harmonic graph probe uses 7004 Hz at 48 kHz, +6 dB Presence/Air with
both frequency controls at 7 kHz, source levels −36/−6 dBFS and both macro/gain
gesture states. Natural H3 at 21012 Hz is reported separately from predicted
folds of orders 5/7/9/11/13/15. It has no high-rate subtraction reference and
does not characterize automation sidebands, the entire frequency axis, or
every alias component. A worse selected folded-product ratio must not be
concealed by a stronger desired harmonic.

## Executed observations and decision limits

- On the generated mixed-music fixture, Density 0/50/100% produced maximum
  reduction 0/9.098/22.966 dB, but output crest factor 17.787/13.542/18.385 dB.
  Thus this fixture does not support a monotonic “more GR means denser final
  output” interpretation. It does not establish a listener preference.
- At +6 dB Presence, 2.5 kHz, −36 dBFS peak and Harmonic=1, the +12/−12 gesture
  changed H3 from −66.247 to −45.032 dBc and fundamental gain from 5.964 to
  5.592 dB. At −6 dBFS input it instead changed H3 from −33.215 to −37.221 dBc
  and fundamental gain from 3.681 to 2.751 dB. More internal drive is not a
  universal increase in this individual harmonic, nor is unchanged contour
  gain guaranteed under the nonlinear setting.
- At the selected 7.004 kHz strong Harmonic probe, the gesture changed folded
  product power from −29.939 to −28.767 dBc. This is a measured risk to assess,
  not evidence that the gesture is musically unacceptable or preferable.
- Harmonic all +12 dB at −6 dBFS input already exceeded unity output at
  Input/Output 0/0. At their extremes the final ±16 bound was reached. Counts
  in `over_unity_channel_samples` and `at_16_channel_samples` refer to output
  samples, not instrumented internal guard activations. Floating-point
  headroom is not a safe playback level. Raw signals require external safe
  attenuation in the listening pack.

Short checks include coherent amplitude/phase/orthogonality, exact flat-Harmonic
null, linked-gain equivalence of flat/cut/Harmonic-zero controls within 1e-6
absolute RMS, a +6 dB linear bell oracle, active-branch and selected-gesture
positive controls, exact Density dry unity, and Density's inactive hero macro
when Crush is zero. These establish measurement behavior and known signal
contracts, not artistic success. Listening examples must be matched externally
after rendering, with common comparison headroom attenuation; never modify
Density Output to fake a level match through its nonlinear Protection stage.

A deliberately broken temporary copy removed the gesture's −12 dB Output
compensation while retaining +12 dB Input. Its check executable exited 1 with
`Harmonic linked-gain linear negative control`; the unchanged diagnostic
executable passed. This fault injection verifies that this check can reject
an incorrectly constructed comparison, rather than merely exercising code.

## Independent relative-loudness gate check

`research/power/meter_checks.hpp` adds `power::meterChecks()`, independent of
the colour diagnostics. Its oracle uses long-double direct-form-I filtering
and direct sums of each overlapping window, rather than the meter's
transposed filter state and prefix subtraction. It applies the relative gate
in linear energy (one tenth of the absolute-gated mean). Both use the same
published 48 kHz coefficients; this is not independent coefficient validation.

Each 12-second dual-mono 1 kHz fixture has four seconds at 0.1 peak followed
by eight seconds at the stated quieter peak. The 0.4-second windows advance
by 0.1 second and include the level transition and filter transient.

| Quiet peak | Absolute-eligible windows | Relative-retained windows | Oracle/current LUFS | Without relative gate LUFS |
|---|---:|---:|---:|---:|
| 0.001 | 117 | 40 | −20.159260823 | −24.819644950 |
| 0.04 | 117 | 117 | −23.594335354 | −23.594335354 |

The strict release build and clean check passed with agreement inside the
1e-5 LU tolerance. A scratch copy changing the final gate comparison to the
absolute −70 LUFS threshold failed with exit 1 and
`relative-gate independent oracle mismatch`. No shared meter source changed.
These tests verify relative rejection and retention on two separated cases;
they do not certify an external reference meter, true-peak behavior, arbitrary
sample rates, or every signal near a gating boundary. They are not benchmarks
or subjective listening evaluations.
