# Combined instrument and research verification

This local contract verifies the integrated work from `improve/musical-control`,
`research/musical-power-audit`, `research/n01-temporal-lab` and
`research/r01-resonators`. Published main history is preserved. No production DSP
or plugin target is changed by the integration itself; N-01 and R-01 remain
independent offline laboratories.

The gate runs the complete native plugin suite, the six production core suites
under AddressSanitizer/UndefinedBehaviorSanitizer, original legacy-audio
comparisons, click/sub checks, the original audit's measurement/candidate checks,
and both new-instrument laboratories in Release and sanitized Debug builds.
The R-01 suite includes long stress. The historical audit uses pinned original
DSP and retains its intentionally failing host-defect witness; that is not a
test of current production or an inverted passing assertion. Every tracked integration
input is declared, including this contract and its lock.

```sh
mkdir -p build-integration-evidence
sykli run research/integration/sykli.json --json > build-integration-evidence/receipt.json
sykli verify build-integration-evidence/receipt.json --contract research/integration/sykli.json --commit HEAD
```

Like the existing playability contract, this local contract uses the already
cached pinned JUCE checkout on the development Mac, and verifies its revision
and cleanliness. On another machine, point it at that machine's pinned checkout
and re-lock before running. This does not add hosted CI or a dependency.

Passing establishes the enumerated checks, not a Cubase session, human listening
preference, native accessibility approval or a release. Historical WAV packs,
reports and receipts keep their original provenance; regenerate into new
directories rather than overwriting them.

## Isolated research evidence without Sykli

`reproduce_labs.py` builds the two offline labs only, in Release and ASan/UBSan,
renders/compares two complete packs, validates WAVs and checksums, optionally
runs compiled fault injection, then benchmarks sequentially. It records the
actual source hashes (including uncommitted product comparison processors),
provides matching source, and refuses existing output. See the
[cycle-2 report](../../reports/N01-R01%20research%20cycle%202.md). This separate
runner does not certify the six-plugin build, packaging or live DAW behavior.
