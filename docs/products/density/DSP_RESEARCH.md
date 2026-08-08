# Density DSP research log

## Cycle 1 hypothesis

A zero-latency, feed-forward linked peak detector is the smallest reference
topology. The detector has a first-order high-pass filter, fast attack, and a
release that lengthens continuously with detector level as Density rises. The
gain computer operates in decibels with a hard knee. Density lowers threshold
and raises ratio, saturation drive, release curvature, and crush make-up.

This is an original behavioural prototype, not an emulation. Its `tanh`
saturator is not oversampled and is expected to alias under strong high-frequency
drive. That limitation is intentional and must be quantified before choosing an
oversampler. The output clipper is a memoryless cubic knee; protection is
sample-peak only and must not be called true-peak safe.

## Cycle 19 — Nonlinear alias baseline

The lab measures the actual production transfer functions with a coherent tone
near 7 kHz. Saturation receives a 0.9-peak sine at the production Density 72%
drive of 3.88. The crush result then passes through its controlled clipper; the
protection clip is measured separately with a 1.1-peak sine. Values are summed
folded energy from odd harmonics 3 through 63 relative to the fundamental, not
total broadband alias power.

| Sample rate | Saturation | Saturation + crush clip | Protection clip |
|---:|---:|---:|---:|
| 44.1 kHz | -19.303 dBc | -18.790 dBc | -33.511 dBc |
| 48 kHz | -19.303 dBc | -18.790 dBc | -33.511 dBc |
| 88.2 kHz | -27.013 dBc | -26.761 dBc | -42.018 dBc |
| 96 kHz | -27.013 dBc | -26.761 dBc | -42.018 dBc |
| 176.4 kHz | -49.774 dBc | -49.516 dBc | -56.780 dBc |
| 192 kHz | -57.347 dBc | -54.592 dBc | -61.761 dBc |

The approximately -19 dBc crush-path result at base rates is too large to
ignore. It justifies comparing oversampled implementations, but this bounded
single-tone measurement does not yet select a factor, filter, or quality mode.

## Cycle 20 — Offline oversampling references

A lab-only Blackman-windowed sinc reference uses 64 taps per phase for both
interpolation and decimation. Circular convolution avoids startup leakage in
the coherent measurement; the corresponding causal round trip is 64 base-rate
samples for both factors.

| Factor | Filter taps | Crush-path alias | Improvement | Fundamental shift |
|---:|---:|---:|---:|---:|
| 1x | 0 | -18.790 dBc | 0.000 dB | 0.000000 dB |
| 2x | 129 | -43.057 dBc | 24.267 dB | -0.000018 dB |
| 4x | 257 | -78.155 dBc | 59.365 dB | -0.000018 dB |

At 48 kHz, 4x provides 35.098 dB more crush-path suppression than 2x without a
meaningful additional gain or reference-filter latency cost. This supports a
4x production prototype, not yet an ADR: a real-time polyphase implementation,
CPU cost, transitions, and host latency reporting remain unmeasured.

## Cycle 21 — Streaming 4x prototype

A concrete Density-local class implements 4x polyphase interpolation, the
production saturation-plus-crush-clip transfer, and decimation with fixed
preallocated state. It is not connected to `Processor`.

| Measurement | Result |
|---|---:|
| Residual folded energy | -78.155 dBc |
| Improvement over 1x | 59.365 dB |
| Difference from offline reference | <0.001 dB |
| Fundamental shift | -0.000018 dB |
| Measured impulse-peak latency | 64 samples |
| Variable-block maximum delta | 0 samples |
| State per channel | 2,336 bytes |
| Stereo CPU, 48 kHz / 127, M4 Pro | about 3.60% of one core |

The prototype proves the reference can stream deterministically without audio
thread allocation, but its scalar 64-tap-per-phase implementation misses the
1.0% default CPU budget. Production integration is deferred while shorter 4x
filters are measured; custom SIMD is not justified yet.

## Cycle 22 — FIR length comparison

The streaming prototype now accepts a prepare-time FIR length without changing
its fixed maximum storage or allocating. Four coherent tones at approximately
7, 8.5, 10, and 15 kHz expose both the earlier operating point and the critical
transition band.

| Taps/phase | Latency | Worst alias | Maximum gain shift | Stereo CPU |
|---:|---:|---:|---:|---:|
| 16 | 16 samples | -22.014 dBc | 0.004537 dB | 1.054–1.061% |
| 32 | 32 samples | -28.493 dBc | 0.006929 dB | 1.956–1.973% |
| 48 | 48 samples | -37.661 dBc | 0.006099 dB | 2.801–2.827% |
| 64 | 64 samples | -50.920 dBc | 0.006428 dB | 3.692–3.705% |

