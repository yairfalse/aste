# Resonator R-01 — first playable VST3

0.6.0 development build. A modal-led product fork of the comparative lab.
Waveguide research and historical tests remain independent; this first plugin
exposes one coherent instrument rather than a laboratory architecture switch.

## Play in Cubase

Add an **Instrument Track**, select **Resonator R-01**, and play MIDI or press
EXCITE. Default MIDI mode plays a chord from each root note: Minor 7, Suspended,
Minor 9 or Open. Individual-tone mode accepts four held/sustained notes within
two octaves, MIDI 24–100; excess/out-of-range notes are rejected and counted in
the footer. Velocity controls excitation energy; CC64 sustain is supported.

EXCITE energizes the selected root/structure without requiring MIDI. Changing
Root or Structure does not invent an excitation: press RETUNE to move existing
energy, or EXCITE to add a burst. The exciter supplies energy; the twelve tuned
complex modes create the chord. White, approximate pink, shaped noise, impulse
and granular-noise bursts are available, with no factory audio samples.

Shared minor structure, Short impacts and Moving memory are complete starting
sounds. Short impacts compensates its single-impulse energy and uses +6 dB
Master. Other scenes start at unity Master; all have -36..+18 dB adjustment after
bounded instrument output. Shift-drag, exact text entry and control resets work.

## Harmonic memory

Reset discards the previous bank's energy before a strike. Shared retains and
retunes it. Partial damps by the chosen retained-energy fraction. Overlap uses
two alternating banks, preserving one old chord until that bank is reused.
Energy Transfer rotates modal coordinates between tones; Stretch and
Inharmonic change relationships. PERSIST stops ordinary decay while held; key
release/pedal-up releases persistence. A manual EXCITE with no held keys can
persist until Clear or disabling PERSIST. KEY RELEASE DAMPING optionally shortens
normal tails on note release; it is off by default.

CLEAR ENERGY empties both banks and the burst generator. The display reports
actual per-bank frequencies and state energy, not decorative activity or an
FFT. It is not a calibrated acoustic loudness meter. Host output is shown
separately at the Master control. Follow the MIDI mode note limit when playing
new chords with the pedal down. MIDI all-notes/sound-off clears the instrument.

## Parameters and state

Schema 1, product/root `resonator-r01`; bundle `invalid.aste.resonator-r01`,
plugin code `Res1`, Instrument/Synth. Nineteen parameters cover excitation,
root/structure/input mode, decay/damping, coupling/stretch/inharmonicity,
memory/retention/persistence/mutation/seed, key-release behavior, Master and
Bypass. The root/structure fields are manual settings; the actual mode display
shows the structure last played by MIDI or an explicit action.

Malformed, duplicate-known, non-finite, cross-product and unknown-schema states
are rejected atomically. Missing controls use defaults. Controls recall, but
sounding energy, held notes, pedal state and the current burst start empty after
reload. Restore is applied before new MIDI so the first note is not discarded.
The experimental parameter IDs are now session identities; future changes need
explicit migration rather than renaming the controls.

## Engine and limits

The framework-independent C++20 fork is in apps/resonator. The modal bank
requires no heap sample storage while processing. All DSP state, held keys,
events and UI meter snapshots are bounded. Callback MIDI is processed at sample
offsets; long SysEx is skipped without allocation. A radial energy guard and
bounded output prevent unbounded driven persistence. They are nonlinear;
no alias-free, hardware-model or true-peak-limiter claim is made.

No external-audio bus, MPE, aftertouch, arbitrary chord library, per-mode editor,
chorus or automatic dub-delay chain was added. Host start/stop is not an internal
sequencer; MIDI supplies the event timing. Musical playability and identity still
need hands-on use. Native editor minimum is 1000×740, default 1240×840. The
universal arm64/x86_64 build is VST3 only, separate from the six-product .pkg.
