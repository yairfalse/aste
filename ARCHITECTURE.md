# Architecture

## Product boundary

Each product owns its processor graph, parameters, state schema, UI, fixtures,
and documentation. Products never link to each other. Shared code is extracted
only after a second real consumer appears or when correctness requires one
implementation.

The framework-independent targets remain small; the opt-in plugin adds one thin
JUCE boundary:

```text
density_dsp  <- density_tests
      ^  ^
      |  +--- density_lab
      |
      +------ JUCE VST3 adapter/UI <- density_plugin_tests
```

The JUCE VST3 target adapts host buffers, automation, state, and UI to
`density_dsp`; the DSP library contains no JUCE headers. No universal plugin
engine or speculative shared library tree exists.

## Density D-01 graph

```text
input --+------------------------------ dry -------------------+
        |                                                       |
        + drive -> detector HPF -> continuous stereo link       |
                -> per-channel peak envelopes                   |
                -> hard gain computers -> saturation -> clip ---+-> blend
                                                                 -> output
                                                                 -> protection
```

Both branches currently have zero algorithmic latency, so phase alignment is
exact without a delay line and reported latency is zero. Oversampling or
lookahead may be added only with measured benefit; either requires one crush
latency value, an equal dry delay, and matching host latency reporting.

Stereo link interpolates each detector toward the greater channel detector.
At 0% the envelopes are independent; at 100% both channels receive the same
detector signal. The parameter is smoothed over 10 ms without topology changes.

## Ownership

- Host/UI thread: parameters and serialized state.
- Audio thread: processor, detector/filter history, smoothers, and meter write.
- UI meter handoff: three `std::atomic<float>` snapshots, guarded by a compile-time
  lock-free assertion on supported targets; the UI applies display decay at 30 Hz.
- Lab filesystem output: command-line thread, after each process call.

## Product philosophy

The family exposes audible phenomena rather than circuit trivia. Interfaces
are compact, matte, technical, and restrained; motion reports signal state.
Historical circuits are evidence about headroom, control laws, recovery, and
loading—not templates for branding, panels, or fidelity claims.

Density is an intentionally assertive parallel dynamics instrument. Subtle
settings increase continuity while retaining the unprocessed transient path;
strong settings make the crushed branch audible and physical. It is not a
hardware clone and its current output stage is protection, not a true-peak
mastering limiter.
