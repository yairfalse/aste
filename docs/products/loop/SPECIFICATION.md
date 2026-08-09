# Loop L-01 specification

Loop is a playable memory effect, not a generic delay with cosmetic damage.
Audio is captured into a bounded circular memory and read by mechanisms whose
imperfections arise from transport, splice, pitch heads, degradation, and
amplifier loading.

```text
live input -----+----------------------------------------------+
                |                                              |
                +-> capture/overdub -> circular memory         |
                                      -> start/splice           |
                                      -> varispeed/reverse      |
                                      -> dual-head pitch        |
                                      -> wow/flutter/drift      |
                                      -> degradation/amplifier -+-> mix -> output
```

The processor accepts mono or stereo audio and MIDI. Capture can be latched by
the parameter or held by a MIDI note; MIDI transitions are applied at their
sample offsets. Host sync uses current BPM and a dedicated beat length. Free
operation uses a dedicated duration in seconds. Speed and pitch are independent
concepts and controls.

The internal prototype is zero latency, has a 30-second maximum memory, and
stores control state only. Captured audio does not yet survive instance reload;
this is a known release blocker, not silent behavior.

The panel is one scalable surface with oxidized-teal function accents, a large
position/capture display, boundary meters, visible transport switches, exact
numeric fields, five presets, and an explicit memory-clear action.
