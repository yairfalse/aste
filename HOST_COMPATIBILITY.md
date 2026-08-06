# Host compatibility

## Automated validation

| Date | Tool | Artifact | Result |
|---|---|---|---|
| 2026-08-06 | pluginval 1.0.4, locally built from tag | arm64 VST3 0.1.0, 11 parameters | Pass, strictness 10 |
| 2026-08-06 | Steinberg VST3 SDK 3.8.0 validator | arm64 VST3 0.1.0, 11 parameters | Pass, 47/47 tests |

The run included cold/warm loading, editor open during processing, state and
state restoration, background-thread state, parameter thread safety and fuzzing,
automation, mono/stereo buses, six sample rates from 44.1 to 192 kHz, and block
sizes 1, 2, 7, 16, 32, 64, 127, 128, 256, 511, 512, 1024, and 2048. Reported
latency and tail were both zero. The independent Steinberg validator covered
module structure, buses, state transitions, processing, automation, bypass,
silence flags, variable blocks, and threaded processing; all 47 tests passed.

## DAWs

| Host | Version | Status |
|---|---|---|
| Cubase | 14 | Not yet tested |
| Ableton Live | 13/beta | Not yet tested |
| Additional VST3 host | Pending selection | Not yet tested |

No target DAW is installed on the current test machine.

No compatibility claim is made until a row records machine, macOS version,
architecture, sample rate, buffer, test procedure, and result.
