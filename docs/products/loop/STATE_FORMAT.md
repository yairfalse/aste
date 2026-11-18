# Loop L-01 state

Schema 4 retains the schema-3 tapes and adds required root `behavior="0"` or
`behavior="1"`. New instances use 1: the shared motion phase wraps after the
existing frequencies' common 1000-second period. Schemas 1–3 restore behavior
0 and retain the original one-second clock. This choice travels with the
prepared tape engine, survives save/reopen and changes neither parameter IDs
nor tape data. Missing/invalid schema-4 behavior rejects the complete state.
Older binaries reject schema 4; keep the original session when moving between
binary revisions. No product release version is selected by this change.

The appended `transport` choice saves Play/Stop/Release without reinterpreting
any old parameter. Missing transport restores Play. A stopped session reopens
silent with tape retained. Live head position and release-envelope progress
remain transient; recalling Release starts a new release from the tape start.

Loop uses a product-owned portable XML schema:

```xml
<loop-l01 schema="3" product="loop-l01">
  <PARAM id="feedback" value="85.0"/>
  <PARAM id="speed" value="1.0"/>
  <TAPES sample_rate="48000" active="0">
    <TAPE index="0" generation="0" frames="0" audio=""/>
    <TAPE index="1" generation="0" frames="0" audio=""/>
    <TAPE index="2" generation="0" frames="0" audio=""/>
  </TAPES>
</loop-l01>
```

Known values are finite-checked and bounded. Missing values restore defaults;
unknown IDs are ignored. Duplicate known IDs, malformed roots, non-finite
values, cross-product data, and unknown schema versions leave current state
unchanged.

Schema 2 added `tape_speed`; schema 1 supplies its 7 1/2 IPS default. Both older
schemas remain accepted and restore empty audio memory.

Schema 3 saves recorded audio and retained generations. Each tape contains
interleaved little-endian float32 samples encoded as standard Base64. The
decoder checks deck uniqueness, generation consistency, finite samples in
[-8, 8], a maximum 16 seconds per deck, exact payload size, and sample rate
between 8 and 384 kHz. Invalid audio leaves both controls and tapes unchanged.
The XML envelope is bounded to 256 MiB before parsing.

Snapshots are immutable while saved and never lock the audio thread. A partial
capture is included; an unfinished RELOOP destination is excluded. Restored
playback starts at the beginning of the active tape, with linear resampling
if the host rate differs. State loaded before the first host preparation keeps
its source rate until the host announces the actual rate, avoiding an intermediate
conversion through the default rate.
See [ADR 0016](../../adr/0016-playable-defaults-and-tape-recall.md).

New instances select appended First take, identical to the original reset
controls except Overdub 0% instead of 50%. Original preset values and missing-key
defaults are unchanged. This writes full input before tape processing; it does
not promise unity output through the recording, degradation and output stages.

## Protected history and editing

The Safe Takes update keeps schema 4 and its existing three PCM tape records.
It appends five optional parameters; missing `safe_takes` restores **off**, so
old sessions keep continuous overdubbing, recycling history and their original
signal path. Fresh instances and First take enable Safe Takes; the six earlier
factory presets retain the original mode. Do not reopen Safe Takes sessions in
older binaries: those binaries ignore the new controls and cannot honor their
playback semantics. Retain a project copy when switching binaries.

Safe Takes never evicts a generation or deletes later branches automatically.
All three slots occupied means CAPTURE/RELOOP are unavailable. Remove an
inactive take explicitly to continue. Removal and Clear have bounded,
off-audio undo checkpoints containing actual PCM; audition/parameter edits
use the same undo manager. At most 8192 KiB of accounted undo payload is kept,
except the newest transaction is always retained even if larger. Undo history
is transient; the three retained audio generations and current controls are
saved in the project. Removed takes are not saved as hidden extra history.

Undo checkpoints refuse to replace audio if a subsequent capture/print has
changed the tape set. They must never silently erase a newly created take.
History edits prepare replacement engines off-thread. At adoption, unchanged
active audio inherits its playback/motion/filter state on the audio thread,
so removing another take does not restart the playing tape. Host recall still
starts at the beginning. Same-rate restore preserves the sample bits, including
signed zero; rate changes retain the existing linear conversion policy.

An unfinished print is still excluded; partial captures remain saveable. A
Safe Takes save stores Capture off without interrupting the live recording,
so restoring a partial take cannot re-arm a new recording. Legacy capture
latches retain their previous serialized behavior. A
live cursor, a partially printed destination, the temporary Hold gesture and
undo/redo stacks are not persistent performance recordings. Use RELOOP to
commit a gesture into a retained, portable generation.
