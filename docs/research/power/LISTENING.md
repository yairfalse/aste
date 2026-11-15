# Listening packs and verification index

The six packs were rendered twice with the same Release binary; recursive
comparison of every generated file passed byte-for-byte. Six identical copies
of the root manifest were subsequently added to the product folders for
standalone packaging. Each ZIP passed `unzip -tq`. Native `afinfo` confirms the
representative WAV as 48 kHz, two-channel float32, eight seconds;
[format output](evidence/wav-format.txt). The renderer independently rejects
nonfinite output. No human audition or musical selection is claimed.

Local artifacts are retained in the audit working copy:
`/Users/yair/projects/aste/build-power/worktree/`.
The expanded pack is `build-power-listening-final/`; audio is intentionally
not committed to Git. Recreate it using the [lab commands](../../../research/power/README.md)
and preserve the supplied compiler/build provenance for byte comparisons.
Cross-toolchain bit identity is not promised.

| Product | Standalone archive | Variants / WAVs | Approximate ZIP size | Measurements |
|---|---|---:|---:|---|
| Density | `build-power-density.zip` | 49 / 98 | 119 MiB | [CSV](evidence/density-listening.csv) |
| Harmonic | `build-power-harmonic.zip` | 56 / 112 | 137 MiB | [CSV](evidence/harmonic-listening.csv) |
| Sequence | `build-power-sequence.zip` | 21 / 42 | 41 MiB | [CSV](evidence/sequence-listening.csv) |
| Loop | `build-power-loop.zip` | 56 / 112 | 90 MiB | [CSV](evidence/loop-listening.csv) |
| Impulse | `build-power-impulse.zip` | 36 / 72 | 92 MiB | [CSV](evidence/impulse-listening.csv) |
| Field | `build-power-field.zip` | 49 / 98 | 243 MiB | [CSV](evidence/field-listening.csv) |

[Archive SHA-256](evidence/archive-sha256.txt) and
[all 559 packaged-source file SHA-256 records](evidence/listening-sha256.txt)
allow independent integrity checks. The latter includes 534 WAVs and 25
manifest/CSV files. Run `shasum -a 256 -c` on the file list from the expanded
pack directory. Response sheets are blank; their hashes change when filled.

## Start with a controlled question

Use the matched `audio/` files, not raw `source/` files, which may exceed unity.
Each product's answer key maps the opaque filenames to fixture and settings.
Prefer a second person revealing the key after an initial pass; the fixed
reversed-rotation IDs are not a randomized or independently blinded study.

1. **Sequence:** compare `default` and `release_aware_default` in `gapped_MIDI`,
   then `overlapping_MIDI`. Determine whether new attacks improve deliberate
   phrasing without damaging genuine legato. Host-pattern comparison is a
   no-change control, not an alternate sequencer.
2. **Loop:** compare `interacting_controls` with `continuous_wow_clock` on the
   same source. They differ only in the clock. Compare ordinary generation
   settings separately, without confusing louder prints with better evolution.
3. **Harmonic:** compare `character_candidate` with `linked_12db_gesture`, first
   on bass/chords, then percussion and noise. Decide whether coloration becomes
   controllable and useful, not merely more obvious.
4. Across every product, compare default, conservative, extreme, fast automation
   and interacting settings; use effect dry references and individual Impulse
   objects. A character-candidate label is not an approved sweet spot.

Record desired outcome, audible difference, control/predictability, preference
and confidence separately. “No reliable difference” and “worse” are useful
results. Then repeat a composition task from the [acceptance backlog](BACKLOG.md)
in Cubase, noting build, source level, sample rate, buffer and session recall.
Generated layered fixtures are not real project excerpts; those remain an
explicit missing input to musical acceptance.

## Verification ledger

- Release experimental measurement/candidate contract passed in 1.03 seconds;
  ASan/UBSan counterpart passed in 14.98 seconds on the first Sykli run.
  These timings are gate runtimes, not processor performance measurements.
- Full Release and sanitizer CTest each remain **1/2** because the unchanged
  Sequence host-gate partition test fails. Retained logs:
  [Release](evidence/full-release-ctest.log),
  [sanitizers](evidence/full-sanitizer-ctest.log).
- The original six-plugin suite built and ran 68 tests. With the repaired
  documentation link independently rechecked, **67/68** have passed; the
  dependency security review remains expired. No all-green release claim.
  Retained [original suite log](evidence/original-plugin-suite.log) and
  [documentation recheck](evidence/documentation-recheck.log) preserve both
  the initial failures and the repaired link check.
- Negative controls are retained for the
  [Sequence attack before substitution](evidence/candidates-before.log),
  [Loop clock before substitution](evidence/loop-before.log),
  [Loop zero-motion perturbation](evidence/loop-zero-negative-control.log) and
  [Loop nonfinite perturbation](evidence/loop-finite-negative-control.log).
  Independent gain/meter negative controls are documented in
  [colour methods](COLOUR_METHODS.md).
- Sykli contract scope: Release and sanitizer **experimental** checks only.
  Final exact-commit run/verification receipts stay under `.sykli/receipts/`
  and `build-power-evidence/`, outside the committed report. They do not
  override either retained failure or establish human acceptance.

The [evidence report](EVIDENCE.md) separates measured results, source findings,
proposed changes and missing judgments. All production graphs and Cubase state
contracts remain unchanged, and no PR was opened.
