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