The 7 kHz result alone understated the filter requirement: 16 taps still
measures -77.873 dBc there, but degrades to -22.014 dBc at 8.5 kHz where the
third harmonic enters the decimator transition. No tested length combines the
1.0% default CPU budget with credible suppression. A structurally cheaper
filter topology is required before SIMD or product integration is justified.

## Cycle 23 — Sparse two-stage half-band prototype

The second 4x prototype cascades two 2x Blackman-windowed half-band stages and
omits the analytically zero coefficients. The first stage controls rejection
around the base-rate Nyquist limit; the second controls the 2x-to-4x image.

| Stage taps | Latency | Worst alias | Stereo CPU |
|---:|---:|---:|---:|
| 33/33 | 24 samples | -22.013 dBc | 0.753–0.757% |
| 65/33 | 40 samples | -28.492 dBc | 0.901–0.910% |
| 97/33 | 56 samples | -37.660 dBc | 1.152–1.164% |
| 113/33 | 64 samples | -43.643 dBc | 1.293–1.303% |
| 129/33 | 72 samples | -50.919 dBc | 1.399–1.402% |
| 97/65 | 64 samples | -37.661 dBc | 1.531–1.537% |

At equivalent rejection, the sparse cascade reduces CPU by 28–62% versus the
direct FIR. Extending only the second stage from 33 to 65 taps changes worst
alias by less than 0.001 dB while adding roughly 0.38% CPU and 8 samples, so it
is rejected. The first stage remains the quality/cost boundary. Production is
still unchanged.

## Cycle 24 — Kaiser half-band coefficients

The 113/33 sparse topology remains fixed at 64 samples latency. Only the first
stage's prepare-time window changes; therefore runtime operations and the
measured 1.293–1.303% stereo CPU range are unchanged.

| First-stage window | Worst alias | Maximum gain shift |
|---|---:|---:|
| Blackman | -43.643 dBc | 0.005654 dB |
| Kaiser β3 | -51.524 dBc | 0.013571 dB |
| Kaiser β5 | -51.277 dBc | 0.006334 dB |
| Kaiser β7 | -49.992 dBc | 0.005604 dB |
| Kaiser β9 | -42.506 dBc | 0.005627 dB |
| Kaiser β11 | -38.470 dBc | 0.005652 dB |

β3 improves the four-tone worst case by 7.881 dB. β5 is only 0.247 dB worse
in that aggregate but measures -70.406 dBc at the critical 8.5 kHz tone versus
-65.258 dBc for β3, with half the maximum gain shift. These sparse tone points
are not sufficient to choose between sidelobe distributions; both advance to a
dense frequency and level sweep.

## Cycle 25 — Dense Kaiser sweep

Blackman, Kaiser β3, and Kaiser β5 were measured at 39 target frequencies from
1 to 20 kHz and three peak levels. Each target resolves to an odd coherent FFT
bin so folded high harmonics cannot collide with lower natural harmonics.

| First-stage window | Worst alias | 95th percentile | Points above -50 dBc | Maximum gain shift |
|---|---:|---:|---:|---:|
| Blackman | -17.555 dBc | -38.977 dBc | 24/117 | 0.003364 dB |
| Kaiser β3 | -17.578 dBc | -43.215 dBc | 20/117 | 0.066467 dB |
| Kaiser β5 | -17.564 dBc | -41.180 dBc | 20/117 | 0.011067 dB |

All three worst cases occur at 8.001 kHz and 0.90 peak, where the third
harmonic reaches the base-rate Nyquist transition. The windows differ by only
0.022 dB there. β3 has the best minimax and 95th-percentile alias results, but
its maximum fundamental shift near 19.5 kHz is 0.055 dB greater than β5. β3 is
the alias-rejection candidate; production remains unchanged pending multi-rate
measurement and the unresolved 1% CPU budget.

## Cycle 26 — Supported-rate Kaiser sweep

β3 and β5 were measured from 1 to 20 kHz at all six supported sample rates,
retaining the three Cycle 25 levels and collision-free coherent bins. Each
window produced 702 measurements.

