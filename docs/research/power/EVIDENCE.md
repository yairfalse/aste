# Executed musical-power audit evidence

This is engineering evidence, not a listening verdict. Baseline production is
`8a603dcbc59b52a58637fe6305676efc5a406f80`; the laboratory build and final audio
identify `1901865aa011ac0816c25b33d764ba6a2fd569fe`, AppleClang
21.0.0.21000334, Release C++20, macOS 27.0.1 arm64. No production implementation,
parameter ID, preset interpretation or state schema changed. The installed
Cubase build has not been identified against this source baseline.

Read the [critical assessment](../../../reports/Aste%20musical%20power%20audit.md),
[six-product investigation summary](DSP_FINDINGS.md),
[interface evidence](INTERFACE.md) and [ranked acceptance backlog](BACKLOG.md).
Reproduction commands and matching details are in the
[laboratory README](../../../research/power/README.md).

## Three completed experiments

| Experiment | Actual result | Interpretation and boundary |
|---|---|---|
| Sequence release-aware MIDI attack | Second-note envelope peak changes from approximately 0.4229 to 1 at all six supported rates; genuine overlapping-note render remains identical | Restores the tested fresh attack. Musical preference and broader performance acceptance remain open; existing MIDI sound changes, so no silent production adoption |
| Loop continuous motion clock | Four-second closed-form Wow displacement oracle: maximum error 5974.856 samples in production versus 0.005534 in candidate; zero-motion audio identical at all six rates | Corrects the mathematical clock mismatch without changing modulation depth. The 1000-second wrap itself was not executed; drift smoothing and pitch laws remain unchanged |
| Harmonic existing +12/−12 dB gesture | At Presence +6 dB/2.5 kHz, −36 dBFS input and full Harmonic, H3 changes from −66.247 to −45.032 dBc; fundamental gain 5.964 to 5.592 dB | Operating point materially changes coloration, not just output level. At −6 dBFS H3 instead moves −33.215 to −37.221 dBc and gain 3.681 to 2.751 dB. Not a universal improvement or a replacement topology |

Sequence and Loop are separately named generated source variants, each with a
single-occurrence checked replacement. Production is the retained control.
Harmonic uses its unchanged graph and existing controls. The Loop one-second
clock and Sequence release treatment are rejected as satisfying the stated
technical intentions, not declared musically inferior after listening.
Harmonic is retained as a conditional experiment rather than promoted.

Evidence: [note response](evidence/note_response.csv),
[motion oracle](evidence/loop_motion_oracle.csv),
[nonlinear products](evidence/harmonic-products.csv).
Methods: [colour](COLOUR_METHODS.md), [space](SPACE_METHODS.md).

## Newly exposed existing-product failures

The complete standalone CTest has two tests and **one genuine failure**:
`power_existing_host_automation`. It is not inverted or marked `WILL_FAIL`.
At 48 kHz/120 BPM, 127- versus 511-frame partitions first diverge in Sequence
at sample 16680. Equivalent step phases compute as 2.7800000000000002 versus
2.7799999999999998; the 0.78 gate ends one sample differently. Fast automation
amplifies the output difference to RMS 0.035236865; default RMS difference is
0.0000089791. A diagnostic +1e−12 PPQ bias nulls both differences. This is a
causal control, **not a proposed epsilon fix**. The renderer retains original
timing. Other tested product/rate combinations in this two-rate comparison
null exactly; this does not prove every host schedule.

Impulse MIDI variation uses callback-local offsets. Deliberately off-grid
notes produce difference RMS 0.109389376 against reference RMS 0.151331776
under the same partition comparison. Its tested host-pattern render nulls;
these are distinct scheduling paths. Low Tone endpoint renders null exactly
at all six rates, consistent with the unused control in the Low source.

[Host partitions](evidence/host_partition.csv),
[independent gate probe](evidence/sequence_gate_probe.txt),
[MIDI and object probes](evidence/note_response.csv).

## Six-product diagnostic coverage

