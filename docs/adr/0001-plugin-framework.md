# ADR 0001: JUCE at the product boundary

- Status: accepted for the first VST3 prototype
- Date: 2026-08-06

## Decision

Use JUCE 8 for VST3 integration and UI, pinned to reviewed release 8.0.13. Keep
all product DSP in ordinary C++20 libraries. The opt-in build fetches JUCE for
internal development; acquire the correct commercial licence before building or
sharing a closed-source distributable binary.

## Comparison

| Concern | JUCE | iPlug2 | Thin VST3 SDK + UI |
|---|---|---|---|
| VST3/hosts | Mature wrapper; broad production use | Supports VST3 and major formats | Direct control; all host quirks are ours |
| UI/text/scaling | Complete cross-platform component and text stack | Flexible IGraphics backends | VSTGUI or a second toolkit still required |
| Accessibility | Native platform accessibility support | No equally complete documented default found | Must design and maintain it |
| MIDI/automation/state | Integrated plugin abstractions | Integrated plugin abstractions | Maximum control, maximum glue code |
| Distinct visual design | Custom rendering and controls supported | Strong custom rendering options | Unlimited, but expensive |
| Testability | DSP stays isolated; JUCE has host/test utilities | DSP can stay isolated | Wrapper tests must be built from scratch |
| Maintenance/binary size | Largest dependency and binaries | Smaller, permissive framework | Smallest possible binary, largest team burden |
| Licence | JUCE EULA or AGPLv3; commercial distribution needs the applicable licence | zlib-like permissive licence | Steinberg SDK terms plus UI dependencies |
| Long-term control | Good below a narrow adapter boundary | Good, smaller community/compatibility corpus | Complete control, including every defect |

JUCE wins because host compatibility, accessibility, text, and four-product UI
maintenance are more valuable than shaving wrapper code or binary size. The
boundary preserves the option to replace it without rewriting DSP.

## Licence gate

JUCE 8 is offered under its EULA or AGPLv3 and its licence tiers depend on
revenue/funding. Internal pre-release evaluation uses the open-source option
described by JUCE's licence documentation. Before any closed binary is shared
outside the organisation, record the commercial licence in
`THIRD_PARTY_NOTICES.md`. This ADR is engineering guidance, not legal advice.

## Sources

- JUCE features and accessibility: https://juce.com/
- JUCE 8 licence: https://juce.com/legal/juce-8-licence/
- iPlug2 formats and licence: https://github.com/iPlug2/iPlug2
- Steinberg VST3 SDK contents/tools: https://steinbergmedia.github.io/vst3_doc/sdk.overview.html
- Steinberg VST3 licence: https://steinbergmedia.github.io/vst3_dev_portal/resources/VST3_License_Agreement.pdf
