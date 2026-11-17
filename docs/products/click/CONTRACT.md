# K-01 parameter, state and resource contract

Product `click-k01`, manufacturer `Aste`, code `Clk1`, VST3 instrument/drum/sampler.
Framework-independent C++20 processor; JUCE adapter/editor only at the boundary.
The product uses the normal repository build and shared strict scalar-state,
precision-entry and Master components. Existing products are not linked into it.

## Stable host order

**2592 parameters**, version hints 1. IDs and order are permanent for this identity:

- 22 global IDs, in source order: `mode`, `master_length`, `region_start`,
  `region_end`, `repeat`, `nested`, `reverse`, `hold`, `time_mode`, `void`, `crush`,
  `shift`, `shift_steps`, `inner_span`, `pressure`, `recovery`, `dirt`, `parallel`,
  `fill`, `roll_span`, `swing`, `seed`.
- 34 fields for each of four objects: `o{1..4}_{field}`. Exact field order,
  physical bounds/defaults, units and skew centres live in `Processor::controls`
  and `objectIds` in `apps/click/click_plugin.cpp`.
- 19 fields for each of 32 events in each object, object-major then event-major:
  `o{1..4}_e{1..32}_{field}`. On, Lock, Velocity, Pitch, Start, Cutoff, Morph,
  Decay, Microtiming, Probability, Span, Curve, End Velocity, End Pitch, Mutation,
  Irregularity, Sound Object, Condition, Count. Exact ASCII field names are in
  `eventIds`. Every event field is independently automatable.
- Shared `master` (−36..+18 dB) and `bypass` follow all product fields.

Object/step host titles contain their object/event address. No emulated VST3
MIDI-CC parameters are appended. Host values are normalized by physical ranges;
text edits reject nonfinite/invalid decimal input. Principal dynamics and live
object levels use 15 ms rate-correct smoothing; Void uses 2 ms. Voice sound
parameters are latched per hit, including event sound locks. Enabling a lock in
the editor captures current source settings; automation uses stored lock values.
Master uses the family's 20 ms smoothing. Core MIDI dispatch is sample-accurate;
parameter targets follow the existing block-atomic-values contract.

## Voice, scheduler and DSP bounds

Four independent rows, 32 stored events each; a fixed 48-voice pool and
512 queued roll hits. The active budget is 48 voices at rates up to 48 kHz,
24 up to 96 kHz and 12 above 96 kHz, chosen once at preparation to retain 2×
nonlinear quality while meeting the measured CPU budget. New hits steal the quietest sounding envelope with a 2 ms
residual fade. Queue exhaustion drops/counts pending hits, with no allocation.
Sound choice refers to an existing object; target mute/solo still applies.
Probability is a deterministic hash of absolute cycle/step, row and Seed.
Event microtiming spans ±0.49 steps; roll count 1–16; roll span .03125–8 steps,
scaled by global Roll Span. Global scaling is clamped to eight steps. Rolls have
curved/irregular spacing, velocity and pitch curves, and morph/filter mutation.
Internal sequence rolls use musical time and pause with Hold; direct MIDI rolls
use the sample clock. There is no internal sequencer fallback if no host PPQ is
available. MIDI-only mode ignores the internal sequencer; Host mode ignores MIDI
notes, while audition and performance CCs remain available.

Generation, original dual-resonance TPT morph filtering and nonlinear dynamics
run at 2×. A 33-tap Blackman-windowed FIR decimates, with **8 host samples**
reported latency. Filters update envelope-driven coefficients every sixteen
internal samples; fixed coefficients and voice decay/frequency terms are cached
at trigger. This avoids redundant transcendental work in dense rolls. The
extreme captured-fragment Crush is an intentional nonlinear performance action.
Nearest playback and digital quantization are explicit optional artifacts, not
claims of transparent interpolation or a proprietary sampler emulation.

Host rates 22050–192000 Hz. Invalid preparation fails silent. Sample pitch is
±48 semitones; synthesized Foundation 18–16000 Hz; oscillator increments are
bounded below internal Nyquist, and high-register modulation depth is reduced.
DC servos, bounded nonlinearities and the final post-Master ceiling keep output
finite/bounded. This is not a claim of alias-free extreme settings.

## Sample handoff and memory

Each object has three fixed stereo banks of 1,048,576 floats/channel. Worker-only
reservation CASes a bank from zero users to Writing (−1). Publishing transfers
one ownership reference to an atomic pending index; the callback transfers that
reference to active ownership. Sounding voices retain their bank independently.
A worker cannot write an active/pending/sounding bank. Replacement shortens old
sample-bearing tails to 5 ms, so bank contention resolves promptly. The worker
checks request generations before publication; stale loads are cancelled.
No shared_ptr destruction, vector resize, file IO, locks or allocation occurs in
rendering. Path/status/waveform metadata is guarded only off the audio thread.
The UI reads bounded scalar meters and loader-produced waveform bins.

Two stereo eight-second fragment banks record while inactive. Capture freezes
ring indices and length; it does not copy seconds of audio in a callback. A
fragment's buffer is immutable until release. An inactive Inner bank records the
performed outer output, allowing actual nested audio capture. Seam and entry/
exit fades limit accidental discontinuities. History is discarded on host stop,
seek, prepare, restore and panic. There is no serialized live audio buffer.

Sample-bank heap: 96 MiB. At 192 kHz, two stereo fragment banks add 23.44 MiB;
core heap stays below 128 MiB, excluding JUCE/editor/decoder objects. Loaded files
are bounded before decoding; sample metadata accepts at most 4096-character paths.
The loader can temporarily allocate a two-channel decode buffer up to 8 MiB.
All per-sample DSP, MIDI, queued hits and meter storage is fixed.

## State and presets

Shared binary ValueTree/XML schema **1**, root/type/product `click-k01`, retains
family material metadata (currently no alternative K-01 response). Scalars are
validated transactionally: wrong root/product/schema, duplicate known IDs or
nonfinite/malformed values reject restore; missing IDs restore reset defaults;
unknown IDs are ignored; finite out-of-range values clamp.

Four `sample0`–`sample3` attributes contain absolute references or `builtin:0`–`3`.
A `SCENES` child holds up to four validated scalar snapshots with sample paths.
Optional `AUDIO0`–`AUDIO3` children embed loaded external samples, including in
each stored scene. Format 1 stores rate, frames and interleaved little-endian
float32 Base64; maximum 1,048,576 frames, 8–192 kHz, finite samples in [-8, 8].
Duplicate or malformed audio rejects the complete restore. The state envelope
is limited to 256 MiB before parsing. Embedded audio takes precedence over the
path and loads through the existing off-thread sample handoff. Original
path-only state is still accepted; older binaries cannot use embedded audio.
Sample-boundary automation values are normalized fractions; editor text fields
show frames and accept either frame numbers or ms. Their continuous host ranges
preserve subframe precision rather than quantizing edits to percentages.

No voices, pending hits, compressor detector, random execution history or
captured master audio is serialized. Restore clears live DSP and queues
sample decoding on the worker; missing path-only references remain relinkable.
Full `.k01` presets use the host state format. `.k01sound` is a separate strict
`click-sound` schema 1 XML format containing sound fields, sample reference and
optional embedded `AUDIO` using the same format;
pattern, MIDI mapping, cycle and mute/solo are preserved on sound import.
