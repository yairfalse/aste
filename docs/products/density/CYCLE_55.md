# Cycle 55 — macOS 15 parser portability

## What changed

Replaced floating-point `std::from_chars` at the two state and fixture parsing
boundaries with one strict, classic-locale decimal parser. Integer and hash
fields continue to use `std::from_chars`. Added focused tests for scientific
notation and rejection of whitespace, trailing data, locale commas, non-finite
values, and overflow.

## Why

The first two hosted CI runs failed while compiling every job. A clean build
against the macOS 15.4 SDK reproduced the cause: that libc++ version deletes
the floating-point `std::from_chars` overload. The newer local SDK had hidden
the deployment-environment incompatibility.

The replacement is intentionally confined to non-real-time text parsing. It
keeps saved-state and golden-fixture input deterministic without adding a
dependency or weakening validation.

## Evidence

- A clean macOS 15.4 SDK core build passes 29/29 tests.
- A clean macOS 15.4 SDK VST3 build passes 32/32 tests.
- Native arm64 Release, arm64 ASan/UBSan, and x86_64 Release matrices each pass
  32/32 tests.
- A clean macOS 15.4 SDK universal VST3 build passes 32/32 tests.
- `codesign --verify --strict --deep` accepts the universal bundle.
- `lipo -verify_arch arm64 x86_64` accepts the plugin executable.
- `git diff --check` reports no whitespace errors.

## Risks

`std::istringstream` may allocate and is slower than `std::from_chars`. Both
call sites are state or laboratory parsing boundaries and never run in the
audio callback, so this does not weaken the real-time contract. The local
compatibility build combines the older SDK with the installed compiler; the
hosted runner remains the authoritative check for its complete toolchain.

## Next step

Push the fix and require all four hosted CI jobs to pass. If the runner exposes
another compiler-specific failure, reproduce it locally before changing code.