| Sample rate | β3 worst | β5 worst | β3 p95 | β5 p95 |
|---:|---:|---:|---:|---:|
| 44.1 kHz | -27.991 dBc | -25.011 dBc | -35.769 dBc | -35.781 dBc |
| 48 kHz | -17.578 dBc | -17.564 dBc | -43.215 dBc | -41.180 dBc |
| 88.2 kHz | -27.991 dBc | -25.011 dBc | -52.399 dBc | -49.303 dBc |
| 96 kHz | -17.578 dBc | -17.564 dBc | -51.649 dBc | -43.597 dBc |
| 176.4 kHz | -36.314 dBc | -33.271 dBc | -62.674 dBc | -59.685 dBc |
| 192 kHz | -32.976 dBc | -31.061 dBc | -65.123 dBc | -55.514 dBc |

β3 has the better 95th percentile at five of six rates and 62/702 points above
-50 dBc versus 71/702 for β5. β5 is marginally better at 44.1 kHz p95 and has
far flatter gain: its maximum shift is 0.017055 dB, while β3 reaches +0.106176
dB near 19.5 kHz at 44.1 kHz. The alias result favors β3; the passband result
favors β5. Neither advances to production until the high-frequency passband
behavior is isolated from the nonlinear comparison.

## Cycle 27 — Linear passband and phase

The nonlinear transfer was bypassed inside the same streaming half-band
prototype. A 250 Hz target grid measured magnitude and phase from 1 to 20 kHz
at every supported sample rate, producing 462 points per window.

| Sample rate | β3 maximum deviation | β5 maximum deviation |
|---:|---:|---:|
| 44.1 kHz | 0.127896 dB | 0.020496 dB |
| 48 kHz | 0.082985 dB | 0.013308 dB |
| 88.2 kHz | 0.044090 dB | 0.007219 dB |
| 96 kHz | 0.043968 dB | 0.007210 dB |
| 176.4 kHz | 0.040138 dB | 0.007017 dB |
| 192 kHz | 0.040223 dB | 0.007023 dB |

Both windows retain the expected 64-sample linear phase: maximum
latency-compensated residual is 0.000001 degrees. β3's nonlinear-report gain
shift was therefore filter passband behavior, not an alias-measurement
artifact. β5 stays within 0.0205 dB across the entire matrix while giving up
0.462 dB in aggregate alias p95. For a mastering signal path, β5 is selected
as the coefficient candidate. It remains lab-only because the 113/33 topology
still misses the CPU budget.

## Cycle 28 — β5 length crossover

The second stage remains fixed at 33 taps while β5 first-stage length moves
from 65 to 113 taps. Each topology receives the Cycle 25 48 kHz alias sweep,
the Cycle 27 44.1 kHz linear sweep, and the established stereo CPU benchmark.

| First-stage taps | Latency | Alias p95 | Points > -50 dBc | Max magnitude | CPU range |
|---:|---:|---:|---:|---:|---:|
| 65 | 40 | -35.267 dBc | 25/117 | 0.124032 dB | 0.923–0.929% |
| 73 | 44 | -36.313 dBc | 24/117 | 0.039351 dB | 0.958–0.961% |
| 81 | 48 | -38.102 dBc | 24/117 | 0.038969 dB | 0.986–0.998% |
| 89 | 52 | -39.144 dBc | 24/117 | 0.027025 dB | 1.111–1.121% |
| 97 | 56 | -39.791 dBc | 22/117 | 0.020957 dB | 1.170–1.177% |
| 113 | 64 | -41.180 dBc | 20/117 | 0.020496 dB | 1.288–1.294% |

The 65-tap candidate fails the provisional 0.1 dB passband bound. Lengths 89
and above fail the local CPU target. This leaves 73/33 as the margin candidate
and 81/33 as the quality candidate. The latter loses 3.078 dB of alias p95
versus 113/33 and has only 0.002–0.014 percentage points of measured CPU margin.
Neither is selected for production before supported-rate comparison.

## Cycle 29 — Six-rate topology finalist

The 73/33 and 81/33 β5 finalists each receive 702 nonlinear alias measurements
and 462 linear-response measurements across all supported sample rates.

| Taps | Latency | Aggregate alias p95 | Points > -50 dBc | Max magnitude | Phase residual |
|---:|---:|---:|---:|---:|---:|
| 73/33 | 44 samples | -37.266 dBc | 87/702 | 0.039351 dB | 0.000001° |
| 81/33 | 48 samples | -38.516 dBc | 86/702 | 0.038969 dB | 0.000001° |

81/33 improves aggregate alias p95 by 1.250 dB and moves one additional point
below -50 dBc. Passband differs by only 0.000382 dB and phase is equivalent.
Those quality gains do not justify consuming nearly all local CPU margin and
adding four samples. The 73/33 β5 topology is retained as the sole integration
candidate; 81/33 is rejected.

