# Aste: make the existing instruments playable

This implements the concrete timing, recall and control findings from the
[second audit](../re-audit/README.md), on `improve/musical-control`, without a PR
or changes to main. The target music computer is confirmed Apple silicon Mac.
No Cubase session or human preference assessment has been performed here.

## What changed

| Instrument | Completed change | Musical purpose and boundary |
|---|---|---|
| Density | New Pulse / parallel start; accurate Drive help; visible response upgrade | Retain dry attack while shaping body, rather than start fully wet. Grip remains available. This does not establish a preferred compression aesthetic. |
| Harmonic | New Color / open start; PUSH/PULL reports partial increments and disables at limits | Four pushes available from the new start instead of one, with Harmonic below maximum. Existing nonlinear EQ topology and all previous snapshots remain unchanged. |
| Sequence | G/A/S editing inside a focused step; logical keyboard groups; accurate Accent help; in-place response upgrade | Perform pattern edits through the same host parameter attachments as clicks. Does not add velocity-to-filter modulation or replace the shared envelope. |
| Loop | First take writes full input before tape processing; explicit 100% Overdub warning; in-place response upgrade preserving tapes | Make the first recording predictable and permit current motion with existing recorded material. Overdub/feedback remain capture laws, not playback feedback. |
| Impulse | Sample-anchored retriggers; active-object view; all objects one action away; explicit clock/response upgrade | Click/sub work opens with two rows rather than eight. Ratchet 3 no longer depends on callback partition in current mode. Eight objects and all existing pattern controls remain. |
| Field | Explicit response upgrade and visible held-audio recall explanation | Make independent hold coloration and its persistence limits understandable. Held audio is not serialized; printing audio preserves that performance. |

All six editors identify matching factory parameter snapshots after reopen;
unmatched recalled controls say CUSTOM. A modified open-editor preset retains
its name with an asterisk. Recognition does not load a preset or alter state.
Parameter identity does not imply a response mode: that is shown separately.

Impulse's focused view reveals externally programmed rows. Mutation, MIDI-only
mode and empty programs expose every object. Selected/keyboard-focused rows
remain available. Switching views makes no host parameter changes and triggers
no sound. SHOW ALL OBJECTS is always the route to add another object to a sparse
pattern. This remains Impulse, not the proposed Non-Machine.

## Compatibility and implementation

No stable parameter IDs, ranges, reset defaults or old preset values changed.
New starting points are appended, not substituted under existing names. Old
partial-state migrations retain their historical defaults. Density's first
five presets still select the original detector; Grip and Pulse use current
response. Loading saved state supersedes constructor starting values.

Impulse schema 5 separately saves `clock` and `behavior`. Schemas 1–4 retain
their old clock, including schema-4 direct sound. Current timing anchors a
continuous sample count to host PPQ, re-anchors for transport/tempo changes,
and rejects non-finite or excessively large PPQ before integer conversion.
Opening an editor never upgrades a recalled session. USE CURRENT RESPONSE is
an explicit saved edit, with non-parameter host-change notification; it changes
neither controls nor patterns nor Loop tapes. Older binaries do not understand
newer schemas: retain original project copies when testing.

UI snapshots and response controls run on the message thread. Loop's response
bit is atomic; its off-thread lifecycle lock protects current/pending engine
selection and is released before notifying the host. No new callback allocation,
lock, wait, logging or filesystem operation is introduced. DSP stays C++20 and
framework-independent; no dependency, public modulation contract or new plugin
identity was added.

## Measurements, not listening claims

Fresh audio source commit: `600659b75115e5ea66859e55a7e6f8553ada6465`.
Later changes in this study are test/documentation/evidence only unless stated
in the delivery metadata. The pack has **532 WAVs**: 356 historical six-product
comparisons, 168 new starting-point/gesture comparisons, and eight current
click/sub examples. The historical Impulse section intentionally remains Space;
use `click-sub/` for the current sparse scenes. Earlier packs are untouched.

Seven synthetic effect sources cover percussion, bass, oscillator, noise,
chord, generated music and sparse input. They are not supplied productions.
Matched effect files use the existing iterative K-weighted gated meter, a common
target no greater than −23 LUFS and sample peaks no greater than 0.8. The meter
has an independent oracle, not external certification or true-peak guarantees.
Silent Loop 100% output reports unknown loudness, never zero LUFS.

