# Real-time safety

`DensityProcessor::process` and `harmonic::Processor::process` are `noexcept`
and perform no allocation, release, locking, I/O, logging, container resizing,
lazy initialization, or system call. Their state is fixed-size and prepared
before processing. Global allocation counters cover both cores and full JUCE
processor boundaries.

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

Current DSP ownership is single-audio-thread only. Each adapter publishes three
meter snapshots through `std::atomic<float>` guarded by a compile-time lock-free
assertion on supported targets; UI-side decay occurs on the message thread.

On macOS, both plugin tests link a test-only dynamic library using dyld
interposition. A thread-scoped guard counts POSIX mutex/rwlock/condition waits,
unfair locks, `fopen`/`open`/`openat`, and raw `write` calls during
`processBlock`. The guard deliberately performs one lock, open, and write before
the test and fails if those hooks do not self-calibrate. This is targeted
runtime evidence, not proof that every possible OS API is absent.
