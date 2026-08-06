# Aste Signal Instruments

`Aste` is a neutral internal codename for a family of four professional audio
instruments. It is not a company name and must not become a public namespace,
bundle identifier, preset signature, or installer path.

The current Density D-01 prototype contains a framework-independent DSP core,
offline measurement and benchmark modes, focused tests, and an opt-in VST3.

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/density_lab density.csv
./build/density_lab --benchmark
./build/density_lab --detector-compare detector-comparison.csv
./build/density_lab --detector-auditions detector-auditions
./build/density_lab --detector-blind detector-blind
```

The CSV renderer uses generated signals and performs no file or heap work in
the processor callback.

The internal-only VST3 prototype is opt-in because it fetches licensed JUCE
source:

```sh
cmake -S . -B build-plugin -DASTE_BUILD_VST3=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-plugin --target DensityD01_VST3 density_plugin_tests
ctest --test-dir build-plugin --output-on-failure
```
