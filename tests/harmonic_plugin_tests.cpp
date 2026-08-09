#include "harmonic_plugin.hpp"

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

#if JUCE_MAC
#include <pthread.h>
#endif

namespace {
std::atomic<std::size_t> allocations{0};
#if JUCE_MAC
std::atomic<bool> allocationAuditActive{false};
std::atomic<std::uintptr_t> allocationAuditThread{0};

bool shouldCountAllocation() noexcept {
  return allocationAuditActive.load(std::memory_order_relaxed) &&
         allocationAuditThread.load(std::memory_order_relaxed) ==
             reinterpret_cast<std::uintptr_t>(pthread_self());
}

void setAllocationAuditActive(bool active) noexcept {
  if (active) {
    allocationAuditThread.store(
        reinterpret_cast<std::uintptr_t>(pthread_self()),
        std::memory_order_relaxed);
  }
  allocationAuditActive.store(active, std::memory_order_relaxed);
}
#else
bool shouldCountAllocation() noexcept { return true; }
void setAllocationAuditActive(bool) noexcept {}
#endif
}  // namespace

#if JUCE_MAC
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int active);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif

void* operator new(std::size_t size) {
  if (shouldCountAllocation()) {
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

int failures = 0;

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

using Harmonic = aste::harmonic::plugin::HarmonicAudioProcessor;

void setValue(Harmonic& processor, const char* id, float plainValue) {
  auto* parameter = processor.state().getParameter(id);
  require(parameter != nullptr, "Harmonic parameter ID exists");
  if (parameter != nullptr) {
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
  }
}

float rawValue(Harmonic& processor, const char* id) {
  const auto* value = processor.state().getRawParameterValue(id);
  require(value != nullptr, "Harmonic raw parameter ID exists");
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
  for (int index = 0; index < component.getNumChildComponents(); ++index) {
    if (auto* match = findTitled(*component.getChildComponent(index), title)) {
      return match;
    }
  }
  return nullptr;
}

void testParameterContract() {
  struct Contract {
    const char* id;
    const char* name;
    const char* unit;
    float value;
    const char* text;
    float tolerance;
  };
  constexpr std::array<Contract, 11> contracts{{
      {"input", "Input", "dB", -3.25F, "-3.25 dB", 0.011F},
      {"foundation_gain", "Foundation", "dB", 4.5F, "4.50 dB", 0.011F},
      {"foundation_frequency", "Foundation Frequency", "Hz", 72.5F, "72.5 Hz",
       0.11F},
      {"body_gain", "Body", "dB", -2.25F, "-2.25 dB", 0.011F},
      {"body_frequency", "Body Frequency", "Hz", 525.0F, "525.0 Hz", 0.11F},
      {"presence_gain", "Presence", "dB", 3.75F, "3.75 dB", 0.011F},
      {"presence_frequency", "Presence Frequency", "Hz", 3125.0F, "3125.0 Hz",
       0.11F},
      {"air_gain", "Air", "dB", 2.5F, "2.50 dB", 0.011F},
      {"air_frequency", "Air Frequency", "Hz", 14500.0F, "14500.0 Hz", 0.11F},
      {"harmonic", "Harmonic", "%", 67.5F, "67.50 %", 0.011F},
      {"output", "Output", "dB", -1.5F, "-1.50 dB", 0.011F},
  }};
  Harmonic processor;
  require(processor.getParameters().size() == 12,
          "Harmonic exposes exactly twelve stable parameters");
  for (const auto& contract : contracts) {
    auto* parameter = processor.state().getParameter(contract.id);
    require(parameter != nullptr && parameter->getName(64) == contract.name &&
                parameter->getLabel() == contract.unit,
            "Harmonic parameter name and unit are stable");
    if (parameter == nullptr) {
      continue;
    }
    const float parsed =
        parameter->convertFrom0to1(parameter->getValueForText(contract.text));
    require(std::abs(parsed - contract.value) <= contract.tolerance,
            "Harmonic exact parameter text parses within its interval");
  }
}

void testStateAndPresets() {
  Harmonic source;
  setValue(source, "foundation_gain", 4.5F);
  setValue(source, "presence_frequency", 3125.0F);
  setValue(source, "harmonic", 67.5F);
  setValue(source, "bypass", 1.0F);
  juce::MemoryBlock state;
  source.getStateInformation(state);
  require(!state.isEmpty(), "Harmonic state serialization produces data");

  Harmonic restored;
  restored.setStateInformation(state.getData(),
                               static_cast<int>(state.getSize()));
  require(std::abs(rawValue(restored, "foundation_gain") - 4.5F) < 0.011F &&
              std::abs(rawValue(restored, "presence_frequency") - 3125.0F) <
                  0.11F &&
              std::abs(rawValue(restored, "harmonic") - 67.5F) < 0.011F &&
              rawValue(restored, "bypass") >= 0.5F,
          "Harmonic schema 1 restores equivalent values");
  juce::MemoryBlock repeated;
  restored.getStateInformation(repeated);
  require(state == repeated,
          "Harmonic state bytes round-trip deterministically");

  const float before = rawValue(restored, "harmonic");
  constexpr char malformed[] = "not-harmonic-state";
  restored.setStateInformation(malformed, static_cast<int>(sizeof(malformed)));
  require(rawValue(restored, "harmonic") == before,
          "Malformed Harmonic state leaves current state unchanged");

  auto hostile = source.state().copyState();
  hostile.setProperty("schema", 2, nullptr);
  hostile.setProperty("product", "harmonic-h01", nullptr);
  auto hostileBytes = binaryState(hostile);
  restored.setStateInformation(hostileBytes.getData(),
                               static_cast<int>(hostileBytes.getSize()));
  require(rawValue(restored, "harmonic") == before,
          "Unknown Harmonic schemas are rejected");

  hostile = source.state().copyState();
  hostile.setProperty("schema", 1, nullptr);
  hostile.setProperty("product", "harmonic-h01", nullptr);
  hostile.getChildWithProperty("id", "harmonic")
      .setProperty("value", "nan", nullptr);
  hostileBytes = binaryState(hostile);
  restored.setStateInformation(hostileBytes.getData(),
                               static_cast<int>(hostileBytes.getSize()));
  require(rawValue(restored, "harmonic") == before,
          "Non-finite Harmonic state is rejected");

  hostile = source.state().copyState();
  hostile.setProperty("schema", 1, nullptr);
  hostile.setProperty("product", "harmonic-h01", nullptr);
  hostile.addChild(hostile.getChildWithProperty("id", "harmonic").createCopy(),
                   -1, nullptr);
  hostileBytes = binaryState(hostile);
  restored.setStateInformation(hostileBytes.getData(),
                               static_cast<int>(hostileBytes.getSize()));
  require(rawValue(restored, "harmonic") == before,
          "Duplicate Harmonic parameter IDs are rejected");

  require(restored.factoryPresetCount() == 6,
          "Harmonic exposes six internal-beta starting points");
  for (int index = 0; index < restored.factoryPresetCount(); ++index) {
    require(!restored.factoryPresetName(index).isEmpty(),
            "Every Harmonic preset has a visible name");
    restored.loadFactoryPreset(index);
    juce::MemoryBlock first;
    juce::MemoryBlock second;
    restored.getStateInformation(first);
    restored.getStateInformation(second);
    require(first == second,
            "Every Harmonic preset serializes deterministically");
  }
}

void testStateFuzz() {
  Harmonic processor;
  std::uint32_t random = 0xA501U;
  for (int test = 0; test < 512; ++test) {
    std::array<std::byte, 257> bytes{};
    random = random * 1664525U + 1013904223U;
    const std::size_t size = random % bytes.size();
    for (std::size_t index = 0; index < size; ++index) {
      random = random * 1664525U + 1013904223U;
      bytes[index] = static_cast<std::byte>(random >> 24U);
    }
    processor.setStateInformation(bytes.data(), static_cast<int>(size));
    for (const char* id : std::array<const char*, 12>{
             "input", "foundation_gain", "foundation_frequency", "body_gain",
             "body_frequency", "presence_gain", "presence_frequency",
             "air_gain", "air_frequency", "harmonic", "output", "bypass"}) {
      const auto* parameter = processor.state().getParameter(id);
      const float value = rawValue(processor, id);
      require(parameter != nullptr && std::isfinite(value) &&
                  value >= parameter->getNormalisableRange().start &&
                  value <= parameter->getNormalisableRange().end,
              "Fuzzed Harmonic state leaves bounded parameters");
    }
  }
}

void testLifecycleAndProcessingBoundary() {
  Harmonic processor;
  juce::AudioProcessor::BusesLayout mono;
  mono.inputBuses.add(juce::AudioChannelSet::mono());
  mono.outputBuses.add(juce::AudioChannelSet::mono());
  juce::AudioProcessor::BusesLayout stereo;
  stereo.inputBuses.add(juce::AudioChannelSet::stereo());
  stereo.outputBuses.add(juce::AudioChannelSet::stereo());
  juce::AudioProcessor::BusesLayout mismatched;
  mismatched.inputBuses.add(juce::AudioChannelSet::mono());
  mismatched.outputBuses.add(juce::AudioChannelSet::stereo());
  require(processor.isBusesLayoutSupported(mono), "Harmonic supports mono");
  require(processor.isBusesLayoutSupported(stereo), "Harmonic supports stereo");
  require(!processor.isBusesLayoutSupported(mismatched),
          "Harmonic rejects mismatched layouts");
  setValue(processor, "foundation_gain", 6.0F);
  setValue(processor, "presence_gain", 4.0F);
  setValue(processor, "harmonic", 80.0F);
  processor.prepareToPlay(48000.0, 511);
  require(processor.getLatencySamples() == 0,
          "Harmonic adapter reports zero latency");
  juce::AudioBuffer<float> audio{2, 511};
  juce::MidiBuffer midi;
  for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
    const float value = 0.7F * std::sin(static_cast<float>(sample) * 0.11F);
    audio.setSample(0, sample, value);
    audio.setSample(1, sample, value);
  }
  processor.processBlock(audio, midi);
  require(processor.inputPeak() > 0.0F && processor.outputPeak() > 0.0F,
          "Harmonic publishes boundary meters");

  setValue(processor, "bypass", 1.0F);
  const float bypassSample = audio.getSample(0, 10);
  processor.processBlock(audio, midi);
  require(audio.getSample(0, 10) == bypassSample,
          "Harmonic owned bypass leaves audio unchanged");
  setValue(processor, "bypass", 0.0F);

  const auto before = allocations.load(std::memory_order_relaxed);
#if JUCE_MAC
  asteRealtimeAuditReset();
  asteRealtimeAuditSetActive(1);
#endif
  setAllocationAuditActive(true);
  processor.processBlock(audio, midi);
  setAllocationAuditActive(false);
#if JUCE_MAC
  asteRealtimeAuditSetActive(0);
#endif
  const auto after = allocations.load(std::memory_order_relaxed);
  require(after == before,
          "Harmonic JUCE processing boundary performs no allocation");
#if JUCE_MAC
  require(asteRealtimeAuditLockCalls() == 0 &&
              asteRealtimeAuditFileOpenCalls() == 0 &&
              asteRealtimeAuditWriteCalls() == 0,
          "Harmonic processing performs no locks, file access, or writes");
#endif
  for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
    for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
      require(std::isfinite(audio.getSample(channel, sample)),
              "Harmonic adapter output remains finite");
    }
  }
  processor.releaseResources();
}

