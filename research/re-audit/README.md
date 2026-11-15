# Second audit: independent evidence inspection

No production edits. `inspect.rs` is a dependency-free offline Rust tool, not a
plugin, DSP reimplementation or certified audio meter. It reads only the existing
little-endian IEEE float32 mono/stereo RIFF exports, rejects malformed/non-finite
audio, and never normalizes or rewrites input files. No Cargo/release dependency.

```sh
mkdir -p build-audit2-evidence
rustfmt --check research/re-audit/inspect.rs
rustc --edition=2021 -D warnings --test research/re-audit/inspect.rs -o build-audit2-evidence/inspect-tests
build-audit2-evidence/inspect-tests
rustc --edition=2021 -D warnings -O research/re-audit/inspect.rs -o build-audit2-evidence/inspect
build-audit2-evidence/inspect scan build-audit2-listening
build-audit2-evidence/inspect scan build-audit2-clicks
build-audit2-evidence/inspect suite build-audit2-listening
build-audit2-evidence/inspect clocks
```

Render inputs using the existing [six-product lab](../quality/README.md) and
[current Impulse lab](../click-sub/README.md), passing new output directories
`build-audit2-listening` and `build-audit2-clicks` respectively. Do not overwrite
the historical packs. The six-product lab still uses historical Impulse Space.

`scan` uses full-file samples: near-peak occupancy includes silent tails;
quiet-window counts use complete nonoverlapping 10 ms combined-channel RMS
windows below 0.001 (−60 dBFS). They are level-dependent, not perception claims.
`suite` compares each after-start file with before-start, same-controls legacy
and extreme. The fitted scalar minimizes squared error; relative residual is
`sqrt(sum((candidate-scale*reference)^2)/sum(candidate^2))`. Exact null is
`-inf`; silence has undefined ratio and fails instead of inventing a value.

`clocks` is explicitly an arithmetic witness of the current Impulse expression,
not a production renderer. It emits the first differing tick for each of
6 rates × 4 tempos × 4 ratchets, comparing 127/511-frame partitions for 8 seconds.
Zero timing, 1/16 division; no tempo change, seek or host-rounding simulation.

## Actual production-audio reproduction

The unmodified `impulse_tests` passes. A debugger changes only the Click ratchet
in the test's existing `render` fixture before its sample loop. This avoids
editing either production DSP or its assertions. At audited source `7376000`,
the breakpoint is `tests/impulse_tests.cpp:42`; relocate it if that source moves.
Debugging a child process on macOS may require execution outside the sandbox.

```sh
cmake --build build-sanitize --target impulse_tests --parallel 3
lldb --batch \
  -o 'breakpoint set --file impulse_tests.cpp --line 42' \
  -o 'breakpoint command add 1 -o "expression -- parameters.tracks[1].ratchet = 3"' \
  -o 'breakpoint modify --auto-continue true 1' \
  -o run -- build-sanitize/impulse_tests
```

Require three printed assignments of 3, then
`FAIL: Impulse output is independent of block partitioning` and **child** exit 1.
LLDB's own exit code can still be 0. A debugger run that did not apply the
assignments is not a reproduction. The checked-in evidence is the corrected run.

## Inspector negative controls

Only mutate an ignored scratch copy, never the real tool or product:

```sh
sed -e 's/let scale = a.iter()/let scale = 0.5 * a.iter()/' \
    -e 's/as f64)/as f64 * 2.0)/' research/re-audit/inspect.rs \
    > build-audit2-evidence/inspect-fault.rs
rustc --edition=2021 --test build-audit2-evidence/inspect-fault.rs -o build-audit2-evidence/inspect-fault
build-audit2-evidence/inspect-fault
```

Both tests must fail (exit 101). Scanning `research/re-audit`, which has no WAVs,
must separately fail with `no WAV evidence found; coverage unknown`.
Sykli's audit contract checks the Rust inspector, not product acceptance.
See the [audit and evidence](../../docs/research/re-audit/README.md).
