#include "density_plugin.hpp"

#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <string_view>
#include <thread>

namespace {

int failures = 0;

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

void setValue(aste::density::plugin::DensityAudioProcessor& processor,
              const char* id, float plainValue) {
  auto* parameter = processor.state().getParameter(id);
  require(parameter != nullptr, "Parameter ID exists");
  if (parameter != nullptr) {
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
  }
}

float rawValue(aste::density::plugin::DensityAudioProcessor& processor,
               const char* id) {
  const auto* value = processor.state().getRawParameterValue(id);
  require(value != nullptr, "Raw parameter ID exists");
  return value == nullptr ? 0.0F : value->load();
}

juce::MemoryBlock binaryState(const juce::ValueTree& state) {
  juce::MemoryBlock binary;
  if (const auto xml = state.createXml()) {
    juce::AudioProcessor::copyXmlToBinary(*xml, binary);
  }
  return binary;
}

juce::Component* findTitled(juce::Component& component,
                            const juce::String& title) {
  if (component.getTitle() == title) {
    return &component;
  }
  for (int i = 0; i < component.getNumChildComponents(); ++i) {
    if (auto* match = findTitled(*component.getChildComponent(i), title)) {
      return match;
    }
  }
  return nullptr;
}

void testStateRoundTrip() {
  aste::density::plugin::DensityAudioProcessor source;
  setValue(source, "density", 83.25F);
  setValue(source, "drive", 7.5F);
  setValue(source, "release", 432.1F);
  setValue(source, "stereo", 37.5F);
  setValue(source, "protection", 0.0F);
  setValue(source, "bypass", 1.0F);

  juce::MemoryBlock state;
  source.getStateInformation(state);
  require(!state.isEmpty(), "State serialization produces data");

  aste::density::plugin::DensityAudioProcessor restored;
  restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  require(std::abs(rawValue(restored, "density") - 83.25F) < 0.011F,
          "Density restores exactly within parameter interval");
  require(std::abs(rawValue(restored, "drive") - 7.5F) < 0.011F,
          "Drive restores exactly within parameter interval");
  require(std::abs(rawValue(restored, "release") - 432.1F) < 0.11F,
          "Release restores exactly within parameter interval");
  require(std::abs(rawValue(restored, "stereo") - 37.5F) < 0.011F,
          "Stereo link restores exactly within parameter interval");
  require(rawValue(restored, "protection") < 0.5F, "Boolean state restores");
  require(rawValue(restored, "bypass") >= 0.5F, "Bypass state restores");

  juce::MemoryBlock repeated;
  restored.getStateInformation(repeated);
  require(state == repeated, "Equivalent state serialization is deterministic");

  const auto beforeMalformed = rawValue(restored, "density");
  constexpr char malformed[] = "not-density-state";
  restored.setStateInformation(malformed, static_cast<int>(sizeof(malformed)));
  require(rawValue(restored, "density") == beforeMalformed,
          "Malformed state leaves current state unchanged");

  auto invalidValue = source.state().copyState();
  invalidValue.setProperty("schema", 1, nullptr);
  invalidValue.setProperty("product", "density-d01", nullptr);
  invalidValue.getChildWithProperty("id", "density")
      .setProperty("value", "nan", nullptr);
  const auto invalidBinary = binaryState(invalidValue);
  restored.setStateInformation(invalidBinary.getData(),
                               static_cast<int>(invalidBinary.getSize()));
  require(rawValue(restored, "density") == beforeMalformed,
          "Non-finite parameter state is rejected");

  auto duplicate = source.state().copyState();
  duplicate.setProperty("schema", 1, nullptr);
  duplicate.setProperty("product", "density-d01", nullptr);
  duplicate.addChild(duplicate.getChildWithProperty("id", "density").createCopy(),
                     -1, nullptr);
  const auto duplicateBinary = binaryState(duplicate);
  restored.setStateInformation(duplicateBinary.getData(),
                               static_cast<int>(duplicateBinary.getSize()));
  require(rawValue(restored, "density") == beforeMalformed,
          "Duplicate parameter IDs are rejected");

  auto partial = source.state().copyState();
  partial.setProperty("schema", 1, nullptr);
  partial.setProperty("product", "density-d01", nullptr);
  for (int index = partial.getNumChildren(); --index >= 0;) {
    if (partial.getChild(index).getProperty("id").toString() != "density") {
      partial.removeChild(index, nullptr);
    }
  }
  const auto partialBinary = binaryState(partial);
  restored.setStateInformation(partialBinary.getData(),
                               static_cast<int>(partialBinary.getSize()));
  require(std::abs(rawValue(restored, "density") - 83.25F) < 0.011F,
          "Present parameters restore from partial state");
  require(std::abs(rawValue(restored, "drive")) < 0.011F,
          "Missing parameters restore documented defaults");
  require(std::abs(rawValue(restored, "stereo") - 100.0F) < 0.011F,
          "Older partial state receives the fully linked Stereo default");
}

void testLifecycleAndAudio() {
  aste::density::plugin::DensityAudioProcessor processor;
  require(processor.getNumPrograms() == 1 &&
              processor.getProgramName(0) == "Default",
          "The single factory program has a valid name");
  juce::AudioProcessor::BusesLayout stereo;
  stereo.inputBuses.add(juce::AudioChannelSet::stereo());
  stereo.outputBuses.add(juce::AudioChannelSet::stereo());
  require(processor.isBusesLayoutSupported(stereo),
          "Stereo layout is supported");
  processor.prepareToPlay(48000.0, 127);
  require(processor.getLatencySamples() == 0, "Host latency reports zero");

  juce::AudioBuffer<float> audio{2, 127};
  for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
    const float value = 0.8F * std::sin(static_cast<float>(sample) * 0.17F);
    audio.setSample(0, sample, value);
    audio.setSample(1, sample, value);
  }
  juce::MidiBuffer midi;
  processor.processBlock(audio, midi);
  for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
    for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
      require(std::isfinite(audio.getSample(channel, sample)),
              "Plugin output remains finite");
    }
  }
  require(processor.inputPeak() > 0.0F, "Input meter receives audio");
  require(processor.outputPeak() > 0.0F, "Output meter receives audio");

  setValue(processor, "bypass", 1.0F);
  const float bypassInput = audio.getSample(0, 10);
  processor.processBlock(audio, midi);
  require(audio.getSample(0, 10) == bypassInput,
          "Owned bypass parameter leaves audio unchanged");
  processor.releaseResources();
}

