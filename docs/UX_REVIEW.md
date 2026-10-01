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
