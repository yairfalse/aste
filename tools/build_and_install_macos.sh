#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(dirname -- "$script_dir")
build_dir=${1:-"$repository_dir/build-plugin-universal"}

if [ "$(uname -s)" != "Darwin" ]; then
  echo "Signal Instruments build: macOS is required" >&2
  exit 1
fi

cmake -S "$repository_dir" -B "$build_dir" \
  -DASTE_BUILD_VST3=ON \
  -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build "$build_dir" --parallel 3
ctest --test-dir "$build_dir" --output-on-failure

"$repository_dir/tools/install_density_macos.sh" \
  "$build_dir/DensityD01_artefacts/Release/VST3/Density D-01.vst3"
"$repository_dir/tools/install_harmonic_macos.sh" \
  "$build_dir/HarmonicH01_artefacts/Release/VST3/Harmonic H-01.vst3"
