# Musical-control comparison laboratory

Independent C++20 executable: no JUCE, third-party libraries or audio devices.
Build from a Git checkout containing original production commit
`8a603dcbc59b52a58637fe6305676efc5a406f80`. CMake extracts and namespace-renames
that exact source into the build directory, then compiles it alongside current
DSP. It never substitutes the current legacy switch for the original oracle.

```sh
cmake -S research/quality -B build-comparison -DCMAKE_BUILD_TYPE=Release
cmake --build build-comparison --parallel 3
build-comparison/quality_comparison --check
build-comparison/quality_comparison --check-fault
# The fault command MUST exit 1; it intentionally corrupts a reference comparison.
build-comparison/quality_comparison --pack build-listening
build-comparison/quality_comparison --spectral build-comparison/spectral.csv
# Run without other builds/render jobs for meaningful timing:
build-comparison/quality_comparison --benchmark build-comparison/callback.csv
```

`--check` compares exact stereo samples for six engines, four control profiles,
six rates (44.1/48/88.2/96/176.4/192 kHz), eight seconds each. Loop includes
capture and reprinting. It also tests the loudness meter against a separately
implemented long-double oracle. These bounded fixtures do not prove equality
for every possible session. `--check-fault` modifies a sample and must fail.
For ASan/UBSan configure with `-DQUALITY_SANITIZERS=ON` and Debug.

`sykli run research/quality/sykli.json` covers functional plugin tests and core /
reference ASan+UBSan checks. Its first task explicitly excludes the expired
dependency review, **not** any DSP, adapter or host check. This is a functional
receipt, not a release/security pass. Run the complete `ctest --test-dir
build-plugin --output-on-failure` separately; the report retains its failure.
The local contract verifies the pinned clean JUCE checkout at the cached path
used in this workspace. On another machine, update that path and re-lock before
running; no new dependency is fetched by this contract.

The pack contains 356 stereo float32 WAVs at 48 kHz, eight seconds each, plus a
measurement CSV. Seven generated effect sources cover percussion, bass, a
simple oscillator, noise, a chord, a constructed musical phrase and sparse
material. These are synthetic diagnostics, not supplied production recordings.
Sequence and Impulse use their own common phrases. `audio/` contains matched
comparisons; `raw/` contains unattenuated diagnostics that can exceed unity.
Use a new output directory each time: exports refuse to overwrite responses.

Within each source/product group the iterative K-weighted gated loudness match
targets at most -23 LUFS and applies shared sample-peak headroom of 0.8. This is
not true-peak protection or a certified meter. Unknown loudness remains unknown.
Matching methods and sources reuse `audio.hpp` and `meter_checks.hpp` from the
completed Aste audit at `7cdd7d1`; no runtime dependency on that worktree exists.

Variants:

- `dry`: effects only.
- `before_start`: original DSP and original starting controls.
- `after_start`: new DSP and new starting controls. **Loop is the exception:**
  Wow 100% demonstrates the clock repair; its factory default is unchanged.
- `same_controls_legacy`: the `after_start` controls with original behavior,
  separating the algorithm change from the starting-point change.
- `current_extreme`: multiple parameters at strong settings.
- `current_automation_or_MIDI`: callback-delivered control changes, or MIDI.

Loop captures two seconds. Field enters hold at two seconds. Sequence compares
gapped MIDI attacks, host sequencing at the extreme, and overlapping MIDI in
the last variant. Impulse compares visible patterns and all-eight-object MIDI.
Automation is delivered at renderer chunk boundaries, not claimed sample-exact
25 ms automation. The dedicated Sequence regression separately tests exact
partition independence and transport changes.

The benchmark measures only the DSP process-call duration, not initialization,
allocation, rendering, WAV writing, JUCE or host overhead. Each row renders five
seconds; 100% means one core fully occupied at real time. Requested blocks are
maximum sizes: chunks also split at 512-frame and quarter-second boundaries.
Three trials cover legacy/current, six products, 48/96 kHz and 128/511 frames.
Loop uses two-second tapes, not a full-capacity memory benchmark. Results are
local observations, not hard realtime or cross-machine performance guarantees.

The spectral diagnostic measures Density's actual Grip graph with steady
996/7004 Hz tones at -36/-18/-6 dBFS and six sample rates. H3 and the folded fifth
are projected over the final 0.25 seconds after 0.5 seconds settling. It does
not measure total alias energy, intersample peaks or perceptual acceptability.
See the [report](../../docs/research/quality/README.md) for results and limits.
