# ADR 0012: Sequence shared-state character filter

- Status: accepted for internal beta
- Date: 2026-08-10

## Decision

Sequence uses one nonlinear four-stage low-pass state graph. Filter Weight
interpolates its second- and fourth-stage outputs with bounded resonance/body
compensation. Filter Drive controls input loading. The waveform control spans
saw, pulse, and sine continuously, with a separate pulse-width parameter.

## Why

The original Filter Form crossfaded two unrelated filters running in parallel.
Their different phase and gain responses could reduce body at intermediate
positions, and the control described implementation choice rather than a stable
musical phenomenon. A shared state makes the response continuous without
topology changes and gives Weight one legible direction: open to heavy.

## Evidence and limits

The processor test renders Weight at 0%, 50%, and 100%, requires every endpoint
to remain finite and audible, rejects a destructive midpoint RMS collapse, and
requires materially different endpoints. Six sample rates, irregular blocks,
deterministic reset, zero allocation, schema migration, editor visibility, and
VST3 loading remain covered. This is an original behavioral filter, not a
component model or named synthesizer emulation.

