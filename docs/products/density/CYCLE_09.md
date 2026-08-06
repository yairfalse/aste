# Cycle 09 — Feedback-inspired detector

## What changed

Added the sixth and final lab topology: a bounded zero-delay feedback-inspired
control loop. Each sample uses six damped fixed-point iterations and a 3.65x
sidechain calibration at the research operating point.

## Why

A detector observing predicted attenuated output behaves differently from the
five feed-forward candidates. The bounded solve tests that behavior without a
one-sample control delay or an unbounded iteration on the audio thread.

## Evidence

- Sustained reduction matches current within 0.003 dB.
- Sustained release matches within 0.063 ms.
- Impulse reduction is 4.024 dB harder; burst reduction is 1.947 dB lighter.
- Short-burst recovery is 83.813 ms faster.
- Maximum solved-gain residual is 0.000419 after six iterations.
- Four six-way audition sets are RMS-matched within 0.001 dB.
- All twenty-four WAVs parse as 48 kHz IEEE Float and reproduce identical
  SHA-256 hashes after regeneration.

## Risks

The calibration is valid only at this operating point. Six iterations are too
expensive to assume suitable for production, and the aggressive impulse
response may be musically unacceptable. Fixtures remain synthetic and mono.

## Next step

Stop adding detectors. Run the six-way blind listening protocol, eliminate
clear failures, and only then move one or two finalists into compile-time
production experiments with CPU and stereo measurements.
