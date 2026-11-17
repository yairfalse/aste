#include "workflow_checks.hpp"
#include "field_plugin.hpp"
#include "master_output_checks.hpp"
#include "response_upgrade_checks.hpp"
#include "preset_recall_checks.hpp"
#include "editor_artifact.hpp"
#include "behavior_state_checks.hpp"

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
  require(source.getParameters().size() == 19,
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
  require(restored.factoryPresetCount() == 15,
          "Field preserves nine original presets and adds six shipping "
          "starting points");
}

void testProcessingMidiAndRealtime() {
  Field processor;
  processor.prepareToPlay(48000.0, 127);
  setValue(processor, "forever", 1.0F);
  setValue(processor, "blend", 100.0F);
  juce::AudioBuffer<float> audio{2, 127};
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::noteOn(1, 60, 0.9F), 23);
  const std::array<unsigned char, 32> payload{};
  midi.addEvent(
      juce::MidiMessage::createSysExMessage(payload.data(), payload.size()),
      45);
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
  require(editor && editor->getWidth() == 1080 && editor->getHeight() == 800 &&
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
    require(writeMinimumEditorArtifact(
                *editor, file.getSiblingFile("field-minimum.png")),
            "Field controls fit and render at the minimum size");
  }
}

void testEmptyHoldAndClear() {
  Field processor;
  processor.prepareToPlay(48000, 127);
  setValue(processor, "forever", 1);
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  auto* hold = findTitled(*editor, "FOREVER");
  require(hold && hold->getDescription().contains("HOLD ACTIVE") &&
              !hold->getDescription().contains("HOLDING AUDIO"),
          "Hold reports its control state without claiming to know total "
          "stored energy");
  juce::AudioBuffer<float> audio{2, 127};
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0F), 0);
  for (int block = 0; block < 200; ++block) {
    audio.clear();
    processor.processBlock(audio, midi);
    midi.clear();
  }
  require(processor.fieldEnergy() > 0, "MIDI excitation fills the field");
  auto* clear = dynamic_cast<juce::Button*>(findTitled(*editor, "CLEAR FIELD"));
  require(clear != nullptr, "The panel provides a clear action");
  if (clear) clear->onClick();
  audio.clear();
  processor.processBlock(audio, midi);
  for (int i = 0; i < 4; ++i) {
    audio.clear();
    processor.processBlock(audio, midi);
  }
  require(processor.fieldEnergy() == 0 && audio.getMagnitude(0, 0, 127) == 0,
          "Clear empties stored energy and output without changing the hold "
          "control");
}
int renderListening(const char* directoryPath) {
  const juce::File directory{juce::String::fromUTF8(directoryPath)};
  if (directory.createDirectory().failed()) return 2;
  for (int scene = 0; scene < 4; ++scene) {
    Field processor;
    processor.loadFactoryPreset(scene == 3 ? 3 : 0);
    if (scene == 1) setValue(processor, "texture", 0);
    processor.prepareToPlay(48000, 127);
    const auto name =
        std::array{"Dry input", "Field - texture off", "Field - moving pitch",
                   "Field - grain cloud"}[scene];
    auto file = directory.getChildFile(juce::String{name} + ".wav");
    std::unique_ptr<juce::OutputStream> stream = file.createOutputStream();
    if (!stream || !stream->setPosition(0)) return 2;
    juce::WavAudioFormat format;
    auto writer =
        format.createWriterFor(stream, juce::AudioFormatWriterOptions{}
                                           .withSampleRate(48000)
                                           .withNumChannels(2)
                                           .withBitsPerSample(24));
    if (!writer) return 2;
    juce::AudioBuffer<float> audio{2, 127};
    double energy = 0;
    float peak = 0;
    for (int at = 0; at < 48000 * 8;) {
      const int count = std::min(127, 48000 * 8 - at);
      audio.setSize(2, count, false, false, true);
      for (int i = 0; i < count; ++i) {
        const int n = at + i;
        const int local = n % 48000;
        float input = 0;
        if (n < 48000 * 3 && local < 24000) {
          const float envelope =
              (1.F - std::exp(-local / 120.F)) * std::exp(-local / 6000.F);
          for (float hz : {130.8128F, 164.8138F, 195.9977F})
            input += .08F * envelope *
                     (std::sin(n * hz * .000130899694F) +
                      .3F * std::sin(n * hz * .000261799388F));
        }
        audio.setSample(0, i, input);
        audio.setSample(1, i, input);
      }
      juce::MidiBuffer midi;
      if (scene != 0) processor.processBlock(audio, midi);
      for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < count; ++i) {
          const float x = audio.getSample(ch, i);
          if (!std::isfinite(x)) return 3;
          peak = std::max(peak, std::abs(x));
          energy += x * x;
        }
      if (!writer->writeFromAudioSampleBuffer(audio, 0, count)) return 2;
      at += count;
    }
    std::cout << name << " peak=" << peak
              << " rms=" << std::sqrt(energy / (48000 * 16)) << '\n';
  }
  return 0;
}
}  // namespace

