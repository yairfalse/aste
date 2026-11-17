#include "workflow_checks.hpp"
#if ASTE_NONMACHINE
#include "non_machine_plugin.hpp"
using Processor = aste::nonmachine::plugin::NonMachineAudioProcessor;
#else
#include "resonator_plugin.hpp"
using Processor = aste::resonator::plugin::ResonatorAudioProcessor;
#endif
#include "editor_artifact.hpp"
#include "master_output_checks.hpp"
#include <atomic>
#include <cstdlib>
#include <iostream>
#include <new>
#if ASTE_MAC_AUDIT
#include <pthread.h>
#include <unistd.h>
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif

namespace {
thread_local bool audit{};
thread_local unsigned allocations{};
int failures{};
}  // namespace
void* operator new(std::size_t n) {
  if (audit) ++allocations;
  if (auto* p = std::malloc(std::max<std::size_t>(n, 1))) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  if (audit) ++allocations;
  std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
namespace {
void require(bool ok, const char* text) {
  if (!ok) {
    ++failures;
    std::cerr << "FAIL: " << text << '\n';
  }
}
void stateAndRender() {
  Processor p;
  require(p.getParameters().size() == (ASTE_NONMACHINE ? 445 : 19),
          "Exact experimental parameter count");
  juce::MemoryBlock state, again;
  p.getStateInformation(state);
  Processor copy;
  copy.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  copy.getStateInformation(again);
  require(state == again, "Controls round-trip byte-stably");
  const char junk[] = "invalid";
  copy.setStateInformation(junk, sizeof(junk));
  copy.getStateInformation(again);
  require(state == again, "Malformed state is rejected atomically");
#if ASTE_NONMACHINE
  p.set("sequence", 0);  // Explicit MIDI object input, rather than simultaneous
                         // pattern + MIDI.
#endif
  for (double rate : {44100., 48000., 96000., 192000.}) {
    p.prepareToPlay(rate, 511);
    juce::AudioBuffer<float> audio{2, 511};
    juce::MidiBuffer midi;
    midi.addEvent(juce::MidiMessage::noteOn(1, 48, .8F), 23);
    const std::array<unsigned char, 16> sysex{};
    midi.addEvent(juce::MidiMessage::createSysExMessage(
                      sysex.data(), static_cast<int>(sysex.size())),
                  40);
    allocations = 0;
#if ASTE_MAC_AUDIT
    asteRealtimeAuditReset();
    asteRealtimeAuditSetActive(1);
#endif
    audit = true;
    p.processBlock(audio, midi);
    audit = false;
#if ASTE_MAC_AUDIT
    asteRealtimeAuditSetActive(0);
    require(asteRealtimeAuditLockCalls() == 0 &&
                asteRealtimeAuditFileOpenCalls() == 0 &&
                asteRealtimeAuditWriteCalls() == 0,
            "Callback performs no audited locks, file opens or writes");
#endif
    require(allocations == 0, "MIDI callback allocates/frees nothing");
    for (int n = 0; n < 23; ++n)
      require(audio.getSample(0, n) == 0,
              "MIDI onset honors its sample offset");
    float peak = audio.getMagnitude(0, 0, 511);
    for (int i = 0; i < 10; ++i) {
      midi.clear();
      p.processBlock(audio, midi);
      peak = std::max(peak, audio.getMagnitude(0, 0, 511));
    }
    require(peak > .0001F,
            "Instrument produces audible MIDI sound without host playback");
    require(p.getLatencySamples() == 0, "Zero reported latency");
  }
}
int renderExamples(const juce::File& directory) {
  struct Clock final : juce::AudioPlayHead {
    double ppq{};
    juce::Optional<PositionInfo> getPosition() const override {
      PositionInfo p;
      p.setBpm(120.);
      p.setPpqPosition(ppq);
      p.setIsPlaying(true);
      return p;
    }
  } clock;
  if (directory.createDirectory().failed()) return 2;
  for (int scene = 0; scene < 4; ++scene) {
    Processor p;
    p.scene(scene);
#if ASTE_NONMACHINE
    p.setPlayHead(&clock);
#endif
    p.prepareToPlay(48000, 127);
    auto file =
        directory.getChildFile("scene-" + juce::String(scene + 1) + ".wav");
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
    double sum = 0;
    float peak = 0;
    for (int at = 0; at < 48000 * 8;) {
      const int count = std::min({127, 48000 * 8 - at, 48000 - at % 48000});
      audio.setSize(2, count, false, false, true);
      juce::MidiBuffer midi;
      clock.ppq = at / 24000.;
#if !ASTE_NONMACHINE
      if (at % 96000 == 0) {
        midi.addEvent(juce::MidiMessage::allNotesOff(1), 0);
        midi.addEvent(
            juce::MidiMessage::noteOn(1, 48 + (at / 96000 % 2) * 5, .85F), 1);
      }
#endif
      p.processBlock(audio, midi);
      for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < count; ++n) {
          const float sample = audio.getSample(ch, n);
          if (!std::isfinite(sample) || std::abs(sample) > 1) return 3;
          sum += sample * sample;
          peak = std::max(peak, std::abs(sample));
        }
      if (!writer->writeFromAudioSampleBuffer(audio, 0, count)) return 2;
      at += count;
    }
    std::cout << p.getName() << " scene " << scene + 1 << " peak=" << peak
              << " rms=" << std::sqrt(sum / (48000 * 16)) << '\n';
  }
  return 0;
}
void editor(const juce::File& folder) {
  Processor p;
  std::unique_ptr<juce::AudioProcessorEditor> editor{p.createEditor()};
  require(editor && editor->isResizable(), "Usable resizable native editor");
#if !ASTE_NONMACHINE
  const auto findRoot = [&](auto&& self, juce::Component& c) -> juce::Slider* {
    if (c.getTitle() == "ROOT MIDI")
      if (auto* slider = dynamic_cast<juce::Slider*>(&c)) return slider;
    for (auto* child : c.getChildren())
      if (auto* found = self(self, *child)) return found;
    return nullptr;
  };
  if (auto* root = findRoot(findRoot, *editor)) {
    require(root->getTextFromValue(48) == aste::ui::noteName(48),
            "Resonator root shows the note name after host attachment");
    require(root->getValueFromText("C4") == 60 &&
                root->getValueFromText("48") == 48,
            "Resonator root accepts note names and MIDI numbers");
  } else
    require(false, "Resonator root is directly editable");
#endif

  if (folder.createDirectory().failed()) {
    require(false, "Artifact directory exists");
    return;
  }
  require(
      writeMinimumEditorArtifact(*editor, folder.getChildFile("minimum.png")),
      "All visible controls fit the minimum editor");
}
#if ASTE_NONMACHINE
void materialRecallAndTimeline() {
  Processor p;
  require(
      dynamic_cast<juce::AudioProcessorParameterWithID*>(p.getParameters()[407])
                  ->paramID == "master" &&
          dynamic_cast<juce::AudioProcessorParameterWithID*>(
              p.getParameters()[408])
                  ->paramID == "bypass",
      "New materials preserve old host parameter order");
  juce::MemoryBlock current;
  p.getStateInformation(current);
  auto tree = juce::ValueTree::fromXml(*juce::AudioProcessor::getXmlFromBinary(
      current.getData(), static_cast<int>(current.getSize())));
  for (std::size_t v = 0; v < 3; ++v)
    for (const auto* field :
         {"material", "structure", "strike", "bend", "roughness"})
      tree.removeChild(
          tree.getChildWithProperty("id", Processor::trackId(v, field)),
          nullptr);
  juce::MemoryBlock old;
  auto xml = tree.createXml();
  juce::AudioProcessor::copyXmlToBinary(*xml, old);
  p.setStateInformation(old.getData(), static_cast<int>(old.getSize()));
  for (std::size_t v = 0; v < 3; ++v)
    require(p.value(Processor::materialIndex(v)) == 0,
            "Old state restores original source formulas");
  const auto beforePosition = p.value(Processor::eventIndex(0, 1) + 1);
  p.set("v1_e1_pitch", 431);
  p.set("v1_e1_decay", 37);
  p.set("v1_e1_color", 65);
  p.applyHitSoundToObject(0, 0);
  for (std::size_t e = 0; e < 8; ++e)
    require(std::abs(p.value(Processor::eventIndex(0, e) + 3) - 431) < .02F,
            "Apply sound reaches every hit in the chosen object");
  require(p.value(Processor::eventIndex(0, 1) + 1) == beforePosition,
          "Apply sound preserves event placement");
  p.emptyPattern();
  for (std::size_t v = 0; v < 3; ++v)
    for (std::size_t e = 0; e < 8; ++e)
      require(p.value(Processor::eventIndex(v, e)) == 0,
              "Empty pattern removes every scheduled hit");
  p.set("v1_e1_position", 10);
  p.set("v1_e1_enabled", 1);
  std::unique_ptr<juce::AudioProcessorEditor> editor{p.createEditor()};
  const auto find = [&](auto&& self, juce::Component& c) -> juce::Component* {
    if (c.getTitle() == "Voice 1 event timeline") return &c;
    for (auto* child : c.getChildren())
      if (auto* match = self(self, *child)) return match;
    return nullptr;
  };
  if (auto* lane = find(find, *editor)) {
    require(lane->keyPressed(juce::KeyPress{juce::KeyPress::rightKey}),
            "Timeline accepts exact keyboard movement");
    require(std::abs(p.value(Processor::eventIndex(0, 0) + 1) - 16.25F) < .02F,
            "Timeline moves the actual scheduled event");
    lane->keyPressed(juce::KeyPress{juce::KeyPress::deleteKey});
    require(p.value(Processor::eventIndex(0, 0)) == 0,
            "Timeline delete removes actual event");
  } else
    require(false, "Direct event timeline is visible");
}
void rhythmEdits() {
  struct Clock : juce::AudioPlayHead {
    double ppq{};
    juce::Optional<PositionInfo> getPosition() const override {
      PositionInfo p;
      p.setBpm(120.);
      p.setPpqPosition(ppq);
      p.setIsPlaying(true);
      return p;
    }
  } clock;
  Processor p;
  p.setPlayHead(&clock);
  p.set("void_length", 0);
  for (int v = 1; v <= 3; ++v)
    for (int e = 1; e <= 8; ++e)
      p.set("v" + juce::String(v) + "_e" + juce::String(e) + "_enabled", 0);
  p.set("v1_cycle", 1);
  p.set("v1_e1_enabled", 1);
  p.set("v1_e1_position", 50);
  p.set("v1_e8_repeats", 8);
  p.set("v1_e8_interval", 1);
  p.set("v1_e8_acceleration", 2);
  p.prepareToPlay(48000, 127);
  juce::AudioBuffer<float> audio{2, 127};
  juce::MidiBuffer midi;
  float before = 0, after = 0;
  for (int at = 0; at < 24000; at += 127) {
    clock.ppq = at / 24000.;
    p.processBlock(audio, midi);
    for (int i = 0; i < 127; ++i) {
      auto& peak = at + i < 12000 ? before : after;
      peak = std::max(peak, std::abs(audio.getSample(0, i)));
    }
  }
  require(!p.invalidPattern(),
          "Disabled excessive repeats cannot reject other edits");
  require(before == 0 && after > .001F,
          "Event position moves audible onset to half cycle");
  p.set("v1_e1_repeats", 8);
  p.set("v1_e1_interval", 1);
  p.set("v1_e1_acceleration", 2);
  clock.ppq = 1;
  p.processBlock(audio, midi);
  require(!p.invalidPattern() && p.limitedRepeats(),
          "Enabled excess repeats are locally limited");
  Processor a, b;
  a.generateRhythm(false);
  b.generateRhythm(false);
  juce::MemoryBlock first, second, next;
  a.getStateInformation(first);
  b.getStateInformation(second);
  require(first == second, "Same seed generates identical recallable rhythm");
  a.generateRhythm();
  a.getStateInformation(next);
  bool moved = false;
  for (std::size_t v = 0; v < 3; ++v)
    for (std::size_t e = 0; e < 8; ++e)
      moved |= a.value(Processor::eventIndex(v, e) + 1) !=
               b.value(Processor::eventIndex(v, e) + 1);
  require(moved && first != next, "New rhythm changes actual event positions");
  a.scene(0);
  const auto position0 = a.value(Processor::eventIndex(1, 0) + 1);
  a.scene(1);
  const auto position1 = a.value(Processor::eventIndex(1, 0) + 1);
  a.scene(2);
  const auto position2 = a.value(Processor::eventIndex(1, 0) + 1);
  require(position0 != position1 && position1 != position2,
          "Scenes have distinct timing layouts");
}
void hostClock() {
  struct Clock : juce::AudioPlayHead {
    double ppq{};
    bool playing{true};
    juce::Optional<PositionInfo> getPosition() const override {
      PositionInfo p;
      p.setBpm(87.);
      p.setPpqPosition(ppq);
      p.setIsPlaying(playing);
      return p;
    }
  } clock;
  Processor p;
  p.setPlayHead(&clock);
  p.prepareToPlay(48000, 127);
  juce::AudioBuffer<float> a{2, 127};
  juce::MidiBuffer m;
  float peak = 0;
  for (int at = 0; at < 24000; at += 127) {
    clock.ppq = at * 87. / (60. * 48000.);
    p.processBlock(a, m);
    peak = std::max(peak, p.outputPeak());
  }
  require(peak > .001F && p.running(),
          "Host BPM/PPQ drive the percussion without MIDI");
  clock.playing = false;
  p.processBlock(a, m);
  require(!p.running(), "Host stop changes to manual mode");
  p.clear();
  p.processBlock(a, m);
  require(p.outputPeak() == 0, "Clear silences sources and fragments");
}
void capturedTime() {
  struct Clock : juce::AudioPlayHead {
    double ppq{};
    juce::Optional<PositionInfo> getPosition() const override {
      PositionInfo p;
      p.setBpm(120.);
      p.setPpqPosition(ppq);
      p.setIsPlaying(true);
      return p;
    }
  } clock;
  Processor p;
  p.setPlayHead(&clock);
  p.prepareToPlay(48000, 127);
  juce::AudioBuffer<float> a{2, 127};
  juce::MidiBuffer m;
  int sample = 0;
  const auto tick = [&] {
    clock.ppq = sample / 24000.;
    p.processBlock(a, m);
    sample += 127;
  };
  for (int i = 0; i < 100; ++i) tick();
  p.set("time_mode", 1);
  p.set("hold", 1);
  tick();
  const float frozen = p.phrasePosition();
  for (int i = 0; i < 20; ++i) tick();
  require(std::abs(p.phrasePosition() - frozen) < .0001F,
          "Master Suspend holds phrase time while host advances");
  p.set("reverse", 1);
  p.set("fragment", 75);
  tick();
  require(std::abs(p.phrasePosition() - frozen) < .0001F,
          "Reverse/shorten preserve suspended time ownership");
  p.set("hold", 0);
  tick();
  require(p.phrasePosition() > frozen, "Release resumes suspended phrase");
  p.rejoin();
  tick();
  require(std::abs(p.phrasePosition() - sample / 24000.) < .0001,
          "Rejoin returns to host phase");
}
#else
void memoryAndClear() {
  Processor p;
  p.prepareToPlay(48000, 127);
  p.excite();
  juce::AudioBuffer<float> a{2, 127};
  juce::MidiBuffer m;
  float peak = 0;
  for (int i = 0; i < 100; ++i) {
    p.processBlock(a, m);
    peak = std::max(peak, p.outputPeak());
  }
  require(peak > .001F, "EXCITE works without MIDI");
  const auto beforeRoot = p.frequency(0, 0);
  p.set("root", 48);
  p.processBlock(a, m);
  require(p.frequency(0, 0) > beforeRoot * 1.8F && p.outputPeak() > 0,
          "Root gesture retunes retained energy without another strike");
  p.retune();
  p.processBlock(a, m);
  require(p.outputPeak() > 0, "Retune preserves sounding energy");
  p.clear();
  p.processBlock(a, m);
  require(p.outputPeak() == 0, "Clear stops energy and the exciter");
}
#endif
#if ASTE_NONMACHINE
void objectSoundOwnership() {
  Processor p;
  p.set("sequence", 0);
  p.set(Processor::eventId(0, 0, "pitch"), 230);
  p.set(Processor::eventId(0, 0, "decay"), 310);
  p.lockHitSound(0, 3, true);
  require(p.hitLocked(0, 3), "Explicit hit sound lock is enabled");
  require(p.state()
                  .getRawParameterValue(Processor::eventId(0, 3, "pitch"))
                  ->load() == 230,
          "Lock begins from current object pitch");
  require(p.state()
                  .getRawParameterValue(Processor::eventId(0, 3, "decay"))
                  ->load() == 310,
          "Lock begins from current object decay");
  p.lockHitSound(0, 3, false);
  require(!p.hitLocked(0, 3), "Unlock returns hit to object sound");
  p.prepareToPlay(48000, 511);
  juce::AudioBuffer<float> a{2, 511};
  juce::MidiBuffer m;
  p.set("sequence", 1);
  m.addEvent(juce::MidiMessage::noteOn(1, 48, .8F), 12);
  p.processBlock(a, m);
  require(p.ignoredMidi() > 0,
          "Pattern input reports ignored MIDI instead of double triggering");
  p.set("sequence", 0);
  p.processBlock(a, m);
  require(a.getMagnitude(0, 0, 511) > .00001F,
          "MIDI object mode produces sound");
  p.audition(0, 3);
  m.clear();
  p.processBlock(a, m);
  require(p.soundOwner() == 5, "Isolated audition reports sound ownership");
}
#endif
}  // namespace
int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI juce;
  aste::tests::workflowChecks<Processor>(require);
