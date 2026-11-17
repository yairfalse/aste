# Density / Field character revision, 0.6.2

User feedback: Density feels weak and boring; Field lacks movement and sounds dull
and muddy. These are revisions for another Cubase audition, not a claim of musical
acceptance. No human listening assessment was performed by the implementation agent.

Density adds an automatable Character macro after every previous host parameter.
It blends a DC-filtered asymmetric tanh branch, with first-order antiderivative
antialiasing, and shifts release from the previous envelope law toward a stronger
level-dependent recovery. Attack, Release, Crush, Blend and Drive remain separate.
The new-instance preset is **Bite / breathing**, Character 80 and Blend 85.
Character zero preserves previous processing and avoids computing the new shaper.
The antialiasing branch averages across adjacent inputs; it has a frequency-dependent
response, including a small phase shift. No block latency or oversampling was added.

Field adds Focus after Texture. It removes low-frequency injection and feedback
buildup with bounded first-order filters, reduces high-frequency absorption, blends
continuous grain-head cycling, and modulates the two pitch-return ratios in opposite
directions. Motion controls excursion; Grain/Pitch/Texture still set their layers.
The new-instance preset is **Moving glass**, Focus 75. Focus zero preserves the
previous graph, including its grain timing. The dry path is unaffected by Focus.

Both parameter reset defaults are zero; older saved states with no new ID restore
zero regardless of the receiving instance. Old preset values and indices remain;
new presets append. Current instances need the new preset or an explicit macro
change to hear the revision. Existing schema 1/2 validation and behavior flags remain.
No allocation, lock, wait, file access or logging was added to the callback.

## Reproduce

Build `character_lab` through the root CMake project, then run:

```
build-plugin-universal/character_lab
build-plugin-universal/character_lab build-evidence/character-062-listening
```

The export refuses an existing folder. It creates 30 stereo Float32 WAVs: five
synthetic sources (0 percussion, 2 oscillator, 4 chord, 5 generated music, 6 sparse),
two products, and three macro settings. Filename suffixes 0/1/2 mean previous
processing / moderate macro / new starting macro. All other controls are identical
within each group; effect output is wet for Field, 85% processed for Density.
These compare the DSP macro, not the old versus new factory preset as a whole.

The common target is at most -23 LUFS per group, iteratively K-weighted/gated using
the existing quality laboratory meter, with shared sample-peak headroom 0.8.
This is not a certified true-peak meter. Source audio is generated, not a recording
provided by the musician. The CSV records raw peak/RMS and matched loudness.
`cpu_realtime_percent` includes offline setup and fixture generation, potentially
concurrent builds; it is diagnostic throughput, not a callback budget benchmark.

## Completed evidence

- Exact constant-control output equality at 127/511 frame partitions at
  44.1/48/88.2/96/176.4/192 kHz, with extreme Drive or retained feedback.
- Finite bounded output for 60 seconds at 8 kHz and extreme controls.
- Density 997 Hz H2/fundamental rises from 3.81e-11 to 0.00561 with Character 80.
- On the 60 + 2000 Hz wet probe, Field's low/high amplitude ratio falls from
  2.494 to 1.245 with Focus 75. This is a spectral probe, not a mud preference test.
- Density's selected folded fifth for a 7001 Hz input changes from -30.61 to
  -39.12 dBc. The initial naive character branch measured -18.42 dBc and was
  replaced before installation. This one bin does not establish total alias energy
  or alias-free processing; the original 1x nonlinear stages remain.
- Core, JUCE state/real-time audit, native minimum editor and actual VST3 host
  checks cover both products. New state tests remove each new ID from a real
  serialized state and check zero migration and byte-stable new-control recall.
- AddressSanitizer/UndefinedBehaviorSanitizer run the independent laboratory.

Limitations: no new acoustic-room model, extra compressor modes, oversampling
quality menu, external audio listening session or Cubase automation signoff.
Field remains a lossy moving feedback instrument; Focus can shorten bass decay.
Character deliberately changes dynamics and distortion, so level matching remains
necessary. Numerical differences and passed tests do not prove the requested sound.
