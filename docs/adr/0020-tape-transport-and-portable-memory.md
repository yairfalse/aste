# ADR 0020 — Tape transport and portable source memory

Status: implemented for local evaluation, 2026-11-18.

## Problem and decision

The album workflow needs two practical corrections: stopping Loop must not
require erasing its tape, and a saved Grain/Click sound must not depend on
recreating a capture or retaining a sample at a machine-specific path.

Loop appends a single Play/Stop/Release parameter after Master. Stop freezes
the tape and fades only the wet output; Play resumes it. Release suppresses
capture and printing, cancels an unfinished print and decays playback without
damaging the retained tape. Feedback sets a bounded 50 ms–8 s T60 release.
This is a playback envelope, not a new feedback-delay topology or cumulative
erasure of the recording. Existing Play processing and parameter identities
are preserved. Live transport phase and envelope progress are not serialized.

Grain saves each node's latest completed source bank. Its existing three-bank
ownership gains a lock-free reader pin: a saving thread copies a published,
immutable bank; the audio thread skips pinned banks rather than waiting.
Only non-audio setup prepares restored engines. A bounded atomic pointer
handoff installs them; retired engines are destroyed off the audio thread.
This avoids copying entire four-second buffers inside callbacks and avoids
adding another full-size snapshot pool.

Click caches encoded external samples when its existing loader completes.
Sessions, scenes and independent sounds carry those payloads. Restore decodes
them on that worker using the existing sample-bank handoff; files are fallback
references for older path-only state, not prerequisites for new loaded sounds.
Factory fragments retain their existing deterministic built-in identifiers.

Both products use one small JUCE-boundary codec for bounded interleaved
little-endian float32 Base64. Payload size, rate, finite samples and duplicate
entries are validated before changing controls. No new dependency is added.

## Tradeoffs and boundaries

- Saving Grain does not freeze active grains, playback cursors, routing delay
  state or an unfinished capture. It restores source material, not a bit-exact
  continuation. Different host rates use bounded linear resampling on setup.
- A Grain save can temporarily delay capture-bank reuse; it never blocks audio.
  Nodes are captured independently, not as a sample-synchronous network image.
- Click sample loads must finish before their audio is available to save.
  Loading/missing status remains visible. Live master/inner repeat buffers are
  still transient, and are not misrepresented as embedded sample sources.
- Embedded PCM increases session size. A maximum external Click sample uses
  about 10.7 MiB of Base64; four unique samples in each of four scenes plus the
  current state can approach 213 MiB before XML overhead. Reused strings share
  storage in memory. Grain's maximum three saved banks use about 23.4 MiB of
  Base64. State parsers cap their envelopes at 256 and 64 MiB respectively.
- Old binaries cannot recover the new embedded payloads. Preserve originals
  when deliberately moving a session back to an older binary.
- No new product version, migration of old sounds, PR or remote push is part
  of this change. Cubase and human listening acceptance remain separate from
  automated correctness evidence.
