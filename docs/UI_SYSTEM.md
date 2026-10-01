# UI system

## Family language

Panels use a near-black matte surface, off-white technical typography, strong
negative space, large musical controls, and one restrained functional accent.
Motion reports signal state only. No fake materials, fasteners, rack furniture,
decorative animation, tabs, or hidden pages for essential controls are allowed.

Product accents are deep burgundy for Density, warm ochre for Harmonic, oxidized
teal for Loop, restrained signal orange for Impulse, cold laboratory blue for
Sequence, and desaturated horizon blue for Field. These are identifiers and
state cues, not decoration.

## Interaction contract

- Generous control bounds and a clear visual hierarchy.
- Drag, host-standard fine adjustment, editable numeric fields, and
  double-click reset.
- Stable accessibility titles, descriptions, and explicit keyboard order.
- Fixed meaning at every supported scale; resizing must not reveal or hide
  essential controls.
- Meters update at a bounded rate and never drive DSP state.
- Hover descriptions explain musical effects and inactive dependencies. They
  supplement visible first-action cues rather than replacing them.
- Preset names stay visible in the open editor; an asterisk marks edits.
- Input/output bars use a -60 to 0 dBFS scale with numeric readings where space
  permits, rather than linear-amplitude fill.

## Density D-01

The current 980×540 editor scales between 760×420 and 1520×840. Density is the
first and largest control; input/output boundaries and gain reduction occupy the
left meter column; Drive, Crush, Attack, Release, Blend, Stereo, Detector HPF,
and Output form the secondary grid; Protection remains visible. A compact
header menu applies five factory starting points without adding a browser or
hidden page. Keyboard order follows the musical hierarchy and reaches the menu
after the ten essential controls.

Headless 1x/2x artifacts and paint timing are regression evidence, not a Retina
or usability claim. Native DAW, VoiceOver, contrast, and first-time-use review
remain release work.

## Sequence S-01

The 1280×760 editor scales from 960×600 to 1600×1000. All sixteen steps remain
visible and directly expose pitch, gate, accent, and slide. Voice and filter
controls occupy two rows above the program; Pressure and filter controls lead
the keyboard order. The only motion is output level and the current host-clocked
step. No sequencer page, modulation page, keyboard decoration, or circuit-brand
switch exists.

## Loop L-01

The 1160×650 editor scales from 920×540 to 1840×1080. Its left memory panel
shows capture fill, playback position, input/output boundaries, and one explicit
clear action. Capture, host sync, and reverse remain visible above a three-row
control field. Oxidized teal marks active memory and values; motion is limited
to signal levels and memory travel. All essential controls remain on one panel.

## Impulse I-01

The 1480×900 editor scales from 1120×700 to 1800×1120. Energy leads the global
row; Variation, Mutation, Seed, Output, Division, Sequence, and Snapshots remain
visible. Eight 32-step rows remain visible. Click a track name to audition it
and select its sound panel: five sound controls above eight rhythm controls.
Steps outside the track length are dimmed but can be programmed for later.
The new-instance groove is a shared, predictable 16 steps; the polymetric and
mutation starting points are explicitly labelled evolving or wild.

## Field F-01

The 1080×620 editor scales from 900×520 to 1800×1040. FOREVER occupies the
largest central hit target and communicates held state through fill, outline,
and text rather than animation. Input/output, stored energy, and retention are
the only moving displays. Mass, Grain, Pitch, Motion, Distance, Blend, Output,
and the compact preset menu remain visible on the single panel with explicit
keyboard order and editable numeric fields.

FOREVER distinguishes an empty armed hold from held audio. Clear Field empties
the memory without changing the hold switch. Its additional grain, pitch and
motion influence is explained on the panel. Sequence groups voice controls by
oscillators, filter, and envelope, displays note names, and explains its host
sequence versus MIDI-keyboard modes. Loop distinguishes empty, recording, and
playing memory and disables the inactive duration control.
