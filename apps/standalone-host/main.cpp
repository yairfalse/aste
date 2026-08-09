#include <juce_audio_processors_headless/juce_audio_processors_headless.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>

namespace {

int fail(const juce::String& message) {
  std::cerr << "vst3_smoke_host: " << message << '\n';
  return 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc != 5 && argc != 7) {
    return fail(
        "usage: vst3_smoke_host <plugin.vst3> <component> <parameter> "
        "<count> [--write-state|--read-state <file>]");
  }
  const juce::String expectedComponent = argv[2];
  const juce::String expectedParameter = argv[3];
  const int expectedParameterCount = juce::String{argv[4]}.getIntValue();
  const juce::String stateMode = argc == 7 ? argv[5] : "";
  if (argc == 7 && stateMode != "--write-state" &&
      stateMode != "--read-state") {
    return fail("unknown state exchange mode");
  }

  juce::AudioPluginFormatManager formats;
  juce::addHeadlessDefaultFormatsToManager(formats);
  const auto availableFormats = formats.getFormats();
  const auto vst3Entry = std::find_if(
      availableFormats.begin(), availableFormats.end(),
      [](const auto* format) { return format->getName() == "VST3"; });
  if (vst3Entry == availableFormats.end()) {
    return fail("JUCE VST3 hosting is unavailable");
  }
  auto* vst3 = *vst3Entry;

  juce::OwnedArray<juce::PluginDescription> descriptions;
  vst3->findAllTypesForFile(descriptions, juce::String::fromUTF8(argv[1]));
  if (descriptions.size() != 1 || descriptions[0]->name != expectedComponent) {
    return fail("bundle did not expose exactly one expected component");
  }

  constexpr double sampleRate = 48000.0;
  constexpr int maximumBlockSize = 2048;
  juce::String error;
  auto plugin = formats.createPluginInstance(*descriptions[0], sampleRate,
                                             maximumBlockSize, error);
  if (plugin == nullptr) {
    return fail("instantiation failed: " + error);
  }
  if (plugin->getTotalNumInputChannels() != 2 ||
      plugin->getTotalNumOutputChannels() != 2) {
    return fail("default bus layout is not stereo");
  }
  if (plugin->getLatencySamples() != 0) {
    return fail("reported latency is not zero");
  }

  const auto& parameters = plugin->getParameters();
  if (parameters.size() != expectedParameterCount) {
    return fail("unexpected parameter count");
  }
  const auto portabilityEntry = std::find_if(
      parameters.begin(), parameters.end(), [&](const auto* parameter) {
        return parameter->getName(64) == expectedParameter;
      });
  if (portabilityEntry == parameters.end()) {
    return fail("state-portability parameter is unavailable");
  }
  auto* portabilityParameter = *portabilityEntry;

  constexpr float portabilityValue = 0.8125F;
  const juce::File stateFile{argc == 7 ? argv[6] : ""};
  if (stateMode == "--read-state") {
    juce::MemoryBlock importedState;
    if (!stateFile.loadFileAsData(importedState)) {
      return fail("could not read exchanged state");
    }
    plugin->setStateInformation(importedState.getData(),
                                static_cast<int>(importedState.getSize()));
    if (std::abs(portabilityParameter->getValue() - portabilityValue) >
        1.0e-6F) {
      return fail("exchanged state restored the wrong parameter value");
    }
  } else if (stateMode == "--write-state") {
    portabilityParameter->setValueNotifyingHost(portabilityValue);
  }

  juce::MemoryBlock initialState;
  juce::MemoryBlock changedState;
  juce::MemoryBlock restoredState;
  plugin->getStateInformation(initialState);
  if (stateMode == "--write-state" &&
      !stateFile.replaceWithData(initialState.getData(),
                                 initialState.getSize())) {
    return fail("could not write exchanged state");
  }
  portabilityParameter->setValueNotifyingHost(
      portabilityParameter->getValue() < 0.5F ? 1.0F : 0.0F);
  plugin->getStateInformation(changedState);
  if (initialState.isEmpty() || initialState == changedState) {
    return fail("parameter mutation did not change serialized state");
  }
  plugin->setStateInformation(initialState.getData(),
                              static_cast<int>(initialState.getSize()));
  plugin->getStateInformation(restoredState);
  if (restoredState != initialState) {
    return fail("state did not restore byte-for-byte");
  }

  plugin->setRateAndBufferSizeDetails(sampleRate, maximumBlockSize);
  plugin->prepareToPlay(sampleRate, maximumBlockSize);
  plugin->setNonRealtime(false);

  constexpr std::array blockSizes{1, 2, 7, 127, 511, maximumBlockSize};
  juce::AudioBuffer<float> audio{2, maximumBlockSize};
  juce::MidiBuffer midi;
  double checksum = 0.0;
  std::uint64_t sampleOffset = 0;
  for (const int blockSize : blockSizes) {
    audio.setSize(2, blockSize, false, false, true);
    for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
      for (int sample = 0; sample < blockSize; ++sample) {
        const double phase = static_cast<double>(sampleOffset + sample) *
                             (channel == 0 ? 0.013 : 0.017);
        audio.setSample(channel, sample,
                        static_cast<float>(0.4 * std::sin(phase)));
      }
    }
    plugin->processBlock(audio, midi);
    for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
      for (int sample = 0; sample < blockSize; ++sample) {
        const float output = audio.getSample(channel, sample);
        if (!std::isfinite(output)) {
          plugin->releaseResources();
          return fail("processing produced a non-finite sample");
        }
        checksum += std::abs(static_cast<double>(output));
      }
    }
    sampleOffset += static_cast<std::uint64_t>(blockSize);
  }
  plugin->releaseResources();
  if (!(checksum > 0.0) || !std::isfinite(checksum)) {
    return fail("processing produced no finite signal");
  }

  std::cout << "{\"component\":\"" << expectedComponent
            << "\",\"parameters\":" << parameters.size()
            << ",\"latency_samples\":" << plugin->getLatencySamples()
            << ",\"blocks\":" << blockSizes.size()
            << ",\"state_bytes\":" << initialState.getSize()
            << ",\"finite\":true}\n";
  return 0;
}
