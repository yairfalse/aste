# Real-time safety

`DensityProcessor::process` is `noexcept` and performs no allocation, release,
locking, I/O, logging, container resizing, lazy initialization, or system call.
All state is fixed-size and prepared before processing. Global allocation
counters cover both the core and full JUCE processor boundary.

The lab-only 73/33 processor mode uses fixed half-band state and two fixed
44-sample dry-delay arrays. The core allocation counter covers this processing
call as well. The mode is not reachable from the plugin adapter.

Rules for future processing code:

- Prepare memory and coefficients before audio starts.
- Bound every loop by the host-provided channel/frame count or a fixed constant.
- Sanitize restored parameters before they reach DSP.
- Replace non-finite input with silence and keep output finite.
- Handle zero frames and non-power-of-two frames.
- Repeated prepare/process/release cycles may allocate during setup, never in
  `processBlock` or its bypass path.
- Flush denormal-prone state to zero explicitly; hosts may also enable FTZ/DAZ.
- Do not throw through the process boundary.
- Do not assume `std::atomic<T>` is lock-free; verify the exact type/platform
  before using it between audio and UI threads.

Current ownership is single-audio-thread only. A future meter bridge must use
verified lock-free atomics or a bounded wait-free snapshot, with a test and ADR.

On macOS, the plugin test links a test-only dynamic library using dyld
interposition. A thread-scoped guard counts POSIX mutex/rwlock/condition waits,
unfair locks, `fopen`/`open`/`openat`, and raw `write` calls during
`processBlock`. The guard deliberately performs one lock, open, and write before
the test and fails if those hooks do not self-calibrate. This is targeted
runtime evidence, not proof that every possible OS API is absent.
