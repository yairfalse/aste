# 0.8.0 verification record — 2026-11-16

Local macOS development build; dirty personal working tree, not a tagged release.
The [design and reproduction notes](README.md) distinguish completed behavior
from musical evaluation still owed to the musician.

Completed checks:

- All eight universal arm64/x86_64 VST3 bundles built with version 0.8.0.
- All eight plugin adapter suites passed, including state/adoption, recall,
  MIDI/output, real-time allocation/OS audits and new N-01 input/lock tests.
- Minimum-editor artifact suites passed for Density, Sequence, Loop, Impulse,
  Field, N-01 and R-01. Harmonic’s separate artifact command also passed.
  All eight minimum images were visually inspected.
- All eight VST3 host tests passed with 1/2/7/127/511/2048-frame buffers,
  parameter counts, state recall, stereo bus and finite/nonzero processing.
- Six original DSP suites and the playable-engine suite passed (7/7).
- Offline current/reference lab passed 48 exact previous-response comparisons,
  updated partition checks and causal behavior tests. AddressSanitizer and
  UndefinedBehaviorSanitizer run completed with exit 0 and no diagnostics.
- Documentation links and `git diff --check` passed.
- Development installer `--check-only` verified all eight bundle identities,
  version, architecture slices and signatures.

The first host run failed N-01 because the old harness supplied MIDI while
leaving the new explicit Pattern input selected, without host transport. The
harness now selects MIDI input for its no-transport MIDI scenario. The adapter
suite separately tests playing PPQ, host clock, seeks and captured time. After
that correction all eight host tests passed. The original failure remains in
the retained adapter/editor log rather than being silently erased.

Field’s initial normalization still lost too much energy under grain motion.
The final design measures fixed integer delay taps and bounds normalization to
8×. At six rates, measured late/early held-window power ratios were
0.715–0.743, and release/held ratios below 4e-20 in the specified 435 Hz burst
probe. These are one controlled stimulus, not a general decay or perceptual
specification. R-01’s same-chord impulse superposition residual measured
-146.515 dB previously and -1.734 dB with energy-dependent response enabled.

Local evidence folder: `build-evidence/material-revision/`:

- `listening-final/`: 36 eight-second matched WAVs, raw/matched CSV, SHA-256s.
- `offline-results.log`, `sanitizers.log`, `core-results.log`.
- `adapter-editor-results.log`, `vst3-host-results.log`.
- `editors/`: final minimum-size images.

No Cubase listening session, subjective success, assistive-technology approval,
callback deadline benchmark or alias-free result is claimed. Human musical
acceptance remains open. The installed-bundle check and rollback location are
recorded after local installation in the same evidence folder.

## Installed copies

All eight system VST3s were installed as 0.8.0 and passed the same real-host
checks against `/Library/Audio/Plug-Ins/VST3` after installation. The prior
bundles and receipt are retained in
`/Users/yair/Library/Application Support/Aste/Plugin Backups/20261116-142104-0.8.0-dlgsdps4`.
The local `installed-verification.json` records versions, counts and host checks.
Cubase musical listening remains open.
