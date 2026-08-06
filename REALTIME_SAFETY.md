# Real-time safety

`DensityProcessor::process` is `noexcept` and performs no allocation, release,
locking, I/O, logging, container resizing, lazy initialization, or system call.
All state is fixed-size and prepared before processing. The test executable
counts global allocations around representative process calls.

Rules for future processing code:

- Prepare memory and coefficients before audio starts.
- Bound every loop by the host-provided channel/frame count or a fixed constant.
- Sanitize restored parameters before they reach DSP.
- Replace non-finite input with silence and keep output finite.
- Handle zero frames and non-power-of-two frames.
- Flush denormal-prone state to zero explicitly; hosts may also enable FTZ/DAZ.
- Do not throw through the process boundary.
- Do not assume `std::atomic<T>` is lock-free; verify the exact type/platform
  before using it between audio and UI threads.

Current ownership is single-audio-thread only. A future meter bridge must use
verified lock-free atomics or a bounded wait-free snapshot, with a test and ADR.
