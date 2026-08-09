# Loop L-01 music-machine test

Record host version, Mac model, CPU architecture, sample rate, buffer size, and
result for every run.

1. Load Loop as a mono and stereo effect in Cubase and Ableton.
2. Capture with the button and with a held MIDI note; check event timing.
3. Compare 1, 4, 7, and 16 beat loops across stop, start, locate, tempo change,
   project loop, offline bounce, and real-time bounce.
4. Confirm free length ignores host tempo.
5. Verify Speed changes duration and pitch together while Pitch approximately
   preserves duration.
6. Exercise reverse, start, splice, feedback, overdub, and memory clear during
   playback; document discontinuities.
7. Save/reopen and confirm all controls recall. Confirm and record the known
   prototype behavior that captured audio does not recall.
8. Automate every parameter rapidly and inspect finite output, CPU, and clicks.
9. Listen on full mixes, ambient fields, percussion, bass, and sparse acoustic
   capture at level-matched output.
10. Check scaling, Retina rendering, keyboard focus, exact entry, and window
    reopen behavior.
