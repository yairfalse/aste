# Field F-01 0.12.0 — installed release

## Delivered

Universal arm64/x86_64 Release VST3:
`/Library/Audio/Plug-Ins/VST3/Field F-01.vst3`.

Installed binary SHA256:
`e7ca23ac3ce3c42d7504dcb15520d648d2c03b5277f95e3d9fb12c1b85181bd5`.
The installed binary matches the tested build exactly; strict/deep signature and
architecture checks pass. Only Field was replaced by this installation.
Previous Field and installation receipt are backed up at:
`/Users/yair/Library/Application Support/Aste/Plugin Backups/20261117-124114-0.12.0-ga4q_qkg`.

The new engine promotes the diffused 16-line Hadamard foundation. Independent
per-line decay, stereo input diffusion and early taps provide the conventional
space. Fragment and pitch transformations no longer impose the earlier pitch
feedback gain reduction. Wet gain is calibrated separately from dry level.
FOREVER closes input and holds existing network energy, with smooth entry/release
and orthogonal internal movement. Clear uses a short output fade. Existing MIDI
excitation, optional phrase memory, mono/stereo operation and output controls
remain available. No new host parameters were added.

## Start making music

New instances select **Clean room**. **Deep hall** gives a larger conventional
space. **Dark dub**, **Enormous space** and **Evolving field** move toward the
stored-space identity. Feed sound, then engage **FOREVER** to hold the existing
field; disengage to admit new sound and resume decay. On a send, set Blend to
100%; on an insert, start with the preset balance. Transform=0 removes the
fragment/pitched readout while preserving the reverb. Master Output adjusts the
final level independently.

Old projects without engine metadata use the original DSP. Choose **USE UPDATED
SOUND** to adopt the new engine with current controls, or choose one of the six
new factory starting points. The nine original snapshots still select their
original engine. Existing parameter IDs, indices, ranges, behavior/material
metadata and old state formats are retained. Delay audio itself is not saved.

## Completed validation

- Original Field DSP tests, adapter/editor/preset/state tests and actual VST3
  host validation: 3/3 passed on the final release.
- Production DSP versus frozen original: sample-exact old stereo rendering for
  exercised independent/legacy hold fixtures. Old-state absent-engine fallback,
  explicit adoption preserving all values, unknown-engine transactional rejection,
  all 15 factory preset recall and byte-stable session state tested.
- Six-rate processing: 44.1/48/88.2/96/176.4/192 kHz. Exact output across 127/511
  block partitions and one-sample/127 partitions with aligned automation.
- No audio allocations or calibrated OS lock/file/write calls in exercised
  callbacks. FOREVER energy stays within the test tolerance across a long hold;
  empty hold remains silent and clear removes stored energy.
- Separate ASan/UBSan run passed in 39.47 seconds. The long-hold wrap-boundary
  defect detected in the first run was fixed and rerun, rather than waived.
- Installed native and Intel VST3 loads pass: 19 parameters, zero latency, six
  buffer sizes, finite audio, byte-stable recall. Extended host validation runs
  all parameters over four sample rates and simulated tempo/start/stop/seek
  changes, then restores state successfully.
- Minimum-size editor was visually inspected; controls and exact values fit.
- Six production-engine WAV examples and CPU/memory CSV are rendered in
  `build-evidence/field-012/release-audio`. Benchmark: 128 frames, all main
  transformation controls at maximum, 100 warm-up + 350 measured blocks.
  At 48 kHz median is 64.583 µs, p99 74.458 µs and max 89.667 µs versus a
  2666.667 µs deadline; allocated processor memory is 4,747,568 bytes. Full
  results cover 44.1/48/96/192 kHz. Local benchmarks are not DAW-load guarantees.

## Actual Cubase status and limits

Cubase 14 was launched after installation and its process was observed running.
macOS then denied osascript Accessibility access (-1728). Field insertion,
playback, automation recording, preset save/reopen and session recall were
therefore **not interactively tested in Cubase**. The successful plugin tests
above belong to the independent suitable VST3 host and adapter tests. No existing
Cubase project was edited. Do not describe this release as Cubase-playback tested.

The agent did not subjectively audition the rendered audio. Musical excellence
is not established by the presence of a known topology or by passing tests.
This is the implemented, installed candidate using the strongest available
foundation, with concrete presets and technical evidence for immediate use.
Automation is applied at block boundaries with smoothing; MIDI excitation keeps
sample offsets. FOREVER holds existing energy rather than continuously recording
new input. Live tails and phrase audio are intentionally not serialized.
