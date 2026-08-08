# Cycle 56 — compiler-aware warning policy

## What changed

Made the standalone host's `-Wno-unnecessary-virtual-specifier` suppression
conditional on CMake proving that the active C++ compiler accepts it without a
diagnostic.

## Why

The first Cycle 55 hosted rerun made both core jobs green, then both jobs that
enable the VST3 host failed late in their build. The host was the only project
boundary carrying an unprobed compiler-specific option. That warning was added
to Clang in 2025, so an older compiler can reject the suppression itself under
the repository-wide `-Werror` policy.

Compiler-specific warning controls are capabilities, not stable language
features. CMake's built-in flag check preserves the suppression where needed
and omits it where unsupported.

## Evidence

- Hosted `Core / macos-15` and `Core / macos-15-intel` both pass build, test,
  and performance stages after the decimal parser fix.
- Both hosted VST3-enabled jobs progressed beyond the product code and failed
  in their build stage, isolating the remaining difference to the plugin/host
  build boundary.
- LLVM added `-Wunnecessary-virtual-specifier` in 2025:
  <https://github.com/llvm/llvm-project/pull/131188>.
- CMake's `check_cxx_compiler_flag` accepts the suppression with the installed
  compiler and both old-SDK VST3 configurations rebuild successfully.
- The macOS 15.4 SDK sanitizer VST3 build completes from a clean tree.

## Risks

Hosted logs require authenticated access, so the exact runner diagnostic could
not be retrieved from the public API. The cause is inferred from the failing
target boundary, compiler generation, and the recently introduced flag. The
next hosted run is therefore the decisive test; no wider warning relaxation
has been made.

## Next step

Push the capability probe and require the hosted universal VST3 and sanitizer
jobs to pass before accepting the diagnosis.
