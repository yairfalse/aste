# Contributing

Density is the only implementation priority until its release gates close.
Keep changes product-local unless a second real consumer or a foundational
correctness requirement justifies sharing.

Before submitting a change:

1. Explain the musical or engineering failure it addresses.
2. Keep UI, state, and DSP ownership separate.
3. Add the smallest deterministic check that would catch a regression.
4. For DSP changes, attach before/after measurements and listening notes.
5. Run `cmake --build build-plugin` and
   `ctest --test-dir build-plugin --output-on-failure`.
6. Run the sanitizer build for processing, state, or lifecycle changes.
7. Update the applicable cycle report and never silently replace golden data.

Audio callbacks may not allocate, lock, access files or networks, log, throw,
or perform lazy initialization. Parameter IDs and state meaning are release
contracts; changing either requires a migration and an ADR.

Use C++20, repository formatting, strict warnings, and no new dependency without
licence and maintenance review. Do not include third-party manuals, proprietary
code, firmware, sample libraries, or unsupported fidelity claims.
