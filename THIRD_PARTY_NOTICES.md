# Third-party notices

Historical documents in the research catalog are linked, not redistributed.

The opt-in internal VST3 build fetches JUCE 8.0.13 from its official repository.
JUCE is copyright Raw Material Software Limited and available under the JUCE 8
EULA or AGPLv3. This prototype is not a distribution grant; the applicable
commercial licence and full release notices remain a packaging gate.

- Source: https://github.com/juce-framework/JUCE/tree/8.0.13
- Licence: https://github.com/juce-framework/JUCE/blob/8.0.13/LICENSE.md

The Density product SBOM also identifies the MIT-licensed Steinberg VST3 SDK
3.8.0 sources bundled within the pinned JUCE tree. This is distinct from the
standalone validator source described below.

CI fetches Steinberg VST3 SDK 3.8.0 at commit
`9fad9770f2ae8542ab1a548a68c1ad1ac690abe0` and builds only its official
command-line validator target. The SDK source and validator are MIT-licensed.
They are validation tooling, are not linked into Density, and are not included
in product artifacts.

- Source: https://github.com/steinbergmedia/vst3sdk/tree/v3.8.0_build_66
- Licence: [LICENSES/Steinberg-VST3-SDK-MIT.txt](LICENSES/Steinberg-VST3-SDK-MIT.txt)
