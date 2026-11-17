# C-01 development contract — 0.1

The framework-independent C++20 engine is in `research/c01`. The JUCE boundary
and native performance editor are in `apps/contamination`. The isolated build
uses the existing pinned JUCE checkout; the root product build, installer and
existing instruments are untouched. This is a listening candidate, not a release.

## Host parameters

Order is permanent for this development identity. JUCE version-hint 1; ASCII IDs.

| Index | ID | Physical range | Default | Role |
|---|---|---|---|---|
| 0 | pressure | 0–100% | 35 | Nonlinear oscillator-return drive and phase depth |
| 1 | contamination | 0–100% | 15 | Squared gain into correlated-noise return injection |
| 2 | instability | 0–100% | 10 | Correlated frequency/ratio movement |
| 3 | memory | 0–100% | 50 | Local/shared operating-state participation and relaxation |
| 4 | motion | 0–100% | 25 | Envelope-driven phase depth and state-dependent spectral opening |
| 5 | colour | 80–8000 Hz | 1100 | Centre of the two-integrator return |
| 6 | attack | 0.5–4000 ms | 3 | Linear amplitude rise |
| 7 | decay | 10–8000 ms | 250 | Exponential approach to Sustain |
| 8 | sustain | 0–100% | 55 | Held amplitude |
| 9 | release | 5–12000 ms | 200 | Approximately −60 dB release time |
| 10 | master | −36–18 dB | 0 | Shared clean final trim |
| 11 | bypass | off/on | off | Silence and request reset |

Colour/Attack/Decay/Release use shared skewed physical mappings (centres
1100 Hz/100 ms/500 ms/600 ms); other mappings are linear. Engine controls use
15 ms exponential smoothing, with convergence snapping below 1e−9 in physical
units. Master uses the existing 20 ms gain ramp. No macro changes MIDI timing.
Pressure maps one operating condition into drive `1+5p` and phase depth
`.35+1.7p`; it is not output gain. Post-drive normalization is sqrt(drive), not
a perceptual loudness correction. The listening pack separately RMS-matches it.

## Events, voices and deterministic state

12 fixed voices, full MIDI 0–127 pitch, stereo or mono output, no audio input.
Frequency is capped at 12% of the internal rate for pathological high-note/bend
combinations. Sample rates 22050–192000 Hz; invalid preparation fails closed.
Generation runs at the smallest power-of-two factor reaching 176.4 kHz
internally: 8× at 22.05 kHz, 4× at 44.1/48 kHz, 2× at 88.2/96 kHz, 1× at
176.4/192 kHz. FIR length is 32×factor+1 (129 taps at 48 kHz), with a Blackman
window and cutoff .44/factor in internal-rate cycles. Reported latency is always
16 host samples. No runtime quality switch. The lab supports explicit 1/2/4/8×.

MIDI is dispatched at its sample offset before generation; audible onset also
includes the reported FIR latency. All channels feed one instrument; this is
not MPE. Velocity controls excitation/amplitude; CC64 holds released voices;
CC1 adds up to one radian of phase depth; channel pressure adds up to 20% drive
pressure; pitch wheel is ±2 semitones. Polyphonic key pressure is not supported.
CC120/123 perform a full panic, including memory and pedals. Note-on velocity
zero is note-off. Duplicate note-off releases all matching held instances.

Allocation chooses the first idle slot, otherwise the quietest release, otherwise
the oldest held voice. A stolen voice contributes its last sample through a
5 ms linear fade. New oscillators and fast integrators restart; no unbounded
tail list or allocation occurs. Memory remains associated with a physical voice
slot, not a MIDI pitch. That allocation dependence is explicit and deterministic.

The product uses Hybrid memory. Voice-local rectified excitation and signed
bias survive release/steal; global energy includes rectified summed low/high
band differences, so voicing changes loading. The reservoir never emits sound
by itself. It changes the depth and operating point of later synthesis. This
is an invented relaxing state, not a physical hysteresis model. Instability
retains the seeded PRNG/drift across successive occupied slots. At zero
Instability, zero Memory and fixed controls, oscillator/noise initial conditions
repeat exactly after release. No wall clock, host block size, or undefined data
seeds the engine. Determinism is within a compiler/platform math implementation;
cross-platform bit identity is not promised.

| Boundary/action | Policy |
|---|---|
| Note-off | Amplitude release; operating state relaxes continuously |
| Voice steal | Keep scaled local memory/bias, restart fast voice, 5 ms tail |
| Lower Memory | Smoothly remove participation and shorten relaxation; no hard discontinuity |
| CLEAR MEMORY | Discharge local/shared reservoir, bias and drift; held notes continue and can refill it |
| ALL SOUND OFF / CC120/123 | Clear voices, FIR, state, drift, controllers and counters |
| Observed transport playing→stopped | Full panic; notes played while already stopped are allowed |
| Seek while playing | No reset; explicitly replay from preparation/panic for repeatable offline comparisons |
| Prepare / sample-rate change | Full reset |
| Session restore | Scalar controls restored; full engine reset on next render or preparation |
| Bypass / releaseResources | Silence; full reset before resumed rendering |

## Persistence and automation

Shared `experimental::Instrument` schema 1 ValueTree binary/XML convention;
root/type/product `contamination-c01`, schema `1`, shared `material` metadata.
Material metadata is retained for family compatibility and currently does not
select different C-01 DSP. No live voices, reservoir, phase, random generator,
FIR or controller state is serialized. A restored session starts from a defined
empty circuit; live history is not a portable preset. Missing IDs restore reset
defaults; unknown IDs are ignored; duplicate known IDs, malformed/nonfinite
values, wrong product/root/schema reject the transaction. Finite values clamp.
Presets use ordinary host-notifying gestures and request a reset. There are four
starting snapshots; these are not listening-approved factory content.

The inherited adapter reads atomic parameters once per block. Host automation
therefore follows the family's block-target plus smoothing contract, not sample
accurate within-block parameter interpolation. MIDI remains sample accurate.
Actual Cubase automation/session tests remain required; JUCE tests cannot prove
Cubase behavior. No installation or existing sessions were changed.

## Real-time and bounds

All voices, history and FIR storage are fixed arrays. Rendering has no dynamic
containers, IO, locks, logging or filesystem work. GUI reads lock-free scalar
atomics at 20 Hz; no engine state is accessed from the GUI. Pre-Master output
is bounded by tanh plus a DC servo (absolute theoretical bound 2); deliberate
Master gain can exceed 0 dBFS. Feedback is inside bounded sin/tanh functions;
fast filter coefficients stay in (0,1), so their updates are convex combinations
of bounded states. Reservoir energy is capped at 2 and signed global bias at
±1. Local excitation state has a finite geometric-series bound. Invalid control
values sanitize before smoothing. This is bounded nonlinear behavior, not a
claim of chaos or an analogue filter stability proof.