| Product | Executed support | Important qualification |
|---|---|---|
| Density | 182 interaction rows; 432 selected nonlinear-stage fold rows; matched seven-source settings | On generated music, Density 0/50/100 gives maximum GR 0/9.098/22.966 dB but crest 17.787/13.542/18.385 dB. More compression is not monotonic final density. Stage fold probes are not whole-graph alias measurements |
| Harmonic | 288 contour/phase rows, 176 product rows, 352 selected two-tone rows, eight folds and eight headroom rows | Strong 7.004 kHz folded probe changes −29.939 to −28.767 dBc with linked gain. All-boost raw output can exceed unity and protection is not fabricated. Selected products are not total THD/IMD or alias-free proof |
| Sequence | Common host, gapped MIDI and overlapping MIDI phrases; six-rate note response; independent timing diagnosis | Shared-envelope, Pressure/Weight and filter implications are source-supported hypotheses, not a listening verdict; pitch bend/aftertouch/pedal expressiveness remains an interface/API question |
| Loop | Three capture, 12 generation, 18 speed/pitch/reverse and 96 clock rows | First capture default Overdub halves saved input (−6.0206 dB); maximum captures silence on an empty deck. Default generation RMS rises 0.080506→0.368954 over four generations. This refutes a blanket claim of negligible change, not establishes useful evolution |
| Impulse | Every object independently at three settings; Tone endpoint probes; MIDI/host comparisons; high-frequency Click folds | At 48 kHz, selected 1/6/13/20 kHz folded products are −5.165/−3.604/−2.391/−1.457 dBc; 192 kHz render is approximately −22 dBc at those bins. Restricted 7 kHz pulse diagnostic, not all objects or a certified oversampling comparison |
| Field | 720 decay rows and 72 meter witnesses across excitation/control/rate cases; matched seven-source settings | FOREVER is not a lossless freeze. Same 0.999350 retention display accompanies very different decay; display coefficient is not measured energy retention |

Field at 48 kHz, same-polarity excitation: first to ninth silent-second RMS
is 0.0299781→0.00301072 (zero motion controls),
0.0219098→0.0000361859 (defaults), and 0.0122230→7.31660e−9 (maximum).
A DC witness has fifth-second output RMS 0.0167703 while the meter reads
6.60e−11. These demonstrate limits of the observation/label, not a subjective
failure of the space. Loop reverse 1×/+12 semitones has its strongest measured
20–4000 Hz peak at 28.761 Hz, versus conventional combined-transposition
expectation 880 Hz; windowed sidebands mean strongest peak is not necessarily
perceived pitch. No replacement pitch engine is claimed.

Raw tables: [Density interactions](evidence/density-interactions.csv),
[Density stage folds](evidence/density-stage-folds.csv),
[Harmonic contours](evidence/harmonic-contours.csv),
[IMD probes](evidence/harmonic-imd.csv), [fold probes](evidence/harmonic-folds.csv),
[headroom](evidence/harmonic-headroom.csv),
[Loop capture](evidence/loop_capture.csv), [generations](evidence/loop_generations.csv),
[pitch interactions](evidence/loop_pitch_interactions.csv),
[Impulse Click](evidence/impulse_click_fold.csv),
[Field decay](evidence/field_decay.csv), [meter witness](evidence/field_meter_witness.csv).

## Audio and measurement validity

The final listening pack contains **267 variants / 534 WAVs**, each eight
seconds, stereo float32/48 kHz. Seven procedural sources cover percussion,
bass, oscillator, noise, chords, a layered generated passage and sparse events.
Sequence adds three phrase types; Impulse includes two phrases and eight
independent objects. Defaults, conservative, character-candidate, extreme,
fast automation and interacting controls are explicit lab recipes. They are
not approved sweet spots. Loop explicitly captures before playback; its empty
default instance is not misrepresented as sounding. Impulse uses the current
plugin starter, not historical core defaults.

Each product has raw `source/`, matched `audio/`, measurements, answer keys and
blank response sheets. **Use `audio/` to audition**: raw floating diagnostics
can exceed 0 dBFS. Matching occurs outside the graph using independently
checked BS.1770-style K weighting/gating. It remeasures after gain and lowers
one shared comparison target when needed for ≤0.8 sample peak. This is not
true-peak limiting or certified metering. Unknown levels remain unknown.
Source edges and cutoff transients are intentional; maximum sample delta
alone cannot identify processor clicks.