void testNestedMemoryState() {
  Field p;
  require(
      dynamic_cast<juce::AudioProcessorParameterWithID*>(p.getParameters()[11])
              ->paramID == "focus",
      "Nested controls append after existing Field parameters");
  juce::MemoryBlock saved;
  p.getStateInformation(saved);
  auto tree = juce::ValueTree::fromXml(*juce::AudioProcessor::getXmlFromBinary(
      saved.getData(), static_cast<int>(saved.getSize())));
  for (const auto* id : {"loop_hold", "loop_ms", "inner_repeat", "inner_ms",
                         "loop_speed", "loop_reverse", "loop_mix"})
    tree.removeChild(tree.getChildWithProperty("id", id), nullptr);
  auto xml = tree.createXml();
  juce::MemoryBlock old;
  juce::AudioProcessor::copyXmlToBinary(*xml, old);
  p.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
  require(value(p, "loop_hold") == 0 && value(p, "inner_repeat") == 0 &&
              value(p, "loop_mix") == 0,
          "Old sessions keep their existing field");
  p.loadFactoryPreset(7);
  setValue(p, "loop_hold", 1);
  setValue(p, "inner_repeat", 1);
  p.prepareToPlay(48000, 127);
  juce::AudioBuffer<float> audio{2, 127};
  juce::MidiBuffer midi;
  for (int block = 0; block < 620; ++block) {
    for (int i = 0; i < 127; ++i)
      audio.setSample(0, i, .1F * std::sin((block * 127 + i) * .11F));
    audio.copyFrom(1, 0, audio, 0, 0, 127);
    const auto before = allocations.load();
    setAudit(true);
#if JUCE_MAC
    asteRealtimeAuditReset();
    asteRealtimeAuditSetActive(1);
#endif
    p.processBlock(audio, midi);
#if JUCE_MAC
    asteRealtimeAuditSetActive(0);
#endif
    setAudit(false);
    require(allocations.load() == before,
            "Active nested capture never allocates");
#if JUCE_MAC
    require(asteRealtimeAuditLockCalls() == 0 &&
                asteRealtimeAuditFileOpenCalls() == 0 &&
                asteRealtimeAuditWriteCalls() == 0,
            "Active nested capture has no OS operations");
#endif
  }
  require((p.loopStatus() & 3U) == 3U,
          "Nested capture becomes active from real input history");
  p.getStateInformation(saved);
  p.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
  audio.clear();
  p.processBlock(audio, midi);
  require((p.loopStatus() & 1U) == 0,
          "Restoring controls does not pretend to restore captured audio");
}
void testNewColorRecall() {
  Field p;
  juce::MemoryBlock current;
  p.getStateInformation(current);
  auto tree = juce::ValueTree::fromXml(*juce::AudioProcessor::getXmlFromBinary(
      current.getData(), static_cast<int>(current.getSize())));
  tree.removeChild(tree.getChildWithProperty("id", "focus"), nullptr);
  juce::MemoryBlock old;
  auto xml = tree.createXml();
  juce::AudioProcessor::copyXmlToBinary(*xml, old);
  p.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
  require(p.state().getRawParameterValue("focus")->load() == 0.F,
          "Old sessions disable new coloration");
  auto* parameter = p.state().getParameter("focus");
  parameter->setValueNotifyingHost(parameter->convertTo0to1(73.25F));
  juce::MemoryBlock saved, repeated;
  p.getStateInformation(saved);
  p.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
  p.getStateInformation(repeated);
  require(saved == repeated &&
              std::abs(p.state().getRawParameterValue("focus")->load() -
                       73.25F) < .02F,
          "New coloration recalls byte-stably");
}
void testEngineRevision() {
  Field fresh;
  require(fresh.usesDiffuseSpace(), "New Field uses diffused foundation");
  juce::MemoryBlock bytes;
  fresh.getStateInformation(bytes);
  auto xml = juce::AudioProcessor::getXmlFromBinary(bytes.getData(),
                                                    int(bytes.getSize()));
  xml->removeAttribute("engine");
  juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
  Field restored;
  restored.setStateInformation(bytes.getData(), int(bytes.getSize()));
  require(!restored.usesDiffuseSpace(),
          "Old sessions without engine retain original sound");
  std::vector<float> before;
  for (auto* p : restored.getParameters()) before.push_back(p->getValue());
  restored.useCurrentResponse();
  require(restored.usesDiffuseSpace(), "Explicit upgrade selects dense engine");
  for (int i = 0; i < restored.getParameters().size(); ++i)
    require(restored.getParameters()[i]->getValue() == before[std::size_t(i)],
            "Upgrade preserves all parameter values");
  restored.getStateInformation(bytes);
  juce::MemoryBlock stable = bytes;
  auto bad = juce::AudioProcessor::getXmlFromBinary(bytes.getData(),
                                                    int(bytes.getSize()));
  bad->setAttribute("engine", 3);
  for (auto* child : bad->getChildIterator())
    if (child->getStringAttribute("id") == "mass")
      child->setAttribute("value", 0);
  juce::AudioProcessor::copyXmlToBinary(*bad, bytes);
  restored.setStateInformation(bytes.getData(), int(bytes.getSize()));
  restored.getStateInformation(bytes);
  require(bytes == stable, "Unknown engine rejects entire state transaction");
  for (int i = 9; i < 15; ++i) {
    fresh.loadFactoryPreset(i);
    require(fresh.usesDiffuseSpace(), "Six shipping presets use dense engine");
  }
}
int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI juce;
  aste::tests::workflowChecks<Field>(require);
  if (argc == 3 && std::string{argv[1]} == "--listening-artifacts")
    return renderListening(argv[2]);
  aste::tests::masterOutputChecks<aste::field::plugin::FieldAudioProcessor>(
      require);
  aste::tests::responseUpgradeChecks<aste::field::plugin::FieldAudioProcessor>(
      require, 1);
  aste::tests::presetRecallChecks<aste::field::plugin::FieldAudioProcessor>(
      require);
  aste::tests::behaviorStateChecks<Field>(require, 1, 2);
  testEngineRevision();
  testNestedMemoryState();
  testNewColorRecall();
  testContractAndState();
  testProcessingMidiAndRealtime();
  testEmptyHoldAndClear();
  testEditor(argc == 3 && std::string{argv[1]} == "--editor-artifacts"
                 ? argv[2]
                 : nullptr);
  if (failures == 0) std::cout << "field_plugin_tests: ok\n";
  return failures == 0 ? 0 : 1;
}
