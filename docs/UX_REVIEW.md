# Six-instrument usability cycle — 2026-10-01

## Problem and resulting behavior

Hands-on feedback found the interfaces unrewarding and the drum patterns too
wild. The changes prioritize predictable first actions and honest feedback,
without changing parameter IDs or rewriting the sound engines.

- Impulse opens with Steady Start: a shared 16-step kick/click/crack groove,
  no mutation or variation, and 100% hit probability. Track names audition the
  voices. Pattern feedback updates while stopped, inactive steps are dimmed,
  sound/rhythm controls occupy separate rows, and experimental starting points
  are labelled evolving or wild. Starting points reset the complete sound state.
  Bypass discards pending auditions and frozen voices; restarting audio also
  discards stale audition requests, preventing delayed surprise hits.
- Loop presents Capture Input / Stop Capture, explicit recording/playback cues,
  and visibly inactive timing controls. Impossible RELOOP/history actions are
  disabled. MIDI panic and note releases during bypass no longer latch capture.
  Bypass retains the visible tape history, and navigation buttons respect its
  oldest/newest retained generations.
  Schema 3 saves recorded audio and generations without blocking playback.
  Loading state before the host announces its sample rate preserves the original
  tape rate until preparation, avoiding a lossy detour through 48 kHz.
  The VST3 reports an infinite tail: 16 seconds is tape capacity, not a limit
  on how long a loop can play.
- Sequence groups oscillator, filter and envelope controls, shows note names,
  explains keyboard versus host-sequence behavior, and retains the restored
  division when its editor opens. Step switches are labelled and stacked so
  they remain usable at minimum size. Bypass clears stale held MIDI notes.
- Density explains the relationship between Drive, Density, Crush and Blend;
  its gain-reduction display now fits the minimum panel size.
- Harmonic explains that boosting a band engages its nonlinear contribution;
  flat and cut bands still stay clean.
- Field distinguishes an empty armed hold from held audio, exposes Clear Field,
  and explains that FOREVER also increases grain, pitch and motion.
- All six provide useful hover descriptions and retain the selected starting
  point's name in the open editor, with an asterisk after edits. Level displays
  use a dB scale; minimum-size previews cover the complete family. Custom step
  and hold controls show keyboard focus.

## Verification

- Native arm64 Release: 68/68 CTest checks passed, including all six direct
  adapters and all six dynamically loaded VST3 ABI tests.
- ASan/UBSan: 68/68 checks passed on the final full rerun. The stopped-grid
  test waits for the actual UI update with a bounded timeout, rather than
  assuming timer scheduling latency.
- Loop's concurrent capture/snapshot test passed ThreadSanitizer. Adapter
  coverage includes concurrent save/restore/retirement, malformed tape payloads,
  same-rate exact samples, sample-rate changes, and allocation-free callbacks.
  The deeper pass reproduced high-rate pre-prepare recall loss and stale drum
  auditions with failing tests before fixing both lifecycle paths.
- Each of the six bundles passed 537/537 Steinberg extensive-validator tests.
- Core static analysis, repository formatting, documentation-link checks and
  `git diff --check` passed. Minimum-size panel renders were visually inspected.

CTest used fast research mode with timing benchmarks skipped; the separate
Release benchmark measurements are recorded below.

The local SDK 27 build encountered deprecated JUCE Metal APIs under `-Werror`.
These builds use the installed macOS 15.2 SDK instead; warnings were not disabled
and the pinned JUCE dependency was not changed.

## Loop performance and recall boundaries

Five Release runs at 48 kHz / 127 frames, rendering 30 seconds of repeated
generational printing per run, measured a median 0.517628% of one local CPU
core without snapshots and 0.821238% with snapshots. These are local elapsed-time
measurements, not oldest-supported-hardware or loaded-DAW evidence.

