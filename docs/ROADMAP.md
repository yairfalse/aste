# Roadmap

## Now — complete-line external validation

The first working VST3, DSP lab, state model, industrial editor, and automated
foundation exist. A deterministic internal package rehearsal now checks bundle
contents, provenance, and an SPDX dependency bill of materials without
publishing an artifact. The exact packaged dependencies also have an offline,
90-day expiring advisory review whose digest and validity window travel inside
the archive. Density and Harmonic now contain one explicitly measured character
revision prompted by music-machine feedback; further range changes require the
updated blind and DAW checks.

The authoritative master-brief audit is
[SPECIFICATION_STATUS.md](SPECIFICATION_STATUS.md). Before external validation,
it now has zero local `Open` rows. Density is specification-complete internal
beta; every remaining item below requires DAWs, target hardware, controlled
listening, real musical projects, or final release identity.

Density's remaining release work:

1. Complete the automation, Density-macro, stereo-link, detector, and
   oversampling listening sheets.
2. Test Cubase 14, Ableton Live 13/beta, and one additional VST3 host.
3. Verify keyboard navigation, VoiceOver, and Retina behavior in a real host.
4. Compare real-time, offline, freeze, and reopen behavior in those hosts.
5. Measure the CPU budget on the oldest supported Apple Silicon and Intel Macs.
6. Record ten real musical projects and review the final golden change set.

## Next — Density release candidate

- Refine ranges or macro mappings only when listening evidence identifies a
  concrete failure.
- Select oversampling only if blind results justify its measured 44-sample
  latency and CPU cost; otherwise retain the documented 1x graph.
- Convert the internal package rehearsal into signed, notarized distribution
  packaging only after the company identity, distribution terms, licence
  material, presets, and release notes are decided.

## Harmonic H-01 internal beta

Harmonic now has an independent four-band production DSP, 12-parameter schema,
six factory starting points, VST3 adapter, industrial editor, product report,
and automated core/adapter/ABI-host boundaries. Candidate 3 remains provisional.
Its stronger 1-to-6 drive and square-root boost participation are measured and
automated, but repository evidence only justifies putting it in a DAW, not
claiming the sound is finished.

The next Harmonic gate is the music-machine checklist: Cubase and Ableton load,
save/reopen, automation, mono/stereo, offline/real-time bounce, scaling, and
level-matched listening on full mixes, ambient, percussion, bass-heavy, and
sparse material. Findings may revise ranges or the nonlinear contribution while
schema-1 IDs remain stable. Oldest-supported Apple Silicon and native Intel CPU
evidence are also outstanding.

## Current — Loop L-01 validation

Sequence S-01 has passed hosted core, sanitizer, universal, signature, and
independent Steinberg validation. Its immediate gate is the music-machine
checklist. Loop L-01 now has an independent working VST3 prototype; its next
gate is the Loop music-machine checklist and a race-free captured-memory state
snapshot. Impulse I-01 now has an independent working VST3 prototype; its next
gate is deterministic music-machine transport and deep-kick validation. Extract
shared UI, transport, or amplifier code only after identical behavior is
measured in two products.

## Explicitly deferred

- No universal plugin engine.
- No dynamic oversampling or quality parameter.
- No user preset browser or cloud service.
- No neural or machine-learning DSP.
- No unsupported hardware-emulation claims.
