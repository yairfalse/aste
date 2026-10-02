# Third-party notices

Historical documents in the research catalog are linked, not redistributed.

The cleanup utility links the Rust standard library. Its original copyright
and license notices (MIT/Apache-2.0 and the dependencies listed by Rust) are
installed under `Aste/ThirdParty/Rust` alongside the exact compiler version.
The corresponding-source archive also includes these files in `Rust-Licenses/`.
These notices come from the toolchain used to build the utility, rather than a
separately downloaded or possibly mismatched license inventory.

The opt-in plugin build fetches JUCE 8.0.13 from its official repository.
JUCE is copyright Raw Material Software Limited and available under the JUCE 8
EULA or AGPLv3. Aste selects the AGPLv3 route; no commercial JUCE license is
required for this compatible open-source distribution. Aste's original code is
licensed under AGPL-3.0-only; see [LICENSE](../LICENSE).

The macOS development download includes corresponding Aste source and the
complete pinned JUCE source tree, including its dependency license texts and
copyright notices. Keep this source archive alongside redistributed binaries.
Third-party components retain their original licenses, not Aste's blanket
license. JUCE's LICENSE.md inventories its dependencies; Audio Units additionally
use its Apache-2.0-licensed AudioUnitSDK. This license choice does not establish
Apple notarization, DAW compatibility, or readiness for public release.

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
- Licence: [LICENSES/Steinberg-VST3-SDK-MIT.txt](../LICENSES/Steinberg-VST3-SDK-MIT.txt)