void testEditorContract() {
  aste::density::plugin::DensityAudioProcessor processor;
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  require(editor != nullptr, "Editor can be created");
  if (editor == nullptr) {
    return;
  }

  require(editor->isResizable(), "Editor is host-resizable");
  const auto* constrainer = editor->getConstrainer();
  require(constrainer != nullptr && constrainer->getMinimumWidth() == 760 &&
              constrainer->getMinimumHeight() == 420 &&
              constrainer->getMaximumWidth() == 1520 &&
              constrainer->getMaximumHeight() == 840,
          "Editor exposes its supported scale range");

  auto* density = dynamic_cast<juce::Slider*>(findTitled(*editor, "DENSITY"));
  require(density != nullptr, "Density control has a stable accessible title");
  if (density != nullptr) {
    require(density->isTextBoxEditable(), "Density accepts exact numeric entry");
    require(density->getWantsKeyboardFocus(), "Density accepts keyboard focus");
    require(density->isDoubleClickReturnEnabled() &&
                density->getDoubleClickReturnValue() == 50.0,
            "Density resets to its documented default");
    require(std::abs(density->getValueFromText("73.25 %") - 73.25) < 0.001,
            "Density parses an exact value with its unit");
    require(density->isAccessible(),
            "Density is exposed to accessibility clients");
  }

  auto* protection = findTitled(*editor, "PROTECTION");
  require(protection != nullptr && protection->getWantsKeyboardFocus() &&
              protection->isAccessible(),
          "Protection is keyboard and accessibility reachable");

  auto* output = dynamic_cast<juce::Slider*>(findTitled(*editor, "OUTPUT"));
  require(output != nullptr && output->getTextFromValue(-0.000001) == "0.00 dB",
          "Rounded zero never displays a negative sign");
  if (output != nullptr) {
    auto* textBox = dynamic_cast<juce::Label*>(output->getChildComponent(0));
    require(textBox != nullptr && textBox->getText() == "0.00 dB",
            "Output text box initially displays canonical zero");
  }

  editor->setSize(760, 420);
  juce::Image image{juce::Image::RGB, editor->getWidth(), editor->getHeight(),
                    true};
  juce::Graphics graphics{image};
  editor->paintEntireComponent(graphics, true);
  require(image.getPixelAt(0, 0).getAlpha() == 255,
          "Minimum-size editor paints an opaque panel");
}