The retained topology's 0.958–0.961% measurement covers only the oversampler.
Combined with the existing processor baseline it cannot yet satisfy the 1%
whole-instance default budget, so this decision does not authorize production
integration or a quality-mode claim.

## Cycle 30 — End-to-end 73/33 prototype

An explicitly lab-only processor mode places the retained β5 73/33 filters
around the crush-path saturation and clipper, delays both dry channels by 44
samples, and leaves output protection at base rate. The plugin adapter cannot
select this mode.

| Measurement | Result |
|---|---:|
| Reported latency | 44 samples |
| Measured dry latency | 44 samples |
| Measured wet latency | 44 samples |
| Variable-block maximum delta | 0 samples |
| Production full-chain CPU | 0.231–0.231% |
| Oversampled full-chain CPU | 1.042–1.043% |
| Incremental CPU | 0.810–0.812 percentage points |

Processing remains allocation-free under the core counter. The dry and wet
impulse peaks align exactly, and the thirteen-size variable schedule is
sample-identical to fixed 127-sample processing. The CPU result fails the 1%
default-quality budget in all three runs, so the prototype does not advance to
the product path. A quality/latency policy is required before deciding whether
it belongs in a higher-quality mode or should be optimized further.

## Cycle 32 — Blind oversampling auditions

The lab now renders the production 1x graph against the lab-only β5 73/33
graph as four deterministic anonymous stereo pairs. The 1x graph is delayed by
44 samples before whole-file RMS matching; both versions then receive the same
peak normalization to -1 dBFS.

| Fixture | RMS match error | Null RMS |
|---|---:|---:|
| Transient | 0.000000148 dB | -59.752 dBFS |
| Bass | -0.000000114 dB | -109.058 dBFS |
| Dense | -0.000000171 dB | -47.440 dBFS |
| Ambient | -0.000000033 dB | -86.253 dBFS |

Fixed seed `0xD0132` reproduces the A/B assignment. The distinct null residuals
show programme-dependent differences after alignment and level matching; they
do not establish audibility or preference. No listening conclusion has been
recorded, and these synthetic fixtures do not replace musical material.

## Candidate set

The required six topologies have now been rendered against the same fixtures.
No additional detector should be added until listening ranks this set.

## Cycle 4 — RMS-like detector with peak influence

The first alternative is deliberately lab-only. It combines a 10 ms
mean-square integration with 15% instantaneous squared level, applies a
sqrt(2) calibration so a steady sine matches the peak detector, and reuses the
production gain-computer mapping and nonlinear release law.

At Density 70%, Crush 100%, attack 0.1 ms, release 180 ms, and detector HPF
20 Hz, the deterministic fixture measured:

| Measurement | Current peak | RMS/peak candidate |
|---|---:|---:|
| Sustained gain reduction | 11.239 dB | 11.269 dB |
| Full-scale impulse gain reduction | 3.055 dB | 0.000 dB |
| 10 ms burst gain reduction | 15.225 dB | 13.527 dB |
| Short-burst release to below 1 dB | 469.792 ms | 415.125 ms |
| Sustained release to below 1 dB | 308.125 ms | 328.229 ms |

The candidate matches sustained reduction within 0.03 dB, applies no gain
reduction to the isolated single-sample impulse, and recovers 54.67 ms sooner
after the short burst. Those differences justify audition renders, not a
production topology change.

## Cycle 5 — Level-matched auditions

The lab applies each detector's gain-reduction trace directly to the same
generated input, excluding saturation and make-up so the detector remains the
only changing stage. Each pair is sample-RMS matched and then receives a common
gain placing the louder peak at -1 dBFS.

| Fixture | RMS match error | Candidate attenuation before common gain |
|---|---:|---:|
| Transient | 0.000 dB | -2.646 dB |
| Bass | 0.000 dB | -0.050 dB |
| Dense | 0.000 dB | -3.566 dB |
| Ambient | 0.000 dB | -1.535 dB |

The variable attenuation confirms meaningful programme dependence and prevents
loudness bias during listening. These synthetic renders are diagnostic, not a
substitute for copyrighted or user-supplied musical fixtures.

## Cycle 6 — Dual-time-constant detector

The second alternative runs a fast peak follower with 0.1 ms attack and 60 ms
release beside a slow follower with 20 ms attack and 600 ms release. The larger
envelope drives the same gain computer used by the other traces.

