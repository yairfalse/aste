#!/bin/sh
set -eu

script_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
repository_dir=$(dirname -- "$script_dir")
source_bundle=${1:-"$repository_dir/build-plugin-universal/LoopL01_artefacts/Release/VST3/Loop L-01.vst3"}
plugin_dir=${2:-"${HOME:?}/Library/Audio/Plug-Ins/VST3"}
plugin_name="Loop L-01.vst3"
executable="$source_bundle/Contents/MacOS/Loop L-01"

if [ "$(uname -s)" != "Darwin" ]; then
  echo "Loop installer: macOS is required" >&2
  exit 1
fi
if [ ! -d "$source_bundle" ] || [ ! -f "$executable" ]; then
  echo "Loop installer: VST3 bundle not found: $source_bundle" >&2
  exit 1
fi

codesign --verify --strict --deep "$source_bundle"
lipo "$executable" -verify_arch arm64 x86_64
mkdir -p "$plugin_dir"
stage_root=$(mktemp -d "$plugin_dir/.loop-install.XXXXXX")
trap 'rm -rf -- "$stage_root"' EXIT HUP INT TERM
staged_bundle="$stage_root/$plugin_name"
ditto "$source_bundle" "$staged_bundle"

destination="$plugin_dir/$plugin_name"
if [ -e "$destination" ]; then
  backup="$plugin_dir/$plugin_name.backup-$(date +%Y%m%d-%H%M%S)-$$"
  mv "$destination" "$backup"
  echo "Loop installer: previous bundle preserved at $backup"
fi
mv "$staged_bundle" "$destination"
codesign --verify --strict --deep "$destination"
lipo "$destination/Contents/MacOS/Loop L-01" -verify_arch arm64 x86_64

echo "Loop installer: installed $destination"
echo "Quit and reopen the DAW, then rescan VST3 effects if Loop is not listed."
