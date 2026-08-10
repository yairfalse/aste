# Field F-01 music-machine test

Record the DAW version, Mac model, architecture, sample rate, block size, and
bundle commit. Test Cubase 14, Ableton Live 13/beta, and one additional VST3
host where available.

1. Insert Field on mono and stereo tracks; verify audio and MIDI note excitation.
2. Press FOREVER while audio is sounding, wait 30 seconds, then release it.
3. Automate every control abruptly and slowly while the field is occupied.
4. Save/reopen with FOREVER both off and on; confirm controls recall and note
   that live audio memory intentionally starts empty.
5. Compare realtime, offline, freeze, bypass, duplicate-instance, sample-rate,
   and variable-buffer behavior.
6. Check centered bass, hard-panned transients, decorrelated ambience,
   polarity-inverted stereo, voice, guitar, synth, and sparse acoustic input.
7. Check whether Grain creates useful movement without recurring clicks;
   whether Pitch remains luminous rather than metallic; and whether Distance
   changes depth strongly enough.
8. Observe first-time use: can a musician discover press-to-hold and
   press-to-release without documentation?
9. Verify exact entry, reset, focus order, Retina scaling, multiple windows,
   idle UI CPU, and host automation indication.

For every failure or useful setting, record input, controls, level-matched
observation, whether it repeats, and the smallest proposed change. “Huge,”
“vintage,” or “analog” without a repeatable observation is not actionable.
