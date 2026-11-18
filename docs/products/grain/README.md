# Grain G-01

Grain is a stereo/mono VST3 **audio effect**. Insert it on an audio track, group
or send in Cubase; find **Grain G-01** among effects. It is not a MIDI instrument.

Feed sound into A. Its grains can feed B, whose grains can feed C, and any node
can feed any other, including itself. All three have independent source memory,
fragment timing, pitch, spread and erosion. The default includes some dry audio
while the first source bank fills. Set Dry / Network to 1 for only the network.

## Start playing

1. Feed audio for at least half a second. Three node cards show stored duration,
   capture/freeze state, active grains, capture-input RMS and output RMS.
   Each card exposes size, density, speed, pitch, stereo scatter, monitor level,
   memory length, live input, capture, freeze and reverse without changing views.
2. Change Grain ms, Grains / Second, Speed and Pitch. Independent time moves
   fragment selection without multiplying in-grain pitch; Varispeed couples them.
3. Freeze one node to keep its material. Other nodes can keep capturing and
   transforming its output. Reverse, position variation, time/stereo scatter,
   edge shape, cluster, erosion and accumulation are local to the selected node.
4. Use Time, Spread, Erosion and Inheritance to perform the network. Morph to
   Cycle moves the matrix toward a feedback cycle without changing node sounds.
5. Press **SEND A TO...**, then **TO B**, and adjust **SEND AMOUNT x**.
   An inactive route opens at 0.65; an existing route keeps its amount. Any node
   can send to any other, including itself. Routing starting points change
   connections/live sends only. The optional **MATRIX** edits the same base
   levels; the routing map displays targets after morph and row limiting.
   Route transitions smooth over 15 ms. Monitor Level affects the audible mix;
   it does not remove that node from other nodes’ inputs.
6. **CLEAR** requires a second click within three seconds and forgets only that
   node. Other nodes may refill it. **CLEAR NETWORK**, under **DETAILS**, uses
   the same confirmation and resets all sound. Bypass suspends processing and preserves memory, passing
   dry audio. Master Output is the final level after dry/network mixing.

Mutate Position writes the real Position parameter through host gestures, so
Cubase can record the change as automation.

## SEND is a connection, not a copy

SEND routes a node's live granular output into another node's **capture input**.
It never clears either memory. The destination must have Capture enabled and
must not be frozen to replace its stored material. It publishes a new grain
source when the memory window fills. A frozen destination keeps playing its
existing sound; the send strip explains this rather than silently unfreezing it.
Stopping Capture or enabling Freeze seals any partial capture, which may include
a silent gap. A stored silent bank is not an empty bank.

**HOLD SEND** (mouse or focused Space key) temporarily raises the selected route
to at least 0.65. Drag up/down while holding to adjust it. Release, loss of focus,
application deactivation, switching routes or closing the editor restores its
previous amount. A different value arriving from host automation/state recall
takes ownership and is not overwritten on release. **LATCH SEND** keeps a route
open for hands-free editing; **REMOVE ROUTE** closes it without erasing audio.
All these actions use the existing automatable route parameters and the existing
15 ms DSP smoothing. New material already captured during a hold remains after
release; this is not an audio undo operation.

The routing map groups arrows by destination, with numerical target gains.
Amber identifies self-routing and routes participating in feedback cycles.
Click an arrow row to inspect it without opening a closed connection; keyboard
users can select source/destination with the labeled buttons and set amount zero.
The same route is available in the matrix. Targets are not activity meters:
the node cards show actual last-processed-block capture/output RMS and grain count.
REC's display floor is -100 dB; it is not a claim of measured signal below that.

When **MORPH TO CYCLE** is nonzero, direct SEND editing pauses explicitly:
**EDIT CURRENT ROUTES** writes the current morph target into the existing base
parameters and resets morph to zero (subject to host float normalization).
It preserves the resulting target routing, not a separate copy of audio. The
matrix remains available for deliberate base-level editing while morphing.
If Cubase is automating Cycle Morph, stop writing that automation before taking
manual ownership. Routing starting points also reset morph, as before.

**DETAILS** exposes all 24 controls for the selected node, plus Inheritance,
Seed, Mutate Position and Clear Network. SEND and all three node identities and
statuses remain visible. Sliders accept typed values, fine adjustment and reset;
buttons and standard JUCE controls are keyboard-focusable. No drag-only essential
operation or hidden modifier is required. Screen-reader behavior in Cubase still
requires testing with the creator's actual setup.

