# ADR 0008: Sequence voice and clock

- Status: accepted for internal beta
- Date: 2026-08-09

## Decision

Sequence uses a product-local monophonic dual-polyBLEP oscillator voice, a
continuous parallel morph between a two-pole state-variable low-pass and a
bounded four-pole ladder-informed low-pass, one ADSR, and a sixteen-step program
indexed directly from host PPQ. MIDI notes transpose the running program or play
the voice when the program is inactive.

## Rationale

This is the smallest architecture that supplies the requested broad oscillator
voice, two materially different filter behaviors, bass-sequencer immediacy, and
easy visible programming. A continuous morph avoids stateful topology changes.
Absolute host PPQ makes restart and recall deterministic without a speculative
shared transport framework.

## Rejected for this cycle

Polyphony, a modulation matrix, circuit-level solver, internal clock, swing,
dynamic oversampling, and shared sequencer/filter libraries add cost before a
second measured consumer or musical failure exists.

## Claims

The voice is topology-informed and original. It is not an emulation of the
patents or any named synthesizer. Historical documents are link-only research;
production behavior is defined by source and measurements.