The meter has an independent long-double direct-form/window-sum reference.
For the high-contrast gate fixture, both give −20.159260823 LUFS (117 eligible,
40 retained windows); removing the relative gate gives −24.819644950 LUFS and
fails the check. Lower contrast retains all 117 windows at −23.594335354 LUFS.
Neutral/cut/Harmonic-zero linked-gain controls and analytical amplitude/phase
checks constrain misleading nonlinear measurements.

The [listening index](LISTENING.md) records export verification and artifact locations.
No files were auditioned by a human as part of this audit. No preferences,
Cubase validation, real-project excerpt analysis or native accessibility
results are inferred from rendering. Listening and composition tasks in the
backlog remain the acceptance boundary.

## Local performance observations

Three five-second trials per configuration, 48/96 kHz and 128/511 frames,
with setup, file I/O and analysis outside the timed callbacks. Nonzero finite
output is checked. Table: median callback time as percentage of represented
audio duration at **48 kHz/128 frames**, plus largest observed callback in
those three trials. Clock/lambda overhead is included. No concurrent audit
build/render was started during this benchmark; unrelated system load is not
controlled. These are short local observations, not worst-case deadlines,
supported-hardware certification or Cubase CPU measurements.

| Graph | Median % realtime | Maximum callback µs | Core object bytes |
|---|---:|---:|---:|
| Density | 0.268732 | 21.250 | 4856 |
| Harmonic | 0.641886 | 31.375 | 1600 |
| Harmonic linked gain | 0.665038 | 34.541 | 1600 |
| Sequence | 0.283170 | 10.416 | 360 |
| Sequence candidate | 0.279251 | 17.083 | 360 |
| Impulse, eight active tracks | 1.191260 | 46.083 | 584 |
| Field FOREVER | 0.382799 | 20.958 | 1180056 |
| Loop, snapshots enabled | 0.549483 | 25.667 | 720 |
| Loop candidate, snapshots enabled | 0.562548 | 27.500 | 720 |

[All 108 timing observations](evidence/benchmark.csv) include 96 kHz and
511-frame results. Loop's 720-byte object excludes variable tape/snapshot heap;
that runtime allocation size is **unknown**, not zero. Other listed cores use
fixed storage. All six production processor APIs report zero algorithmic
latency in their normal paths; this is a source contract, not a new end-to-end
host measurement. Density's unselected oversampling prototype is different.

## Verification and retained failures

- Baseline core suite: 50/51 passed; dependency-security review expired
  2026-11-07. No waiver or review-date fabrication.
- All six existing VST3 targets built with cached pinned JUCE. Full plugin
  suite initially 66/68: expired security review and the audit's then-missing
  evidence-document link. The link is repaired and its CTest recheck passes:
  combined result 67/68, with the expired security review still failing.
- Experimental measurement/candidate contract is separate from the genuinely
  failing existing Sequence host-automation assertion. The Sykli contract
  deliberately runs only that experimental contract in Release and ASan/UBSan;
  it must not be described as an all-green product or full laboratory suite.
- Sequence/Loop candidate assertions failed before substitutions. Faulted
  linked +12/0 gain fails the neutral control; missing loudness relative gate
  fails the independent oracle; added Loop DC fails zero-motion identity;
  injected extreme-motion NaN fails finite-output checks.
- Callback ordinary allocation/deallocation interception is calibrated with a
  positive control, then observes zero for the tested candidate paths,
  including Loop snapshots. It is not a universal OS-operation audit.
- Generated candidates pass Clang static analysis. Research C++ formatting and
  strict warning builds pass. Existing adapter tests, not headless screenshots,
  supply the automated host-boundary/state/automation evidence.

Sanitizer, archive/repeat-export and final exact-commit receipt results are
recorded in the [listening/verification index](LISTENING.md). Outstanding musical acceptance,
native UI accessibility and Cubase work are not disguised as completed tests.
