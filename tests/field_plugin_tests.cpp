#include "field_plugin.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <new>
#include <string>

#if JUCE_MAC
#include <pthread.h>
#endif

namespace {
std::atomic<std::size_t> allocations{};
#if JUCE_MAC
std::atomic<bool> audit{};
std::atomic<std::uintptr_t> thread{};
bool countAllocation() noexcept {
  return audit.load(std::memory_order_relaxed) &&
         thread.load(std::memory_order_relaxed) ==
             reinterpret_cast<std::uintptr_t>(pthread_self());
}
void setAudit(bool enabled) noexcept {
  if (enabled) thread.store(reinterpret_cast<std::uintptr_t>(pthread_self()));
  audit.store(enabled);
}
#else
bool countAllocation() noexcept { return true; }
void setAudit(bool) noexcept {}
#endif
int failures{};
}  // namespace

#if JUCE_MAC
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int active);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif

void* operator new(std::size_t size) {
  if (countAllocation()) allocations.fetch_add(1, std::memory_order_relaxed);
  if (void* value = std::malloc(size)) return value;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

namespace {
using Field = aste::field::plugin::FieldAudioProcessor;

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}
void setValue(Field& processor, const char* id, float plain) {
  auto* parameter = processor.state().getParameter(id);
  require(parameter != nullptr, "Field parameter exists");
  if (parameter)
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
}
float value(Field& processor, const char* id) {
  const auto* raw = processor.state().getRawParameterValue(id);
  return raw ? raw->load() : 0.0F;
}
juce::Component* findTitled(juce::Component& component,
                            const juce::String& title) {
  if (component.getTitle() == title) return &component;
  for (int index = 0; index < component.getNumChildComponents(); ++index)
    if (auto* found = findTitled(*component.getChildComponent(index), title))
      return found;
  return nullptr;
}

void testContractAndState() {
  Field source;
  require(source.getParameters().size() == 9,
          "Field exposes nine stable parameters");
  require(source.acceptsMidi() && source.getTailLengthSeconds() >= 120.0,
          "Field advertises MIDI excitation and its extended tail");
  setValue(source, "forever", 1.0F);
  setValue(source, "grain", 73.25F);
  setValue(source, "pitch", 61.0F);
  juce::MemoryBlock state;
  source.getStateInformation(state);
  Field restored;
  restored.setStateInformation(state.getData(),
                               static_cast<int>(state.getSize()));
  require(value(restored, "forever") > 0.5F &&
              std::abs(value(restored, "grain") - 73.25F) < 0.02F &&
              std::abs(value(restored, "pitch") - 61.0F) < 0.02F,
          "Field state restores exactly");
  juce::MemoryBlock repeated;
  restored.getStateInformation(repeated);
  require(state == repeated, "Field state bytes are deterministic");
  constexpr char malformed[] = "bad-field";
  restored.setStateInformation(malformed, sizeof(malformed));
  require(std::abs(value(restored, "grain") - 73.25F) < 0.02F,
          "Malformed Field state is rejected");
  require(restored.factoryPresetCount() == 5,
          "Field provides five deliberate starting points");
}

void testProcessingMidiAndRealtime() {
  Field processor;
  processor.prepareToPlay(48000.0, 127);
  setValue(processor, "forever", 1.0F);
  setValue(processor, "blend", 100.0F);
  juce::AudioBuffer<float> audio{2, 127};
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9F), 23);
  const auto before = allocations.load(std::memory_order_relaxed);
#if JUCE_MAC
  asteRealtimeAuditReset();
  asteRealtimeAuditSetActive(1);
#endif
  setAudit(true);
  processor.processBlock(audio, midi);
  setAudit(false);
#if JUCE_MAC
  asteRealtimeAuditSetActive(0);
#endif
  require(allocations.load(std::memory_order_relaxed) == before,
          "Field JUCE boundary allocates nothing under MIDI excitation");
#if JUCE_MAC
  require(asteRealtimeAuditLockCalls() == 0 &&
              asteRealtimeAuditFileOpenCalls() == 0 &&
              asteRealtimeAuditWriteCalls() == 0,
          "Field processing performs no locks, file access, or writes");
#endif
  midi.clear();
  for (int block = 0; block < 30; ++block) {
    audio.clear();
    processor.processBlock(audio, midi);
  }
  require(processor.fieldEnergy() > 0.0F && processor.outputPeak() > 0.0F,
          "Sample-offset MIDI excitation creates and meters a spatial field");
  for (int channel = 0; channel < audio.getNumChannels(); ++channel)
    for (int sample = 0; sample < audio.getNumSamples(); ++sample)
      require(std::isfinite(audio.getSample(channel, sample)),
              "Field adapter output remains finite");
  require(processor.getLatencySamples() == 0,
          "Field adapter reports zero latency");
}

void testEditor(const char* artifactDirectory) {
  Field processor;
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  require(editor && editor->getWidth() == 1080 && editor->getHeight() == 620 &&
              editor->isResizable(),
          "Field editor uses its intended scalable panel");
  if (!editor) return;
  for (const char* title :
       std::array<const char*, 9>{"FOREVER", "MASS", "GRAIN", "PITCH", "MOTION",
                                  "DISTANCE", "BLEND", "OUTPUT", "PRESETS"})
    require(findTitled(*editor, title),
            "Every essential Field control remains visible");
  juce::Image image{juce::Image::RGB, editor->getWidth(), editor->getHeight(),
                    true};
  juce::Graphics graphics{image};
  editor->paintEntireComponent(graphics, true);
  require(image.getPixelAt(0, 0).getAlpha() == 255,
          "Field editor paints an opaque panel");
  if (artifactDirectory) {
    std::filesystem::create_directories(artifactDirectory);
    juce::PNGImageFormat png;
    juce::File file{juce::String{artifactDirectory}};
    file = file.getChildFile("field-f01-editor.png");
    if (auto stream = file.createOutputStream())
      png.writeImageToStream(image, *stream);
  }
}

}  // namespace

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI juce;
  testContractAndState();
  testProcessingMidiAndRealtime();
  testEditor(argc == 3 && std::string{argv[1]} == "--editor-artifacts"
                 ? argv[2]
                 : nullptr);
  if (failures == 0) std::cout << "field_plugin_tests: ok\n";
  return failures == 0 ? 0 : 1;
}