void testEditorContract() {
  Harmonic processor;
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  require(editor != nullptr && editor->getWidth() == 1120 &&
              editor->getHeight() == 620 && editor->isResizable(),
          "Harmonic editor opens at its intended scalable size");
  if (editor == nullptr) {
    return;
  }
  for (const char* title : std::array<const char*, 12>{
           "HARMONIC", "INPUT", "OUTPUT", "FOUNDATION", "BODY", "PRESENCE",
           "AIR", "FOUNDATION FREQUENCY", "BODY FREQUENCY",
           "PRESENCE FREQUENCY", "AIR FREQUENCY", "PRESETS"}) {
    require(findTitled(*editor, title) != nullptr,
            "Every essential Harmonic control is visible and titled");
  }
  editor->setSize(860, 480);
  juce::Image image{juce::Image::RGB, editor->getWidth(), editor->getHeight(),
                    true};
  juce::Graphics graphics{image};
  editor->paintEntireComponent(graphics, true);
  require(image.getPixelAt(0, 0).getAlpha() == 255,
          "Minimum Harmonic editor size paints an opaque panel");
}

}  // namespace

int main() {
  juce::ScopedJuceInitialiser_GUI juce;
  testParameterContract();
  testStateAndPresets();
  testStateFuzz();
  testLifecycleAndProcessingBoundary();
  testEditorContract();
  if (failures == 0) {
    std::cout << "harmonic_plugin_tests: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
