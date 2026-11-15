# R-01 laboratory contract — provisional

This is a research API, not a stable public parameter or session contract.
The [types](../../../research/r01/core.hpp) and [implementation](../../../research/r01/core.cpp)
are independent C++20. No JUCE, product parameter, state format or plugin is added.

## Ownership and scheduling

`prepare` selects architecture and memory policy and allocates two banks of
waveguide history. Call it outside processing. The processing owner holds all
state; `snapshot` is a same-thread observation, **not** a concurrent UI API.
Control snapshots are accepted at callback boundaries. Events are sorted sample
offsets within the callback, maximum 64; malformed batches are rejected as a
whole. Events at the same sample are coalesced before its input is processed.
Panic overrides every other event in that group. Invalid controls retain the
last valid snapshot; invalid events and sanitized input are counted.

Strike applies memory policy and tuning but **does not generate excitation**.
The caller supplies a mono energy stream. Retune changes tuning without applying
the strike policy. Release switches to a decay of at most 300 ms; per-mode
decay multipliers still apply. Panic clears both banks and held notes. A later
Strike reactivates a bank. A zero-frame call is inert. A missing left output
does not process; a missing input supplies silence; right output is optional.
Inputs must refer to valid arrays of `frames` samples. Left/right outputs must
not alias each other; the mono input may alias left because each input sample
is read before output is written.

MIDI note-on/off is an offline event model, not a host adapter. Up to four held
notes, MIDI 24–100 and maximum span 24 semitones, are supported. Excess notes
are rejected, not silently stolen. Same-sample replacement should send old
note-offs before new note-ons. The last note-off releases persistence; an
accepted same-sample new note-on cancels that release. Velocity, pedal, MPE,
tempo/PPQ, seek, transport restoration and host automation ordering are absent.
The MIDI pack fixture supplies externally scheduled bursts at note-on groups;
it does not include the cents offsets of the chord-snapshot fixtures.

## Physical control values

| Field | Accepted range / meaning |
|---|---|
| Sample rate | 44,100–192,000 Hz; six standard rates tested |
| Chord | root MIDI 24–100; 1–4 tones; intervals −24…24 semitones; cents −100…100; root+interval 0…127 |
| `t60` | 0.03–120 seconds, nominal amplitude decay; modal partial decay further scaled below |
| `damping` | 0–0.8; modal divisor `1 + damping * partialIndex²`; waveguide one-pole feedback coefficient |
| `modeDecay[12]` | 0.1–4 modal decay multiplier; waveguide requires all 1 |
| `couplingHz` | 0–20, rotation rate between adjacent tones' corresponding modal quadratures; waveguide requires 0 |
| `stretch` | 0–0.3; partial ratio `(k+1)^(1+stretch)`; waveguide requires 0 |
| `inharmonic` | 0–1; extra ratio `sqrt(1 + inharmonic * 0.12 * k²)`; waveguide requires 0 |
| `dispersion` | 0–0.8 first-order waveguide allpass coefficient; no effect on modal engine |
| `partialRetain` | 0–1 retained modal energy fraction on Partial strikes; ignored by other policies |
| `mutationCents` | 0–50 deterministic per-tone signed mutation; seed + strike ordinal determine it |
| `persist` | modal unit-radius autonomous decay; waveguide feedback gain 0.999999, still subject to interpolation/filter loss |

Control snapshots are abrupt in this experiment. There is no smoothing guarantee.
A new tuning rotates existing modal coordinates instead of resetting phase;
delay retuning moves the read location and can make a discontinuity. Maximum
adjacent-sample difference is measured, not certified inaudible. Transition
smoothing/crossfades should be compared against these raw changes before choosing
a musical policy. Modal frequencies at or above 0.4 Fs are muted. Waveguide
chord frequencies below 17 Hz or at/above 0.4 Fs are rejected. These are explicit
prototype bounds, not an unrestricted microtuning system.

## Memory and safety

- Reset: clear the active bank, then retune.
- Shared: retain coordinates/history and retune that bank; the old chord does
  not keep independent tuning.
- Partial: scale modal coordinates by `sqrt(partialRetain)` before retuning.
  Waveguide Partial is rejected, not approximated.
- Overlap: alternate two banks, preserving the other bank's tuning. Replacing
  an active target increments `stolen`, including an initially active empty
  bank. Only the current bank receives new input. This counter is an allocation
  event count, not a claim that an audible tail was truncated.

Coupling rotates both quadratures orthogonally. This bounds unforced modal
energy even with changing rotation angles; it does not bound accumulated input
when radius is one. Each modal bank is radially projected to norm 4 if needed.
Waveguide writes clamp to ±4. Input above ±1 clamps, non-finite input becomes
zero, and all interventions are counted. Guards are nonlinear and may alias.
The stereo sum is scaled by 0.1; two-bank bounds are 0.8 modal and 1.6 waveguide,
so a future product needs an explicitly evaluated output policy. Float WAV
export refuses values above ±1; matched copies cap peak at 0.8.

Waveguide delay arrays are allocated once for a 16 Hz lower capacity bound.
Reset invalidates history in constant time rather than clearing the entire
buffer. A compensated rolling sum avoids stale energy-meter residue after
decay. Modal state energy is sum of squared quadratures; waveguide energy is sum
of squared stored delay samples, **excluding filter states**. Neither is joules,
perceived loudness, or a cross-architecture common scale. Each bank publishes its
own tuning and energy; inactive/unused mode slots are zero, not invented modes.

## Reproduction and deferred boundary

The exciter supplies white, approximate Voss pink, low-pass-shaped noise,
impulses or eight windowed noise fragments. The pack's external-input test uses
a generated chirp; there is no file importer. Pack bursts have unit squared-
sample energy at 48 kHz. Fixed seeds and event/control history reproduce output
on the same build; cross-compiler bit identity is not promised.

No state serializer exists. Future saved controls must declare whether loading
starts with empty history or restores a versioned energy snapshot. Do not adopt
stable IDs until the musical and interaction gates pass. A future JUCE boundary
must own MIDI conversion, sample-accurate automation, bounded queues, coherent
meter publication and host state—not place JUCE or locks in this core.

There is no extra block buffering or oversampling latency. Waveguide excitation
emerges after its resonant propagation delay; that is sound generation, not a
measured host latency declaration. No host/plugin latency value is implemented.
