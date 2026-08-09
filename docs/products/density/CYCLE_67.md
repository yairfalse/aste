# Cycle 67 — formatting and static analysis

## What changed

Defined the repository C++ style, normalized all ten tracked C++ source files,
and added check-only formatting to CI. Added a Clang path-sensitive analysis
target for the framework-independent production Density DSP and made it a CI
gate on both macOS architectures.

## Why

Warnings-as-errors catch compiler diagnostics but do not replace consistent
source formatting or path-sensitive analysis. Both gates now use the Apple
developer tools already required by the macOS build instead of adding another
dependency.

## Evidence

- `clang-format --dry-run --Werror` accepts every tracked `.cpp` and `.hpp`.
- `density_static_analysis` completes with no diagnostics.
- Release builds and the full 33-test suite pass after the mechanical rewrite.

## Risks

The analyzer target covers the product-owned DSP translation unit, not JUCE
internals. Adapter-specific defects remain covered by strict compilation,
sanitizers, fuzzing, two validators, and the standalone VST3 host. Formatter
output may change when the Xcode toolchain changes, so upgrades require review.

## Next step

Implement and test the compact Density preset menu, the final local `Open` row
before specification-complete internal beta.
