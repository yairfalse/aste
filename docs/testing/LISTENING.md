# Listening protocol

The detector candidate set is closed at six topologies. First compare every
candidate against current; eliminate severe failures before comparing the
remaining candidates with one another.

Generate the repeatable blind pack with:

```sh
./build/density_lab --detector-blind build/density_detector_blind
```

Play only files in `audio/`, fill in `responses.csv`, and do not inspect
`source/` or `answer-key.csv` until every response is recorded. Confidence is
`0` for a guess, `1` slight, `2` repeatable, and `3` strong.

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