#if ASTE_MAC_AUDIT
  pthread_mutex_t mutex;
  pthread_mutex_init(&mutex, nullptr);
  asteRealtimeAuditReset();
  asteRealtimeAuditSetActive(1);
  pthread_mutex_lock(&mutex);
  pthread_mutex_unlock(&mutex);
  auto* file = std::fopen("/dev/null", "r");
  if (file) std::fclose(file);
  (void)::write(-1, "x", 1);
  asteRealtimeAuditSetActive(0);
  pthread_mutex_destroy(&mutex);
  require(asteRealtimeAuditLockCalls() > 0 &&
              asteRealtimeAuditFileOpenCalls() > 0 &&
              asteRealtimeAuditWriteCalls() > 0,
          "Real-time OS audit detects deliberate calibration operations");
#endif
  if (argc == 3 && juce::String{argv[1]} == "--render")
    return renderExamples(juce::File{juce::String::fromUTF8(argv[2])});
  stateAndRender();
  aste::tests::masterOutputChecks<Processor>(require);
#if ASTE_NONMACHINE
  objectSoundOwnership();
  materialRecallAndTimeline();
  rhythmEdits();
  hostClock();
  capturedTime();
#else
  memoryAndClear();
#endif
  if (argc == 2) editor(juce::File{juce::String::fromUTF8(argv[1])});
  std::cout << "experimental instrument: " << failures << " failures\n";
  return failures ? 1 : 0;
}
