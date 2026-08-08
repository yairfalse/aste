# Cycle 16 — Deterministic state restoration fuzzing

## What changed

Added 3,072 fixed-seed cases at the complete `setStateInformation` boundary.
The corpus combines arbitrary byte blocks, valid-state truncation, bit flips,
appended data, and valid binary XML with hostile structural and scalar changes.

## Why

Handwritten malformed-state examples cover known mistakes. Deterministic
mutation exercises JUCE's binary XML boundary and Density's validation logic
without making failures irreproducible or adding a fuzzing dependency.

## Evidence

- 2,048 byte-level cases cover random data from zero to 2,048 bytes, valid-state
  truncation, one-to-eight bit flips, and random trailing data.
- 1,024 structured cases cover wrong schema/product/root, unknown and missing
  IDs, duplicate children, missing values, non-finite text, and extreme values.
- Every case leaves all eleven parameters finite and within their ranges.
- Consecutive state serializations are byte-identical after every case.
- Reset renders are sample-identical and finite after every case.
- Release and ASan/UBSan plugin builds each pass all six CTest checks.
- Failures report seed, case number, reason, and complete input bytes in hex.

## Risks

This is a bounded deterministic corpus, not coverage-guided fuzzing. It does
not exercise huge memory-pressure inputs, concurrent host misuse, or every
behavior inside JUCE's XML parser. State parsing remains an off-audio-thread
operation.

## Next step

Create the first reviewed production-DSP golden render and metric manifest;
updates must remain explicit rather than automatic.
