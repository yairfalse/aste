#include "workflow_checks.hpp"
#include "loop_plugin.hpp"
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
#include <limits>
#include <new>
#include <thread>
#include <chrono>

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
  require(source.getParameters().size() == 27,
          "Loop appends five workflow parameters after its original 22 host "
          "indices");
  require(dynamic_cast<juce::AudioProcessorParameterWithID*>(
              source.getParameters()[20])
                  ->paramID == "master",
          "Master retains its existing host index");
  source.setTapeTransport(aste::loop::Transport::Stop);
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
  require(restored.tapeTransport() == aste::loop::Transport::Stop,
          "Tape transport recalls with the controls");
  auto currentXml = juce::AudioProcessor::getXmlFromBinary(
      state.getData(), static_cast<int>(state.getSize()));
  require(currentXml && currentXml->getStringAttribute("behavior") == "1",
          "New Loop state selects continuous motion");
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
  require(restored.tapeTransport() == aste::loop::Transport::Play,
          "Old sessions without transport retain automatic playback");
  restored.getStateInformation(repeated);
  auto legacyXml = juce::AudioProcessor::getXmlFromBinary(
      repeated.getData(), static_cast<int>(repeated.getSize()));
  require(legacyXml && legacyXml->getStringAttribute("behavior") == "0",
          "Loop migration preserves the legacy clock on save");
  Loop reopened;
  reopened.setStateInformation(repeated.getData(),
                               static_cast<int>(repeated.getSize()));
  reopened.prepareToPlay(48000, 127);
  reopened.getStateInformation(repeated);
  legacyXml = juce::AudioProcessor::getXmlFromBinary(
      repeated.getData(), static_cast<int>(repeated.getSize()));
  require(legacyXml && legacyXml->getStringAttribute("behavior") == "0",
          "Legacy Loop clock survives reopen and engine preparation");
  require(restored.factoryPresetCount() == 7,
          "Loop appends First take without replacing five snapshots");
  Loop firstTake;
  require(value(firstTake, "overdub") == 0,
          "A fresh Loop records full input on its first take");
  firstTake.loadFactoryPreset(0);
  require(value(firstTake, "overdub") == 50,
          "Clean Memory retains its original overdub setting");
}

