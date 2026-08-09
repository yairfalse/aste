#include "sequence_plugin.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <memory>
#include <new>
#include <string_view>

#if JUCE_MAC
#include <pthread.h>
#endif

namespace {
std::atomic<std::size_t> allocations{};
std::atomic<bool> auditActive{};
#if JUCE_MAC
std::atomic<std::uintptr_t> auditThread{};
bool shouldCount() noexcept {
  return auditActive.load(std::memory_order_relaxed) &&
         auditThread.load(std::memory_order_relaxed) ==
             reinterpret_cast<std::uintptr_t>(pthread_self());
}
#else
bool shouldCount() noexcept {
  return auditActive.load(std::memory_order_relaxed);
}
#endif
}  // namespace

void* operator new(std::size_t size) {
  if (shouldCount()) {
    allocations.fetch_add(1, std::memory_order_relaxed);
  }
  if (void* pointer = std::malloc(size)) {
    return pointer;
  }
  throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
  std::free(pointer);
}

namespace {

int failures{};
using Sequence = aste::sequence::plugin::SequenceAudioProcessor;

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

void setValue(Sequence& processor, const juce::String& id, float plain) {
  auto* parameter = processor.state().getParameter(id);
  require(parameter != nullptr, "Sequence parameter ID exists");
  if (parameter != nullptr) {
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
  }
}

float rawValue(Sequence& processor, const juce::String& id) {
  const auto* value = processor.state().getRawParameterValue(id);
  require(value != nullptr, "Sequence raw parameter ID exists");
  return value == nullptr ? 0.0F : value->load();
}

juce::Component* findTitled(juce::Component& component,
                            const juce::String& title) {
  if (component.getTitle() == title) {
    return &component;
  }
  for (int index = 0; index < component.getNumChildComponents(); ++index) {
    if (auto* found = findTitled(*component.getChildComponent(index), title)) {
      return found;
    }
  }
  return nullptr;
}

void testParameterAndStateContract() {
  Sequence source;
  require(source.getParameters().size() == 83,
          "Sequence exposes its exact stable parameter set");
  for (const char* id : std::array<const char*, 19>{
           "pressure", "shape", "osc_mix", "detune", "sub", "cutoff",
           "resonance", "filter_form", "env_amount", "attack", "decay",
           "sustain", "release", "glide", "output", "root", "division",
           "sequence", "bypass"}) {
    require(source.state().getParameter(id) != nullptr,
            "Every main Sequence parameter ID is stable");
  }
  setValue(source, "pressure", 71.25F);
  setValue(source, "step_04_note", -7.0F);
  setValue(source, "step_04_accent", 1.0F);
  juce::MemoryBlock state;
  source.getStateInformation(state);
  require(!state.isEmpty(), "Sequence state serialization produces data");
  Sequence restored;
  restored.setStateInformation(state.getData(),
                               static_cast<int>(state.getSize()));
  require(std::abs(rawValue(restored, "pressure") - 71.25F) < 0.011F &&
              rawValue(restored, "step_04_note") == -7.0F &&
              rawValue(restored, "step_04_accent") >= 0.5F,
          "Sequence schema restores voice and pattern values");
  juce::MemoryBlock repeated;
  restored.getStateInformation(repeated);
  require(repeated == state,
          "Sequence state bytes round-trip deterministically");

  constexpr char malformed[] = "not-sequence-state";
  restored.setStateInformation(malformed, static_cast<int>(sizeof(malformed)));
  require(std::abs(rawValue(restored, "pressure") - 71.25F) < 0.011F,
          "Malformed Sequence state leaves current state unchanged");
  require(restored.factoryPresetCount() == 4,
          "Sequence exposes four internal starting points");
  for (int index = 0; index < restored.factoryPresetCount(); ++index) {
    restored.loadFactoryPreset(index);
    require(!restored.factoryPresetName(index).isEmpty(),
            "Every Sequence preset has a visible name");
  }
}

void testStateFuzz() {
  Sequence processor;
  std::uint32_t random = 0x5e901U;
  for (int test = 0; test < 256; ++test) {
    std::array<std::byte, 257> bytes{};
    random = random * 1664525U + 1013904223U;
    const std::size_t size = random % bytes.size();
    for (std::size_t index = 0; index < size; ++index) {
      random = random * 1664525U + 1013904223U;
      bytes[index] = static_cast<std::byte>(random >> 24U);
    }
    processor.setStateInformation(bytes.data(), static_cast<int>(size));
    for (auto* parameter : processor.getParameters()) {
      require(std::isfinite(parameter->getValue()) &&
                  parameter->getValue() >= 0.0F &&
                  parameter->getValue() <= 1.0F,
              "Fuzzed Sequence state leaves normalized parameters finite");
    }
  }
}

void testInstrumentBoundary() {
  Sequence processor;
  juce::AudioProcessor::BusesLayout mono;
  mono.outputBuses.add(juce::AudioChannelSet::mono());
  juce::AudioProcessor::BusesLayout stereo;
  stereo.outputBuses.add(juce::AudioChannelSet::stereo());
  require(processor.isBusesLayoutSupported(mono),
          "Sequence supports mono instrument output");
  require(processor.isBusesLayoutSupported(stereo),
          "Sequence supports stereo instrument output");
  setValue(processor, "sequence", 0.0F);
  processor.prepareToPlay(48000.0, 511);
  require(processor.getLatencySamples() == 0,
          "Sequence adapter reports zero latency");
  juce::AudioBuffer<float> audio{2, 511};
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::noteOn(1, 48, 0.8F), 0);
  processor.processBlock(audio, midi);
  require(processor.outputPeak() > 0.0F && processor.envelopeLevel() > 0.0F,
          "Sequence publishes audible voice meters");
  for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
    require(std::isfinite(audio.getSample(0, sample)) &&
                audio.getSample(0, sample) == audio.getSample(1, sample),
            "Sequence adapter output is finite and mono-compatible");
  }

  midi.clear();
  const auto before = allocations.load(std::memory_order_relaxed);
