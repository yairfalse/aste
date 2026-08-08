# Aste Signal Instruments

`Aste` is a neutral internal codename for a family of four professional audio
instruments. It is not a company name and must not become a public namespace,
bundle identifier, preset signature, or installer path.

The current Density D-01 prototype contains a framework-independent DSP core,
offline measurement and benchmark modes, focused tests, and an opt-in VST3.

Engineering policy and current priorities are indexed by
[ARCHITECTURE.md](ARCHITECTURE.md), [ROADMAP.md](ROADMAP.md),
[TESTING.md](TESTING.md), and [RELEASE_CHECKLIST.md](RELEASE_CHECKLIST.md).

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
./build/density_lab density.csv
./build/density_lab --benchmark
./build/density_lab --detector-compare detector-comparison.csv
./build/density_lab --detector-auditions detector-auditions
./build/density_lab --detector-blind detector-blind
./build/density_lab --golden density-golden
./build/density_lab --production-consistency density-consistency.csv
./build/density_lab --alias-report density-alias.csv
./build/density_lab --oversampling-report density-oversampling.csv
./build/density_lab --oversampling-prototype density-oversampling-prototype.csv
./build/density_lab --halfband-prototype density-halfband-prototype.csv
./build/density_lab --kaiser-prototype density-kaiser-prototype.csv
./build/density_lab --kaiser-sweep density-kaiser-sweep.csv
./build/density_lab --kaiser-rate-sweep density-kaiser-rate-sweep.csv
./build/density_lab --kaiser-linear-report density-kaiser-linear.csv
./build/density_lab --kaiser-length-report density-kaiser-length.csv
./build/density_lab --kaiser-finalist-report density-kaiser-finalists.csv
./build/density_lab --oversampled-chain-report density-oversampled-chain.csv
./build/density_lab --oversampling-auditions density-oversampling-auditions
./build/density_lab --automation-report density-automation.csv
./build/density_lab --output-smoothing-report density-output-smoothing.csv
./build/density_lab --drive-smoothing-report density-drive-smoothing.csv
./build/density_lab --attack-smoothing-report density-attack-smoothing.csv
./build/density_lab --blend-smoothing-report density-blend-smoothing.csv
./build/density_lab --automation-auditions density-automation-auditions
./build/density_lab --stereo-stability-report density-stereo-stability.csv
./build/density_lab --stereo-stability-auditions density-stereo-stability-auditions
./build/density_lab --density-macro-auditions density-macro-auditions
```

The CSV renderer uses generated signals and performs no file or heap work in
the processor callback.

The internal-only VST3 prototype is opt-in because it fetches licensed JUCE
source:

```sh
cmake -S . -B build-plugin -DASTE_BUILD_VST3=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build-plugin --target DensityD01_VST3 density_plugin_tests \
  density_vst3_host
ctest --test-dir build-plugin --output-on-failure
```

The `density_vst3_host` CTest loads the built bundle through its VST3 ABI; it
does not link the Density adapter or DSP implementation.

An internal universal bundle can be built without changing product state:

```sh
cmake -S . -B build-plugin-universal -DASTE_BUILD_VST3=ON \
  -DCMAKE_BUILD_TYPE=Release '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build-plugin-universal --target DensityD01_VST3
```
