# Objects and nested memory — 0.7.0 working revision

The musician's feedback rejects the previous result: Non-Machine offers too
little sound shaping and too much of the same sequence; Field reads as mild
reverb rather than a loop within a loop. This revision builds controllable
composition actions. It does not claim to reproduce an artist or establish
musical success from tests. No human audition by the implementation agent is
claimed; the WAVs below were actually rendered for the musician's next listen.

## Chosen alternatives

For N-01, more random preset variations would leave sound design unchanged.
Instead, each of the three objects can select Pressure, Resonant metal, Noise
object or Broken fragment, with Original retained for old sessions. These are
restrained synthesized sources, not eight separate drum machines. The resonant
material uses frequency modulation and inharmonic partial relationships rather
than an imported sample or an emulated physical circuit. Structure changes
partial spacing/modulation or noise/fragment structure; Strike changes excitation
and attack; Pitch Fall controls initial pitch excursion; Roughness adds broken
noise edges. Color, pitch and decay are still per-event locks. Material changes
apply at the next hit, not halfway through an existing captured fragment.

For timing, the implemented event lanes map horizontal position to local cycle
percentage. Click space to add, drag to move, double-click/Delete to remove;
numbered selectors and exact inspector fields remain available. Repeat marks
show the local repeat train. EMPTY PATTERN preserves sounds/cycles and disables
all appointments, allowing composition from silence. NEW RHYTHM remains optional.
This revision does not add an artist-style generator, a drum library or random
activity to fill structural voids. The default Objects / build example keeps
three independent cycles and an explicit final void.

For Field, stronger modulation alone cannot create controllable phrase capture.
The chosen addition is an immutable stereo outer phrase with a selected inner
fragment. An overdubbing feedback looper was rejected for this smallest iteration:
its memory ownership and generational gain would obscure the explicit return.
CAPTURE PHRASE swaps recording and playback banks in constant time. REPEAT INSIDE
selects the currently reached outer position, with its own repeated cursor. The
outer advances behind it. Release of the inner layer rejoins the reached outer
position; release of capture returns to the live spatial field. Reverse, speed
and length are explicit controls. FOREVER continues to govern spatial retention,
not capture ownership. MEMORY / FIELD controls the captured layer's contribution.

## Memory and time model

Field records raw stereo input plus its MIDI excitation, preserving recognizable
phrase detail. Two stereo vectors per bank are allocated in prepare, never in
processing. The captured bank is immutable; the other bank continues recording.
Inner selection reads the captured bank without copying it or capturing its own
output. Outer shortening is bounded by the originally captured duration; expansion
requires recapture. Times are sample-derived milliseconds, not host beat lengths.
The outer phase always advances at Speed, even beneath inner playback. Reverse
changes read direction while that phase continues; it does not change ownership.

Two-ms action tapers and a 0.5-ms boundary taper reduce cuts without an unqualified
click-free claim. Very short fragments, reverse switches and aggressive resizing
remain intentionally audible. No allocations, locks, waits, logs or file calls
were added to either callback. Field's additional banks cost 64 bytes per Hz of
sample rate plus a few counters; total fixed + capture storage stays below
16 MiB through 192 kHz, 32 MiB at the internal 384 kHz clamp. Old states leave the
capture path off; their audio path stays selected. Live captures are not serialized.
State restore clears history instead of reusing unrelated captured audio.

N-01 appends 15 host parameters after the old 409 slots; Master and Bypass keep
their indices. Field appends seven after the old 12 slots. Old source formulas /
zero memory mix are the missing-ID defaults. Existing numerical parameter IDs,
research fixtures and independent original laboratories were retained. No stable
finished-instrument quality claim is implied by these development identities.

## Reproduction and completed checks

Build the root `object_memory_lab` target, then:

```
build-plugin-universal/object_memory_lab
build-plugin-universal/object_memory_lab build-evidence/object-memory-070-final
build-plugin-universal/non_machine_plugin_tests --render /absolute/new/pattern-folder
```

Exports refuse an existing laboratory folder. The listening export contains ten
2-second stereo sound examples (five materials including Original, two settings)
and `field-capture-inner-release.wav` (8 seconds). Sound examples are peak-normalized
to 0.7, not loudness matched; Original deliberately ignores the new material macros.
The Field example feeds one generated phrase, captures at 2 s, repeats 90 ms at
3 s, reverses at 4 s, shortens to 37 ms at 5 s, releases the inner at 6 s, and
returns live at 7 s. Four separate adapter renders compare N-01 starting scenes.
These are synthetic diagnostics, not recordings supplied by the musician.

Implemented automated evidence:

- Exact object output equality at 127/511-frame partitions over six supported
  rates, all four new materials; finite bounded samples.
- Normalized sound-shape differences for changed material macros at 48 kHz:
  Pressure 1.4163, Resonant metal 1.4098, Noise object 0.2790, Broken fragment
  1.3098. These show waveform change beyond gain, not musical preference.
- Nested-memory tests at 8/44.1/48/88.2/96/176.4/192 kHz: exact repeated inner
  audio after transitions, preserved stereo relation and deterministic outer
  return, release to live, reset invalidation, and bounded speed/reverse/length
  changes. The reference outer runs separately with no inner operation.
- Real JUCE tests verify new parameter ordering, old-state migration, direct
  keyboard timeline movement/deletion and empty-pattern silence. Capture is
  exercised while auditing callback allocation and OS calls. Field ignores
  SysEx directly rather than allocating a temporary MIDI message.
- Native minimum-size editors were rendered and visually inspected. Actual
  VST3 ABI host checks pass for both bundles; shared UI-layout changes also pass
  Resonator's adapter tests. Nine targeted CTest checks pass.
- AddressSanitizer/UndefinedBehaviorSanitizer cover the independent object /
  capture laboratory and the Field core suite.

## Try composing

In N-01, select Objects / build, then EMPTY PATTERN. Select an object, choose a
material and use HIT while shaping it. Set its cycle before placing a few events.
Drag one onset off the even spacing; give just one event a short accelerating
repeat, another a condition, and leave space. The inspector edits selected hits;
the material row edits the whole object. Capture holds must be released to hear
new underlying sound edits.

In Field, select Memory inside memory and feed at least 1.5 s of material.
Press CAPTURE PHRASE, then REPEAT INSIDE at a deliberate point. Change Inner
Fragment, Speed or Reverse, then release Repeat Inside to hear the reached
outer phrase. Release Capture Phrase before taking a new phrase. FOREVER is a
separate spatial-memory operation. Print audio to retain a performance.

## Known limits / next listening decision

This is a minimal working iteration: three monophonic objects, eight events each,
one outgoing articulation tail, no imported N-01 sound files, no pattern undo or
gesture recorder, no Field overdub/feedback-generation recording, no waveform
browser or beat-quantized capture. Source synthesis remains 1x; nonlinear/FM
materials are not alias-free. Direct pointer and keyboard edits have component
checks, not Cubase/VoiceOver usability approval. Final live Cubase audition and
preference remain with the musician. The relevant question is whether these
sound and time operations support deliberate sparse, unstable compositions with
predictable returns, rather than simply adding more activity.
