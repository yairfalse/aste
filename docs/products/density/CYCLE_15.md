# Cycle 15 — Audio-thread forbidden-call audit

## What changed

Added a macOS-only test dynamic library that interposes twelve lock, file-open,
and write APIs. The existing full-adapter lifecycle test activates the audit
only for its callback thread and requires zero calls during `processBlock`.

## Why

Source inspection and allocation counting do not detect a mutex hidden in a
framework call or accidental file/log output. A dependent test library can
observe those dynamic calls without modifying production processing code.

## Evidence

- The audit deliberately locks a mutex, opens `/dev/null`, and performs a
  zero-byte write; the test fails unless all three categories are observed.
- The active guard is bound to the audited `pthread_t`, preventing unrelated
  JUCE threads from creating false allocation or forbidden-call failures.
- Twenty consecutive Release executions cover 17,920 adapter callback calls
  with zero counted allocations, audited locks, file opens, or direct writes.
- The normal Release and ASan/UBSan builds each pass all six CTest checks.

## Risks

The audit is macOS-specific and covers named dynamic APIs, not direct syscalls,
network APIs, semaphores, Mach waits, memory mapping, Objective-C allocation,
or priority inversion. Dyld interposition is a test mechanism, not product code.

## Next step

Add deterministic malformed-state fuzz coverage at the full plugin boundary;
retain exact failure bytes for every discovered crash or invalid restoration.
