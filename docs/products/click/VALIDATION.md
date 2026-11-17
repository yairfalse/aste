# K-01 local build and validation

2026-11-17, family version **0.12.0**. This is the actual instrument in the root
VST3 build, with no separate research application or listening-study pack.

## Build and tests

- Universal **arm64 + x86_64** Release VST3 builds; ad-hoc signature verifies.
- K-01 engine, plugin and VST3-host CTests pass.
- AddressSanitizer + UndefinedBehaviorSanitizer engine suite passes.
- macOS callback audit observes no allocation/free, lock, file-open or write
  activity on the tested plugin callback; sample loading runs on its worker.
- Engine checks include exact negative microtiming, block-size invariance,
  curved rolls, MIDI/sequence exclusion, nested capture, Continue/Hold, stop,
  sample looping, voice stealing, finite extremes and supported sample rates.
- Plugin checks cover sample loading/replacement, missing paths, independent
  sound presets, user scenes, new-instance sample-path recall, byte-stable project
  state, malformed-restore rejection, final limiting, rates and automation.
- All three native views and Advanced Sound render at minimum size. Snapshots
  live under `build-plugin-universal/click_editor_artifacts`.
- Native and Rosetta/x86_64 VST3 smoke hosts pass discovery, instantiation,
  2592-parameter identity, eight-sample prepared latency, MIDI output and state
  recall. Native-to-Intel state exchange passes. The exchange uses a Master
  value aligned to its physical .01 dB grid.
- The existing Density, Harmonic, Sequence, Loop, Impulse and Field DSP regression
  suites all pass. No existing installed product was replaced.

Tracktion **pluginval 1.0.4, strictness 5, seed 1974** passes on the built VST3:
editor creation and opening whilst processing, audio/rate/block-size cases,
state, automation/editor automation, parameter and bus tests. Default validator
rates are 44.1/48/96 kHz and blocks 64/128/256/512/1024; K-01's own tests also
cover 22.05 and 192 kHz. The separate Steinberg SDK validator was not supplied
and pluginval explicitly skips that subtest. This is not a Cubase test claim.

## CPU and memory

AppleClang 17, native arm64 Release. Sustained long-decay synthesis, 128-frame
blocks, 50 warmup + 500 measured callbacks, no competing build/render. Numbers
are wall-clock callback time / audio duration, not total-system CPU percentages.
The complete small record is [engine-profile.txt](validation/engine-profile.txt).

| Rate | Voice budget | Mean | P99 | Maximum |
|---|---:|---:|---:|---:|
| 48 kHz | 48 | .425 | .469 | .494 |
| 96 kHz | 24 | .451 | .508 | .520 |
| 192 kHz | 12 | .496 | .600 | .653 |

The initial fixed 48-voice run exceeded real time at 192 kHz. The preparation-
time 48/24/12 budget preserves 2× processing and stays below the local targets
of .60 mean/.80 P99 in this specific sustained-synthesis measurement. Typical
short percussion expires much sooner. These are short local observations,
not worst-case DAW, sample-interpolation or multi-instance guarantees.

The measured maximum retained core heap after a 192 kHz preparation is
**125,239,296 bytes** (119.4 MiB). This includes all fixed sample and fragment
banks, and excludes JUCE/UI/decoder overhead. No large audio pack was generated.

## Installation

Installed **Click K-01 0.12.0** after Cubase was saved and closed:

`/Library/Audio/Plug-Ins/VST3/Click K-01.vst3`

Only `click-k01` was selected. No other installed Aste product was replaced.
The installed binary SHA-256 matches the validated build; ad-hoc signing and
universal architecture checks pass. Native and Rosetta hosts load the installed
bundle successfully. The small [installation record](validation/installed.json)
and [pluginval log](validation/pluginval-level5.txt) are retained here. The local
installer receipt is under `~/Library/Application Support/Aste/Plugin Backups/20261117-230511-0.12.0-_au_dkkb`.

Reopen Cubase and add **Click K-01** on an Instrument Track. Select MIDI only
for notes 36/38/42/46, or leave Host sequence enabled and start transport.

## Remaining musician/host checks

Cubase insertion, its actual automation lanes/session reload, keyboard/screen-
reader usability, preferred sound/preset balances and the musical result have
not been tested in Cubase. The creator can evaluate these directly using the
[quick guide](README.md). There is no claim of production acceptance or musical
quality merely because engineering tests pass. Sample references require the
original files; live master captures are deliberately not serialized.