void testProcessingMidiAndRealtime() {
  Loop processor;
  setValue(processor, "safe_takes", 0);
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

void testSafeTakesAndUndo() {
  Loop processor;
  processor.prepareToPlay(8000, 127);
  setValue(processor, "sync", 0);
  setValue(processor, "free_length", .05F);
  setValue(processor, "capture", 1);
  juce::AudioBuffer<float> audio{2, 127};
  juce::MidiBuffer midi;
  const auto tick = [&] {
    audio.clear();
    processor.processBlock(audio, midi);
  };
  for (int block = 0; block < 5; ++block) {
    for (int channel = 0; channel < 2; ++channel)
      for (int i = 0; i < 127; ++i)
        audio.setSample(channel, i, .3F * std::sin((block * 127 + i) * .08F));
    processor.processBlock(audio, midi);
  }
  require(!processor.isCapturing() && processor.generation() == 1,
          "A fresh plugin finishes one capture without an editor or note-off");
  const auto original = processor.tapeSnapshot();
  tick();
  require(processor.tapeSnapshot().audio == original.audio,
          "One-shot capture cannot overwrite itself while its host parameter "
          "remains high");
  setValue(processor, "capture", 0);
  setValue(processor, "speed", .5F);
  setValue(processor, "reverse", 1);
  setValue(processor, "degradation", 70);
  processor.reloop();
  for (int block = 0; block < 8; ++block) tick();
  auto second = processor.tapeSnapshot();
  require(second.generations[1] == 2 && second.audio[1].size() == 1600 &&
              second.audio[0] == original.audio[0] &&
              processor.generation() == 1,
          "RELOOP commits a slowed generation without replacing or "
          "double-processing the playing source");
  processor.selectTake(2);
  tick();
  require(
      processor.generation() == 2 && value(processor, "speed") == 1 &&
          value(processor, "reverse") == 0 &&
          value(processor, "degradation") == 0,
      "History audition selects actual PCM and clears temporary transforms");
  juce::MemoryBlock flush;
  processor.getStateInformation(flush);
  processor.undoManager().undo();
  tick();
  require(
      processor.generation() == 1 && value(processor, "reverse") == 1,
      "Undo history audition restores the source and its transform controls");
  processor.undoManager().redo();
  tick();
  require(processor.generation() == 2,
          "Redo returns to the selected generation");
  // Let the deliberate 5-ms varispeed transition reach unity before using
  // a one-sample-per-frame oracle for the following block.
  for (int i = 0; i < 8; ++i) tick();
  const float positionBeforeRemoval = processor.loopPosition();
  processor.removeTake(1);
  tick();
  require(processor.retainedGenerations() == 1,
          "Explicit removal frees an inactive history slot");
  require(
      std::abs(processor.loopPosition() -
               std::fmod(positionBeforeRemoval + 127.F / 800.F, 1.F)) < .00001F,
      "Removing an inactive generation preserves the playing tape head");
  processor.undoManager().undo();
  tick();
  require(processor.retainedGenerations() == 2 &&
              processor.tapeSnapshot().audio == second.audio,
          "Undo removal recovers exact stereo PCM without clearing its own "
          "undo action");
  processor.clearWithUndo();
  tick();
  require(processor.retainedGenerations() == 0,
          "Clear memory is a real audio operation");
  processor.undoManager().undo();
  tick();
  require(processor.retainedGenerations() == 2,
          "Undo clear restores all retained generations");
  processor.getStateInformation(flush);
  Loop restored;
  restored.setStateInformation(flush.getData(),
                               static_cast<int>(flush.getSize()));
  restored.prepareToPlay(8000, 127);
  require(
      restored.tapeSnapshot().audio == processor.tapeSnapshot().audio &&
          value(restored, "safe_takes") == 1,
      "Safe workflow and all generation audio survive complete plugin reload");
  auto oldState = juce::AudioProcessor::getXmlFromBinary(
      flush.getData(), static_cast<int>(flush.getSize()));
  for (auto* child : oldState->getChildIterator())
    if (child->getStringAttribute("id") == "safe_takes") {
      oldState->removeChildElement(child, true);
      break;
    }
  juce::AudioProcessor::copyXmlToBinary(*oldState, flush);
  restored.setStateInformation(flush.getData(),
                               static_cast<int>(flush.getSize()));
  require(value(restored, "safe_takes") == 0,
          "Sessions predating Safe Takes retain the original overdub and "
          "recycling behavior");
}

juce::MouseEvent mouseEvent(juce::Component& component,
                            juce::Point<float> position,
                            juce::Point<float> origin, bool dragged = false) {
  return {juce::Desktop::getInstance().getMainMouseSource(),
          position,
          juce::ModifierKeys{juce::ModifierKeys::leftButtonModifier},
          1,
          0,
          0,
          0,
          0,
          &component,
          &component,
          juce::Time::getCurrentTime(),
          origin,
          juce::Time::getCurrentTime(),
          1,
          dragged};
}

void testHostTimingAndAutomation() {
  class Host final : public juce::AudioPlayHead {
   public:
    double bpm{120}, ppq{};
    bool playing{true};
    juce::Optional<PositionInfo> getPosition() const override {
      PositionInfo position;
      position.setBpm(bpm);
      position.setPpqPosition(ppq);
      position.setIsPlaying(playing);
      return position;
    }
  } host;
  Loop processor;
  processor.setPlayHead(&host);
  processor.prepareToPlay(8000, 511);
  setValue(processor, "length_beats", 1);
  setValue(processor, "capture", 1);
  juce::MidiBuffer midi;
  juce::AudioBuffer<float> block{2, 511};
  for (int i = 0; i < 20; ++i) {
    block.setSize(2,
                  std::array{1, 7, 127, 511}[static_cast<std::size_t>(i % 4)],
                  false, false, true);
    for (int c = 0; c < 2; ++c)
      for (int s = 0; s < block.getNumSamples(); ++s)
        block.setSample(c, s, .25F);
    processor.processBlock(block, midi);
    host.bpm = 180;
    host.ppq = i % 2 ? 32 : -4;
    host.playing = i % 3 != 0;
  }
  block.setSize(2, 511, false, false, true);
  for (int i = 0; i < 4; ++i) {
    block.clear();
    processor.processBlock(block, midi);
  }
  auto tape = processor.tapeSnapshot();
  require(tape.audio[0].size() == 8000 && tape.generations[0] == 1 &&
              !processor.isCapturing(),
          "Tempo changes and transport jumps cannot resize or restart a "
          "capture already in progress");
  setValue(processor, "capture", 0);
  for (int i = 0; i < 40; ++i) {
    setValue(processor, "start", static_cast<float>((i * 13) % 100));
    setValue(processor, "region_end", static_cast<float>((i * 17) % 100));
    setValue(processor, "reverse", static_cast<float>(i % 2));
    setValue(processor, "repeat", 1);
    setValue(processor, "repeat_division", static_cast<float>(i % 5));
    setValue(processor, "speed", i % 2 ? .125F : 4.F);
    setValue(processor, "pitch", i % 2 ? -12.F : 12.F);
    host.bpm = i % 3 ? 20 : 400;
    block.clear();
#if JUCE_MAC
    asteRealtimeAuditReset();
    asteRealtimeAuditSetActive(1);
#endif
    const auto before = allocations.load();
    setAudit(true);
    processor.processBlock(block, midi);
    setAudit(false);
#if JUCE_MAC
    asteRealtimeAuditSetActive(0);
    require(asteRealtimeAuditLockCalls() == 0 &&
                asteRealtimeAuditFileOpenCalls() == 0 &&
                asteRealtimeAuditWriteCalls() == 0,
            "Safe performance automation performs no locks or I/O");
#endif
    require(allocations.load() == before,
            "Safe performance automation allocates nothing");
    for (int c = 0; c < 2; ++c)
      for (int s = 0; s < block.getNumSamples(); ++s)
        require(std::isfinite(block.getSample(c, s)),
                "Crossing regions and extreme host automation stay finite");
  }
  require(processor.tapeSnapshot().audio == tape.audio,
          "Host and automation stress leaves recorded PCM intact");
  processor.setPlayHead(nullptr);
}

void testPerformanceGestures() {
  Loop processor;
  processor.prepareToPlay(8000, 512);
  setValue(processor, "sync", 0);
  setValue(processor, "free_length", .05F);
  setValue(processor, "capture", 1);
  juce::AudioBuffer<float> block{2, 512};
  block.clear();
  juce::MidiBuffer midi;
  processor.processBlock(block, midi);
  setValue(processor, "capture", 0);
  auto editor =
      std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
  auto* wave = findTitled(*editor, "LOOP REGION");
  require(wave != nullptr,
          "Actual tape waveform supports direct region editing");
  if (!wave) return;
  const juce::Point<float> first{10 + .2F * (wave->getWidth() - 20), 50};
  const juce::Point<float> last{10 + .6F * (wave->getWidth() - 20), 50};
  wave->mouseDown(mouseEvent(*wave, first, first));
  wave->mouseDrag(mouseEvent(*wave, last, first, true));
  wave->mouseUp(mouseEvent(*wave, last, first, true));
  require(std::abs(value(processor, "start") - 20) < .01F &&
              std::abs(value(processor, "region_end") - 60) < .01F,
          "Waveform dragging changes both real playback boundaries");
  require(wave->keyPressed(juce::KeyPress{juce::KeyPress::rightKey}),
          "Region has keyboard navigation");
  require(std::abs(value(processor, "start") - 21) < .01F,
          "Keyboard region nudge changes DSP parameters");
  auto* hold = findTitled(*editor, "HOLD TO REPEAT");
  require(hold != nullptr, "Momentary repeat is permanently accessible");
  if (!hold) return;
  hold->mouseDown(mouseEvent(*hold, {10, 10}, {10, 10}));
  require(value(processor, "repeat") == 1,
          "Press begins real fragment repetition");
  hold->mouseDrag(mouseEvent(*hold, {-90, -90}, {10, 10}, true));
  require(std::abs(value(processor, "region_end") - 41) < .02F &&
              value(processor, "speed") == 2,
          "One held drag simultaneously shortens the real fragment and doubles "
          "varispeed");
  setValue(processor, "speed", .25F);
  setValue(processor, "reverse", 1);
  setValue(processor, "region_end", 24);
  hold->mouseUp(mouseEvent(*hold, {10, 10}, {10, 10}));
  require(value(processor, "repeat") == 0 && value(processor, "speed") == 1 &&
              value(processor, "reverse") == 0 &&
              std::abs(value(processor, "region_end") - 61) < .01F,
          "Release restores the phrase's region, speed and direction");
}

void renderWorkflowExamples(const juce::File& folder) {
  require(folder.createDirectory().wasOk(), "Listening folder exists");
  Loop processor;
  processor.prepareToPlay(48000, 127);
  processor.neutralTransforms();
  setValue(processor, "sync", 0);
  setValue(processor, "free_length", .5F);
  setValue(processor, "output", -3);
  setValue(processor, "capture", 1);
  juce::MidiBuffer midi;
  juce::AudioBuffer<float> block{2, 127};
  int time{};
  for (; time < 24003; time += 127) {
    for (int channel = 0; channel < 2; ++channel)
      for (int i = 0; i < 127; ++i) {
        const double t = (time + i) / 48000.0;
        const double envelope = std::exp(-8 * std::fmod(t, .25));
        const double sound =
            std::sin(t * 2 * juce::MathConstants<double>::pi * 110) +
            .5 * std::sin(t * 2 * juce::MathConstants<double>::pi * 130.8128) +
            .3 * std::sin(t * 2 * juce::MathConstants<double>::pi *
                          (channel ? 165.2 : 164.8138));
        block.setSample(channel, i, static_cast<float>(sound * envelope * .18));
      }
    processor.processBlock(block, midi);
  }
  setValue(processor, "capture", 0);
  const auto render = [&](const char* name) {
    juce::AudioBuffer<float> audio{2, 96000};
    float peak{};
    double square{};
    const auto begin = std::chrono::steady_clock::now();
    for (int offset = 0; offset < audio.getNumSamples();) {
      const int count = std::min(127, audio.getNumSamples() - offset);
      block.setSize(2, count, false, false, true);
      block.clear();
      processor.processBlock(block, midi);
      for (int channel = 0; channel < 2; ++channel) {
        audio.copyFrom(channel, offset, block, channel, 0, count);
        for (int i = 0; i < count; ++i) {
          const float sample = block.getSample(channel, i);
          require(std::isfinite(sample), "Workflow render remains finite");
          peak = std::max(peak, std::abs(sample));
          square += sample * sample;
        }
      }
      offset += count;
    }
    const auto elapsed = std::chrono::duration<double, std::milli>(
                             std::chrono::steady_clock::now() - begin)
                             .count();
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> stream =
        folder.getChildFile(juce::String(name) + ".wav").createOutputStream();
    require(stream && stream->setPosition(0), "Listening WAV opens");
    auto writer =
        format.createWriterFor(stream, juce::AudioFormatWriterOptions{}
                                           .withSampleRate(48000)
                                           .withNumChannels(2)
                                           .withBitsPerSample(24));
    require(writer && writer->writeFromAudioSampleBuffer(audio, 0,
                                                         audio.getNumSamples()),
            "Production adapter renders real stereo WAV");
    std::cout << name << ", peak=" << peak
              << ", rms=" << std::sqrt(square / 192000)
              << ", render_ms=" << elapsed << '\n';
  };
  render("01-captured-chord");
  setValue(processor, "speed", .5F);
  setValue(processor, "reverse", 1);
  setValue(processor, "degradation", 75);
  setValue(processor, "amplifier", 65);
  render("02-slow-reverse-erosion");
  processor.reloop();
  render("03-printing-second-generation");
  processor.selectTake(2);
  render("04-second-generation-neutral-playback");
  setValue(processor, "start", 20);
  setValue(processor, "region_end", 70);
  setValue(processor, "repeat_division", 4);
  setValue(processor, "repeat", 1);
  setValue(processor, "pitch", -7);
  render("05-fragment-repeat-down-seven");
  processor.reloop();
  render("06-printing-third-generation");
  processor.selectTake(3);
  render("07-third-generation-neutral-playback");
  auto editor =
      std::unique_ptr<juce::AudioProcessorEditor>(processor.createEditor());
  require(writeMinimumEditorArtifact(
              *editor, folder.getChildFile("loop-three-generations.png")),
          "Actual populated plugin interface renders at minimum size");
  juce::MemoryBlock state;
  processor.getStateInformation(state);
  require(folder.getChildFile("three-generations.state")
              .replaceWithData(state.getData(), state.getSize()),
          "Example state stores rendered generations");
}

void testEditor(const char* artifactDirectory) {
  Loop processor;
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  require(editor && editor->getWidth() == 1160 && editor->getHeight() == 820 &&
              editor->isResizable(),
          "Loop editor uses its intended scalable panel");
  if (!editor) return;
  for (const char* title : std::array<const char*, 14>{
           "CAPTURE", "RELOOP", "PREVIOUS GENERATION", "NEXT GENERATION",
           "HOST SYNC", "REVERSE", "TAPE SPEED", "OVERDUB", "PITCH", "OUTPUT",
           "CLEAR MEMORY", "PLAY", "STOP", "RELEASE"})
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
    require(writeMinimumEditorArtifact(*editor,
                                       file.getSiblingFile("loop-minimum.png")),
            "Loop controls fit and render at the minimum size");
  }
}

