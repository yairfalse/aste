# DSP research policy

Production algorithms must enter through a falsifiable question, a deterministic
reference or comparison, machine-readable measurements, and listening where the
decision is perceptual. A successful build is not evidence of sonic quality.

Density's experiments, measurements, limitations, and selected production
behavior are recorded in
[docs/products/density/DSP_RESEARCH.md](docs/products/density/DSP_RESEARCH.md).
Cycle reports in `docs/products/density/` retain the before/after evidence and
decision sequence. The executable methods are documented in [TESTING.md](TESTING.md).

Rules:

- Keep research-only algorithms unreachable from released product state until
  selected by evidence.
- Compare optimized code against a simpler or higher-precision reference.
- Test nonlinear stages for aliasing across level, frequency, and sample rate.
- Separate measurement validity from an unvalidated perceptual threshold.
- Never update a golden baseline automatically.
- Record negative results; they prevent repeated mythology.
- Use “inspired by” or “topology-informed” unless measurements support a
  stronger claim.