Snapshot storage reserves roughly 88 MiB per 48 kHz instance and scales with
sample rate; restore temporarily needs another engine and decoded audio. The
state format saves the latest published tape snapshot, restarts playback from
the beginning, and excludes unfinished RELOOP destinations. Existing schema-1/2
Loop states restore their controls with empty tape memory. Existing Impulse
states keep their stored settings and historical missing-field defaults.
See [ADR 0016](adr/0016-playable-defaults-and-tape-recall.md).

## Next musical check

Try the native review bundles in a DAW: audition drums before starting transport,
build a groove from Steady Start, capture and reopen a Loop project, explore the
Sequence controls while stopped and playing, compare Density/Harmonic at matched
levels, and arm/fill/clear Field. Confirm whether these actions feel immediate
and understandable. No listening-quality, VoiceOver, universal-binary or DAW
compatibility claim is made by this cycle.

## 2026-11-15 — 0.5.0 music-machine follow-up

User feedback: Impulse hard to find in Cubase, family levels too low, master
level hard to locate, Field too mellow, Sequence sounds thin and lacks fine
control. Local inspection found Impulse present in Cubase's native cache,
available and loadable through the actual VST3 ABI; no blocklist entry was
found. Category is now Instrument/Drum. This does not establish visibility in
the user's current Cubase selector; that still requires an instrument-track
check with the Default collection.

All six now expose a final header Master Output (-36..+18 dB), independently
smoothed after sound shaping. Older states missing the added parameter restore
unity. Fresh Sequence starts at +3 dB and Impulse at +6 dB. Existing Output
parameters and automation indices are unchanged. Shared slider interaction adds
500-pixel normal / 5000-pixel Shift-drag sensitivity and exact typed values.

Sequence's new voice aligns the saw/pulse/sine fundamental polarity to avoid
a hollow midpoint. Its original mono DSP stays available. Independent filter
attack, decay and sustain are appended after Master; attack/decay zero follow
the amp. The editor displays oscillator detune in cents while keeping the
semitone host parameter stable. This changes 0.4.0 Poly/Mono renders; keep that
binary for exact prior new-voice sound. No OB-8 fidelity claim is made.

Field exposes overlapping windowed grains and a direct stereo fifth/octave
layer via Texture. Higher pitch return reduces direct feedback; Texture adds
wet makeup up to 2.8x. Fresh Open Field uses active controls; old states select
Texture 0. Two timing/behavior variants at Texture 0 matched the previous DSP
for 508,000 samples with automated Grain/Pitch.

Local validation: 68/68 CTest checks passed, including six actual VST3 host
loads, six independent final-gain scaling/meter/old-state checks, parameter and
preset recall, and minimum editor artifacts. Field and Sequence core ASan/UBSan
checks passed. Minimum editor images were inspected for Density, Loop, Impulse,
Field and Sequence. Sequence examples through actual presets peaked 0.488–0.780
at the fresh +3 dB Master. Field's matched phrase produced peaks 0.127 with
Texture off, 0.150 with moving pitch and 0.089 for Grain Cloud. These level and
stability checks are not listening approval. WAV examples are ignored local
build artifacts in field-listening-050 and sequence-listening-050.

Family VST3 parameter counts: Density 12, Harmonic 13, Loop 21, Impulse 369,
Field 11, Sequence 90. Existing AU build support is unchanged; only universal
VST3 builds are prepared for this Cubase session. Installation needs Cubase
closed, preserves previous bundles outside scan paths, and leaves presets,
projects and DAW preferences in place.

## 2026-11-16 — 0.7.1 family workflow pass

Feedback identifies hidden capabilities and disconnected workflow across all
eight products. The [task audit](research/workflow/README.md) records concrete
obstacles and implemented interface changes. Impulse opens with all objects and
selected rhythm controls visible; Field promotes capture and separates spatial
hold; Resonator uses task groups; Non-Machine exposes sound-edit scope and a
whole-object apply action; Loop uses print/take language. Family control help is
visible in the panel and follows focus/pointer, with correct product-specific
meanings and current inactive reasons. This is implemented interface behavior,
not a completed human usability review.
