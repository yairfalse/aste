# 0021: Loop protected performance workflow

## Context

Loop's production tape storage already retained real PCM and saved it in
sessions. Its capture button continuously overdubbed, RELOOP branched by
discarding newer generations, and its three drawn deck lines were not an
editable audio waveform. These behaviors made preservation and performance
unnecessarily difficult.

## Decision

Append an explicit `safe_takes` parameter and region/repeat/monitor controls.
Keep the original 22 host indices, parameter ranges, old signal path and
schema-4 PCM layout. Missing Safe Takes means off; fresh instances explicitly
enable it. Existing projects therefore retain their interpretation.

Safe capture writes one new take. RELOOP writes the auditioned wet signal once,
before mix/monitor/output stages, into an empty deck. It does not evict audio or
destroy later branches. Three occupied slots block new capture/printing until
the user removes an inactive take. This reuses the proven bounded allocation
rather than multiplying snapshot memory for an arbitrary history count.

Printing leaves the source performing; choosing a history tile selects the
new PCM and neutralizes temporary transforms in one undo transaction. This
avoids a UI-thread race in resetting automated parameters at the exact audio
sample where a print completes. Both source and result remain immediately
available. History deletions and Clear use off-thread audio checkpoints;
unchanged playing material keeps its head/motion state during adoption.
Stale undo checkpoints refuse to erase later recordings.

Repeat has its own cursor. The underlying selected-loop timeline keeps its
entry region, speed and direction, independent of temporary fragment edits.
Release returns to that phase with a short fade. The mouse Hold gesture restores
its entry edits; Latch deliberately retains edits. Host Sync supplies musical
duration, not quantized capture starts or PPQ-locked tape playback.

## Consequences

No extra runtime dependency, plugin identity, product version or separate
application. DSP remains C++20 with no JUCE; UI and state operations remain at
the adapter boundary. The three-generation limit is visible rather than
silent, but musicians wanting a larger archive must save project copies.
Undo history and live cursor positions are not extra persisted generations.
Older binaries cannot honor Safe Takes semantics, so backwards file opening
is not promised; forward loading of old sessions is preserved.

Production adapter rendering, gesture tests, exact print comparison, callback
auditing, state/migration tests and timeline fault injection accompany the
change. Cubase and musical preference remain creator-side validation.
