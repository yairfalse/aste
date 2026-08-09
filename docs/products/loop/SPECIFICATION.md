# Loop L-01 specification

Loop is a generational tape-memory instrument, not a generic delay with
cosmetic damage. Audio is caught on one of three bounded tape decks. RELOOP
prints the currently transformed playback through a record/reproduce stage to
the next deck, so pitch, reversal, splicing, loading, and loss become part of a
new generation and can be printed again.

```text
live input -> capture/overdub -> Tape A
Tape A/B/C -> start/splice -> varispeed/reverse -> dual-head pitch
           -> wow/flutter/drift -> reproduce loading -> mix -> output
                                      |
                                      +-> RELOOP record amp
                                          -> speed-dependent bandwidth/loss
                                          -> next tape deck
```

The processor accepts mono or stereo audio and MIDI. C holds capture, D prints
RELOOP, B selects the previous retained generation, and C-sharp selects the
next; events are applied at their sample offsets. Host sync uses current BPM
and a dedicated beat length. Free operation uses seconds. Varispeed and the
duration-preserving pitch mechanism remain separate concepts.

Three preallocated 16-second stereo decks retain the current generation and up
to two earlier prints. The generation number can continue indefinitely because
the oldest retained deck is reused. No memory is allocated or freed while
processing. Tape speed selects the calibration of each new print: slower speeds
lose more bandwidth, while Record and Loss determine compression and
generational damage. RELOOP is ignored until usable audio exists and while a
print is already in progress.

The internal prototype is zero latency and stores control state only. Captured
audio and retained generations do not yet survive instance reload; this remains
a release blocker. The scalable panel shows all three tape paths, the active
deck, generation number, retained history, print progress, a large RELOOP
action, previous/next navigation, boundary meters, and explicit memory clear.
