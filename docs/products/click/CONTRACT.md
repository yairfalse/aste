# K-01 parameter, state and resource contract

Product `click-k01`, manufacturer `Aste`, code `Clk1`, VST3 instrument/drum/sampler.
Framework-independent C++20 processor; JUCE adapter/editor only at the boundary.
The product uses the normal repository build and shared strict scalar-state,
precision-entry and Master components. Existing products are not linked into it.

## Stable host order

**2697 parameters**, version hints 1. The original 2592 IDs and positions are
unchanged; 105 performance parameters are appended after `master` and `bypass`:

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
- Shared `master` (−36..+18 dB) and `bypass` follow the original product fields.

The extension adds `skew`, `compressor_mode`, `memory_play`, `memory_repeat`,
`repeat_division`, `repeat_free`, `memory_generation`, `capture_audio`,
`resample_audio`, then `o{1..4}_speed` and `o{1..4}_playback` in object order.
Finally eight generations `m{0..7}_{field}` each have Start, End, Loop Start,
Loop End, Pitch, Speed, Cutoff, Resonance, Morph, Playback and Reverse, in that
order. Exact IDs are `memoryIds`. Speed uses a binary 1/8192 grid so neutral 1×
survives host normalization exactly. Pitch and speed are varispeed, not time
stretching. Missing extensions restore neutral playback, zero SKEW and Original
dynamics. Fresh factory starting points select Impact dynamics explicitly.

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
family material metadata; dynamics selection has its own explicit parameter. Scalars are
validated transactionally: wrong root/product/schema, duplicate known IDs or
nonfinite/malformed values reject restore; missing IDs restore reset defaults;
unknown IDs are ignored; finite out-of-range values clamp.

Four `sample0`–`sample3` attributes contain absolute references or `builtin:0`–`3`.
A `SCENES` child holds up to four validated scalar snapshots with sample paths.
Optional `AUDIO0`–`AUDIO3` children embed loaded external samples, including in
each stored scene. Format 1 stores rate, frames and interleaved little-endian
float32 Base64; maximum 1,048,576 frames, 8–192 kHz, finite samples in [-8, 8].
Duplicate or malformed audio rejects the complete restore. The state envelope
is limited to 512 MiB before parsing. Embedded audio takes precedence over the
path and loads through the existing off-thread sample handoff. Original
path-only state is still accepted; older binaries cannot use embedded audio.
Sample-boundary automation values are normalized fractions; editor text fields
show frames and accept either frame numbers or ms. Their continuous host ranges
preserve subframe precision rather than quantizing edits to percentages.

No voices, pending hits, compressor detector or random execution history is
serialized. Restore clears live DSP and queues
sample decoding on the worker; missing path-only references remain relinkable.
Full `.k01` presets use the host state format. `.k01sound` is a separate strict
`click-sound` schema 1 XML format containing sound fields, sample reference and
optional embedded `AUDIO` using the same format;
pattern, MIDI mapping, cycle and mute/solo are preserved on sound import.
Optional Speed and Playback fields extend this sound format; missing fields
restore 1× / Clean. Old binaries do not understand the new sound fields.

## Durable master generations

`MasterMemory` receives the Engine's rendered stereo output **after** its
compressor, old transient repeats and performance effects, **before** the durable
sampler and final Master trim/ceiling. Its live ring never receives its own
playback. CAPTURE freezes the requested recent master-region audio by swapping
preallocated buffers. A partial history captures only the available material;
an empty capture reports that playback is required.

Eight immutable generations (original plus seven successors) and one live ring
each hold at most eight seconds, stereo float32. Additional heap is 27,648,000
bytes at 48 kHz or 110,592,000 at 192 kHz. Existing Engine memory is additional.
There is no eviction: full history refuses another capture/print visibly, leaving
every generation playable. Session serialization and replacement allocate only
off audio; old engines retire to a control-thread collection list.

RESAMPLE freezes the source generation and controls at invocation, resets the
print filter state, and writes one processed cycle into the next unused bank
over audio callbacks. It publishes only a complete generation. An output cycle
longer than eight seconds is rejected, not cropped. A seek cancels an unfinished
print and resets playback phase but retains completed PCM. New generations have
independent neutral parameters, so baked pitch/filter/PCM effects are not applied
again. Playback seams and monitor entry/exit use short fades. Continuous controls
have 3 ms smoothing; printing uses the frozen targets for reproducibility.

`MASTER_MEMORY` format 1 contains contiguous `GEN0`…`GEN7` codec children and a
selected index. Each child uses the existing bounded audio codec, at 22.05–192
kHz and at most eight seconds. Duplicate, out-of-order, mixed-rate, oversized or
nonfinite payloads reject the entire restore. Same-rate recall preserves sample
bits, including signed zero. Rate changes resample off audio. Full `.k01` files
include these generations. Scenes reference the session-wide generations rather
than duplicating their audio. Closing/reopening preserves generations, not the
in-flight print, live ring, filter integrators or undo stack.

Quick repeat divisions are 1, 1/2, 1/4, 1/8 and 1/16 quarter notes (labels 1/4
through 1/64), plus FREE. Fragment traversal is varispeed. If the selected region
is shorter than the requested period it repeats at its available length. Continue
advances the underlying Engine; Hold sets its existing musical hold while the
durable audio repeat runs. Old outer/inner repeats remain upstream and continue
to own their existing buffers. Releasing the durable repeat reveals their current
output. PLAY CAPTURE is independently latched and can play with host transport
stopped; turn it off to return to the live bus. VOID/CRUSH and dynamics process
the live bus, not an already printed generation.

Undo/redo covers parameter edits, generation selection, and sample replacement;
redo retains decoded PCM after an import completes. Generations stay immutable
and available when their selection is undone. Undo capacity is bounded; sample
edits reserve units for two maximum-size encoded payloads. Undo is not serialized.

## SKEW and playback character

SKEW deforms each row's phase using two sinusoidal terms, deterministic seed
offsets and distinct signed row weights. The maximum derivative deviation is
0.85: phase stays monotonic under fixed settings and master endpoints stay fixed.
Zero bypasses the warp exactly. Pattern data is never rewritten. Automation
changes the phase target at the normal block boundary; it is not random jitter.

Clean playback remains available. The original 16-bit / 44.1 kHz model quantizes
PCM on a modeled source grid; 12-bit / 22.05 kHz and 8-bit / 11.025 kHz are
explicit degraded choices. These are original digital playback models, **not an
E6400 emulation**, measured hardware matches or claims of anti-aliased rate
conversion. Object interpolation remains selectable; durable 16-bit playback
uses linear reconstruction, degraded modes use nearest. The morph filter uses
the product's original two-resonance TPT topology, not proprietary algorithms.

Impact dynamics adds pressure-dependent attack (~1.5 to 0.094 ms), program-
dependent recovery, threshold down to −48 dB, ratio up to 40:1 and stronger Dirt
drive. Original retains its previous equations. Both retain parallel blending,
2× nonlinear processing, DC removal and the final output ceiling. This is an
optional response, not proof of musical superiority or alias-free extremes.