Density's bass fixture changes near-peak occupancy from **6.578% to 0.303%**
and peak/RMS crest from **3.29 to 6.77 dB** between Grip and Pulse. Percussion
crest changes from **8.40 to 11.36 dB**. This is less flattening, not proof of
better groove or impact. At 48 kHz, −18 dBFS, the 7004 Hz diagnostic's folded
fifth at 12980 Hz is **−48.18 dBc** at Pulse, versus the earlier Grip measurement
**−31.92 dBc**. Harmonic content also reduces: 996 Hz H3 is **−25.38 dBc** versus
Grip's **−16.89 dBc**. These are selected steady-state products, not total alias
power. The 1× topology, its aliasing tradeoff and zero reported latency remain.

Fresh callback-only measurements use the existing extreme-profile fixtures,
not the new starting presets: 144 trials, two sample rates, two maximum block
sizes and both responses. At 48 kHz / maximum 128 frames, current median one-core
realtime percentages are Density **0.270%**, Harmonic **0.632%**, Sequence
**0.258%**, Loop **0.247%**, Impulse **1.159%**, Field **0.313%**. No other Aste
build/test/render was running. These are local throughput observations, not
deadline maxima, Cubase overhead or a full-capacity Loop memory benchmark.

## Verification and actual interfaces

The initial complete native run passes **68/68**, including all six standalone
VST3 host checks and the renewed dependency-security check. The 144 original
legacy stereo comparisons are bit-identical to pinned `8a603dc`; the independent
loudness-meter oracle passes. These finite fixtures do not prove equality for
every session or substitute for Cubase validation.
All six core suites also pass AddressSanitizer/UndefinedBehaviorSanitizer.
The independent Rust inspector validated every one of the 532 float32 WAVs.

New regressions were observed failing before their implementations: ratchet-3
partitioning, reopened preset names, response actions, Harmonic limit feedback,
Impulse focus, Sequence shortcuts and the appended starting points. The clock
matrix covers six rates × four ratchets × three microtimings through seek,
stop/restart and tempo changes. Response tests compare entire state XML; Loop
also upgrades real recorded tape and checks concurrent engine adoption.

Actual minimum-size component images are in `screenshots/`. They were inspected
for readability and overlap, not claimed as Cubase screenshots or native
VoiceOver approval. Density is a metered Drive 12 / Density 80 fixture, not its
new start. Field's display is still a tap/activity proxy, not total memory energy.

[Density](screenshots/density-minimum.png) ·
[Harmonic](screenshots/harmonic-minimum.png) ·
[Sequence](screenshots/sequence-minimum.png) ·
[Loop](screenshots/loop-minimum.png) ·
[Impulse](screenshots/impulse-minimum.png) ·
[Field](screenshots/field-minimum.png)

See [reproduction commands](../../../research/playability/README.md), the
[short musical tasks](LISTENING.md), and `evidence/` for recorded outputs.
Sykli receipts and exact-commit verification are retained locally in
`build-playability-evidence/`; the tracked contract declares its actual scope.

## Finding disposition and remaining acceptance

- **Fixed:** triplet clock, lost preset identity, invisible response choices,
  missing in-place upgrades, silent gain-gesture limits, misleading help,
  empty-capture attenuation at the new start, eight-row-only workflow,
  stale UI/state documentation and expired dependency review. Security renewal
  uses six fresh public responses and preserves the expiry check; see the
  [review](../../security/reviews/2026-11-15/README.md).
- **Addressed as a controlled alternative:** Density's over-flattened start
  and Harmonic's lack of control travel. Both old starts remain selectable for
  an honest preference comparison. No louder-is-better criterion was used.
- **Retained architecture, not silently declared repaired:** Impulse's one
  voice per object, trigger-latched timbre, hard-edged pulse sources and
  per-sample transient coefficients; Sequence's shared envelope and velocity
  law; Loop's pitch/reverse and generational laws; Field's lossy network and
  non-static zero-Motion behavior. Changing these would alter instrument
  identity and saved sound. They need isolated, versioned listening experiments,
  not a speculative six-engine rewrite under a usability fix.
- **Still unproven:** subjective identity and reward, native keyboard/VoiceOver
  usability, Cubase automation/recall on the actual music machine, and preference
  on supplied tracks. No claim that all six instruments now pass musical acceptance.

The next decision is not more controls. In Cubase, complete the six short tasks
and record what became easier and what still feels weak. If attack/body contrast,
click/sub placement or held-energy control remains unconvincing, the next work
should target that mechanism with matched audio, while retaining these fixes.