double benchmarkPaint(juce::AudioProcessorEditor& editor) {
  constexpr int frames = 120;
  juce::Image image{juce::Image::RGB, editor.getWidth(), editor.getHeight(),
                    true};
  const auto start = std::chrono::steady_clock::now();
  for (int frame = 0; frame < frames; ++frame) {
    juce::Graphics graphics{image};
    editor.paintEntireComponent(graphics, true);
  }
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  return elapsed * 1000.0 / frames;
}

int createEditorArtifacts(const char* directoryPath) {
  aste::density::plugin::DensityAudioProcessor processor;
  processor.prepareToPlay(48000.0, 512);
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  if (editor == nullptr) {
    return 1;
  }
  editor->setSize(980, 540);

  std::this_thread::sleep_for(std::chrono::milliseconds{40});
  juce::Timer::callPendingTimersSynchronously();
  const double idleMs = benchmarkPaint(*editor);

  setValue(processor, "drive", 12.0F);
  setValue(processor, "density", 80.0F);
  juce::AudioBuffer<float> audio{2, 512};
  juce::MidiBuffer midi;
  for (int block = 0; block < 32; ++block) {
    for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
      const float value =
          0.85F * std::sin(static_cast<float>(block * 512 + sample) * 0.031F);
      audio.setSample(0, sample, value);
      audio.setSample(1, sample, value);
    }
    processor.processBlock(audio, midi);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds{40});
  juce::Timer::callPendingTimersSynchronously();
  const double activeMs = benchmarkPaint(*editor);

  const auto snapshot = editor->createComponentSnapshot(
      editor->getLocalBounds(), true, 2.0F, juce::SoftwareImageType{});
  const juce::File directory{juce::String::fromUTF8(directoryPath)};
  if (directory.createDirectory().failed()) {
    return 2;
  }
  juce::FileOutputStream output{directory.getChildFile("density-d01-editor-2x.png")};
  if (!output.openedOk() || !output.setPosition(0) || output.truncate().failed() ||
      !juce::PNGImageFormat{}.writeImageToStream(snapshot, output)) {
    return 2;
  }
  output.flush();
  std::cout << "{\"width\":" << snapshot.getWidth()
            << ",\"height\":" << snapshot.getHeight()
            << ",\"paint_frames\":120,\"idle_ms_per_frame\":" << idleMs
            << ",\"active_ms_per_frame\":" << activeMs << "}\n";
  processor.releaseResources();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI juce;
  if (argc == 3 && std::string_view{argv[1]} == "--editor-artifacts") {
    return createEditorArtifacts(argv[2]);
  }
  if (argc != 1) {
    return 2;
  }
  testStateRoundTrip();
  testLifecycleAndAudio();
  testEditorContract();
  if (failures == 0) {
    std::cout << "density_plugin_tests: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
