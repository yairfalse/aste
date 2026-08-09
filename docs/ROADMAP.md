# Roadmap

## Now — Density and Harmonic external validation

The first working VST3, DSP lab, state model, industrial editor, and automated
foundation exist. A deterministic internal package rehearsal now checks bundle
contents, provenance, and an SPDX dependency bill of materials without
publishing an artifact. The exact packaged dependencies also have an offline,
90-day expiring advisory review whose digest and validity window travel inside
the archive. Do not add new production DSP until the current blind packs have
been reviewed.

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
and automated core/adapter/ABI-host boundaries. Candidate 3 remains provisional:
repository evidence justifies putting it in a DAW, not claiming the sound is
finished.

The next Harmonic gate is the music-machine checklist: Cubase and Ableton load,
save/reopen, automation, mono/stereo, offline/real-time bounce, scaling, and
level-matched listening on full mixes, ambient, percussion, bass-heavy, and
sparse material. Findings may revise ranges or the nonlinear contribution while
schema-1 IDs remain stable. Oldest-supported Apple Silicon and native Intel CPU
evidence are also outstanding.

## Later — product family

Loop and Impulse remain design consumers, not implementation work. Extract
shared UI or amplifier code only when another product needs identical measured
behavior; transport and rhythm foundations wait for Loop or Impulse.

## Explicitly deferred

- No universal plugin engine.
- No dynamic oversampling or quality parameter.
- No user preset browser or cloud service.
- No neural or machine-learning DSP.
- No unsupported hardware-emulation claims.
