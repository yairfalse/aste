# Cycle 38 — Production Drive cascade

## What changed

Replaced the production Drive control's single 5 ms exponential smoother with
the retained two-stage 3+3 ms cascade. Parameter identity, range, state,
latency, and static-setting output are unchanged.

## Why

The full-graph candidate substantially reduced automation-boundary curvature
while settling sooner than the previous Drive response.

## Evidence

- Drive curvature excess improved from -48.366 to -79.139 dBFS.
- Simultaneous nine-parameter excess improved from -49.469 to -57.174 dBFS.
- The selected lab profile matches production with exactly zero sample delta.
- All four production golden fingerprints remain unchanged.
- Six-rate fixed and variable-block renders retain zero sample delta and their
  previous metric ranges and zero latency.
- Five CPU runs span 0.236635–0.241537% of one M4 Pro core; median is 0.239551%.
- Full Release and Address/UndefinedBehavior sanitizer matrices pass 23/23,
  including the calibrated zero-allocation/lock/file/write callback audit.
- pluginval strictness 10 passes all 78 requested rate/block combinations;
  Steinberg extensive validation passes 537/537.
- Release and sanitizer Drive/automation CSVs are byte-identical with hashes
  `69ee5df4a1e6d0fab55aad8dc36cc4b482e2709d59ca9d568654e096e57e7a4e`
  and `4cda40e841db3f5a8683addbb2cb24879e7557b4d48c18a52cfbd25ab4e24931`.

## Risks

Live Drive manipulation has not been auditioned. The overall automation gate
remains open: Attack now dominates at -53.086 dBFS, Blend measures -56.401
dBFS, and simultaneous automation remains 2.826 dB above the provisional
ceiling. Hardware evidence remains limited to the M4 Pro.

## Next step

Isolate Attack smoothing while preserving the displayed attack-time meaning;
compare boundary curvature with detector response lag before integrating
another change.
