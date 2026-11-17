# Local validation — 2026-11-18

Starting revision: `a5ce188`. Platform: Apple silicon, AppleClang 21,
repository-pinned JUCE, Release and Debug ASan/UBSan configurations.

## Observed results

| Check | Result |
|---|---|
| Loop transport regression before implementation | Failed Stop position preservation, capture inhibition, wet silence and Release decay/preservation checks |
| Grain recall regression before implementation | Frozen saved source did not reopen audibly |
| Click portability regression before implementation | Failed after moving the original WAV away from its saved path |
| Full Release build | Passed after correcting an existing misleading-indentation compiler error in `research/c01/core.cpp`; braces/formatting only |
| Full CTest suite | **84/84 passed**, 20.01 seconds, with CI's `ASTE_FAST_RESEARCH=1 ASTE_SKIP_BENCHMARK=1` |
| Affected ASan/UBSan suites | **5/5 passed**, 139.88 seconds: Loop core/adapter, Grain core/adapter, Click adapter |
| Final Loop/Grain UI and legacy-default edits | Affected adapter sanitizer checks passed again; final compact Loop layout also rechecked |
| Grain concurrent bank save/clear | ThreadSanitizer passed with changing source data; no race report |
| Unchanged-sound differential fixture | Sample-bit fingerprints match starting code at 48 and 192 kHz for both Loop and Grain; see [reproduction](README.md) |
| Static analysis | Loop target and standalone Grain core Clang analysis passed |
| Built VST3 boundary | All eleven repository smoke-host tests passed within the full suite; updated Loop boundary rechecked after the final layout edit |
| Local Loop/Grain/Click bundles | Ad-hoc signatures verified; all three binaries report `arm64` |

The final minimum-size Loop render was inspected. The first render exposed an
output-meter overlap with the new transport buttons. Compact tape-lane spacing
fixed it; both meters and Play/Stop/Release now fit. Grain's rendered help text
was checked against its new recall behavior. This is image/component validation,
not a VoiceOver or Cubase interaction test.

## Remaining limits

- The repository-wide formatting gate still fails on pre-existing upstream
  formatting, beginning in unchanged `apps/click/click_editor.cpp`. Running the
  same formatter on that file from `a5ce188` reproduces the failure. New helpers
  and changed Loop/Grain files pass formatting; unrelated files were not swept
  through a formatter to disguise the baseline failure.
- No Cubase session was opened, no installed plugin was replaced, no human
  listening assessment was performed, and no new version was selected.
- ThreadSanitizer/debug timing printed by the existing Grain test is
  instrumented overhead, **not a real-time performance measurement**. This pass
  makes no new CPU-budget or subjective sound-quality claim.
- Captured source recall is not sample-exact continuation of active grains or
  live master repeats. Click loads must complete before sample audio can be
  embedded; old missing recordings cannot be reconstructed.

Local logs and bundles are in ignored `build-recall/` and
`build-recall-sanitize/`. Relevant logs: `full-tests.log`, `sanitizer-tests.log`,
`sanitizer-final-ui-tests.log`, `loop-ui-sanitize-tests.log`,
`grain-tsan-final.log`, `loop-ui-tests.log`, `format.log` and
`baseline-format.log`. Commands and the compatibility fixture are retained in
this directory so the checks can be rerun without those generated artifacts.
