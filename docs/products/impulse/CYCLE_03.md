# Impulse: sample-owned retriggers

The second musical audit reproduced a ratchet-3 buffer-partition failure in
the real DSP. The permanent regression first failed against the old clock,
then passed with a continuous sample anchor. No gate/audio tolerance was relaxed.

The new matrix compares exact samples at 127/511-frame partitions for all four
ratchets, three microtiming positions and six rates, including a seek, stop,
restart and tempo change. The original 127 versus 1/7/127/511 triplet reproduction
also passes. State tests preserve schema-4 sound and clock, save/reopen it, reject
invalid new clock attributes transactionally and verify explicit upgrade.

Schema 5 adds a session-owned clock choice, not a host parameter. Old sessions
keep historical timing; new instances use the sample-owned clock. Existing
sound engines and snapshot values remain unchanged. See the
[state contract](STATE_FORMAT.md) for discontinuity semantics.

Native adapter and VST3 smoke checks pass. This does not establish Cubase
compatibility or subjective musical approval; the creator's DAW is on another
machine. The overall playability revision owns the final build/listening evidence.
