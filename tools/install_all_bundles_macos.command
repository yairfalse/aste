#!/bin/sh
set -eu

source_dir=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
plugin_dir=${1:-"${HOME:?}/Library/Audio/Plug-Ins/VST3"}

if [ "$(uname -s)" != "Darwin" ]; then
  echo "Aste installer: macOS is required" >&2
  exit 1
fi

set -- \
  "Density D-01.vst3" \
  "Harmonic H-01.vst3" \
  "Sequence S-01.vst3" \
  "Loop L-01.vst3" \
  "Impulse I-01.vst3" \
  "Field F-01.vst3"

for plugin_name do
  source_bundle="$source_dir/$plugin_name"
  executable="$source_bundle/Contents/MacOS/${plugin_name%.vst3}"
  if [ ! -d "$source_bundle" ] || [ ! -f "$executable" ]; then
    echo "Aste installer: bundle not found beside this script: $plugin_name" >&2
    exit 1
  fi
  codesign --verify --strict --deep "$source_bundle"
  lipo "$executable" -verify_arch arm64 x86_64
done

mkdir -p "$plugin_dir"
stage_root=$(mktemp -d "$plugin_dir/.aste-install.XXXXXX")
trap 'rm -rf -- "$stage_root"' EXIT HUP INT TERM

for plugin_name do
  source_bundle="$source_dir/$plugin_name"
  staged_bundle="$stage_root/$plugin_name"
  destination="$plugin_dir/$plugin_name"
  ditto "$source_bundle" "$staged_bundle"

  if [ -e "$destination" ]; then
    backup="$plugin_dir/$plugin_name.backup-$(date +%Y%m%d-%H%M%S)-$$"
    mv "$destination" "$backup"
    echo "Aste installer: preserved previous $plugin_name at $backup"
  fi

  mv "$staged_bundle" "$destination"
  xattr -dr com.apple.quarantine "$destination" 2>/dev/null || true
  codesign --verify --strict --deep "$destination"
  lipo "$destination/Contents/MacOS/${plugin_name%.vst3}" \
    -verify_arch arm64 x86_64
  echo "Aste installer: installed $plugin_name"
done

echo
echo "Installed all six instruments in: $plugin_dir"
echo "Reopen Cubase or Ableton and perform a full VST3 rescan if needed."
