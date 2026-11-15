# Loop and Field diagnostic methods

The isolated [measurement implementation](../../../research/power/space_diagnostics.hpp)
calls unchanged production Loop/Field processors and the separately named Loop
clock candidate. It performs file I/O only after processing calls. Fixtures
are generated, deterministic and bounded; none is a licensed recording or a
human listening result. The parent laboratory invokes `spaceDiagnostics(dir)`
for CSV evidence and `spaceChecks()` for correctness gates.

## Loop capture and generations

`loop_capture.csv` feeds a 0.1-peak 440 Hz sine for exactly one second into
fresh one-second tape storage at 48 kHz. Only Overdub changes: 0%, default 50%,
100%. All other controls retain production defaults except the explicit
one-second duration. Input RMS and **actual saved tape-sample RMS** are read
separately; the latter uses the existing off-thread snapshot interface. A
following second with Capture off supplies observed playback RMS, including
production playback loading, modulation and output trim. Exact silent tape
is written as `-inf` gain, not a fabricated finite floor or missing value.

`loop_generations.csv` preloads a reproducible one-second composite of 220 and
733 Hz tones plus decaying noise packets. Each of three settings renders
generations 1–4, with a full one-second RELOOP print between observations:
default Record/Loss, both maximum, and Loss maximum with Record zero. Motion
and pitch are zero, speed is one, output is 0 dB and splice is 3%. Each record
includes actual generation/history counts, stereo RMS, mid-channel Hann-window
FFT centroid, power fraction above 8 kHz and unshifted left-channel correlation
to the first generation. Neither decreasing correlation nor increasing energy
is scored as an improvement. Missing energy-dependent ratios are `unknown`.
The 8 kHz fraction is spectral content, **not an aliasing measurement**.

## Loop pitch interactions

`loop_pitch_interactions.csv` uses a preloaded, low-level 440 Hz sine, with
motion/loading/loss/splice disabled, 0 dB output and two seconds of playback.
It covers 0.5/1/2× speed, −12/0/+12 semitones and forward/reverse: 18 cases.
The strongest FFT peak between 20 Hz and 4 kHz is measured with a 65,536-sample
Hann window and log-parabolic interpolation. The comparison column
`440 * speed * 2^(pitch/12)` is the **conventional combined-transposition
expectation**, not a claim that existing code satisfies that contract.
Windowed pitch heads create sidebands; their strongest peak is not necessarily
a perceived fundamental. No new pitch engine is proposed or substituted here.

## Independent motion oracle and candidate boundary

Only the common modulation phase wrap changes in the candidate, from one
second to 1000 seconds. The latter is a common integer period of the existing
0.47, 6.71 and 0.173 Hz frequencies. Depths, frequencies, drift filter,
interpolation, gains, capture and state remain unchanged. Candidate generation
is owned by the parent's anchored CMake transformation; no production file is
edited.

For an independent check, the tape has a one-second length, speed one, Wow
100%, Flutter/Drift zero. The intended discrete displacement after N samples
is evaluated **in closed form**, not by another phase accumulator:

```text
w = 2π * 0.47 / Fs
sum(k=1..N, sin(k*w)) = sin(N*w/2) * sin((N+1)*w/2) / sin(w/2)
position = (N + 0.012 * sum) modulo Fs
```

`loop_motion_oracle.csv` compares the public playback-position meter to that
position every half second for four seconds, at 44.1/48/88.2/96/176.4/192 kHz,
for production and candidate. Circular error is in samples, accounting for
tape wrap. The candidate check requires at most 0.03 sample error, allowing
the existing float position meter's quantization. It does **not** bless the
production defect as expected behavior. Zero-motion candidate/production
audio must additionally be bit-identical over two seconds at all six rates.
Maximum-motion, speed, reverse, pitch and loading renders must remain finite.

The check was first compiled against a namespace-renamed but otherwise
unchanged Loop source. It exited 1 with
`Loop candidate fails independent integrated motion oracle`; the saved
failing-before log is `build-space/failing-before.log`. This establishes that
the oracle detects the original defect. A four-second test does not execute
the 1000-second wrap boundary or prove musical preference for the correction.
The sample-rate-dependent drift filter is deliberately not changed by this
candidate and remains a separate audit finding.

The corrected six-rate run passed. Its largest measured position error was
0.005534 sample, versus 5974.856 samples for production over the same four-second
observations. This is a timing-contract result, not proof of a better sound.
Source-identical motion-disabled renders matched exactly at every rate.
Two further isolated scratch-source faults exited 1 as expected: adding 0.001
to candidate playback output failed `Loop zero-motion control changed`, and
injecting NaN only at simultaneous maximum Wow/Flutter failed `nonfinite audio`
in the extreme-control check. Their logs are `build-space/zero-control.log`
and `build-space/finite-extreme.log`; no mutation was applied to working
production or candidate-generation files.

