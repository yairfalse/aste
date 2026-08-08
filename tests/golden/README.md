# Density production golden

`density-production-v1.csv` is the reviewed metric baseline for four generated
stereo fixtures at 48 kHz, rendered in 127-sample blocks with production DSP.

Reference settings:

- Drive: 9 dB
- Crush: 82%
- Attack: 0.25 ms
- Release: 240 ms
- Density: 72%
- Blend: 58%
- Stereo: 65%
- Detector HPF: 90 Hz
- Output: -1 dB
- Protection: on

CTest permits 0.02 dB RMS, peak, and gain-change movement; 0.03 dB crest
movement; 0.002 correlation movement; and 0.05 dB gain-reduction movement.
Latency, rate, frame count, and block size must match exactly. The FNV-1a sample
fingerprint is review evidence but is not the sole gate.

Generate candidates with:

```sh
./build/density_lab --golden build/density_golden \
  tests/golden/density-production-v1.csv
```

The command never rewrites this baseline. Review the generated WAVs and metric
differences, then edit the tracked CSV explicitly if the DSP change is accepted.
