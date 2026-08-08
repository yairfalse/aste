# Cycle 57 — public CI test diagnostics

## What changed

Added a small standard-library CTest runner for CI. It preserves the complete
test stream and JUnit output, then emits the final failure context as a GitHub
annotation when CTest returns non-zero.

## Why

Cycle 56 fixed both hosted VST3 build failures. The universal and sanitizer
jobs then reached CTest and failed, but unauthenticated GitHub APIs exposed only
the step exit code. A public project needs actionable public failure evidence;
the test name and assertion must not depend on access to private job logs.

## Evidence

- Hosted run 31281785456 passes both core jobs and both VST3 build stages.
- The universal VST3 and sanitizer jobs now fail specifically at their Test
  stage rather than during compilation.
- The wrapper returns CTest's own exit status, keeps `--output-on-failure`, and
  continues writing the existing JUnit result file.
- A successful local 32/32 universal run returns zero through the wrapper.

## Risks

The annotation contains only the final 3,800 characters to stay below GitHub's
4,096-character annotation limit. The
complete stream remains in the normal Actions log and the complete structured
result remains in JUnit. This cycle improves diagnosis only; it does not change
test selection, DSP, plugin code, or pass criteria.

## Next step

Use the public annotation from the next hosted run to reproduce and correct the
specific macOS 15 runtime failure.