## Field decay, stereo behavior and meter witness

`field_decay.csv` covers six sample rates, FOREVER off/on, zero/default/maximum
Mass/Grain/Pitch/Motion/Distance, and same/opposed stereo polarity: **72
conditions**. Blend is fully wet and Output 0 dB. A one-second drive combines
110/330 Hz tones with deterministic noise; nine following seconds are genuine
zero input. Each second reports stereo RMS, output mid-channel RMS, ending
energy-meter value, ending retention coefficient and stereo sample peak.

Zero controls are not a promise of a static network: production has residual
motion, and FOREVER adds its documented minimum influences. FOREVER is set
before excitation, so it also changes input injection; the initial stored
states are not energy-matched. Raw output and decay relative to the first
silent second should be interpreted separately. Ten seconds is a bounded
diagnostic, not proof of multi-minute persistence or a measured T60 when a
60 dB decay has not occurred. Controls are not changed midway through these
curves; the main listening pack handles performance gestures.

`field_meter_witness.csv` drives production Field with 0.1 DC for five seconds,
then silence for one second, at all six rates with same/opposed input polarity.
Mass/Grain/Pitch/Motion/Distance are zero, FOREVER off, Blend fully wet, Output
0 dB. DC is an engineering witness, **not musical material**. The opposite-sign
input exercises cancellation in the internal mono projection while the stereo
output can remain occupied. The CSV reports actual processed outputs and the
public meter, not invented access to private delay energy. Comparing meter
values to output RMS does not turn them into common physical units.

`spaceChecks()` additionally processes maximum Field controls/FOREVER and
+12 dB output with a bounded opposed-polarity drive followed by silence at six
rates, requiring finite samples. It does not assert a preferred tail length,
spectral shape or musical impact. The long diagnostic matrix is separate from
the short correctness gate so sanitizer checks remain bounded.

## Interpretation boundary

These methods distinguish a correctable clock failure, predictable-but-confusing
gain/parameter laws, and an incompletely labeled state proxy. Only the clock
correction is an alternative implementation. Capture gain, pitch interactions,
Field topology and Field meter behavior remain production observations, not
unapproved product changes. Musical preference, live Cubase performance and
native interface interaction remain unassessed.

## Focused run observations

The standalone Release driver in `build-space/` completed the checks and all
CSV diagnostics on 2026-11-15. Its evidence is in `build-space/evidence/`; the
parent audit's final evidence record owns the committed/archived results.
These selected observations were actually rendered, not estimated from code:

| Probe | Observation |
|---|---|
| First capture, Overdub 0 / 50 / 100% | Saved sample RMS 0.0707107 / 0.0353553 / exactly 0; default capture attenuation −6.0206 dB |
| Default Record/Loss, generations 1–4 | Playback RMS 0.080506 / 0.166004 / 0.279011 / 0.368954; retained history counts 1 / 2 / 3 / 3 |
| Maximum Record/Loss, generations 1–4 | Playback RMS 0.073872 / 0.166169 / 0.193433 / 0.197491; correlation to first falls to 0.86253 |
| 440 Hz tape, forward 1×, Pitch +12 | Strongest observed spectral peak 908.739 Hz, compared with the conventional 880 Hz target; window sidebands prevent treating this as a clean oscillator pitch |
| 440 Hz tape, reverse 1×, Pitch +12 | Strongest peak in the measured 20–4000 Hz interval is 28.761 Hz; the conventional combined-transposition target is 880 Hz |
| Field DC witness, same-polarity input, 48 kHz, fifth driven second | Stereo output RMS 0.0167703 while the public stored-energy proxy is 6.60e−11 |
| Field DC witness, opposed-polarity input, same conditions | Stereo output RMS 0.00586979 while the public proxy is 1.50e−10 |

At 48 kHz with same-polarity drive and FOREVER on, the retention display was
0.999350 in all three control settings below. Output RMS nevertheless evolved
very differently:

| Control setting | First silent second RMS | Ninth silent second RMS |
|---|---:|---:|
| Zero | 0.0299781 | 0.00301072 |
| Default | 0.0219098 | 0.0000361859 |
| Maximum | 0.0122230 | 0.00000000731660 |

This does not demonstrate that maximum settings ought to be preferable or
lossless. It demonstrates that the current retention scalar does not explain
actual held-output decay. Nor does Loop's increasing generation level prove
musical evolution; matched listening must assess what survives once gain is
removed as a preference cue.
