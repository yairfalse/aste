# Cycle 21 — Streaming 4x prototype

## What changed

Added a fixed, Density-local 4x polyphase crush-path prototype with preallocated
state. Added differential alias, impulse latency, variable-block, allocation,
memory, and five-run stereo CPU checks. The production processor remains 1x.

## Why

The offline 4x reference cannot establish whether a causal streaming
implementation meets real-time correctness and performance requirements.

## Evidence

- Residual folded energy is -78.155 dBc, improving the 1x path by 59.365 dB and
  matching the offline reference within 0.001 dB.
- Fundamental shift is -0.000018 dB.
- Measured impulse-peak latency is 64 samples.
- Fixed and thirteen-size variable block renders are sample-identical.
- Processing performs zero observed heap allocations and state is 2,336 bytes
  per channel.
- Repeated five-run medians measure about 3.60% of one M4 Pro core for stereo
  at 48 kHz / 127 samples; three isolated medians span 3.499–3.696%.
- Release and ASan/UBSan VST3 suites both pass 11/11. Sanitizers run all
  correctness checks but skip the non-actionable instrumented timing loop.

## Risks

The prototype uses scalar convolution and one fixed 64-tap-per-phase filter.
CPU misses the provisional 1.0% default budget, 96 kHz cost is not measured,
and dry-path alignment and host latency reporting are intentionally absent
because the prototype is not integrated.

## Next step

Compare 4x filters at 16, 32, 48, and 64 taps per phase for alias suppression,
latency, passband gain, and CPU before considering code-level optimization.