#if JUCE_MAC
  auditThread.store(reinterpret_cast<std::uintptr_t>(pthread_self()),
                    std::memory_order_relaxed);
#endif
  auditActive.store(true, std::memory_order_relaxed);
  processor.processBlock(audio, midi);
  auditActive.store(false, std::memory_order_relaxed);
  require(allocations.load(std::memory_order_relaxed) == before,
          "Sequence JUCE processing boundary performs no allocation");

  setValue(processor, "bypass", 1.0F);
  processor.processBlock(audio, midi);
  require(audio.getMagnitude(0, 0, audio.getNumSamples()) == 0.0F,
          "Sequence bypass produces instrument silence");
  processor.releaseResources();
}

void testEditorContract() {
  Sequence processor;
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  require(editor != nullptr && editor->getWidth() == 1280 &&
              editor->getHeight() == 760 && editor->isResizable(),
          "Sequence editor opens at its intended scalable size");
  if (editor == nullptr) {
    return;
  }
  for (const char* title : std::array<const char*, 9>{
           "PRESSURE", "CUTOFF", "FILTER FORM", "OUTPUT", "DIVISION",
           "SEQUENCE", "PRESETS", "Step 01 Program", "Step 16 Program"}) {
    require(findTitled(*editor, title) != nullptr,
            "Every essential Sequence control is visible and titled");
  }
  editor->setSize(960, 600);
  juce::Image image{juce::Image::RGB, editor->getWidth(), editor->getHeight(),
                    true};
  juce::Graphics graphics{image};
  editor->paintEntireComponent(graphics, true);
  require(image.getPixelAt(0, 0).getAlpha() == 255,
          "Minimum Sequence editor size paints an opaque panel");
}

int createEditorArtifact(const char* directoryPath) {
  Sequence processor;
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  if (editor == nullptr) {
    return 1;
  }
  const auto snapshot = editor->createComponentSnapshot(
      editor->getLocalBounds(), true, 2.0F, juce::SoftwareImageType{});
  const juce::File directory{juce::String::fromUTF8(directoryPath)};
  if (directory.createDirectory().failed()) {
    return 2;
  }
  juce::FileOutputStream output{
      directory.getChildFile("sequence-s01-editor-2x.png")};
  if (!output.openedOk() || !output.setPosition(0) ||
      output.truncate().failed() ||
      !juce::PNGImageFormat{}.writeImageToStream(snapshot, output)) {
    return 2;
  }
  output.flush();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI juce;
  if (argc == 3 && std::string_view{argv[1]} == "--editor-artifacts") {
    return createEditorArtifact(argv[2]);
  }
  if (argc != 1) {
    return 2;
  }
  testParameterAndStateContract();
  testStateFuzz();
  testInstrumentBoundary();
  testEditorContract();
  if (failures == 0) {
    std::cout << "sequence_plugin_tests: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
