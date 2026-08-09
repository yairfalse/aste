#include "loop_plugin.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iostream>
#include <memory>
#include <new>

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
using Loop = aste::loop::plugin::LoopAudioProcessor;
void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}
void setValue(Loop& processor, const char* id, float plain) {
  auto* parameter = processor.state().getParameter(id);
  require(parameter != nullptr, "Loop parameter exists");
  if (parameter)
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
}
float value(Loop& processor, const char* id) {
  const auto* raw = processor.state().getRawParameterValue(id);
  return raw ? raw->load() : 0;
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
  Loop source;
  require(source.getParameters().size() == 20,
          "Loop exposes 20 stable parameters");
  setValue(source, "feedback", 73.25F);
  setValue(source, "pitch", -7.0F);
  setValue(source, "reverse", 1.0F);
  juce::MemoryBlock state;
  source.getStateInformation(state);
  Loop restored;
  restored.setStateInformation(state.getData(),
                               static_cast<int>(state.getSize()));
  require(std::abs(value(restored, "feedback") - 73.25F) < 0.02F &&
              std::abs(value(restored, "pitch") + 7.0F) < 0.02F &&
              value(restored, "reverse") > 0.5F,
          "Loop control state restores exactly");
  juce::MemoryBlock repeated;
  restored.getStateInformation(repeated);
  require(state == repeated, "Loop control state bytes are deterministic");
  constexpr char malformed[] = "bad-loop";
  restored.setStateInformation(malformed, sizeof(malformed));
  require(std::abs(value(restored, "feedback") - 73.25F) < 0.02F,
          "Malformed Loop state is rejected");
  juce::XmlElement legacy{"loop-l01"};
  legacy.setAttribute("schema", 1);
  legacy.setAttribute("product", "loop-l01");
  auto* legacyFeedback = legacy.createNewChildElement("PARAM");
  legacyFeedback->setAttribute("id", "feedback");
  legacyFeedback->setAttribute("value", 61.0);
  juce::MemoryBlock legacyState;
  juce::AudioProcessor::copyXmlToBinary(legacy, legacyState);
  restored.setStateInformation(legacyState.getData(),
                               static_cast<int>(legacyState.getSize()));
  require(std::abs(value(restored, "feedback") - 61.0F) < 0.02F &&
              std::abs(value(restored, "tape_speed") - 1.0F) < 0.02F,
          "Loop schema 1 migrates with the new tape speed default");
  require(restored.factoryPresetCount() == 5, "Loop has five starting points");
}

void testProcessingMidiAndRealtime() {
  Loop processor;
  processor.prepareToPlay(48000, 127);
  juce::AudioBuffer<float> audio{2, 127};
  for (int sample = 0; sample < 127; ++sample) {
    const float signal = 0.4F * std::sin(sample * 0.1F);
    audio.setSample(0, sample, signal);
    audio.setSample(1, sample, signal);
  }
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0F), 23);
  midi.addEvent(juce::MidiMessage::noteOff(1, 60), 101);
  const auto before = allocations.load();
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
  require(allocations.load() == before,
          "Loop JUCE boundary allocates nothing under MIDI capture");
#if JUCE_MAC
  require(asteRealtimeAuditLockCalls() == 0 &&
              asteRealtimeAuditFileOpenCalls() == 0 &&
              asteRealtimeAuditWriteCalls() == 0,
          "Loop processing performs no locks, file access, or writes");
#endif
  require(processor.inputPeak() > 0 && processor.outputPeak() > 0 &&
              processor.capturedAmount() > 0,
          "Loop MIDI capture and meters operate");
  for (int channel = 0; channel < 2; ++channel)
    for (int sample = 0; sample < 127; ++sample)
      require(std::isfinite(audio.getSample(channel, sample)),
              "Loop output remains finite");
  require(processor.getLatencySamples() == 0,
          "Loop adapter reports zero latency");

  audio.clear();
  midi.clear();
  midi.addEvent(juce::MidiMessage::noteOn(1, 62, 1.0F), 23);
  const auto reloopBefore = allocations.load();
  setAudit(true);
  processor.processBlock(audio, midi);
  setAudit(false);
  require(allocations.load() == reloopBefore,
          "Loop sample-offset MIDI RELOOP printing performs no allocation");
  require(processor.generation() == 2 && processor.retainedGenerations() == 2,
          "Loop adapter prints and publishes a second generation");
}

void testEditor(const char* artifactDirectory) {
  Loop processor;
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  require(editor && editor->getWidth() == 1160 && editor->getHeight() == 650 &&
              editor->isResizable(),
          "Loop editor uses its intended scalable panel");
  if (!editor) return;
  for (const char* title : std::array<const char*, 11>{
           "CAPTURE", "RELOOP", "PREVIOUS GENERATION", "NEXT GENERATION",
           "HOST SYNC", "REVERSE", "TAPE SPEED", "OVERDUB", "PITCH", "OUTPUT",
           "CLEAR MEMORY"})
    require(findTitled(*editor, title),
            "Essential Loop control remains visible");
  juce::Image image{juce::Image::RGB, editor->getWidth(), editor->getHeight(),
                    true};
  juce::Graphics graphics{image};
  editor->paintEntireComponent(graphics, true);
  require(image.getPixelAt(0, 0).getAlpha() == 255,
          "Loop editor paints an opaque panel");
  if (artifactDirectory) {
    std::filesystem::create_directories(artifactDirectory);
    juce::PNGImageFormat png;
    juce::File file{juce::String{artifactDirectory}};
    file = file.getChildFile("loop-l01-editor.png");
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
  if (failures == 0) std::cout << "loop_plugin_tests: ok\n";
  return failures == 0 ? 0 : 1;
}
