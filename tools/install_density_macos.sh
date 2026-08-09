#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(dirname -- "$script_dir")
source_bundle=${1:-"$repository_dir/build-plugin-universal/DensityD01_artefacts/Release/VST3/Density D-01.vst3"}
plugin_dir=${2:-"${HOME:?}/Library/Audio/Plug-Ins/VST3"}
plugin_name="Density D-01.vst3"
executable="$source_bundle/Contents/MacOS/Density D-01"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "Density installer: macOS is required" >&2
  exit 1
fi
if [ ! -d "$source_bundle" ] || [ ! -f "$executable" ]; then
  echo "Density installer: VST3 bundle not found: $source_bundle" >&2
  exit 1
fi

codesign --verify --strict --deep "$source_bundle"
lipo "$executable" -verify_arch arm64 x86_64
mkdir -p "$plugin_dir"
stage_root=$(mktemp -d "$plugin_dir/.density-install.XXXXXX")
trap 'rm -rf -- "$stage_root"' EXIT HUP INT TERM
staged_bundle="$stage_root/$plugin_name"
ditto "$source_bundle" "$staged_bundle"

destination="$plugin_dir/$plugin_name"
if [ -e "$destination" ]; then
  backup="$plugin_dir/$plugin_name.backup-$(date +%Y%m%d-%H%M%S)-$$"
  mv "$destination" "$backup"
  echo "Density installer: previous bundle preserved at $backup"
fi
mv "$staged_bundle" "$destination"
codesign --verify --strict --deep "$destination"
lipo "$destination/Contents/MacOS/Density D-01" -verify_arch arm64 x86_64

echo "Density installer: installed $destination"
echo "Quit and reopen the DAW, then rescan VST3 plugins if Density is not listed."
