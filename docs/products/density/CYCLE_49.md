# Cycle 49 — macOS CI foundation

## What changed

Added a least-privilege GitHub Actions workflow with arm64 and Intel core jobs,
a universal VST3 job, and an arm64 Address/UndefinedBehavior sanitizer job. All
jobs use the repository's CMake and CTest paths; release packaging is not
published while product gates remain open.

## Why

Local checks did not protect future commits or exercise x86_64 compilation.
The workflow reuses existing gates rather than introducing a second build
system.

## Evidence

- The workflow parses as valid YAML.
- A clean local CI-shaped Release configure, build, and reduced research matrix
  passes 28/28 before later provenance checks are added.
- Strict compiler warnings remain errors on every project target.
- CI verifies the VST3 bundle signature and both universal binary slices.
- Jobs write JUnit test results beside machine-readable build metadata.

## Risks

The workflow has not yet run on GitHub-hosted machines. It does not yet install
pluginval, the Steinberg validator, clang-format, clang-tidy, or a networked
dependency vulnerability scanner. Timing-sensitive research benchmarks are
disabled in shared CI; one production performance sanity render remains.

## Next step

Run the workflow on the remote repository, then add external validator and
analysis jobs only when their versions and licences are pinned.
