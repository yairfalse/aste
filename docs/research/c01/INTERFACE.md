# C-01 native interface study

Implemented as a JUCE editor using Aste's shared Look, precision controls,
Master Output and parameter attachments. Native component snapshot is generated
by `c01_plugin_tests`; it is layout evidence, not a DAW/Retina review.

Default 1100×650, minimum 900×560, maximum 1600×1000. Matte near-black,
off-white type and a restrained desaturated green functional accent. No tabs,
oscillator/filter panels, hidden modulation pages, decorative waveform or fake
circuit diagram. Header: identity, starting-sound menu, Master. Visible cue:
play MIDI chords, CC1 phase depth, aftertouch pressure.

Five large controls lead: Pressure, Contamination, Instability, Memory, Motion.
Five secondary controls remain visible: Colour, Attack, Decay, Sustain, Release.
Precise text entry, Shift-drag fine changes and double-click reset follow the
shared slider contract. Parameter names include units and values remain visible.
Tooltips describe each gesture's actual DSP function. Keyboard ordering follows
the five gestures, five articulation controls, Master, presets, Clear, Panic;
VoiceOver traversal and effective focus ordering still require native review.

A 20 Hz text strip reports actual stored-state magnitude, averaged oscillator
return activity, and final output peak in dBFS. These are state readouts, not
spectral plots or meters calibrated in energy units. The memory readout includes
the shared reservoir even when its participation is reduced; it does not claim
an audible reverb tail. A future graphic should use these same observations
with visible scales after the readings prove useful. No audio-thread data
structures are read directly by the UI.

CLEAR MEMORY discharges operating history while notes continue; ALL SOUND OFF
is distinct and clears voices/controllers/history. A held chord can refill the
reservoir immediately. Presets reset the circuit and send normal parameter
gestures. Reopening identifies matching snapshots without changing audio;
unmatched states read CUSTOM; editing retains the last named match with `*`.
Starting points: First circuit, Muted pressure, Slow contamination, Predictable.
These need listening review before factory status.

Evaluation tasks: find a muted stab; keep a suspended chord legible while
raising Contamination; hear sixteen repetitions change with Memory; reduce
Memory and restore predictability; play one chord every three seconds; save,
close/reopen and automate in Cubase. Verify all controls at both size limits,
text entry, keyboard navigation, focus visibility, screen reader names, contrast,
HiDPI scaling, no preset-selection surprises and unambiguous Clear/Panic behavior.
None of these user observations is claimed completed by the snapshot test.
