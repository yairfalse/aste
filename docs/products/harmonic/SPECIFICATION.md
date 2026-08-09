# Harmonic H-01 — product and research specification

Status: pre-implementation specification, 2026-08-09. Harmonic has no VST3
target, production DSP, stable parameter identifiers, presets, or compatibility
claim yet.

## Purpose

Harmonic is a broad musical equalizer in which spectral shaping and nonlinear
behavior are one operation. It should make selected regions feel larger,
denser, clearer, or more distant without presenting circuit controls. Boost and
cut are intentionally different gestures:

- boost may broaden, generate harmonics, and interact with neighboring bands;
- cut should remain cleaner, tighter, and more predictable;
- zero must be a trustworthy neutral position;
- stronger settings may become audible as an instrument, but never unstable.

It is not a Pultec, Neve, API, console, or passive-EQ clone. Historical circuits
may inform proportional-Q behavior, headroom, loading, and feedback, but no
named product topology or panel is a target.

## Non-goals

- Surgical correction, spectrum matching, dynamic EQ, and linear phase.
- Per-band oversampling or quality controls before measurements justify them.
- Analyzer animation, hidden band pages, or unlimited frequency/Q controls.
- Noise, drift, transformer labels, or “analog warmth” without an identified
  behavior and a bypassable musical purpose.
- A shared EQ or nonlinear framework created before a second real consumer.

## Product behavior

The initial research target is four broad overlapping regions: Foundation,
Body, Presence, and Air. Names describe audible regions, not circuit sections.
Each region has a constrained frequency choice and one bipolar contour control.
The final number of regions, frequency steps, and ranges remain uncommitted
until the listening and measurement programme is complete.

Candidate global controls are Input, Harmonic, Bias, Compensation, and Output.
`Harmonic` is the candidate hero macro: clockwise motion must monotonically
increase audible nonlinear participation on boosted material without changing
the meaning of band gain. `Bias` may move continuously between predominantly
odd and even behavior only if level and DC remain controlled. Compensation must
be optional and must never disguise a large output change as improved quality.

No stable host parameter ID is reserved by this document. IDs become permanent
only after the winning topology, useful ranges, text conversion, smoothing, and
automation behavior are tested together.

## Candidate signal graphs

At least four distinct approaches must enter the DSP lab:

1. **Linear reference** — minimum-phase proportional-Q filters with no
   nonlinearity. This is the null and gain/phase reference, not a product
   candidate by itself.
2. **Band residual excitation** — derive a broad band residual, process only
   its boosted contribution nonlinearly, then recombine it with the main path.
3. **Pre-emphasis / nonlinear stage / de-emphasis** — use the contour to drive
   selected frequencies harder into one identifiable nonlinear stage.
4. **Stateful nonlinear filter** — place a bounded nonlinearity inside or around
   the filter feedback path so bandwidth and harmonics interact dynamically.

A serial stack and a parallel split/recombine version must both be evaluated.
Serial processing offers meaningful band interaction but is order-dependent;
parallel processing offers clearer ownership but risks phase and recombination
errors. The first implementation does not win by existing first.

The initial product target is zero algorithmic latency. Lookahead, linear phase,
and dynamic oversampling require an ADR, measured benefit, dry/processed timing
evidence, and correct host latency reporting.

## Required DSP experiments

Every candidate must render machine-readable results at all six supported
sample rates and multiple drive levels:

- magnitude and phase for every band, gain, and frequency step;
- boost/cut bandwidth and Q to prove intentional asymmetry;
- harmonic spectrum by input frequency and level;
- SMPTE and CCIF-style intermodulation fixtures;
- two-band and four-band interaction matrices;
- impulse, step, overload, and recovery responses;
- DC offset and low-frequency stability under asymmetry;
- alias energy before and after any anti-alias strategy;
- neutral-state null, gain, phase, latency, and deterministic reset;
- variable-block identity and aggressive automation curvature;
- five-run stereo CPU and fixed processing-state memory.

The lab must compare the production candidate with a double-precision or slower
reference where one exists. Exact sample hashes are supplemental; magnitude,
phase, harmonic, intermodulation, latency, and perceptual tolerances are the
decision evidence.

## Acceptance gates for a production topology

A candidate may enter product code only when all of these are true:

- neutral settings null to the linear reference within a documented tolerance;
- finite input always produces finite bounded output;
- boost grows monotonically in level and nonlinear participation;
- cut remains measurably cleaner than equal-magnitude boost;
- adjacent-band interaction is repeatable and does not create unexpected gain;
- no parameter step changes topology or emits an avoidable discontinuity;
- phase and latency remain consistent across block schedules;
- alias measurements and blind listening support the selected anti-alias cost;
- at least three musical fixture classes prefer or reliably distinguish the
  behavior without a severe failure in the remaining classes;
- the complete default graph meets a provisional 1% single-core budget at
  48 kHz / 128 samples on the oldest supported Apple Silicon and Intel targets.

Numerical thresholds for null depth, gain error, aliasing, and automation are
research outputs, not invented marketing numbers. They must be frozen before
the candidate becomes production DSP.

## UI direction

Harmonic uses the family matte-black surface, off-white typography, and a warm
ochre or bronze functional accent. Four visible vertical regions form the main
surface. Band contour is large; constrained frequency selection is immediately
adjacent. Harmonic and Output remain globally legible. Input/output meters show
signal boundaries; motion is limited to level and nonlinear activity.

The first panel target is the same carefully scalable footprint class as
Density, with generous hit targets, exact entry, reset, keyboard order, and
accessibility metadata. No essential control may live on another page.

## Ownership and reuse

Harmonic owns its filters, signal graph, parameter layout, schema, UI, fixtures,
goldens, and sonic decisions. It may reuse Density's proven engineering policy
and, only when code exists, extract genuinely identical foundational behavior
for parameter text parsing, smoothing, state migration, metering, validation,
and installation. It may not link against Density or reuse Density's processor.

An amplifier subsystem is not a prerequisite. A shared amplifier component may
be extracted only after Harmonic and another product require the same measured
behavior.

## Implementation sequence

1. Add four framework-independent lab candidates and scientific references.
2. Render the complete measurement matrix and record negative results.
3. Generate level-matched blind packs before selecting a topology.
4. Freeze band count, steps, ranges, parameter IDs, latency, and CPU budget.
5. Implement the product-owned core and automated tests.
6. Add state schema and thin JUCE VST3 boundary.
7. Build the industrial UI only around the frozen behavior.
8. Run validators, target DAWs, listening, and real-project gates.

Density external validation may proceed in parallel with Harmonic lab research.
Harmonic production VST3 work waits until Density has at least passed initial
Cubase and Ableton load, state-recall, automation, and bounce checks.
