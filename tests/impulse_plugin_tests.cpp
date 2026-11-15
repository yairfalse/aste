#include "impulse_plugin.hpp"
#include "response_upgrade_checks.hpp"
#include "preset_recall_checks.hpp"
#include "editor_artifact.hpp"
#include "behavior_state_checks.hpp"

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
void testClockState() {
  Impulse fresh;
  juce::MemoryBlock bytes;
  fresh.getStateInformation(bytes);
  auto xml = juce::AudioProcessor::getXmlFromBinary(
      bytes.getData(), static_cast<int>(bytes.getSize()));
  require(xml && xml->getIntAttribute("schema") == 5 && fresh.usesSampleClock(),
          "Fresh Impulse uses the sample-owned clock");
  if (!xml) return;
  xml->setAttribute("schema", 4);
  xml->removeAttribute("clock");
  juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
  Impulse old;
  old.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
  require(!old.usesLegacyBehavior() && !old.usesSampleClock(),
          "Schema 4 preserves its direct sound and historical clock");
  old.getStateInformation(bytes);
  Impulse reopened;
  reopened.setStateInformation(bytes.getData(),
                               static_cast<int>(bytes.getSize()));
  juce::MemoryBlock repeated;
  reopened.getStateInformation(repeated);
  require(bytes == repeated && !reopened.usesSampleClock(),
          "Old clock choice survives save and reopen");
  auto saved = juce::AudioProcessor::getXmlFromBinary(
      bytes.getData(), static_cast<int>(bytes.getSize()));
  for (const char* clock : {"", "2", "-1", "1.0", "true", "NaN"}) {
    auto invalid = std::make_unique<juce::XmlElement>(*saved);
    if (*clock)
      invalid->setAttribute("clock", clock);
    else
      invalid->removeAttribute("clock");
    juce::MemoryBlock bad;
    juce::AudioProcessor::copyXmlToBinary(*invalid, bad);
    reopened.setStateInformation(bad.getData(),
                                 static_cast<int>(bad.getSize()));
    reopened.getStateInformation(repeated);
    require(bytes == repeated, "Invalid clock rejects the entire state");
  }
  reopened.useCurrentResponse();
  require(reopened.usesSampleClock() && !reopened.usesLegacyBehavior(),
          "Explicit upgrade enables the repaired clock");
}