void testRecallBeforePreparePreservesSamples() {
  Loop source;
  juce::MemoryBlock saved;
  source.getStateInformation(saved);
  auto xml = juce::AudioProcessor::getXmlFromBinary(
      saved.getData(), static_cast<int>(saved.getSize()));
  require(xml != nullptr, "Empty session has serializable tape metadata");
  if (!xml) return;
  auto* tapes = xml->getChildByName("TAPES");
  tapes->setAttribute("sample_rate", 96000);
  tapes->setAttribute("active", 0);
  auto* tape = tapes->getFirstChildElement();
  tape->setAttribute("generation", 1);
  tape->setAttribute("frames", 128);
  juce::MemoryOutputStream bytes;
  for (int frame = 0; frame < 128; ++frame) {
    bytes.writeFloat(frame % 2 == 0 ? 0.25F : -0.25F);
    bytes.writeFloat(frame % 2 == 0 ? -0.25F : 0.25F);
  }
  const auto payload =
      juce::Base64::toBase64(bytes.getData(), bytes.getDataSize());
  tape->setAttribute("audio", payload);
  juce::AudioProcessor::copyXmlToBinary(*xml, saved);
  Loop restored;
  restored.setStateInformation(saved.getData(),
                               static_cast<int>(saved.getSize()));
  restored.prepareToPlay(96000, 127);
  restored.getStateInformation(saved);
  xml = juce::AudioProcessor::getXmlFromBinary(
      saved.getData(), static_cast<int>(saved.getSize()));
  require(xml != nullptr && xml->getChildByName("TAPES")
                                    ->getFirstChildElement()
                                    ->getStringAttribute("audio") == payload,
          "Restore before host preparation preserves high-rate tape exactly");
}

