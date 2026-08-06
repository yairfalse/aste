# Cycle 05 — Detector auditions

## What changed

Added a deterministic audition renderer producing current-peak and RMS/peak
detector pairs for transient, bass-heavy, dense, and ambient generated fixtures.
Outputs are mono 48 kHz Float32 WAV files, four seconds each.

## Why

The Cycle 4 measurements showed a meaningful transient/recovery difference.
Audition files isolate that difference by applying the two reduction traces to
identical input while excluding saturation and make-up.

## Evidence

- Eight WAV files pass RIFF/IEEE Float inspection and macOS audio parsing.
- Re-rendering all eight files produces identical SHA-256 hashes.
- Every pair is sample-RMS matched within 0.001 dB.
- Candidate pre-match level ranged from 0.050 to 3.566 dB louder depending on
  fixture, demonstrating programme-dependent behavior rather than fixed gain.
- Release and ASan/UBSan CTest runs include deterministic audition generation.

## Risks

The fixtures are synthetic, mono, and cover one detector operating point.
They do not replace blind listening on real music or prove that the candidate
belongs in the product.

## Next step

Run the documented blind listening protocol. If the RMS/peak candidate wins
repeatedly across at least three fixture classes, add it as a compile-time DSP
experiment—not a user parameter—and measure its CPU and stereo behavior.
