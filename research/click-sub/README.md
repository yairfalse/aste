# Click/sub laboratory

See the [study and reproduction commands](../../docs/research/click-sub/README.md).
Only the existing Impulse processor and its two new starting points are linked.
No new dependency, source engine or host contract is introduced.

`--check` verifies sound bounds at six sample rates. `--fault` changes the sub
to 160 Hz and must fail. `--pack NEW_DIRECTORY` exports four matched pattern
comparisons and four isolated objects at actual levels. The meter/FFT/WAV
implementation is reused from `research/quality/audio.hpp`.
