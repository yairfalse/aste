# Performance budgets

Budgets are release gates measured on the oldest supported Apple Silicon and
Intel Macs, Release builds, stereo, worst-case automation:

- Default quality: under 1.0% of one performance core at 48 kHz / 128 samples.
- Render quality: under 4.0% at 96 kHz / 128 samples.
- Processing-state memory: under 256 KiB per instance, excluding UI/framework.
- Audio-thread allocation, locks, filesystem and logging: exactly zero.
- Cycle-1 latency: 0 samples. Future default-quality target: at most 64 samples.
- UI closed: no timer or repaint CPU.

Every benchmark records commit, compiler, architecture, sample rate, block
size, quality, and automation mode.

## Density D-01 baseline

On 2026-08-06, the Release `density_lab --benchmark` median was 0.2083% of one
core across five serial runs (range 0.2082–0.2090%). The machine was an Apple
M4 Pro MacBook Pro, arm64, using Apple clang 21.0.0. Each run rendered 120
seconds of stereo at 48 kHz / 128 samples while alternating Density between
20/90% and Stereo link between 0/100% every block. This passes the provisional
1.0% budget on that machine; it is not yet evidence for older Apple Silicon or
Intel systems.

On 2026-08-07, the same machine rendered the complete 980×540 editor in
software 120 times per run. Across five Release runs, median paint time was
0.500 ms idle and 0.455 ms with active meters. Repainting the entire panel at
30 Hz would therefore occupy about 1.50% of one core; production invalidates
only the smaller meter panel. This is a conservative local baseline, not a
cross-machine release threshold or a measurement of native host compositing.

On 2026-08-07, the first streaming 4x crush-path prototype measured about 3.60%
of one M4 Pro core for stereo at 48 kHz / 127 samples across repeated five-run
medians. Its state is 2,336 bytes per channel. It passes the memory budget but
fails the provisional 1.0% default-quality CPU budget, so it is not integrated
into the production processor.

Cycle 22 measured the same scalar 4x topology at four filter lengths in isolated
Release runs on the M4 Pro at 48 kHz / 127 samples:

| Taps per phase | Stereo CPU range |
|---:|---:|
| 16 | 1.054–1.061% |
| 32 | 1.956–1.973% |
| 48 | 2.801–2.827% |
| 64 | 3.692–3.705% |

The shortest candidate narrowly misses the CPU budget and has unacceptable
transition-band aliasing, so reducing tap count alone is not a viable path.

Cycle 23 replaced the direct 4x FIR with two sparse 2x half-band stages. Three
isolated five-run medians at 48 kHz / 127 samples measured:

| First/second-stage taps | Latency | Stereo CPU range |
|---:|---:|---:|
| 33/33 | 24 samples | 0.753–0.757% |
| 65/33 | 40 samples | 0.901–0.910% |
| 97/33 | 56 samples | 1.152–1.164% |
| 113/33 | 64 samples | 1.293–1.303% |
| 129/33 | 72 samples | 1.399–1.402% |
| 97/65 | 64 samples | 1.531–1.537% |

The topology roughly halves CPU at comparable alias suppression, but no tested
configuration simultaneously meets the 1.0% CPU target and the stronger
transition-band suppression demonstrated by the longer filters.

Cycle 28 measured β5 at the 65–113 tap crossover using three isolated Release
runs on the same M4 Pro:

| First/second-stage taps | Stereo CPU range |
|---:|---:|
| 65/33 | 0.923–0.929% |
| 73/33 | 0.958–0.961% |
| 81/33 | 0.986–0.998% |
| 89/33 | 1.111–1.121% |
| 97/33 | 1.170–1.177% |
| 113/33 | 1.288–1.294% |

The 81/33 topology is the longest local result below 1%, but its margin is too
small to establish the release budget on older supported machines. The 73/33
topology retains about four percent local margin. Neither result is a release
gate measurement.

Cycle 29 retains 73/33 as the sole integration candidate because 81/33's
six-rate quality improvement is small relative to its lost CPU margin. The
0.958–0.961% figure is the oversampler alone, not the entire plugin; adding the
current roughly 0.208% processor baseline would exceed the 1% default-quality
budget. The candidate therefore requires an end-to-end measurement and is not
approved as the default path.

Cycle 30 measured the complete lab-only 73/33 processor path over three
isolated five-run medians:

| Path | Stereo CPU range | Reported latency |
|---|---:|---:|
| Production 1x | 0.231–0.231% | 0 samples |
| 73/33 integration prototype | 1.042–1.043% | 44 samples |

