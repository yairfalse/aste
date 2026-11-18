# Loop L-01: playable generational memory

This production update adds a waveform-led Safe Takes workflow without
replacing the original overdub mode or changing its parameter identities.

## First session

1. Insert Loop on an audio track. First take starts in Safe Takes.
2. Choose Beats with Host Sync on, or Free Length with it off. Capture starts
   immediately; Host Sync supplies duration, not a next-bar start.
3. Press CAPTURE NEW TAKE. Watch the recording percentage and actual waveform.
   Recording stops after one duration. FINISH CAPTURE can end it early.
4. Adjust Speed, Reverse, Pitch, Loss and Record. Drag a waveform region or
   type Start/End. These are temporary playback transformations.
5. Press RELOOP. The wet tape is printed into a new PCM generation. The source
   continues playing so its processing is not suddenly applied to the print
   for a second time. Click the new GEN tile to hear the printed take with
   neutral transforms, then transform it again.
6. Use Undo/Redo or another generation to compare. History has three protected
   slots, not an unlimited archive. Remove an inactive take with its `x` to
   free a slot; Undo recovers it until subsequent recording changes the tapes.
7. Save the project. Retained audio travels in plugin state, with no external
   sample-file dependency.

## Performing

Select a waveform region, choose FREE or 1/4–1/32, then HOLD TO REPEAT. While
holding, shorten the region, reverse it, or change speed/pitch. Release returns
those controls and the repeat latch to their earlier values; the underlying
selected loop has continued advancing. LATCH permits hands-free repeating;
unlatching returns to normal playback but keeps deliberately edited controls.
RELOOP during either operation prints the performed sound.

The Hold button is also a two-dimensional performance surface: keep it pressed
and drag left to shorten the region, up to increase speed. Release restores
both. Use Latch when you want to reach the other controls with the mouse.

Dragging either boundary resizes the selection. Arrow keys move it; +/- resize
the end; Shift makes keyboard edits ten times finer. Knob value fields permit
exact entry. Controls remain accessible by keyboard. Hold also accepts Space
and releases on focus loss or editor destruction.

PLAY resumes paused tape, STOP freezes it without erasing, RELEASE fades it
without rewriting its PCM. Feedback sets release time. Monitor Input adds the
track input to playback; monitoring and output-level stages never enter a
new generation. Record and Loss are heard before committing them.

## Compatibility and deliberate bounds

- Original 22 parameter indices/ranges unchanged; five controls appended.
- Old sessions and the six earlier factory presets use the original graph.
  First take/new instances select Safe Takes explicitly.
- Three 16-second stereo decks, with the existing preallocated snapshot pool.
  About 88 MiB at 48 kHz or 352 MiB at 192 kHz for decks plus sample pages,
  excluding metadata, state serialization, undo and transient replacement
  engines. This update does not multiply that existing storage allocation.
- Tape playback is free-running. Beat lengths/repeat divisions use host tempo;
  PPQ seeks and host loop restarts do not rearrange or replace captured PCM.
- A print lasts one fragment at the speed when printing begins, at most
  16 seconds. Later speed changes affect recorded content, not its deadline.
- Independent pitch remains the existing dual-head method, not a transparent
  time-stretch engine. Saturation is not oversampled. This is not a claim of
  hardware tape emulation or subjectively improved musical quality.
- Undo is not saved as additional hidden generations. Save any take you want
  to keep before explicitly removing it. Momentary automation written by a
  host is host-owned; the plugin restores its live controls on gesture release.

## Reproducible checks and audio

Use the existing Release build with pinned JUCE, then:

```sh
cmake --build build-recall --target loop_tests loop_plugin_tests LoopL01_VST3
ctest --test-dir build-recall -R loop --output-on-failure
build-recall/loop_plugin_tests_artefacts/Release/loop_plugin_tests \
  --editor-artifacts "$PWD/build-recall/loop_editor_artifacts"
```

The last command exercises actual JUCE processor/editor operations and writes
seven stereo 48 kHz/24-bit WAVs, populated/minimum-size editor screenshots and
a three-generation state fixture. Audio is a deterministic synthesized chord,
not a copyrighted recording. Files 01–07 progress from the original capture
through slow/reverse erosion, printing, neutral second-generation playback,
pitched fragment repetition and neutral third-generation playback. They retain
actual output levels: **not loudness-matched preference comparisons**.

Checks cover protected one-shot capture, branch preservation, full-history
refusal, exact printed-versus-auditioned PCM, sample/block invariance, timeline
return, real waveform gestures, momentary parameter restoration, undo/redo,
inactive-take deletion without a head jump, state reload/migration, sample-rate
conversion, concurrent state work, extreme automation and callback allocation/
lock/I/O auditing. Existing legacy checks remain active.

The creator's Cubase is on another Apple-silicon Mac. Native plugin-host tests
and editor rendering are not Cubase validation. Human musical listening,
Cubase project reopen/automation and the other machine's audio-device behavior
remain local acceptance checks for the creator.
