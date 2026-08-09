# Loop cycle 02 — generational tape instrument

## What changed

Replaced the single circular memory with three preallocated tape decks. Added
RELOOP printing, cumulative record/reproduce loss, three-speed tape calibration,
generation navigation, sample-offset MIDI performance actions, schema-1
migration, and a panel centered on the three tape paths and generation state.

## Why

The first prototype could capture and transform audio but did not reward
repetition. Printing the processed result into successive generations gives
Loop one clear musical gesture: loop, transform, reloop, and degrade again.

## Evidence

Core tests prove generation creation, bounded three-deck retention, previous
navigation, finite output, variable-block capture, zero latency, and no process
allocation. Adapter tests prove 20-parameter schema-2 recall, schema-1 migration,
sample-offset MIDI capture and RELOOP, lock-free UI requests, editor visibility,
and no allocation during printing. The complete 61-test universal suite passes;
ASan/UBSan and static analysis are clean. Continuous worst-case printing
measured a 0.526622% five-run median of one M4 Pro performance core at 48 kHz /
127 samples, below the provisional 1% budget.

## Risks

The model has not completed DAW listening tests. Printed pitch artifacts,
generation level growth, branch behavior, retrigger expectations, and tape-speed
calibration need musical evidence. Captured audio and generations still do not
survive session reload.

## Next step

Run the nine-generation music-machine protocol and revise only behavior that
produces a repeatable musical failure.
