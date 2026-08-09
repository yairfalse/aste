# Sequence S-01 music-machine test

Record the host, host version, macOS version, architecture, sample rate, block
size, and result for each run.

1. Install `Sequence S-01.vst3` and load it as a VST3 instrument in Cubase and
   Ableton Live.
2. Confirm MIDI plays the voice while transport is stopped.
3. Start transport and confirm the 16-step display stays locked after locate,
   loop, stop/start, tempo automation, offline bounce, and reopen.
4. Edit every step note and toggle gate, accent, and slide; verify exact session
   recall.
5. Automate Pressure, Cutoff, Resonance, Filter Form, envelope times, Output,
   Root, and Division at slow and abrupt rates.
6. Compare the state and ladder endpoints at matched output level on bass,
   lead, and percussive patterns.
7. Test mono and stereo instrument tracks at all supported sample rates and odd
   buffer sizes where the host permits them.
8. Resize to minimum and maximum, type exact values, navigate by keyboard, and
   inspect Retina rendering.

Do not call the voice finished until tuning, bass weight, high-resonance
stability, slide semantics, sequence entry speed, and mix placement have been
tested in real projects.
