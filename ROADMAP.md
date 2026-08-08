# Roadmap

## Now — Density D-01 validation

The first working VST3, DSP lab, state model, industrial editor, and automated
foundation exist. A deterministic internal package rehearsal now checks bundle
contents, provenance, and an SPDX dependency bill of materials without
publishing an artifact. Do not add new production DSP until the current blind
packs have been reviewed.

Remaining release work:

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

## Later — product family

Harmonic, Loop, and Impulse remain design consumers, not implementation work.
Begin Harmonic only after Density is stable. Extract shared UI or amplifier code
only when the second product needs the same behavior; transport and rhythm
foundations wait for Loop or Impulse.

## Explicitly deferred

- No universal plugin engine.
- No dynamic oversampling or quality parameter.
- No user preset browser or cloud service.
- No neural or machine-learning DSP.
- No unsupported hardware-emulation claims.
