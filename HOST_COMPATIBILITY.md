# Host compatibility

## Automated validation

| Date | Tool | Artifact | Result |
|---|---|---|---|
| 2026-08-06 | pluginval 1.0.4, locally built from tag | arm64 VST3 0.1.0, 11 parameters | Pass, strictness 10 |
| 2026-08-06 | Steinberg VST3 SDK 3.8.0 validator | arm64 VST3 0.1.0, 11 parameters | Pass, 47/47 tests |
| 2026-08-07 | pluginval 1.0.4, seed `0xd01` | rebuilt arm64 VST3 after UI baseline | Pass, strictness 10 |
| 2026-08-07 | Steinberg VST3 SDK 3.8.0 validator | same rebuilt arm64 VST3 | Pass, 47/47 tests |
| 2026-08-08 | pluginval 1.0.4, seed `0xd01` | Cycle 32 arm64 VST3, 11 parameters | Pass, strictness 10 |
| 2026-08-08 | Steinberg VST3 SDK 3.8.0 validator, extensive | same Cycle 32 arm64 VST3 | Pass, 537/537 tests |
| 2026-08-08 | pluginval 1.0.4, seed `0xd01` | Cycle 36 Output-smoothing arm64 VST3 | Pass, strictness 10 |
| 2026-08-08 | Steinberg VST3 SDK 3.8.0 validator, extensive | same Cycle 36 arm64 VST3 | Pass, 537/537 tests |
| 2026-08-08 | pluginval 1.0.4, seed `0xd01` | Cycle 38 Drive-smoothing arm64 VST3 | Pass, strictness 10 |
| 2026-08-08 | Steinberg VST3 SDK 3.8.0 validator, extensive | same Cycle 38 arm64 VST3 | Pass, 537/537 tests |
| 2026-08-08 | pluginval 1.0.4, seed `0xd01` | Cycle 40 Attack-smoothing arm64 VST3 | Pass, strictness 10 |
| 2026-08-08 | Steinberg VST3 SDK 3.8.0 validator, extensive | same Cycle 40 arm64 VST3 | Pass, 537/537 tests |
| 2026-08-08 | pluginval 1.0.4, seed `0xd01` | Cycle 42 Blend-smoothing arm64 VST3 | Pass, strictness 10 |
| 2026-08-08 | Steinberg VST3 SDK 3.8.0 validator, extensive | same Cycle 42 arm64 VST3 | Pass, 537/537 tests |
| 2026-08-08 | pluginval 1.0.4, seed `0xd01` | Cycle 47 keyboard-navigation arm64 VST3 | Pass, strictness 10 |
| 2026-08-08 | Steinberg VST3 SDK 3.8.0 validator, extensive | same Cycle 47 arm64 VST3 | Pass, 537/537 tests |
| 2026-08-08 | Steinberg VST3 SDK 3.8.0 x86_64 validator, extensive | thin x86_64 VST3 under Rosetta | Pass, 537/537 tests |
| 2026-08-08 | pluginval 1.0.4 arm64, seed `0xd01` | universal arm64+x86_64 VST3 | Pass, strictness 10 |
| 2026-08-08 | Steinberg VST3 SDK 3.8.0 arm64 and x86_64 validators, extensive | matching slices of universal VST3 | Pass, 537/537 each |
| 2026-08-08 | repository headless VST3 host | thin arm64, thin x86_64, and both universal slices | Pass, dynamic load, state, and six irregular blocks |
| 2026-08-09 | Steinberg VST3 SDK 3.8.0 clean source-built arm64 validator, extensive | signed universal VST3 | Pass, 537/537 tests; pinned CI gate added |

The run included cold/warm loading, editor open during processing, state and
state restoration, background-thread state, parameter thread safety and fuzzing,
automation, mono/stereo buses, six sample rates from 44.1 to 192 kHz, and block
sizes 1, 2, 7, 16, 32, 64, 127, 128, 256, 511, 512, 1024, and 2048. Reported
latency and tail were both zero. The independent Steinberg validator covered
module structure, buses, state transitions, processing, automation, bypass,
silence flags, variable blocks, and threaded processing; all 47 tests passed.

The 2026-08-07 rerun used macOS 26.5.2 build 25F84 on arm64. Pluginval
covered all combinations of 44.1, 48, 88.2, 96, 176.4, and 192 kHz with block
sizes 1, 2, 7, 16, 32, 64, 127, 128, 256, 511, 512, 1024, and 2048. The
rebuilt bundle's final ad-hoc code signature also passes strict verification.

The 2026-08-08 run revalidated the current working artifact after the lab-only
oversampling work. Pluginval again exercised all 78 requested sample-rate and
block-size combinations plus editor, state, automation, buses, thread safety,
and parameter fuzzing. The product path still reports zero latency and tail.

Cycle 52 adds build and validator evidence for x86_64 and a single universal
bundle. Its x86_64 execution used Rosetta on Apple Silicon, so this is not a
native Intel performance or DAW compatibility result.

Cycle 54 adds a repo-owned standalone smoke host. It discovers and instantiates
the built component through VST3, verifies 11 parameters and zero latency,
round-trips state byte-for-byte within each instance, and processes finite
stereo output at block sizes 1, 2, 7, 127, 511, and 2048. A state containing a
non-default Drive value restored correctly from arm64 to x86_64 and back. The
outer JUCE host-state byte representation is architecture-dependent, so exact
bytes are required only for repeated serialization of equivalent state within
one host instance; semantic cross-architecture restoration is the portability
gate.

## DAWs

| Host | Version | Status |
|---|---|---|
| Cubase | 14 | Not installed; not yet tested |
| Ableton Live | 13/beta | Not installed; not yet tested |
| Additional VST3 host | Pending selection | None installed; not yet tested |

An application inventory on 2026-08-07 confirmed that no target or additional
VST3 DAW is installed on the current test machine.

No compatibility claim is made until a row records machine, macOS version,
architecture, sample rate, buffer, test procedure, and result.
