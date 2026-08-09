# Harmonic H-01 — product and research specification

Status: internal-beta implementation contract, 2026-08-09. ADR 0007 promotes
Candidate 3 into the first testable four-band VST3. Parameter identifiers and
schema 1 are frozen; host compatibility and musical preference remain unproven.

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

Measurements, rejected approaches, and topology decisions are retained in
[DSP_RESEARCH.md](DSP_RESEARCH.md).

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

### Candidate 2 advance gate

Before rendering the pre-emphasis candidate, its lab-only advance gate is fixed
as follows:

- test 80, 400, 1 kHz, 4 kHz, and 12 kHz centers at every supported sample
  rate;
- test 0.1, 0.25, 0.5, and 0.9 peak input levels;
- preserve the requested center gain within 0.5 dB through 0.5 peak input;
- preserve center phase within 1 degree of the linear reference;
- increase H3 monotonically at the canonical 1 kHz / 0.5-peak case;
- keep full-macro canonical H3 between -80 and -20 dBc;
- leave equal-magnitude cuts below -140 dBc H3;
- remain finite at 0.9 peak and report, but do not conceal, overload gain loss;
- report folded-harmonic and SMPTE/CCIF IMD proxies without inventing a quality
  ceiling before comparison and listening evidence exists.

Passing this gate retains a candidate for broader testing; it does not select a
production topology or authorize parameter IDs. A harmonic bin that aliases
exactly onto the test fundamental is marked unobservable and excluded from that
row's H3 gate; it is not reported as clean or silently treated as a failure.

### Candidate 3 advance gate

Before rendering the stateful candidate, its lab graph and advance gate are
fixed as follows:

- use a topology-preserving state-variable band section at Q 0.9;
- interpolate its band-integrator state continuously from linear to bounded
  `tanh` behavior while Harmonic moves from 0 to 1;
- move the bounded-stage drive continuously from 1 to 3;
- add eight percent of the nonlinear-minus-linear band state to the unchanged
  proportional-Q contour at full Harmonic;
- apply the same centers, levels, gain, phase, macro-H3, cut, collision, folded
  harmonic, and two-tone protocol as Candidate 2;
- require exact neutral silence, finite impulse and overload output, and a
  maximum tail below -120 dBFS during the final 100 ms after two seconds of
  silent recovery from a 0.9-peak, 100 ms centered sine;
- report maximum impulse and overload peak without inventing a preferred
  ceiling before comparison evidence exists.

Passing retains the topology for direct comparison with Candidate 2. It does
not select the state-variable structure, its Q, the eight-percent mapping, or
any anti-alias strategy for production.

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

1. Candidate 3 is provisionally selected for an external musical test build.
2. Four bands, continuous constrained ranges, parameter IDs, schema 1, and
   zero latency are frozen by ADR 0007.
3. Implement the product-owned core, adapter, UI, and automated tests.
4. Run validators and install the universal bundle on the music machine.
5. Use real projects and level-matched comparisons to accept, revise, or reject
   the provisional topology.
6. Complete multi-band, automation, CPU, and host gates before release-candidate
   status.

Density and Harmonic external validation now proceed together. Harmonic's
production topology remains provisional until its Cubase/Ableton, state-recall,
automation, bounce, level-matched listening, and real-project checks are
recorded.