void testTapeRecallAndMidiRelease() {
  Loop source;
  setValue(source, "safe_takes", 0);
  source.prepareToPlay(8000, 127);
  setValue(source, "sync", 0);
  setValue(source, "free_length", 0.05F);
  setValue(source, "overdub", 0);
  juce::AudioBuffer<float> audio{2, 127};
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0F), 0);
  for (int block = 0; block < 5; ++block) {
    for (int c = 0; c < 2; ++c)
      for (int s = 0; s < 127; ++s) audio.setSample(c, s, 0.25F);
    source.processBlock(audio, midi);
    midi.clear();
  }
  require(source.isCapturing(), "MIDI note starts capture");
  midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
  source.processBlock(audio, midi);
  require(!source.isCapturing(), "MIDI panic releases capture");
  midi.clear();
  juce::MemoryBlock saved;
  source.getStateInformation(saved);
  auto legacy = juce::AudioProcessor::getXmlFromBinary(
      saved.getData(), static_cast<int>(saved.getSize()));
  legacy->setAttribute("behavior", 0);
  juce::AudioProcessor::copyXmlToBinary(*legacy, saved);
  Loop restored;
  restored.setStateInformation(saved.getData(),
                               static_cast<int>(saved.getSize()));
  require(restored.usesLegacyBehavior(),
          "Recorded tape restores legacy response");
  restored.useCurrentResponse();
  juce::MemoryBlock upgradedTape;
  restored.getStateInformation(upgradedTape);
  auto upgradedXml = juce::AudioProcessor::getXmlFromBinary(
      upgradedTape.getData(), static_cast<int>(upgradedTape.getSize()));
  legacy->setAttribute("behavior", 1);
  require(upgradedXml && upgradedXml->toString() == legacy->toString(),
          "Upgrading pending recorded tape preserves every sample and control");
  restored.prepareToPlay(16000, 127);
  audio.clear();
  const auto before = allocations.load();
  setAudit(true);
  restored.processBlock(audio, midi);
  setAudit(false);
  require(
      allocations.load() == before && restored.generation() == 1 &&
          audio.getMagnitude(0, 0, 127) > 0.01F,
      "Reopened tape plays at another sample rate without callback allocation");
  juce::MemoryBlock stable, after;
  restored.getStateInformation(stable);
  auto xml = juce::AudioProcessor::getXmlFromBinary(
      stable.getData(), static_cast<int>(stable.getSize()));
  require(xml != nullptr, "Saved tape has versioned state");
  if (xml) {
    auto* tape = xml->getChildByName("TAPES")->getFirstChildElement();
    tape->setAttribute("frames", 999999999);
    juce::MemoryBlock invalid;
    juce::AudioProcessor::copyXmlToBinary(*xml, invalid);
    restored.setStateInformation(invalid.getData(),
                                 static_cast<int>(invalid.getSize()));
    restored.getStateInformation(after);
    require(stable == after,
            "Oversized audio payload leaves controls and tape unchanged");
    for (int mutation = 0; mutation < 6; ++mutation) {
      auto bad = juce::AudioProcessor::getXmlFromBinary(
          stable.getData(), static_cast<int>(stable.getSize()));
      auto* tapes = bad->getChildByName("TAPES");
      auto* first = tapes->getFirstChildElement();
      switch (mutation) {
        case 0:
          tapes->setAttribute("sample_rate", "nan");
          break;
        case 1:
          tapes->setAttribute("active", 3);
          break;
        case 2:
          first->setAttribute("generation", -1);
          break;
        case 3:
          first->setAttribute("audio", "not base64");
          break;
        case 4:
          bad->setAttribute("schema", "3.5");
          break;
        case 5: {
          juce::MemoryOutputStream bytes;
          const int frames = first->getIntAttribute("frames");
          for (int sample = 0; sample < frames * 2; ++sample)
            bytes.writeFloat(std::numeric_limits<float>::quiet_NaN());
          first->setAttribute(
              "audio",
              juce::Base64::toBase64(bytes.getData(), bytes.getDataSize()));
          break;
        }
      }
      juce::AudioProcessor::copyXmlToBinary(*bad, invalid);
      restored.setStateInformation(invalid.getData(),
                                   static_cast<int>(invalid.getSize()));
      restored.getStateInformation(after);
      require(stable == after,
              "Malformed schema, tape metadata and non-finite audio are "
              "rejected atomically");
    }
  }
  midi.addEvent(juce::MidiMessage::noteOn(1, 60, 1.0F), 0);
  source.processBlock(audio, midi);
  midi.clear();
  midi.addEvent(juce::MidiMessage::noteOff(1, 60), 0);
  source.processBlockBypassed(audio, midi);
  require(source.generation() == 1 && source.retainedGenerations() == 1 &&
              source.isBypassed(),
          "Bypass preserves tape status instead of reporting empty memory");
  midi.clear();
  source.processBlock(audio, midi);
  require(
      !source.isCapturing(),
      "A MIDI release received during bypass does not leave capture latched");
  std::atomic<bool> finished{}, nonfinite{};
  std::thread callback([&] {
    juce::AudioBuffer<float> block{2, 127};
    juce::MidiBuffer events;
    while (!finished.load()) {
      block.clear();
      source.processBlock(block, events);
      for (int sample = 0; sample < 127; ++sample)
        if (!std::isfinite(block.getSample(0, sample))) nonfinite.store(true);
    }
  });
  for (int iteration = 0; iteration < 8; ++iteration) {
    juce::MemoryBlock concurrent;
    source.getStateInformation(concurrent);
    source.setStateInformation(concurrent.getData(),
                               static_cast<int>(concurrent.getSize()));
    source.useCurrentResponse();
    require(!source.usesLegacyBehavior(),
            "Concurrent engine adoption keeps the explicit response choice");
  }
  finished.store(true);
  callback.join();
  require(!nonfinite.load(),
          "Concurrent save, restore and engine retirement preserve finite "
          "playback");
}
}  // namespace

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI juce;
  testSafeTakesAndUndo();
  testHostTimingAndAutomation();
  testPerformanceGestures();
  aste::tests::workflowChecks<Loop>(require);
  aste::tests::masterOutputChecks<aste::loop::plugin::LoopAudioProcessor>(
      require);
  aste::tests::responseUpgradeChecks<aste::loop::plugin::LoopAudioProcessor>(
      require, 3);
  aste::tests::presetRecallChecks<aste::loop::plugin::LoopAudioProcessor>(
      require);
  aste::tests::behaviorStateChecks<Loop>(require, 3, 4);
  testContractAndState();
  testProcessingMidiAndRealtime();
  testRecallBeforePreparePreservesSamples();
  testTapeRecallAndMidiRelease();
  testEditor(argc == 3 && std::string{argv[1]} == "--editor-artifacts"
                 ? argv[2]
                 : nullptr);
  if (argc == 3 && std::string{argv[1]} == "--editor-artifacts")
    renderWorkflowExamples(
        juce::File{juce::String{argv[2]}}.getChildFile("listening"));
  if (failures == 0) std::cout << "loop_plugin_tests: ok\n";
  return failures == 0 ? 0 : 1;
}