void testContractState() {
  Impulse source;
  require(!source.usesLegacyBehavior() && value(source, "energy") == 18,
          "New instances start with restrained click and sub excitation");
  int activeTracks{}, hits{};
  for (const char* track :
       {"kick", "click", "burst", "body", "low", "crack", "metal", "cut"}) {
    int trackHits{};
    for (int step = 1; step <= 32; ++step) {
      const auto id = juce::String{track} + "_step_" +
                      juce::String{step}.paddedLeft('0', 2);
      trackHits += value(source, id.toRawUTF8()) > 0;
    }
    activeTracks += trackHits > 0;
    hits += trackHits;
  }
  require(activeTracks == 2 && hits == 5,
          "Clicks and sub leave eleven of sixteen positions empty");
  require(value(source, "click_step_04") == 1 &&
              value(source, "click_step_08") == 1 &&
              value(source, "click_step_15") == 1 &&
              value(source, "low_step_01") == 1 &&
              value(source, "low_step_11") == 1 &&
              value(source, "kick_step_01") == 0 &&
              value(source, "crack_step_07") == 0,
          "The starting vocabulary is Click and Low, not Kick and Crack");
  require(value(source, "click_decay") <= 8 &&
              value(source, "low_pitch") >= 35 &&
              value(source, "low_pitch") <= 45 &&
              value(source, "low_tone") == 0 && value(source, "low_drive") == 0,
          "Tiny click envelopes contrast with a low-drive sub");
  require(value(source, "variation") == 0 && value(source, "mutation") == 0,
          "New instances start without random variation or mutation");
  for (const auto* track :
       {"kick", "click", "burst", "body", "low", "crack", "metal", "cut"}) {
    const auto prefix = std::string{track};
    require(value(source, (prefix + "_length").c_str()) == 16 &&
                value(source, (prefix + "_probability").c_str()) == 100,
            "Starter groove uses a shared length and predictable hits");
  }
  source.loadFactoryPreset(5);
  require(value(source, "energy") == 30 && value(source, "kick_decay") == 320 &&
              value(source, "kick_step_10") == 1 &&
              value(source, "crack_step_07") == 1 &&
              value(source, "low_step_01") == 0,
          "Space remains available without reinterpretation");
  setValue(source, "kick_pitch", 999);
  source.loadFactoryPreset(0);
  require(value(source, "kick_pitch") == 48,
          "Starting points reset sound settings, not just patterns");
  require(source.getParameters().size() == 368,
          "Impulse exposes eight sounds and eight visible 32-step patterns");
  setValue(source, "seed", 4242);
  setValue(source, "kick_length", 15);
  setValue(source, "kick_pulses", 1);
  setValue(source, "kick_rotation", 3);
  setValue(source, "click_probability", 62.5F);
  setValue(source, "kick_step_04", 2.0F);
  setValue(source, "cut_step_32", 1.0F);
  juce::MemoryBlock state;
  source.getStateInformation(state);
  Impulse restored;
  restored.setStateInformation(state.getData(),
                               static_cast<int>(state.getSize()));
  require(value(restored, "seed") == 4242 &&
              value(restored, "kick_length") == 15 &&
              std::abs(value(restored, "click_probability") - 62.5F) < 0.02F &&
              value(restored, "kick_step_04") == 2.0F &&
              value(restored, "cut_step_32") == 1.0F,
          "Impulse state restores pattern and seed");
  juce::MemoryBlock repeated;
  restored.getStateInformation(repeated);
  require(repeated == state, "Impulse state bytes are deterministic");
  const auto xml = juce::AudioProcessor::getXmlFromBinary(
      state.getData(), static_cast<int>(state.getSize()));
  require(xml != nullptr, "Impulse state exposes migration XML");
  if (xml != nullptr) {
    auto oldDefaults = juce::ValueTree::fromXml(*xml);
    oldDefaults.setProperty("schema", 2, nullptr);
    for (const auto* id : {"click_length", "click_probability", "variation"})
      oldDefaults.removeChild(oldDefaults.getChildWithProperty("id", id),
                              nullptr);
    juce::MemoryBlock oldState;
    juce::AudioProcessor::copyXmlToBinary(*oldDefaults.createXml(), oldState);
    restored.setStateInformation(oldState.getData(),
                                 static_cast<int>(oldState.getSize()));
    require(value(restored, "click_length") == 23 &&
                value(restored, "click_probability") == 85 &&
                value(restored, "variation") == 12,
            "Schema 2 missing fields retain their historical defaults");
    auto legacy = juce::ValueTree::fromXml(*xml);
    legacy.setProperty("schema", 1, nullptr);
    for (int index = legacy.getNumChildren(); --index >= 0;) {
      const auto child = legacy.getChild(index);
      const auto id = child.getProperty("id").toString();
      if (id.contains("_step_") || id.startsWith("low_") ||
          id.startsWith("crack_") || id.startsWith("metal_") ||
          id.startsWith("cut_"))
        legacy.removeChild(index, nullptr);
    }
    juce::MemoryBlock legacyBytes;
    if (const auto legacyXml = legacy.createXml())
      juce::AudioProcessor::copyXmlToBinary(*legacyXml, legacyBytes);
    Impulse migrated;
    migrated.setStateInformation(legacyBytes.getData(),
                                 static_cast<int>(legacyBytes.getSize()));
    require(value(migrated, "seed") == 4242 &&
                value(migrated, "kick_length") == 15 &&
                value(migrated, "low_level") == 72.0F &&
                value(migrated, "kick_step_01") == 0.0F &&
                value(migrated, "kick_step_13") == 1.0F,
            "Schema 1 Impulse state reconstructs old Euclidean playback");
  }
  constexpr char bad[] = "bad";
  restored.setStateInformation(bad, sizeof(bad));
  require(value(restored, "seed") == 4242,
          "Malformed Impulse state is rejected");
  require(
      restored.factoryPresetCount() == 8,
      "Impulse appends two click/sub scenes without replacing six snapshots");
  for (int preset : {6, 7}) {
    Impulse scene;
    scene.loadFactoryPreset(preset);
    juce::MemoryBlock saved;
    scene.getStateInformation(saved);
    Impulse reopened;
    reopened.setStateInformation(saved.getData(),
                                 static_cast<int>(saved.getSize()));
    juce::MemoryBlock again;
    reopened.getStateInformation(again);
    require(saved == again && !reopened.usesLegacyBehavior(),
            "Click/sub scenes recall byte-stably through schema 5");
  }
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
  midi.addEvent(juce::MidiMessage::noteOn(1, 43, 0.8F), 61);
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
  auto* focus =
      dynamic_cast<juce::Button*>(findTitled(*editor, "OBJECT VISIBILITY"));
  auto* emptyRow = findTitled(*editor, "METAL TRACK");
  auto* subRow = findTitled(*editor, "LOW TRACK");
  require(focus && emptyRow && subRow && !emptyRow->isVisible() &&
              subRow->isVisible(),
          "Click/sub focus shows programmed objects without eight empty rows");
  if (focus && emptyRow) {
    juce::MemoryBlock before, after;
    processor.getStateInformation(before);
    focus->onClick();
    require(emptyRow->isVisible(),
            "Show all exposes every object for programming");
    focus->onClick();
    require(!emptyRow->isVisible(), "Active objects restores the compact view");
    processor.getStateInformation(after);
    require(before == after,
            "Object visibility never changes the musical state");
    for (const auto& change : std::array{std::pair{"mutation", 10.0F},
                                         std::pair{"sequence", 0.0F}}) {
      setValue(processor, change.first, change.second);
      for (int attempt = 0; !emptyRow->isVisible() && attempt < 50; ++attempt) {
        juce::Thread::sleep(20);
        juce::Timer::callPendingTimersSynchronously();
      }
      require(emptyRow->isVisible() && !focus->isEnabled(),
              "Mutation and MIDI-only mode expose every potentially sounding "
              "object");
      setValue(processor, change.first, change.second == 0 ? 1.0F : 0.0F);
    }
  }
  auto* cell = findTitled(*editor, "KICK STEP 02");
  setValue(processor, "kick_step_02", 2);
  for (int attempt = 0;
       cell && cell->getDescription() != "Accent" && attempt < 50; ++attempt) {
    juce::Thread::sleep(20);
    juce::Timer::callPendingTimersSynchronously();
  }
  require(cell && cell->getDescription() == "Accent",
          "Stopped pattern cells refresh after an external parameter change");
  require(cell && cell->isVisible(),
          "Automation reveals a newly programmed object");
  setValue(processor, "kick_step_02", 0);
  auto* kick = dynamic_cast<juce::Button*>(findTitled(*editor, "KICK TRACK"));
  require(kick != nullptr, "Kick selection target exists");
  if (kick) {
    processor.prepareToPlay(48000, 127);
    kick->onClick();
    juce::AudioBuffer<float> quiet{2, 127};
    juce::MidiBuffer midi;
    processor.processBlock(quiet, midi);
    require(quiet.getMagnitude(0, 0, 127) == 0,
            "Selecting a track does not trigger it");
  }
  kick = dynamic_cast<juce::Button*>(findTitled(*editor, "KICK AUDITION"));
  require(kick != nullptr, "Explicit audition target exists");
  if (kick) {
    processor.prepareToPlay(48000, 127);
    kick->onClick();
    juce::AudioBuffer<float> audio{2, 127};
    juce::MidiBuffer midi;
    audio.clear();
    const auto before = allocations.load();
    setAudit(true);
    processor.processBlock(audio, midi);
    setAudit(false);
    require(allocations.load() == before && audio.getMagnitude(0, 0, 127) > 0,
            "Click auditions a drum without host playback, MIDI or audio "
            "allocation");
    kick->onClick();
    processor.processBlockBypassed(audio, midi);
    processor.processBlock(audio, midi);
    require(audio.getMagnitude(0, 0, 127) == 0,
            "Leaving bypass does not replay a queued audition or frozen voice");
    kick->onClick();
    processor.releaseResources();
    processor.prepareToPlay(48000, 127);
    processor.processBlock(audio, midi);
    require(audio.getMagnitude(0, 0, 127) == 0,
            "Restarting audio does not replay an old audition request");
  }
  for (const char* title :
       std::array{"ENERGY", "VARIATION", "MUTATION", "SEQUENCE", "KICK LEVEL",
                  "BODY ACCENT", "LOW TRACK", "METAL STEP 32", "CUT GENERATE",
                  "PRESETS"})
    require(findTitled(*editor, title),
            "Impulse essential controls are visible");
  auto* presets = dynamic_cast<juce::ComboBox*>(findTitled(*editor, "PRESETS"));
  auto* clickLevel = findTitled(*editor, "CLICK LEVEL");
  auto* cutLevel = findTitled(*editor, "CUT LEVEL");
  const auto visible = [&editor](juce::Component* control) {
    if (!control) return false;
    for (auto* c = control; c && c != editor.get(); c = c->getParentComponent())
      if (!c->isVisible()) return false;
    return true;
  };
  require(presets && clickLevel && cutLevel,
          "Sparse scenes have accessible preset and sound selectors");
  if (presets && clickLevel && cutLevel) {
    presets->setSelectedId(8, juce::sendNotificationSync);
    require(visible(cutLevel) && !visible(clickLevel) &&
                value(processor, "cut_decay") == 8,
            "Cuts / pressure opens its active transient controls");
    presets->setSelectedId(7, juce::sendNotificationSync);
    require(visible(clickLevel) && !visible(cutLevel) &&
                value(processor, "click_decay") == 5,
            "Clicks / sub opens its active transient controls");
  }
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
    require(writeMinimumEditorArtifact(
                *editor, file.getSiblingFile("impulse-minimum.png")),
            "Impulse controls fit and render at the minimum size");
  }
}
}  // namespace
int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI juce;
  aste::tests::responseUpgradeChecks<
      aste::impulse::plugin::ImpulseAudioProcessor>(require, 4);
  aste::tests::presetRecallChecks<aste::impulse::plugin::ImpulseAudioProcessor>(
      require);
  aste::tests::behaviorStateChecks<Impulse>(require, 3, 5);
  testClockState();
  testContractState();
  testProcessing();
  testEditor(argc == 3 && std::string{argv[1]} == "--editor-artifacts"
                 ? argv[2]
                 : nullptr);
  if (failures == 0) std::cout << "impulse_plugin_tests: ok\n";
  return failures == 0 ? 0 : 1;
}
