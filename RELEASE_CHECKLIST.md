# Density D-01 release checklist

Status as of 2026-08-08 for the internal universal VST3: **16 passed, 9 pending**.
`Pass` means repository evidence exists; it does not widen the supported-host
or hardware claim.

## Gate ledger

| # | Release gate | Status | Evidence or closing action |
|---:|---|---|---|
| 1 | Loads in Cubase and Ableton | Pending | Neither host is installed; run the matrix in [HOST_COMPATIBILITY.md](HOST_COMPATIBILITY.md). |
| 2 | Passes plugin validation | Pass | CI-enforced Steinberg extensive validation plus reviewed pluginval strictness 10 and 537/537 Steinberg results on arm64, x86_64, and the universal bundle. |
| 3 | Restores state exactly | Pass | Deterministic round-trip and 3,072 malformed-state cases. |
| 4 | Dry path is phase aligned | Pass | Production branches measure zero samples; the lab-only prototype aligns both at 44. |
| 5 | Reported latency matches measured | Pass | Production reports and measures zero at every supported rate. |
| 6 | No audio-thread allocations | Pass | Full adapter audit covers 896 lifecycle calls per run. |
| 7 | No audio-thread locks | Pass | Calibrated macOS interposition audit records zero callback locks. |
| 8 | Fuzzed output remains finite | Pass | State, parameter, lifecycle, rate, and block-size cases remain finite. |
| 9 | Automation has no avoidable zipper noise | Pending | All curvature cases pass; the four-pair blind automation pack is generated and awaits completed responses. |
| 10 | All supported sample rates pass | Pass | 44.1 through 192 kHz pass internal, pluginval, and Steinberg matrices. |
| 11 | Variable block sizes pass | Pass | Thirteen nonzero sizes plus zero-length lifecycle calls. |
| 12 | Mono and stereo pass | Pass | Full adapter, allocation, and validator coverage. |
| 13 | Offline and real-time renders agree | Pending | Requires a target DAW bounce/freeze comparison. |
| 14 | CPU budgets are met | Pending | Production passes on M4 Pro and x86_64 executes under Rosetta; oldest supported Apple Silicon and native Intel remain unmeasured. |
| 15 | Golden changes are reviewed | Pass | Tracked production manifest cannot be updated by CTest. |
| 16 | Scaling and Retina rendering pass | Pending | Headless 1x/2x artifacts pass; native Retina host transfer remains. |
| 17 | Density macro is musically monotonic | Pending | Internal and gain-reduction mappings are monotonic; the five-trial blind ranking pack awaits completed responses. |
| 18 | Stereo linking avoids bad image movement | Pending | Objective invariants pass; the six-trial blind A/B/C pack is generated and awaits completed responses. |
| 19 | Clipping and protection are documented | Pass | Protection is explicitly sample-peak only, not a true-peak limiter. |
| 20 | Fidelity claims have measurements | Pass | No emulation claim is made; current nonlinear and latency claims have reports. |
| 21 | Operable without a manual | Pending | Exact entry, reset, accessibility, and explicit keyboard order pass structurally; independent usability observation remains. |
| 22 | Distinct visual identity | Pass | Reviewed Density-first 1960×1080 baseline uses the shared industrial language. |
| 23 | Does not resemble a hardware clone | Pass | Original control hierarchy, graph, naming, and non-skeuomorphic panel. |
| 24 | Ten real musical projects tested | Pending | No real-project count is recorded. |
| 25 | Known limitations are documented | Pass | Aliasing, sample-peak protection, native-Intel/DAW gaps, CPU scope, and missing hosts are explicit. |

## What changed

Created one evidence-linked ledger for all 25 Density release gates and
revalidated the current VST3 artifact with both available validators.

## Why

Release readiness was scattered across cycle reports. The ledger distinguishes
automated evidence from listening, hardware, and DAW work without inventing new
infrastructure.

## Evidence

- Current pluginval 1.0.4 run passes strictness 10 with seed `0xd01`.
- All 78 requested sample-rate/block-size combinations execute.
- Steinberg VST3 SDK 3.8.0 extensive validation passes 537/537 tests.
- Every gate maps to existing evidence or one concrete closing action.

## Risks

Sixteen passes are internal prototype evidence, not a release claim. Six of
the remaining gates need external DAWs, hardware, Retina transfer, usability
observation, or real projects; three need controlled listening.

## Next step

Complete the automation, Density-macro, and stereo-stability response sheets
without opening their answer keys, then record the listening results.
