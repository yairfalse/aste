# Combined instrument and research verification

This local contract verifies the integrated work from `improve/musical-control`
(including `research/musical-power-audit`), `research/n01-temporal-lab` and
`research/r01-resonators`. Published main history is preserved. No production DSP
or plugin target is changed by the integration itself; N-01 and R-01 remain
independent offline laboratories.

The gate runs the complete native plugin suite, the six production core suites
under AddressSanitizer/UndefinedBehaviorSanitizer, original legacy-audio
comparisons, click/sub checks, and both laboratories in Release and sanitized
Debug builds. The R-01 suite includes long stress. Every tracked integration
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
