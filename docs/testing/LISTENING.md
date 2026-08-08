# Listening protocol

## Rules for every blind pack

Play only files under the pack's `audio/` directory and complete its
`responses.csv` before opening `answer-key.csv` or any `source/` material.
Disable loudness normalization and downstream dynamics. Confidence is `0` for
a guess, `1` slight, `2` repeatable, and `3` strong. Listen quietly and at a
normal level on monitors and headphones, then check mono. “No preference” is a
valid result.

Use this order to reduce fatigue:

1. `build-plugin/density-automation-auditions/` — identify artifacts during
   Drive, Attack, Blend, and simultaneous automation.
2. `build-plugin/density-macro-auditions/` — rank four settings from least to
   most dense; the dry-control files must not be forced into an order.
3. `build-plugin/density-stereo-stability-auditions/` — judge image stability
   separately from preference and verify mono.
4. `build-plugin/density-oversampling-auditions/` — decide whether the measured
   latency and CPU cost buys a repeatable preference.
5. `build-plugin/density_detector_blind/` — run the longer detector topology
   selection only after the production-control questions above.

Take a break between packs. Do not compare answers across packs until every
sheet in the current pack is complete.

## Detector protocol

The detector candidate set is closed at six topologies. First compare every
candidate against current; eliminate severe failures before comparing the
remaining candidates with one another.

Generate the repeatable blind pack with:

```sh
./build/density_lab --detector-blind build/density_detector_blind
```

For each detector pair:

1. Randomize which file is A and B without revealing the detector.
2. Verify the files are the provided level-matched pair; do not add automatic
   normalization or dynamics processing.
3. Listen at quiet and normal levels on monitors, headphones, and in mono.
4. Record transient shape, low-frequency stability, continuity, pumping,
   recovery, and preference. Do not use “analog” as a finding.
5. Repeat before revealing A/B identity. A topology advances only when its
   benefit repeats across at least three fixture classes without a severe
   failure in the fourth.

Record one of `prefer A`, `prefer B`, or `no reliable preference`; forced
preferences are not useful evidence.

The generated material is diagnostic. Repeat the protocol with level-matched,
lawfully available full mixes, percussion, bass-heavy, sparse, and ambient
material before changing the production detector.
