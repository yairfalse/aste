# N-01 / R-01 research cycle 2 — evidence index

2026-11-15. Start with the [combined report](../../../reports/N01-R01%20research%20cycle%202.md).
This is offline research, not two newly delivered plugins or a listening verdict.

| Evidence | File |
|---|---|
| Exact source inputs, command arguments, platform, artifact hashes | [receipt](evidence/receipt.json) |
| N-01 20-variant raw measurements | [measurements](evidence/n01-measurements.csv) |
| N-01 articulation boundaries | [all edges](evidence/n01-retrigger-edges.csv), [active repeat summary](evidence/n01-articulation-summary.csv) |
| N-01 CPU / fixed memory | [60 benchmark rows](evidence/n01-benchmark.csv) |
| N-01 7 compiled and detected faults | [mutations](evidence/n01-mutations.csv) |
| R-01 32-variant raw measurements | [measurements](evidence/r01-measurements.csv) |
| R-01 CPU / modal and waveguide memory | [40 benchmark rows](evidence/r01-benchmark.csv) |
| R-01 tuning / restricted alias probe / long stress | [tuning](evidence/r01-tuning.csv), [alias](evidence/r01-alias.csv), [stress](evidence/r01-stress.csv) |
| R-01 10 compiled and detected faults | [mutations](evidence/r01-mutations.csv) |

Both independent pack renders were byte-identical before provenance/checksum
files were added. Both checksum manifests were verified afterward. N-01 and
R-01 each passed their Release and ASan/UBSan suites (2 and 3 tests respectively).
Existing-output refusal was tested. Logs and the 104 raw/matched WAVs remain in
`build-research-cycle2-20261115/`; ZIPs and the corresponding source archive are
there too. They are ignored outputs, not silently committed binary releases.

Reproduce using [the isolated runner](../../../research/integration/reproduce_labs.py)
and the command in the report. Use a new output directory; blank or completed
listener responses are never overwritten. Do not interpret a matching raw
fingerprint as a portable floating-point guarantee across compilers/CPUs.

The old [N-01 evidence](../n01/EVIDENCE.md) and [R-01 evidence](../r01/EVIDENCE.md)
retain their historical provenance and conclusions. The current report clearly
distinguishes old/current product references and new experiments. Native UI,
Cubase, accessibility and human musical evaluation remain unperformed.