The prototype consistently misses the 1% default-quality budget by roughly
0.042 percentage points. Its incremental cost is 0.810–0.812 percentage
points, lower than adding the two isolated figures because it replaces the
production nonlinear calls. It is not approved as the default path.

ADR 0002 therefore fixes the first external build to the measured 1x path.
The Render-quality budget remains a future target, not an implemented mode.

Cycle 36 added one second 3 ms smoother stage to Output. Five Release runs on
the same M4 Pro measured 0.231669–0.246683% of one core, with a 0.233320%
median at 48 kHz / 128 samples under the established automation benchmark.
This remains far below the local 1% default budget; it does not replace the
required oldest-supported-hardware measurement.

Cycle 38 made the same 3+3 ms cascade production behavior for Drive. Five
Release runs measure 0.236635–0.241537% of one core, with a 0.239551% median
under the same 48 kHz / 128-sample automation benchmark. The additional stage
therefore preserves wide local margin beneath the 1% default budget.

Cycle 40 added one 5 ms logarithmic Attack smoother and derives the detector
coefficient per sample. Five Release runs measure 0.262370–0.274137% of one
M4 Pro core, with a 0.265705% median under the same benchmark. This remains
below the local 1% default budget; older supported hardware is still required.

Cycle 42 made a 3+3 ms cascade production behavior for Blend. Five Release
runs measure 0.262276–0.286155% of one M4 Pro core, with a 0.266210% median
under the same benchmark. The additional stage is locally negligible and the
oldest supported Apple Silicon and Intel measurements remain outstanding.

## Harmonic H-01 baseline

Harmonic adopts the same provisional default-quality budget: less than 1.0% of
one performance core at 48 kHz / 128 samples, stereo, with worst-case block-rate
automation. Its processing state must remain below 256 KiB and its callback
allocation, lock, filesystem, logging, and reported latency counts must be zero.

On 2026-08-09, Release `harmonic_lab --product-benchmark` rendered 30 seconds
five times on an Apple M4 Pro MacBook Pro. Every block changed all four gains,
all four frequencies, and the Harmonic macro. The median was 0.682702% of one
core, with a 0.671615–0.758637% range at 48 kHz / 128 samples. This passes the
local provisional budget. It is not evidence for the oldest supported Apple
Silicon machine, native Intel hardware, a loaded DAW, or UI-open performance.

## Sequence S-01 baseline

Sequence adopts a provisional budget below 1.0% of one performance core at
48 kHz / 127 samples for the monophonic worst-case voice. Processing allocation,
locks, filesystem access, logging, and reported latency must remain zero.

On 2026-08-09, Release `sequence_lab --benchmark` rendered 30 seconds with
Pressure at 100%, resonance at 85%, both filter structures active, stereo
output, and the host-synchronised pattern. It measured 0.282010% of one M4 Pro
performance core. This passes the local budget but is not evidence for native
Intel, the oldest supported Apple Silicon, a loaded DAW, or the open editor.

## Loop L-01 baseline

Loop adopts a provisional budget below 1.0% of one performance core at 48 kHz /
127 samples with stereo memory, pitch heads, transport modulation, degradation,
amplifier stages, and generational printing active. Three 16-second stereo decks
are prepared before processing; callback allocations, locks, file access,
writes, and latency must remain zero. The existing single-memory measurement is
retained only as the pre-generational comparison baseline.

On 2026-08-09, Release `loop_lab --benchmark` rendered 30 seconds with pitch at
+7 semitones and wow, flutter, drift, degradation, and amplifier at maximum. It
measured a 0.369393% median of one M4 Pro performance core across five runs
(0.366017–0.378431%). This passes the local budget
but is not native Intel, oldest-supported Apple Silicon, loaded-DAW, or open-UI
evidence.

On 2026-08-10, the generational benchmark continuously printed successive
two-second generations through the three-deck record/loss path while pitch,
motion, Record, and Loss remained at worst case. Five runs measured
0.520554–0.560591%, with a 0.526622% median of one M4 Pro performance core. The
new topology remains below its 1% local budget.

## Impulse I-01 baseline

Impulse adopts a provisional budget below 1.0% of one performance core at 48
kHz / 127 samples with all four voices and densest scheduler settings. Callback
allocation, locks, filesystem access, logging, and latency must remain zero.

On 2026-08-09, five Release `impulse_lab --benchmark` runs used 32 active pulses
on every track, 4× ratchets, 180 BPM, and maximum Energy, Variation, Mutation,
and Drive. The median was 0.352056% of one M4 Pro performance core, with a
0.339692–0.367345% range. This passes the local budget but is not native Intel,
oldest-supported Apple Silicon, loaded-DAW, or open-UI evidence.