## Memory and session recall

Parameters, routes, freeze choices and each node's latest completed source bank
save in the project. Frozen material now reopens without feeding it again.
Audio is embedded, with no external files to collect. Host re-preparation retains
it; a different host sample rate resamples it while preserving duration.
Active grains, read positions and an unfinished capture are not serialized, so
print the performance when exact sample-for-sample continuation matters.
Older controls-only sessions cannot recover previously unsaved audio.
Recalled freezes with empty memory arm for fresh source:
they wait through silence, capture incoming material and then hold it. The graph
says ARMED / FEED AUDIO. Freeze with existing material acts immediately and can
seal a partial capture. Capture must be enabled for an armed freeze to acquire
new material. A silent stored bank is still material; clear it to arm anew.

Each node has three prepared immutable banks, up to four seconds each. Long
grains may pin old banks; capture waits rather than overwrite readers. Graph
and node output meters report actual DSP state. Capture waits and dropped births
are bounded engine behavior, not extra memory allocation. Node Source Memory
can be shorter than its grain: the grain then wraps the selected source bank.

## Timing, quality and bounds

All directed edges have one sample of delay, independent of host block size.
Capture windows add further onset delay to wet sound. Free nodes run while the
DAW is stopped; synced births follow BPM/PPQ and stop with transport, while
existing grains finish. Frozen free material survives transport seeks. Sync qn
uses exact 1/64-quarter-note increments. Host automation arrives at callback
boundaries through JUCE; no VST3 sample-accurate automation claim is made.

Sinc16 interpolation is used. The per-node grain cap is 48 through 48 kHz,
24 through 96 kHz, and 16 above that, based on the measured high-rate cost.
New births are dropped at the cap rather than growing CPU or stealing a reader.
Feedback input rows are scaled when their sum exceeds 0.995; requested matrix
levels remain visible. Soft saturation and finite clamps bound the network.
Density is not a loudness multiplier. Sparse grains, tonal sidebands and sharp
fragment edges are possible; this is not transparent time stretching.

## Identity and state contract

Version 0.9.0; bundle `invalid.aste.grain-g01`; manufacturer `Aste`, plugin code
`Grn1`, VST3 category `Fx|Delay`. Independent C++20 engine under `apps/grain`.
Schema 1 root/product `grain-g01`. There are **94 host parameters**:

- 0–7: Time, Spread, Erosion, Inheritance, Dry/Network, Freeze All, Morph to Cycle,
  Seed (takes effect after Clear Network or the next prepare).
- 8–10: `n1_live` through `n3_live`.
- 11–19: `route_1_to_1` through `route_3_to_3`, destination-major order.
- 20–91: 24 controls per node, IDs `n1_size`, `n1_density`, etc., as declared in
  `GrainAudioProcessor::fieldIds` and `controls()`; ranges/units come from those
  product-owned specs. The three blocks begin at 20, 44, 68.
- 92: `master`, -36..+18 dB with 20 ms smoothing; 93: host `bypass`.

Existing IDs/order are fixed from this playable version; future controls append.
Malformed/cross-product/unknown-schema state is rejected transactionally. Base
response metadata does not select an older Grain algorithm; Grain has no legacy
sound revision. UI meters use lock-free atomics. Core storage is prepared before
processing; callbacks do not allocate/free, lock, log, wait or access files.

Optional `MEMORY format="1"` metadata contains exactly three `NODE0`–`NODE2`
records: sample rate, frame count and interleaved little-endian float32 Base64.
Each bank is bounded to four seconds at 44.1–192 kHz, with finite samples in
[-8, 8]. Malformed audio rejects the whole restore. Input state is capped at
64 MiB before XML parsing. Old state without MEMORY restores empty banks;
older binaries do not restore this new audio metadata. Bank readers pin
immutable audio using lock-free ownership flags; capture never waits on a save.

The [original laboratory](../../../research/g01/README.md) remains reproducible.
Its measurements describe that engine revision, not this product’s reduced
high-rate pool. Cubase musical listening and assistive-technology review remain
open even after the suitable-host and native editor checks.

[Build, performance and installation evidence](EVIDENCE.md).
