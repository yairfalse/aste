# Aste Signal Instruments

[![CI](https://github.com/yairfalse/aste/actions/workflows/ci.yml/badge.svg)](https://github.com/yairfalse/aste/actions/workflows/ci.yml)

Aste is a family of original electronic instruments and audio effects built
around sound as physical material: pressure, harmonic matter, electricity,
impact, repetition, memory and space.

An imaginary Finnish engineering company, founded in Helsinki in 1974, whose
unfinished instruments became possible through contemporary software. The
interfaces are precise, restrained and modern; the instruments explore an
alternative technological history rather than imitate particular hardware.

Read [The Musical Intent](docs/MUSICAL_INTENT.md) for the artistic context.
Engineering and measurement serve the music. Passing tests does not establish
that an instrument sounds good.

## The family

The main VST3 build contains **eleven products**: six MIDI instruments and five
audio effects. Each owns its sound engine, parameter identities, state and UI.
DSP is framework-independent C++20, with JUCE at the plugin boundary.

| Product | Load as | Musical purpose | Guide |
|---|---|---|---|
| **Density D-01** | Audio effect | Physical pressure, nonlinear dynamics and rhythmic recovery | [Parameters](docs/PARAMETER_MODEL.md) |
| **Harmonic H-01** | Audio effect | Broad spectral shaping intertwined with nonlinear harmonic colour | [Specification](docs/products/harmonic/SPECIFICATION.md) |
| **Loop L-01** | Audio effect | Captured tapes, repeated generations, varispeed and transformed memory | [Specification](docs/products/loop/SPECIFICATION.md) |
| **Field F-01** | Audio effect with MIDI excitation | Diffused reverberation, held spatial energy and evolving fragments | [Guide](docs/products/field/RELEASE_012.md) |
| **Grain G-01** | Audio effect | Three granular memories with independent time and recursive routing | [Guide](docs/products/grain/README.md) |
| **Sequence S-01** | MIDI instrument | Electrical articulation, polyphonic harmony and optional programmed patterns | [Specification](docs/products/sequence/SPECIFICATION.md) |
| **Impulse I-01** | MIDI percussion instrument | Eight synthetic impact objects with editable independent cycles | [Parameters](docs/products/impulse/PARAMETERS.md) |
| **Non-Machine N-01** | MIDI percussion instrument | Three sound objects, conditional events and nested rhythmic time | [Guide](docs/products/non-machine/README.md) |
| **Resonator R-01** | MIDI instrument | Noise-excited harmonic structures and retained resonant energy | [Guide](docs/products/resonator/README.md) |
| **Contamination C-01** | MIDI instrument | Polyphonic chord excitation, nonlinear resonance and degrading feedback | [Guide](docs/products/contamination/README.md) |
| **Click K-01** | MIDI percussion instrument / sampler | Four click/sample objects, locked events, expressive rolls and nested audio repeats | [Guide](docs/products/click/README.md) |

## Start playing

In Cubase, add MIDI instruments to **Instrument Tracks** and audio effects to
**inserts or sends**. The same instrument/effect distinction applies in other
VST3 hosts. Audio effects need an incoming signal; a silent input is not a
substitute for feeding or capturing material.

| Product | First action | Explore next |
|---|---|---|
| Density | Insert on a track or group and feed audio. | Density, Drive and Crush; shape breathing with Attack/Release. |
| Harmonic | Insert on a track and shape the four broad frequency regions. | Harmonic adds nonlinear spectral interaction; Master sets final level. |
| Loop | Feed audio and hold Capture to record a tape. | RELOOP, tape speed, reverse and generation changes. |
| Field | Feed audio into Clean room; on a send set Blend to 100%. | Mass/Distance, Transform and FOREVER after sound has entered the field. |
| Grain | Feed audio into A for at least half a second. | Select A/B/C, change grain timing/pitch, freeze material and connect routes. |
| Sequence | Play MIDI; choose a polyphonic voice mode for chords. | Enable its programmed sequence for host-synced electrical patterns. |
| Impulse | Start host transport with its sequencer enabled. | Edit the eight objects' patterns, cycle lengths and impact character. |
| Non-Machine | Enable DAW SEQUENCE and start transport, or press HIT. | Edit event positions/locks, independent cycles and local/master holds. |
| Resonator | Play a MIDI root or press EXCITE. | Choose individual-tone MIDI mode for direct harmony, then explore energy retention and retuning. |
| Contamination | Play notes or chords; start with Submerged Chord or Corroded Stab. | Pressure, Contamination, Instability, Memory and Motion; clear accumulated state intentionally. |
| Click | Start transport in Host sequence, or select MIDI only and play notes 36/38/42/46. | Drag event edges into rolls, load samples in Sound, and perform master/inner repeats. |

Product guides explain exact MIDI mappings, capture boundaries, modulation,
source dependencies and limits. Preset menus provide starting material rather
than requiring a patch to be built from scratch. Editors expose precise values,
fine adjustment, automation feedback and an independent final Master Output.

## Development status and compatibility

The current source build version is **0.12.0**, defined in
[CMakeLists.txt](CMakeLists.txt). Individual product guides also retain the
versions at which their features or state contracts were introduced. A common
build version does not imply every locally installed bundle was replaced.

These are playable local development instruments, not supported 1.0 releases.
Some target names retain “experimental” because of their origins; the current
plugins are built from the main product infrastructure. Offline laboratories
remain separate historical evidence. Local macOS builds are ad-hoc signed,
without Developer ID signing or notarization. Aste remains a provisional project
name; plugin identifiers are kept for session compatibility.

Parameter IDs and saved-state meaning are compatibility contracts. Updated
responses use explicit revision metadata where needed; opening an old project
must not silently adopt a new sound. Consult each product's guide before using
**USE UPDATED SOUND**. Controls and patterns can recall while live resonant,
spatial or captured audio starts empty: each instrument documents its policy.
K-01 sample files are referenced by path and must remain available or be relinked.

Validation is product-specific. See [Field's report](docs/products/field/RELEASE_012.md),
[Grain's evidence](docs/products/grain/EVIDENCE.md),
[Contamination's redesign](research/c01-redesign/README.md) and
[Click's validation](docs/products/click/VALIDATION.md).
Separate-host tests, native editor checks and pluginval are not claims of a
completed musical or Cubase acceptance test for every product.

## Build and test

Requirements: CMake 3.25+, a C++20 compiler, Python 3 for repository checks, and
Xcode Command Line Tools on macOS. Optional VST3 builds fetch pinned JUCE 8.0.13
unless a local source checkout is supplied. Core DSP builds do not require JUCE
or network access.

Build the framework-independent processors and tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Build all eleven universal macOS VST3s and their adapter/host checks:

```sh
cmake -S . -B build-plugin-universal \
  -DASTE_BUILD_VST3=ON -DASTE_BUILD_AU=OFF \
  -DASTE_BUILD_EXPERIMENTAL_INSTRUMENTS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build-plugin-universal --parallel 4
ctest --test-dir build-plugin-universal --output-on-failure
```

For a targeted build, choose a product target from this table:

| Product | Target / bundle folder | Installer ID |
|---|---|---|
| Density | `DensityD01` | `density-d01` |
| Harmonic | `HarmonicH01` | `harmonic-h01` |
| Loop | `LoopL01` | `loop-l01` |
| Field | `FieldF01` | `field-f01` |
| Grain | `GrainG01` | `grain-g01` |
| Sequence | `SequenceS01` | `sequence-s01` |
| Impulse | `ImpulseI01` | `impulse-i01` |
| Non-Machine | `NonMachineN01` | `non-machine-n01` |
| Resonator | `ResonatorR01` | `resonator-r01` |
| Contamination | `ContaminationC01` | `contamination-c01` |
| Click | `ClickK01` | `click-k01` |

Append `_VST3` to a target to build its plugin, for example:

```sh
cmake --build build-plugin-universal --target ClickK01_VST3 --parallel 4
```

Bundles appear under `<target>_artefacts/Release/VST3/`, for example
`build-plugin-universal/ClickK01_artefacts/Release/VST3/Click K-01.vst3`.
Building does not install or publish plugins. For ASan/UBSan checks, configure
a separate build with `-DASTE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug`.
Additional Field production-compatibility checks live in `tests/field_shipping`.

## Local macOS installation

Quit Cubase first. Verify a built selection without changing installed plugins:

```sh
python3 tools/install_development_vst3.py \
  --build-root build-plugin-universal --only click-k01 --check-only
```

Remove `--check-only` to install that selection, with write access to
`/Library/Audio/Plug-Ins/VST3`. Multiple IDs can follow `--only`. The installer
verifies identity, version, architecture and signature, stages replacements,
backs up prior bundles outside DAW scan paths, rolls back on failure and writes
a receipt. Projects and Cubase preferences are not edited.

Omitting `--only` selects **all eleven products**, which must all be built at the
current source version. A selective build does not refresh the other bundles;
the installer rejects version mismatches. Avoid duplicate per-user/system copies.
Reopen Cubase after installation and rescan plugins if necessary.

The native AU/VST3 package workflow covers the original six products; the
eleven-product local VST3 installer is separate. See
[macOS installation](docs/MACOS_INSTALL.txt) and [versioning](docs/VERSIONING.md).
[Published downloads](https://github.com/yairfalse/aste/releases), when available,
have their own source, checksums and validation notes and may lag this branch.

## Engineering and documentation

Audio callbacks must not allocate, lock, wait, log or access files. Processing
uses bounded storage and deterministic scheduling/randomness where specified.
Tests cover state validation, numerical behavior, buffer partitions and host
boundaries; measurements and human listening remain separate evidence.

| Topic | Documentation |
|---|---|
| Artistic direction | [Musical intent](docs/MUSICAL_INTENT.md) |
| Parameters and sessions | [Parameter model](docs/PARAMETER_MODEL.md), product guides and state formats |
| Interfaces and interaction | [UI system](docs/UI_SYSTEM.md) |
| Hosts and validation | [Host compatibility](docs/HOST_COMPATIBILITY.md), [release checklist](docs/RELEASE_CHECKLIST.md) |
| DSP and experiments | [DSP research](docs/DSP_RESEARCH.md), `research/`, `apps/dsp-lab/` |
| Changes and compatibility | [Changelog](CHANGELOG.md), [versioning](docs/VERSIONING.md) |

Historical laboratories/reports describe their recorded source revisions, not
necessarily today's plugin. Build outputs, listening renders and caches stay
outside the source history; reproducible generators and retained text evidence
remain in the repository.

## Contributing and licensing

Read [CONTRIBUTING.md](docs/CONTRIBUTING.md) and [SECURITY.md](docs/SECURITY.md).
Preserve existing sessions, explain the musical or engineering problem, and
report what was actually built, measured and tested.

Aste's original code is **AGPL-3.0-only**; see [LICENSE](LICENSE). This open-source
JUCE build uses its AGPLv3 option. Third-party components retain their licenses;
see [third-party notices](docs/THIRD_PARTY_NOTICES.md) and
[license inventory](LICENSES/README.md). No proprietary plugin code, commercial
recordings or hardware firmware are included.
