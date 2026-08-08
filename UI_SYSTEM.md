# UI system

## Family language

Panels use a near-black matte surface, off-white technical typography, strong
negative space, large musical controls, and one restrained functional accent.
Motion reports signal state only. No fake materials, fasteners, rack furniture,
decorative animation, tabs, or hidden pages for essential controls are allowed.

Product accents are deep burgundy for Density, warm ochre for Harmonic, oxidized
teal for Loop, and restrained signal orange for Impulse. These are identifiers
and state cues, not decoration.

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
and Output form the secondary grid; Protection remains visible. Keyboard order
follows that musical hierarchy.

Headless 1x/2x artifacts and paint timing are regression evidence, not a Retina
or usability claim. Native DAW, VoiceOver, contrast, and first-time-use review
remain release work.
