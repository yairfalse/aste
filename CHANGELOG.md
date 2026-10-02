# Changelog

Versions apply to the complete six-instrument family. See
[versioning](docs/VERSIONING.md) for numbering and compatibility rules.

## 0.2.0 — 2026-10-02 — Track-level impact (development)

- Density engages quieter tracks: the threshold mapping is 16 dB lower, with
  additional crushed-path make-up, a 75% processed default blend, and -6 dB
  default Output trim to avoid presenting increased loudness as the improvement.
- Harmonic boosts shape wider frequency regions without exaggerating the dB
  labels. Five coloured presets now provide stronger tonal contrasts. Zero
  gain, the neutral Default preset, and clean cuts are preserved.
- Fix Harmonic coefficient smoothing stopping short of low-frequency gain
  targets after knob changes; use double-precision EQ coefficients and state.
- **Audible compatibility change:** saved parameter values, IDs, ranges and
  state schemas are unchanged, but Density and boosted Harmonic bands sound
  different. Keep 0.1.1 for exact old renders. Saved Blend values are respected;
  only new instances and the Default preset start at 75% Blend/-6 dB Output.
- These are measured development changes, not listening-approved release
  candidates. Level-match with Output when comparing in a DAW.
- Local verification: 70 regression checks, 537 Steinberg validator checks per
  changed plugin, DSP ASan/UBSan tests, static analysis and formatting passed.

## 0.1.1 — 2026-10-02 — Development update

- Add an opt-in cleanup utility that previews older per-user Aste AU/VST3 copies
  and moves confirmed matches to a dated backup outside plugin scan folders.
  It requires a newer verified system replacement and leaves same/newer,
  unknown, symlinked and unrelated bundles alone. No permanent deletion.

- Establish a shared release history and version-consistency checks for AU,
  VST3 and installer components.
- Remove a fixed version from the CI package inspection path.
- Keep the original 0.1.0 download unchanged. This remains an unnotarized local
  development build, not a tagged stable release.
- Local verification: 70 checks passed, including isolated cleanup fixtures
  covering version/identity checks, symlinks, changed previews and no-overwrite
  moves. No installed plugin was moved during development.

## 0.1.0 — 2026-10-02 — Development baseline

- First locally shared all-six-instrument macOS installer: universal Intel and
  Apple Silicon AU/VST3 bundles, selectable by instrument and format.
- Calmer Impulse defaults and direct drum audition; clearer interactions across
  all six instruments. Loop saves captured audio and retained generations.
- AGPL-3.0-only first-party licensing, with matching Aste and JUCE source included.
- Local evidence: 69 checks passed; extracted plugin bundles and the disk image
  were verified. Source configured without downloading JUCE.
- Not notarized. Intel execution, oldest-macOS and real-DAW compatibility remain
  unverified for this build. It is not a supported stable release.

The older internal prototypes also used 0.1.0. This entry identifies the
`Aste-0.1.0-macOS-universal-development.dmg` baseline, not every historical build
with that embedded number. The original download is preserved unchanged.

Baseline DMG SHA-256:
`9ea34b188d574b85a91dd78add4050f88f7d0d792e4d091d8ad0d03e7f0e0af4`.
