# Cycle 68 — specification-complete internal beta

## What changed

Added a compact accessible `PRESETS` header menu with Default and four
product-local starting points: Continuity, Proximity, Parallel Crush, and
Transient Hold. Each action updates the existing eleven stable parameters and
then returns the menu to its neutral label. No preset browser, filesystem,
database, background worker, state field, or audio-thread behavior was added.

The specification ledger now has zero local `Open` rows and declares Density
specification-complete for internal beta, without changing any external release
gate.

## Why

The shared interaction brief asks for a compact fast preset menu, while the
architecture forbids speculative preset infrastructure. Parameter snapshots
provide the smallest complete behavior and inherit the already-fuzzed portable
state format.

## Evidence

- Five non-empty preset names and all values are tested.
- Every loaded value is finite and within its parameter range.
- Every preset serializes to deterministic schema 1 state.
- Invalid indices leave state bytes unchanged.
- The menu is present, keyboard reachable, accessibility exposed, and included
  in deterministic focus order and editor rendering.
- The full 33-test VST3 suite, static analysis, formatting, and validators must
  pass for this declaration.

## Risks

The starting points are engineering defaults, not listening-approved factory
content. Their musical suitability and naming remain part of external blind and
real-project validation. Loading a preset intentionally creates host-visible
parameter changes; hosts decide how those changes interact with automation
write modes.

## Next step

Freeze the internal-beta artifact and execute the external validation packet:
target DAWs, native hardware, blind listening, usability, and ten projects.
