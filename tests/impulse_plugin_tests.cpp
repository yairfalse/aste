#include "impulse_plugin.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
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
bool count() noexcept {
  return audit.load() &&
         thread.load() == reinterpret_cast<std::uintptr_t>(pthread_self());
}
void setAudit(bool active) noexcept {
  if (active) thread.store(reinterpret_cast<std::uintptr_t>(pthread_self()));
  audit.store(active);
}
#else
bool count() noexcept { return true; }
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
  if (count()) allocations.fetch_add(1);
  if (void* p = std::malloc(size)) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
using Impulse = aste::impulse::plugin::ImpulseAudioProcessor;
void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}
void setValue(Impulse& p, const char* id, float plain) {
  auto* parameter = p.state().getParameter(id);
  require(parameter, "Impulse parameter exists");
  if (parameter)
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plain));
}
float value(Impulse& p, const char* id) {
  const auto* raw = p.state().getRawParameterValue(id);
  return raw ? raw->load() : 0;
}
juce::Component* findTitled(juce::Component& c, const juce::String& title) {
  if (c.getTitle() == title) return &c;
  for (int i = 0; i < c.getNumChildComponents(); ++i)
    if (auto* found = findTitled(*c.getChildComponent(i), title)) return found;
  return nullptr;
}
void testContractState() {
  Impulse source;
  require(source.getParameters().size() == 60,
          "Impulse exposes 60 stable parameters");
  setValue(source, "seed", 4242);
  setValue(source, "kick_length", 15);
  setValue(source, "click_probability", 62.5F);
  juce::MemoryBlock state;
  source.getStateInformation(state);
  Impulse restored;
  restored.setStateInformation(state.getData(),
                               static_cast<int>(state.getSize()));
  require(value(restored, "seed") == 4242 &&
              value(restored, "kick_length") == 15 &&
              std::abs(value(restored, "click_probability") - 62.5F) < 0.02F,
          "Impulse state restores pattern and seed");
  juce::MemoryBlock repeated;
  restored.getStateInformation(repeated);
  require(repeated == state, "Impulse state bytes are deterministic");
  constexpr char bad[] = "bad";
  restored.setStateInformation(bad, sizeof(bad));
  require(value(restored, "seed") == 4242,
          "Malformed Impulse state is rejected");
  require(restored.factoryPresetCount() == 5, "Impulse exposes five snapshots");
  std::uint32_t random = 0x1a2b3c4dU;
  for (int test = 0; test < 256; ++test) {
    std::array<std::byte, 257> bytes{};
    random = random * 1664525U + 1013904223U;
    const auto size = static_cast<std::size_t>(random % bytes.size());
    for (std::size_t index = 0; index < size; ++index) {
      random = random * 1664525U + 1013904223U;
      bytes[index] = static_cast<std::byte>(random >> 24U);
    }
    restored.setStateInformation(bytes.data(), static_cast<int>(size));
    require(std::isfinite(value(restored, "seed")) &&
                value(restored, "seed") >= 0 &&
                value(restored, "seed") <= 65535,
            "Fuzzed Impulse state leaves bounded configuration");
  }
}
void testProcessing() {
  Impulse processor;
  processor.prepareToPlay(48000, 127);
  juce::AudioBuffer<float> audio{2, 127};
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.0F), 23);
  midi.addEvent(juce::MidiMessage::noteOn(1, 39, 0.8F), 61);
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
          "Impulse JUCE boundary allocates nothing");
#if JUCE_MAC
  require(asteRealtimeAuditLockCalls() == 0 &&
              asteRealtimeAuditFileOpenCalls() == 0 &&
              asteRealtimeAuditWriteCalls() == 0,
          "Impulse callback performs no forbidden operation");
#endif
  require(processor.outputPeak() > 0 && processor.getLatencySamples() == 0,
          "Impulse MIDI instrument emits finite zero-latency audio");
  for (int c = 0; c < 2; ++c)
    for (int s = 0; s < 127; ++s)
      require(std::isfinite(audio.getSample(c, s)),
              "Impulse adapter output remains finite");
}
void testEditor(const char* directory) {
  Impulse processor;
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  require(editor && editor->getWidth() == 1480 && editor->getHeight() == 900 &&
              editor->isResizable(),
          "Impulse opens at its scalable product size");
  if (!editor) return;
  for (const char* title :
       std::array{"ENERGY", "VARIATION", "MUTATION", "SEQUENCE", "KICK LEVEL",
                  "BODY ACCENT", "PRESETS"})
    require(findTitled(*editor, title),
            "Impulse essential controls are visible");
  juce::Image image{juce::Image::RGB, editor->getWidth(), editor->getHeight(),
                    true};
  juce::Graphics g{image};
  editor->paintEntireComponent(g, true);
  require(image.getPixelAt(0, 0).getAlpha() == 255,
          "Impulse editor paints opaque");
  if (directory) {
    std::filesystem::create_directories(directory);
    juce::PNGImageFormat png;
    juce::File file{juce::String{directory}};
    file = file.getChildFile("impulse-i01-editor.png");
    if (auto stream = file.createOutputStream())
      png.writeImageToStream(image, *stream);
  }
}
}  // namespace
int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI juce;
  testContractState();
  testProcessing();
  testEditor(argc == 3 && std::string{argv[1]} == "--editor-artifacts"
                 ? argv[2]
                 : nullptr);
  if (failures == 0) std::cout << "impulse_plugin_tests: ok\n";
  return failures == 0 ? 0 : 1;
}
