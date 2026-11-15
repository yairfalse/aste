# Start here: six short comparisons

These are test candidates, not listening-approved sounds. Use the `audio`
folders: they contain matched versions. `raw` contains diagnostic levels.
Listen to the same source at the same monitor level, not to different sources
as a preference test. The filenames reveal the version; this is not blind.

1. **Density:** `density/audio/percussion_before_start.wav`, then
   `percussion_after_start.wav`. Is there useful pressure and articulation, or
   merely flattening? Compare `percussion_same_controls_legacy.wav` with the
   latter to isolate the detector change. Repeat with `bass` and `noise`.
2. **Impulse:** `impulse/audio/phrase_before_start.wav`, then
   `phrase_after_start.wav`. Does the sparse start invite deliberate placement?
   The first comparison changes the pattern, not only the sound. Use the plugin
   to select silently, audition explicitly and open RHYTHM DETAILS when needed.
3. **Sequence:** `sequence/audio/phrase_same_controls_legacy.wav` versus
   `phrase_after_start.wav`. Both use the same gapped notes. Do attacks make the
   phrase more playable? The overlapping-MIDI file checks retained legato.
4. **Loop:** `loop/audio/generated_music_same_controls_legacy.wav` versus
   `generated_music_after_start.wav`. Both use Wow 100% to expose the clock
   change. This deliberately exaggerated demonstration is not a new default.
5. **Harmonic:** `harmonic/audio/chord_before_start.wav` versus
   `chord_after_start.wav`. The graph is unchanged; the starting point exposes
   its existing coloration. In the plugin, boost a band, then PUSH/PULL to
   explore operating level. Those buttons do not perform loudness matching.
6. **Field:** `field/audio/chord_same_controls_legacy.wav` versus
   `chord_after_start.wav`, then the corresponding `sparse` pair. Hold enters at
   two seconds. Does independent color give better control over persistence?

For each comparison record: source; preferred version or no preference; useful
musical task; what became easier; what became worse. A lack of preference is a
valid result. Stronger distortion, higher RMS or more visible activity alone
does not pass musical acceptance.

The six VST3 bundles are native arm64 internal test builds with existing plugin
IDs. They have not been installed, tested in Cubase, notarized or released.
Existing states select legacy DSP; older binaries cannot read the new schemas.
Keep original project copies when switching binaries. The dependency-security
review is expired and remains a release block, not a silently waived check.
