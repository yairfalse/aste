# UI system

## Family language

Panels use a near-black matte surface, off-white technical typography, strong
negative space, large musical controls, and one restrained functional accent.
Motion reports signal state only. No fake materials, fasteners, rack furniture,
decorative animation, tabs, or hidden pages for essential controls are allowed.

Product accents are deep burgundy for Density, warm ochre for Harmonic, oxidized
teal for Loop, restrained signal orange for Impulse, and cold laboratory blue
for Sequence. These are identifiers and state cues, not decoration.

## Interaction contract

- Generous control bounds and a clear visual hierarchy.
- Drag, host-standard fine adjustment, editable numeric fields, and
  double-click reset.
- Stable accessibility titles, descriptions, and explicit keyboard order.
- Fixed meaning at every supported scale; resizing must not reveal or hide
  essential controls.
- Meters update at a bounded rate and never drive DSP state.
- Tooltips and modal dialogs are omitted unless a tested workflow needs them.

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
