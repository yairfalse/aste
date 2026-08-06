# Density performance budgets

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

## Current baseline

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
