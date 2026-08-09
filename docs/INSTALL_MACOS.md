# Install the internal macOS test binaries

The plain transfer folder contains universal arm64+x86_64 builds of Density
D-01, Harmonic H-01, and Sequence S-01. They are ad-hoc signed, not notarized,
and intended only for private testing on your own Macs.

## Install

1. Quit Cubase, Ableton Live, and any other audio host.
2. In Finder choose **Go > Go to Folder** and enter:

```text
~/Library/Audio/Plug-Ins/VST3
```

3. Copy the three `.vst3` bundles from the transfer folder into that folder.
4. Reopen the DAW and perform a full VST3 rescan if necessary.

Density and Harmonic appear as effects. Sequence appears as a VST3 instrument.
All use the internal vendor name `Aste Internal`.

## If macOS blocks the plugin

These internal builds do not have a paid Developer ID signature or Apple
notarization. Open Terminal and run:

```sh
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/Density D-01.vst3"
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/Harmonic H-01.vst3"
xattr -dr com.apple.quarantine "$HOME/Library/Audio/Plug-Ins/VST3/Sequence S-01.vst3"
```

If the DAW cached an earlier failed scan, remove the installed plugin, restart
the Mac, reinstall it with the commands above, and force a full VST3 rescan.

## Remove

Quit all audio hosts, then move these three folders from
`~/Library/Audio/Plug-Ins/VST3/` to the Trash:

```text
Density D-01.vst3
Harmonic H-01.vst3
Sequence S-01.vst3
```

Timestamped `.backup-*` folders are previous internal builds and can be restored
by removing the `.backup-...` suffix while the DAW is closed.
