# Grain G-01 0.9.0 — playable build evidence

Built and installed 2026-11-16. Universal arm64/x86_64 VST3, mono/stereo audio
input/output, Fx/Delay category, 94 parameters, zero reported computational
latency and an infinite tail declaration for retained material.

Completed:

- Product engine tests at six rates, exact block invariance at 1/127/511 frames,
  finite bounded output and 48/24/16 per-node rate-dependent voice limits.
- Full adapter tests: effect buses, parameter count, byte-stable control recall,
  malformed-state rejection, six rates, zero callback allocation/free and audited
  OS calls, retained freeze, dry bypass, final Master, mono folding, routing
  scene sound preservation, empty-freeze arming and native minimum editor bounds.
- Non-Machine and Resonator regression tests pass after extending their shared
  parameter/UI boundary to support an audio effect.
- All five selected CTest checks pass, including the dynamic VST3 host.
- Product core ASan/UBSan executable exits 0 with no diagnostics. Its timing
  printout is instrumented and must not be used as a performance budget.
- Native minimum editor image generated and visually inspected.
- Product audio rendered through the JUCE adapter, not only the research core:
  eight-second wet chord/held-seed example, peak 0.0702784, RMS 0.0192104.
- The installed system bundle passes strict/deep signature verification, both
  architecture checks and dynamic host load/state/process validation at six
  buffer sizes. No Cubase listening session is claimed.

Release core benchmark at saturated prepared voice pools, 127-frame calls:
48 kHz Sinc16 median 613.42 µs, p99 869.71 µs, maximum 989.33 µs against a
2645.83 µs deadline; 192 kHz with its smaller pool: median 216.25 µs,
p99 423.13 µs, maximum 435.75 µs against 661.46 µs. These are local wall-clock
measurements under ordinary desktop load, not hard realtime guarantees or
multi-instance Cubase performance claims.

Local artifacts: `build-evidence/grain-playable/`, including native editor,
callback tests, sanitizer log, release performance, product WAV, source hashes
and installed verification. Installation receipt:
`/Users/yair/Library/Application Support/Aste/Plugin Backups/20261116-183854-0.9.0-bi8wvibn`.
Only Grain was installed; the existing eight system bundles remain 0.8.0.

Captured audio is not serialized. Parameters and freeze choices recall; empty
recalled freezes wait for fresh input. Render frozen results before closing.
Sample-offset core events do not imply sample-accurate VST3 automation: the JUCE
boundary currently samples control values once per callback.
