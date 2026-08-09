# Cycle 66 — immutable workflow actions

## What changed

Replaced all four floating `actions/checkout@v7` references with the exact
official v7 commit and extended repository policy to reject any remote action
that is not pinned by a full 40-character Git SHA.

## Why

A mutable tag can change the executable code that runs with CI access while the
repository commit remains unchanged. The readable version comment preserves
upgrade intent; the SHA preserves reproducibility.

## Evidence

- GitHub's official API resolved v7 to
  `3d3c42e5aac5ba805825da76410c181273ba90b1` on 2026-08-09.
- `python3 tools/check_docs.py .` accepts the pinned workflow.
- A floating tag fixture is rejected by the same policy parser.

## Risks

Pins do not make action code trustworthy and do not freeze GitHub runner images.
Action upgrades now require an explicit source review and SHA update.

## Next step

Add reproducible source formatting and static-analysis gates without adding a
third-party formatting or analysis dependency.
