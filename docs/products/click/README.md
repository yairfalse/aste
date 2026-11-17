# Click K-01

Four sound objects for impacts, sampled fragments, repetition, pressure and silence.
A real VST3 instrument in the main Aste build; there is no separate research app.

## Start in Cubase

Create an Instrument Track and choose **Click K-01**. The default **Host sequence**
mode plays the visible pattern when Cubase runs. For direct notes, choose
**MIDI only** in the always-visible Playback selector. The two input modes are
exclusive; incoming note messages are ignored in Host sequence mode.

Default MIDI notes: object 1 **36**, object 2 **38**, object 3 **42**, object 4 **46**.
Change assignments in Sound / Advanced. PLAY beside an object auditions it in
either mode. Velocity controls impact level; channel note-off does not truncate
these percussion one-shots. CC120/123 clear sounding voices and captured time.

## Sequence

Click an empty cell to add an event; click an existing event to select it.
Double-click, Alt-click, right-click or Delete removes it. Drag the body to move
it; drag its right edge to make a roll. The count is printed on the cell. Arrow
keys select events, Space toggles, Shift-left/right changes the roll count.

The selected event's velocity, roll count/span/acceleration, probability,
condition, sound-object selection and sound locks are immediately below the grid.
**MORE EVENT** exposes microtiming, sample offset, morph, end velocity, roll pitch,
filter mutation and irregular spacing. Negative microtiming moves an event before
its nominal step; timing is not tied to host buffer boundaries. Acceleration is
positive for tightening rolls and negative for relaxing rolls. End velocity and
pitch shape the whole gesture. Conditions include alternating/periodic cycles,
first/after-first and Fill.

Each row has its own length and subdivision. Dimmed cells remain editable for a
future longer cycle. M and S mute/solo; PLAY auditions. Click the object name to
edit its sound. Advanced Sound includes voice fragment repeat, its length, local
roll count and optional reset with the master phrase. Otherwise local cycles
continue independently through master wraps. Master Steps sets the phrase ruler
and capture reference; it does not force every independent cycle to that length.

## Sound

Choose **Synthesis**, **Sample** or **Layer**. Foundation covers 18 Hz–16 kHz;
Tune adds ±48 semitones. Excitation bends the initial oscillator frequency;
Body changes the interacting partials, Decay controls the event's fall, Noise
sets the tonal/noisy relation and Character drives an asymmetric nonlinear path.
The filter morphs through low-pass, interacting resonant bands, a notch region
and high-pass. Filter Envelope moves its centre over the event. This is an
original system, not an implementation of E-mu's proprietary filter algorithms.

LOAD SAMPLE / RELINK or drop an audio file on the waveform. Loading runs on a
worker thread. WAV/AIFF/FLAC/Ogg and platform-supported readers are available;
unsupported files report an error. The limit is **1,048,576 frames per object**
(21.85 seconds at 48 kHz); longer files are refused, not silently truncated.
Samples retain their original rate, with supported file rates 8–192 kHz.

Top waveform handles edit Start/End; bottom handles edit Loop Start/End. All four
also have precise fields under Advanced. Loop Motion moves the loop window over
the amplitude envelope. Reverse, loop, nearest/linear/cubic interpolation, optional
digital quantization and its blend remain separate choices. Nearly coincident
bounds are constrained to a safe minimum internally. Body softens sample trim edges; set it to zero for an unsoftened microscopic
transient. Boundary text fields show sample frames and also accept entries such
as `12.5 ms`. Synthesis-only or sample-only controls dim in the
other source mode. Sample tuning is a playback-speed change, not time stretch.

Replace offers eight synthesis/layer starting sounds. **SAVE SOUND / OPEN SOUND**
in that menu exchange `.k01sound` files for just the selected sound and sample
reference, leaving its pattern and MIDI assignment intact. COPY TO NEXT OBJECT
copies the sound/sample reference while preserving the destination pattern.

The header factory menu supplies six complete starting patterns: First impacts,
Microscopic, Sub pressure, Broken lattice, Fragment material and Sample ready.
Factory sample fragments are original, procedurally generated audio. SAVE/OPEN
in the header exchange complete `.k01` presets including four stored scenes.
Samples are referenced by absolute path, not embedded. A missing file leaves a
visible MISSING/UNREADABLE message; LOAD relinks it. Move production samples into
a durable project folder before saving the Cubase session.

## Perform

The six gesture buttons also stay visible in Sequence and Sound. Hold to perform,
release to return; Shift-click latches, and a subsequent click releases. Space
latches a focused gesture. All gesture values are ordinary host automation.

- **MASTER REPEAT** freezes a stereo fragment of the complete rhythmic output,
  including internal compression and coloration. Drag the master ruler to choose
  its region before capturing. A captured region is immutable until released.
- **INNER REPEAT** captures a shorter span of the currently performed output,
  including an outer repeat. Inner Loop sets its length in quarter notes.
- **VOID** fades the whole output into silence.
- **CRUSH** increases nonlinear destruction, including an already captured repeat.
- **SHIFT** displaces the underlying event phase by Shift Steps. A captured audio
  fragment stays unchanged; its displaced underlying rhythm is heard on release.
- **HOLD / RELEASE** suspends and resumes musical sequence position.

**Continue** lets the underlying sequence advance while repeats sound; release
returns to its current position. **Hold** suspends sequence time during an active
repeat and resumes from that position. Audio playback/envelopes continue. Outer
fragment playback advances while an inner fragment is heard, so releasing Inner
returns to the outer fragment's current position. Releasing Outer while Inner is
held leaves Inner active. These are two explicit nesting levels, not an unbounded
stack. Reverse Fragment reverses the captured audio, not the MIDI event order.

Buffers hold up to eight seconds. If you capture before a whole region has been
recorded, the available material is used and the actual captured seconds are
shown. An empty capture arms until sufficient material is recorded. After release,
the buffer starts collecting new material. Transport stop/seek, sample-rate
preparation, panic and session restore clear live voices/repeats. Captured audio
is temporary performance state; parameters, patterns, scenes and sample paths
are recalled. A recalled latched repeat arms again rather than restoring audio.

**Pressure** controls threshold, ratio and compensated gain in the internal
parallel compressor; **Recovery** sets pumping time; **Dirt** controls asymmetric
saturation. Parallel balances the clean and compressed paths. Master Output is
an independent final trim; a final ±0.98 ceiling prevents runaway output even
at +18 dB trim. The meters show actual final peak and gain reduction.

Four scene buttons recall complete snapshots. Enable STORE SCENE then choose a
slot to replace it. Factory slots start on the first four starting patterns.
Scene recalls notify the host for their changed scalar parameters; the direct
performance gates are the compact automation workflow. CC20–26 provide Master
Repeat, Inner Repeat, Void, Crush, Shift, Hold and Fill respectively.

## Build, validation and limits

Use the normal repository build:

```sh
cmake -S . -B build-plugin-universal -DASTE_BUILD_VST3=ON
cmake --build build-plugin-universal --target ClickK01_VST3 click_engine_tests click_plugin_tests --parallel 3
ctest --test-dir build-plugin-universal -R '^click_' --output-on-failure
python3 tools/install_development_vst3.py --build-root build-plugin-universal --only click-k01
```

See [CONTRACT.md](CONTRACT.md) for parameter/state/resource contracts and
[VALIDATION.md](VALIDATION.md) for actual validation and installation results.
Native Cubase insertion, performance and musical quality must be judged locally;
headless transport tests and pluginval are not a claim of Cubase testing.
