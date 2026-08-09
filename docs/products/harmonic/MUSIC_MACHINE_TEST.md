# Harmonic H-01 music-machine test

This checklist is the first external internal-beta gate. Record the machine,
macOS version, architecture, DAW build, sample rate, and buffer size. A load or
validator pass is not a sonic approval.

## Install

1. Quit Cubase and Ableton.
2. Run `./tools/build_and_install_macos.sh` from the repository root.
3. Reopen each DAW and rescan VST3 plugins if needed.
4. Confirm both `Density D-01` and `Harmonic H-01` appear as audio effects.

## Host behavior

- Insert and remove Harmonic on mono and stereo tracks.
- Open, resize, close, and reopen its editor.
- Type exact values, reset controls, and select every factory starting point.
- Save the project with non-default values, quit the DAW, reopen, and compare
  every restored value.
- Automate Input, all four gains/frequencies, Harmonic, Output, and Bypass with
  slow ramps and abrupt jumps.
- Compare real-time playback, real-time bounce, offline bounce, freeze, and
  unfreeze at matched settings.
- Change sample rate and buffer size, then stop/start and suspend/resume.
- Duplicate instances and test at least ten simultaneous instances.
- Confirm meters move only with signal and the UI remains quiet when closed.

Record crashes, silence, parameter jumps, zipper noise, host warnings, missed
automation, visual corruption, wrong state, or inconsistent bounce as failures
with the smallest reproducible project.

## Musical behavior

Level-match each comparison at the Output control. Test full mixes, ambient
material, percussion, bass-heavy material, and sparse acoustic material.

For every source, compare:

- all gains at zero, Harmonic 0% versus 100%: both should remain neutral;
- one +3 dB boost, Harmonic 0/35/100%: density should rise continuously without
  destroying the requested contour;
- one -6 dB cut, Harmonic 0/100%: the cut should remain effectively unchanged;
- broad multi-band boost and mixed boost/cut settings;
- `Foundation` on centered low end for phase and mono compatibility;
- `Presence` and `Air` on sharp transients for hardness, alias-like grit, and
  listener fatigue;
- presets `Continuum`, `Hard Air`, and `Negative Space` against level-matched
  manual settings.

For each test write: source, setting, what changed, whether it was useful,
failure cases, and preferred Harmonic range. Avoid labels such as “analog”;
describe contour, density, edge, transient motion, stereo image, and fatigue.

## Acceptance boundary

The internal beta advances only if state recall and bounce are deterministic,
automation is clean enough for composition, stereo image remains controlled,
the macro feels monotonic on real material, and the nonlinear band behavior is
preferable to the matched clean setting in identifiable cases. Otherwise keep
the stable IDs and revise ranges or DSP before release-candidate work.
