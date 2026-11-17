# Aste Signal Instruments

[![CI](https://github.com/yairfalse/aste/actions/workflows/ci.yml/badge.svg)](https://github.com/yairfalse/aste/actions/workflows/ci.yml)

Aste is a family of open-source electronic instruments and audio effects built
around sound as physical material: mass, harmonic matter, electricity, memory,
impact and space. These are original instruments, informed by electronic-music
history rather than intended as hardware emulations.

Read [The Musical Intent](docs/MUSICAL_INTENT.md) for the artistic context.
Engineering, interfaces and measurement serve that musical purpose; passing a
test does not establish that an instrument sounds good.

## Current status

The latest validated local development build is **0.12.0** (Field). The build
version is defined in [CMakeLists.txt](CMakeLists.txt). The local VST3 build
includes **ten products**. This documentation describes local development work;
the accompanying code changes and binaries are not published by this
documentation-only update.
Some CMake targets retain the name “experimental”; these are playable plugins,
with their original offline laboratories preserved separately.

The latest production change is **Field F-01's diffused reverb foundation**.
Its universal Release VST3 was installed and verified locally at 0.12.0. This
was a Field-only update; it does not mean every installed Aste plugin was
rebuilt or replaced. See the [Field installation and validation report](docs/products/field/RELEASE_012.md)
and [changelog](CHANGELOG.md).

These are development builds, not supported 1.0 releases. Local macOS bundles
are ad-hoc signed, without Developer ID signing or notarization. Published
downloads may lag the working tree; use each download's own version and notes.
Aste remains a provisional project name, with existing plugin identifiers kept
for session compatibility.

## The family

| Product | Type | Musical purpose | Documentation |
|---|---|---|---|
| **Density D-01** | Audio effect | Pressure, weight and breathing nonlinear dynamics | [Parameter model](docs/PARAMETER_MODEL.md) |
| **Harmonic H-01** | Audio effect | Spectral shaping and nonlinear harmonic matter | [Parameters](docs/products/harmonic/PARAMETERS.md) |
| **Sequence S-01** | MIDI instrument | Polyphonic chords, electrical articulation and optional patterns | [Specification](docs/products/sequence/SPECIFICATION.md) |
| **Loop L-01** | Audio effect / memory instrument | Captured sound transformed through repeated generations | [Specification](docs/products/loop/SPECIFICATION.md) |
| **Impulse I-01** | MIDI percussion instrument | Eight synthetic impact objects and editable track cycles | [Parameters](docs/products/impulse/PARAMETERS.md) |
| **Field F-01** | Audio effect with MIDI excitation | Diffused reverberation and evolving stored spatial energy | [Release guide](docs/products/field/RELEASE_012.md) |
| **Non-Machine N-01** | MIDI percussion instrument | Three sound objects, explicit event patterns and nested rhythmic time | [Guide](docs/products/non-machine/README.md) |
| **Resonator R-01** | MIDI instrument | Excited harmonic structures and retained resonant energy | [Guide](docs/products/resonator/README.md) |
| **Grain G-01** | Audio effect | Three independently timed granular memories and recursive routing | [Guide](docs/products/grain/README.md) |
| **Contamination C-01** | MIDI instrument | Dense chord excitation, nonlinear resonance and interacting dirty feedback | [Guide](docs/products/contamination/README.md) |

DSP is framework-independent C++20. JUCE is used at the plugin boundary.
Products own their sound engines, parameters, state and interfaces; they do not
link to one another's processors. Shared utilities cover genuinely common needs.

## Using Field 0.12.0

Load **Field F-01** as an effect in Cubase. New instances start on **Clean room**.
The six new starting points are:

- **Clean room** — short conventional space.
- **Deep hall** — a larger diffused tail.
- **Dark dub** — darker absorption and transformed material.
- **Enormous space** — long spatial decay.
- **Evolving field** — moving fragments and pitched wet sound.
- **FOREVER** — hold existing spatial energy; feed sound before engaging hold.

MASS sets decay; DISTANCE darkens the space. TRANSFORM enables the fragment and
pitch readouts without the previous feedback-decay loss. Set TRANSFORM to zero
for the conventional foundation. On a send, set BLEND to 100%. MASTER OUTPUT
adjusts final level independently of the sound controls.

**Existing projects retain the original engine.** Choose **USE UPDATED SOUND**
to adopt the new engine with current control values, or select a new factory
preset. The nine older snapshots remain available with their original engine.
Live reverb and phrase audio are not serialized into the session.

The installed build passed DSP regression, real-time audits, sanitizers, state
and preset recall, native/Intel VST3 loading, parameter automation, sample-rate
changes and simulated transport tests in a separate host. Cubase 14 was launched,
but macOS blocked Accessibility control: actual plugin insertion and playback
were **not verified in Cubase**. See the [complete validation limits](docs/products/field/RELEASE_012.md).

## Compatibility and workflow

Parameter IDs and plugin identities are compatibility contracts. Updated sound
engines use explicit revision metadata so older projects can keep their sound.
An absent revision is not permission to reinterpret an existing session.

Editors expose exact values, fine adjustment, automation feedback and final
Master Output. In Cubase, MIDI instruments appear in the instrument list; audio
effects appear in insert/send lists. Consult each product's guide for MIDI,
clock, captured-audio recall and transport behavior.

Contamination's 0.10.1 host-restart fix is retained: it reports a fixed **16 samples
of latency** without repeatedly requesting host reconfiguration. Its revised
0.11.0 circuit preserves older sessions through explicit adoption. Details and
listening comparisons are in the [C-01 redesign report](research/c01-redesign/README.md).

## Build and test

Requirements: CMake 3.25+, a C++20 compiler, Python 3 for repository checks, and
Xcode Command Line Tools on macOS. Optional VST3 builds fetch pinned JUCE 8.0.13
unless a local source path is supplied. The framework-independent core build
does not require JUCE or network access.

Build the core DSP, laboratories and tests:

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
```

Build universal macOS VST3 plugins and their adapter/host checks:

```sh
cmake -S . -B build-plugin-universal \
  -DASTE_BUILD_VST3=ON -DASTE_BUILD_AU=OFF \
  -DASTE_BUILD_EXPERIMENTAL_INSTRUMENTS=ON \
  -DCMAKE_BUILD_TYPE=Release \
  '-DCMAKE_OSX_ARCHITECTURES=arm64;x86_64'
cmake --build build-plugin-universal --parallel 4
ctest --test-dir build-plugin-universal --output-on-failure
```

Bundles are under `<target>_artefacts/Release/VST3/`, for example
`build-plugin-universal/FieldF01_artefacts/Release/VST3/Field F-01.vst3`.
Building does not install or publish plugins. To enable AddressSanitizer and
UndefinedBehaviorSanitizer for a separate test build, configure with
`-DASTE_SANITIZERS=ON -DCMAKE_BUILD_TYPE=Debug`.

The additional production-Field compatibility and FOREVER tests can be run with:

```sh
cmake -S tests/field_shipping -B build-field-shipping -DCMAKE_BUILD_TYPE=Release
cmake --build build-field-shipping --parallel 4
ctest --test-dir build-field-shipping --output-on-failure
```

## Local macOS installation

Quit Cubase before replacing plugins. Verify the selected build without writing:

```sh
python3 tools/install_development_vst3.py \
  --build-root build-plugin-universal --only field-f01 --check-only
```

To install that verified selection, run the same command without `--check-only`
with macOS administrator privileges. It installs to `/Library/Audio/Plug-Ins/VST3`,
stages and verifies replacements, backs up prior bundles outside DAW scan paths,
rolls back on failure and writes a receipt. It does not edit projects or Cubase
preferences. `--only` accepts the product identifiers listed by `--help`.

Omitting `--only` selects all ten products, which must all be built at the current
source version. A selective build does not refresh the other bundles; the
installer rejects version mismatches. Avoid duplicate per-user and system copies.

[Published downloads](https://github.com/yairfalse/aste/releases), when available,
include their own source, checksums and validation notes. The existing native
AU/VST3 package workflow covers the original six products; the ten-product local
VST3 installer is separate. See [macOS installation](docs/MACOS_INSTALL.txt)
and [versioning](docs/VERSIONING.md).

## Engineering and evidence

Audio callbacks must not allocate, lock, wait, log or access files. Processing
uses bounded storage and deterministic scheduling/randomness where specified.
Tests cover state validation, numerical behavior, buffer partitions and host
boundaries; measurement and musical listening remain separate evidence.

Offline laboratories and listening-pack generators live under `research/` and
`apps/dsp-lab/`. Historical reports describe their measured source revisions,
not necessarily today's plugin. Generated local artifacts live under
`build-evidence/`; they are not all committed or part of a published download.

| Documentation | Purpose |
|---|---|
| [Architecture](docs/ARCHITECTURE.md) | Product boundaries and ownership |
| [Parameter model](docs/PARAMETER_MODEL.md) | Stable IDs, ranges and smoothing |
| [State format](docs/STATE_FORMAT.md) | Validation and compatibility |
| [Real-time safety](docs/REALTIME_SAFETY.md) | Callback rules and runtime audits |
| [Testing](docs/TESTING.md) | Test layers and measurement protocols |
| [Host compatibility](docs/HOST_COMPATIBILITY.md) | Host evidence and its limits |
| [UI system](docs/UI_SYSTEM.md) | Family design and interaction conventions |
| [DSP research](docs/DSP_RESEARCH.md) | Experimental methods and claims |
| [Release checklist](docs/RELEASE_CHECKLIST.md) | Distribution gates |
| [Changelog](CHANGELOG.md) | Versioned changes |

## Contributing and licensing

Read [CONTRIBUTING.md](docs/CONTRIBUTING.md) before changing DSP, interfaces or
state contracts, and [SECURITY.md](docs/SECURITY.md) for security reporting.
Preserve unrelated work and existing sessions; document what was actually built,
measured and tested.

Aste's original code is **AGPL-3.0-only**; see [LICENSE](LICENSE). This open-source
JUCE build uses its AGPLv3 option. Third-party components retain their licenses;
see [third-party notices](docs/THIRD_PARTY_NOTICES.md) and
[license inventory](LICENSES/README.md). No proprietary plugin code, commercial
recordings or hardware firmware are included.
