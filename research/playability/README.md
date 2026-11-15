# Playability regression and listening reproduction

This extends the existing labs rather than adding a DSP framework or dependency.
Run in the repository root. Use new output directories: listening exports refuse
to overwrite existing responses.

```sh
sykli run research/playability/sykli.json --json > build-playability-receipt.json
sykli verify build-playability-receipt.json --contract research/playability/sykli.json --commit HEAD
cmake -S research/quality -B build-playability-lab -DCMAKE_BUILD_TYPE=Release
cmake --build build-playability-lab --parallel 3
build-playability-lab/quality_comparison --check
build-playability-lab/quality_comparison --playability-pack build-listening/open-starts
build-playability-lab/quality_comparison --pack build-listening/six-product-comparisons
build-playability-lab/quality_comparison --playability-spectral build-pulse-spectral.csv
cmake -S research/click-sub -B build-click-sub -DCMAKE_BUILD_TYPE=Release
cmake --build build-click-sub --parallel 3
build-click-sub/click_sub --check
build-click-sub/click_sub --pack build-listening/click-sub
rustc --edition=2021 -D warnings -O research/re-audit/inspect.rs -o build-inspect
build-inspect scan build-listening
```

The source commit embedded by the comparison renderer is captured at configure
time; configure from clean source before creating a deliverable. The current
532-file pack is stereo float32, 48 kHz: eight-second comparisons and four-second
isolated click/sub objects. Those isolated objects retain actual levels; use
the supplied scene comparisons for matching.
Unknown loudness stays unknown for the silent Overdub endpoint.

`--check-fault` deliberately corrupts a legacy comparison and must exit 1.
The pre-fix regression outputs are retained in the study's evidence directory.
The independent inspector's negative controls remain documented in
[the second audit](../re-audit/README.md). Run callback benchmarks separately,
without another Aste build/render/test job, using `--benchmark CSV`; they use
the historical extreme profiles, not the new starting presets.

The [report](../../docs/research/playability/README.md) distinguishes repaired
defects from retained architectural questions and pending human evaluation.
Do not infer a Cubase or release pass from the standalone VST3 host checks.
