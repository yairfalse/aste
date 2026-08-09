# Architecture

## Product boundary

Each product owns its processor graph, parameters, state schema, UI, fixtures,
and documentation. Products never link to each other. Shared code is extracted
only after a second real consumer appears or when correctness requires one
implementation.

The framework-independent targets remain small; each opt-in plugin adds one
thin JUCE boundary:

```text
density_dsp  <- density_tests  <- density_lab
      ^
      +------ Density JUCE adapter/UI <- density_plugin_tests

harmonic_dsp <- harmonic_tests <- harmonic_lab
      ^
      +------ Harmonic JUCE adapter/UI <- harmonic_plugin_tests

sequence_dsp <- sequence_tests
      ^
      +------ Sequence JUCE instrument/UI <- sequence_plugin_tests

loop_dsp     <- loop_tests <- loop_lab
      ^
      +------ Loop JUCE effect/UI <- loop_plugin_tests

impulse_dsp  <- impulse_tests <- impulse_lab
      ^
      +------ Impulse JUCE instrument/UI <- impulse_plugin_tests

all VST3 bundles <- vst3_smoke_host (dynamic ABI load only)
```

The JUCE VST3 targets adapt host buffers, automation, state, and UI to their
product DSP; the DSP libraries contain no JUCE headers. No universal plugin
engine or speculative shared library tree exists.

The standalone smoke host uses JUCE's headless VST3 loader and links neither
product adapter nor any product DSP library. It reaches products only through
their built VST3 ABI.

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

ADR 0002 fixes the first external build to this single 1x, zero-latency graph.
The measured 73/33 oversampling path remains lab-only: it adds a 44-sample dry
delay and aligned crush path but fails the default whole-instance CPU budget.
No quality parameter or dynamic latency transition exists in the product.

Stereo link interpolates each detector toward the greater channel detector.
At 0% the envelopes are independent; at 100% both channels receive the same
detector signal. The parameter is smoothed over 10 ms without topology changes.

## Ownership

- Host/UI thread: parameters and serialized state.
- Audio thread: processor, detector/filter history, smoothers, and meter write.
- UI meter handoff: three `std::atomic<float>` snapshots, guarded by a compile-time
  lock-free assertion on supported targets; the UI applies display decay at 30 Hz.
- Lab filesystem output: command-line thread, after each process call.

## Harmonic H-01 graph

```text
input -> Foundation -> Body -> Presence -> Air -> output
           linear cuts / bounded nonlinear-participating boosts
```

Each serial band owns matching linear and bounded state-variable sections. Cuts
take the linear path; positive gain continuously adds the difference between
the colored and linear band sections. The global Harmonic macro coordinates
bounded drive and contribution without changing topology. Input/output gain,
macro values, coefficients, and boost participation are smoothed. The graph is
minimum-phase, has no lookahead or oversampling, and reports zero latency.

Harmonic owns its graph, parameters, schema, UI, presets, and regression
fixtures. It does not link to Density. The only extracted runtime utility is
strict decimal parsing, which now has two identical product consumers.

## Sequence S-01 graph

```text
MIDI + host PPQ -> 16-step note/gate/accent/slide memory
                 -> dual oscillators + sub -> driven mixer
                 -> state low-pass <-> ladder-informed low-pass
                 -> ADSR/VCA -> bounded output
```

Sequence is a no-input monophonic instrument. Host PPQ selects steps directly;
MIDI transposes a running program and plays the voice when sequencing is
inactive. Fixed arrays, oscillators, envelopes, filters, and MIDI scratch
storage belong to the audio thread. The UI reads only lock-free meter and step
snapshots. The product-local graph is zero-latency.

## Loop L-01 graph

```text
audio + MIDI capture -> circular memory -> start/splice
                     -> varispeed/reverse -> dual pitch heads
                     -> wow/flutter/drift -> degradation/amplifier
live input + memory -> mix -> output
```

Loop is a mono/stereo effect with a preallocated 30-second stereo buffer. The
audio thread owns memory writes, playback positions, modulation, and meters.
Host BPM selects synced duration without changing the stable beat parameter;
free duration has its own seconds parameter. MIDI capture changes are processed
at event offsets. The graph reports zero latency. Schema 1 stores controls but
not captured audio; ADR 0009 records that explicit prototype boundary.

## Impulse I-01 graph

```text
host PPQ + seed -> four polymetric schedulers ----+
MIDI C/C#/D/D# -> direct excitation -------------+-> object voices
                                                     -> bounded loading
                                                     -> stereo output
```

Impulse is a no-input instrument. Four fixed voices and pure hash decisions
belong to the audio thread; PPQ derives absolute steps and ratchet ticks without
block accumulation. Stored seed, track, and absolute position fully determine
probability and mutation. The UI reads only lock-free output and step snapshots.
The product-local graph reports zero latency; ADR 0010 owns the boundary.

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

## Next product boundary

All five product source targets now exist. Sequence, Loop, and Impulse consume
host timing locally with different contracts, so transport extraction remains
deferred until identical behavior is proven. Loop and Impulse amplifier stages
also remain product-local because their measured jobs differ.
