# Install the internal macOS test binaries

The binary ZIP contains universal arm64+x86_64 builds of Density D-01 and
Harmonic H-01. They are ad-hoc signed, not notarized, and intended only for
private testing on your own Macs.

## Install

1. Quit Cubase, Ableton Live, and any other audio host.
2. Double-click the ZIP to extract it.
3. Open Terminal and type `cd `, including the trailing space.
4. Drag the extracted folder into Terminal and press Return.
5. Run:

```sh
./Tools/install_density_macos.sh "$PWD/Plugins/Density D-01.vst3"
./Tools/install_harmonic_macos.sh "$PWD/Plugins/Harmonic H-01.vst3"
```

The scripts verify the code signatures and both CPU architectures before and
after copying the plugins to:

```text
~/Library/Audio/Plug-Ins/VST3/
```

Existing copies are preserved beside the installed plugin as timestamped
backups. The installation does not use `sudo`.

Reopen the DAW and rescan VST3 plugins. The effects appear as `Density D-01`
and `Harmonic H-01` under the internal vendor name `Aste Internal`.

## Verify the download

Keep the ZIP and its adjacent `.sha256` file in the same folder, then run:

```sh
cd /path/to/the/download/folder
shasum -a 256 -c Aste-Signal-Instruments-*-macos-universal.zip.sha256
```

The result must say `OK`. Do not override a checksum mismatch.

## If macOS blocks the plugin

These internal builds do not have a paid Developer ID signature or Apple
notarization. First verify the SHA-256 checksum. Then open System Settings,
choose **Privacy & Security**, and use **Open Anyway** if macOS presents that
option. Reopen the DAW and rescan.

If the DAW cached an earlier failed scan, remove the installed plugin, restart
the Mac, reinstall it with the commands above, and force a full VST3 rescan.

## Remove

Quit all audio hosts, then move these two folders from
`~/Library/Audio/Plug-Ins/VST3/` to the Trash:

```text
Density D-01.vst3
Harmonic H-01.vst3
```

Timestamped `.backup-*` folders are previous internal builds and can be restored
by removing the `.backup-...` suffix while the DAW is closed.