| Measurement | Current peak | RMS/peak | Dual-time |
|---|---:|---:|---:|
| Sustained gain reduction | 11.239 dB | 11.269 dB | 11.183 dB |
| Full-scale impulse gain reduction | 3.055 dB | 0.000 dB | 3.055 dB |
| 10 ms burst gain reduction | 15.225 dB | 13.527 dB | 15.168 dB |
| Short-burst release to below 1 dB | 469.792 ms | 415.125 ms | 194.563 ms |
| Sustained release to below 1 dB | 308.125 ms | 328.229 ms | 691.625 ms |

The dual-time candidate nearly matches current sustained, impulse, and burst
depth. It releases 275.229 ms sooner after the short burst but 383.500 ms later
after sustained material. Its audition level required 0.344–0.812 dB
attenuation across the four generated fixtures before common peak normalization.

## Cycle 7 — Measurement correction and programme memory

Earlier comparison fixtures left only 200 ms between events, shorter than the
measured detector recovery. Their impulse and burst figures therefore included
carry-over from sustained material. Cycle 7 resets every detector before the
impulse and burst and verifies 0 dB of pre-event reduction. The corrected
figures above supersede the earlier values.

The programme-memory candidate uses the common 0.1 ms peak attack. A 100 ms
attack / 600 ms release activity memory continuously moves release time between
60 and 600 ms. It measured:

| Measurement | Current peak | Programme memory |
|---|---:|---:|
| Sustained gain reduction | 11.239 dB | 11.245 dB |
| Full-scale impulse gain reduction | 3.055 dB | 3.055 dB |
| 10 ms burst gain reduction | 15.225 dB | 15.183 dB |
| Short-burst release to below 1 dB | 469.792 ms | 177.792 ms |
| Sustained release to below 1 dB | 308.125 ms | 420.104 ms |

This candidate changes recovery according to programme duration while matching
steady, impulse, and burst depth within 0.05 dB. It is lab-only.

## Cycle 8 — Hybrid feed-forward control

The hybrid candidate reuses the 10 ms calibrated RMS body path and 0.1/60 ms
fast peak path. Each path computes gain reduction independently; the result is
the body reduction plus 35% of any excess peak reduction. This is a parallel
feed-forward control law rather than another blended detector envelope.

| Measurement | Current peak | RMS/peak | Hybrid feed-forward |
|---|---:|---:|---:|
| Sustained gain reduction | 11.239 dB | 11.269 dB | 11.267 dB |
| Full-scale impulse gain reduction | 3.055 dB | 0.000 dB | 1.069 dB |
| 10 ms burst gain reduction | 15.225 dB | 13.527 dB | 13.960 dB |
| Short-burst release to below 1 dB | 469.792 ms | 415.125 ms | 88.021 ms |
| Sustained release to below 1 dB | 308.125 ms | 328.229 ms | 59.500 ms |

The hybrid matches steady compression within 0.029 dB, places transient and
burst depth between the RMS and peak paths, and recovers substantially faster.
It was 0.681–3.416 dB louder than current across generated fixtures before RMS
matching. It remains lab-only.

## Cycle 9 — Feedback-inspired control

The final candidate predicts the attenuated detector signal inside a bounded
zero-delay control loop. Six damped fixed-point iterations run per sample. A
3.65x sidechain calibration matches the current detector at the documented
sustained operating point; it is not a general calibration.

| Measurement | Current peak | Feedback-inspired |
|---|---:|---:|
| Sustained gain reduction | 11.239 dB | 11.241 dB |
| Full-scale impulse gain reduction | 3.055 dB | 7.079 dB |
| 10 ms burst gain reduction | 15.225 dB | 13.279 dB |
| Short-burst release to below 1 dB | 469.792 ms | 385.979 ms |
| Sustained release to below 1 dB | 308.125 ms | 308.188 ms |

The maximum residual between the damped gain estimate and solved gain was
0.000419. The topology matches steady and sustained-release behavior but reacts
4.024 dB harder to the isolated impulse and 1.947 dB less to the burst. This
non-intuitive result is retained for listening rather than normalized away.

## Objective behavior map

| Topology | Defining measured behavior |
|---|---|
| Current peak | Reference; deepest burst reduction and long short-burst recovery |
| RMS/peak | No single-sample reduction; moderate recovery |
| Dual-time | Current-like depth; fast short recovery, longest sustained recovery |
| Programme memory | Current-like depth; release follows event duration |
| Hybrid feed-forward | Intermediate transient depth; fastest recovery |
| Feedback-inspired | Hardest isolated impulse; lighter burst; current-like sustained recovery |
