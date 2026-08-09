# Cycle 65 — specification traceability

## What changed

Added one authoritative requirement-to-evidence ledger for Density and the
shared foundation. It distinguishes implemented behavior, accepted product
decisions, later-product scope, external validation, and four locally closable
gaps.

## Why

The master brief spans DSP, UI, research, real-time safety, CI, delivery, and
work that cannot be performed by repository automation. A release-gate list
alone could not show whether an omitted feature was intentional or forgotten.

## Evidence

- The documentation test now requires the ledger and validates its links.
- Every first-milestone Density subsystem maps to code or an automated gate.
- The four local gaps are presets, formatting, static analysis, and immutable
  workflow-action pins.
- DAW, hardware, listening, identity, notarization, and project gates remain
  explicitly external rather than being converted into paper passes.

## Risks

Traceability is only as accurate as its referenced evidence. Any new product
requirement must update this ledger in the same change; a completed local row
does not waive its external release counterpart.

## Next step

Close the three CI and supply-chain gaps, then add the smallest useful Density
preset surface without a browser, database, or new state format.
