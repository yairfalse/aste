# Versioning

All six instruments share one release number while developed and distributed
together. The existing development download is the **0.1.0** baseline.
`D-01`, `H-01`, `S-01`, `L-01`, `I-01` and `F-01` are model names, not versions.

The sole build-version source is `project(... VERSION ...)` in the root
`CMakeLists.txt`. JUCE bundle versions, AU's numeric version, installer receipt
versions, download filenames and build metadata derive from it. The macOS
bundle check rejects mismatches in both build outputs and extracted packages.
Keep the numeric version in `major.minor.patch` form for Apple/JUCE compatibility;
the download's `development` label communicates its pre-release status.

- **Patch**, e.g. 0.1.1: fixes, polish, packaging or version-display improvements.
- **Minor**, e.g. 0.2.0: meaningful new functionality or workflow changes.
- **1.0.0**: a deliberately supported first stable release, after the release
  gates are met. Subsequent breaking releases increase the major version.

Before 1.0, document any breaking behavior in a minor release. Never use a
version bump as permission to silently break session recall: parameter IDs,
plugin identities and state schemas have their own compatibility contracts.
State schema numbers change only when serialized data requires it.

## Preparing the next download

1. Bump the version once in CMake before building a changed download. The next
   fixes-only download after 0.2.0 is 0.2.1. Version 0.2.0 intentionally changes
   Density/Harmonic voicing; the 0.1.x downloads remain available for old mixes.
2. Move relevant Unreleased entries into a dated section of
   [CHANGELOG.md](../CHANGELOG.md), describing user-visible changes and limits.
3. Build, test and package from a clean, committed source revision. Keep the
   matching source archive, test evidence and SHA-256 alongside the binary.
4. Tag that exact committed revision `vX.Y.Z` when explicitly making a release.
   Do not tag a dirty working tree or tag today's tooling as yesterday's binary.
5. Preserve previously shared downloads. Never replace a shared artifact with
   different bytes under the same version. Publishing remains a separate action.

The baseline download was built from a dirty development tree before this
policy. Its bundled corresponding-source archive and checksum identify that
exact build; it is not retrospectively represented as a clean tagged release.
