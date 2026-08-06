# Cycle 06 — Dual-time detector

## What changed

Added a lab-only dual-time-constant detector to the existing comparison and
audition tools. A 0.1/60 ms fast follower and 20/600 ms slow follower run in
parallel; the larger envelope controls the common gain computer.

## Why

The RMS/peak candidate preserves transients and recovers sooner. The dual-time
candidate tests the opposite useful hypothesis: preserve part of the transient
while retaining longer programme memory after sustained energy.

## Evidence

- Sustained gain reduction differs from current by 0.056 dB.
- Ten-millisecond burst reduction differs by 0.058 dB.
- Isolated impulse reduction matches current within 0.001 dB.
- Recovery is 275.229 ms faster after the short burst and 383.500 ms slower
  after sustained material.
- Four three-way audition sets are matched within 0.001 dB and written as
  48 kHz Float32 WAV files.
- Re-rendering all twelve files produces identical SHA-256 hashes.

## Risks

The fixed 60/600 ms constants are research values, not product mappings. Long
memory may add desirable continuity or audible pumping; objective traces cannot
decide that. Fixtures remain synthetic and mono.

Cycle 7 corrected these figures by resetting detector state between events;
the earlier 200 ms gaps allowed recovery carry-over.

## Next step

Blind-listen to current, RMS/peak, and dual-time files. In parallel, prototype
the required programme-dependent-release topology in the lab without exposing
another product parameter.
