#include "click_plugin.hpp"
#include "editor_artifact.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <new>
namespace {
thread_local bool audit{};
thread_local int allocations{};
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
#ifdef ASTE_MAC_AUDIT
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif
using aste::click::plugin::Processor;
void require(bool b, const char* message) {
  if (!b) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
class PlayHead final : public juce::AudioPlayHead {
 public:
  bool playing{true};
  double ppq{}, bpm{120};
  juce::Optional<PositionInfo> getPosition() const override {
    PositionInfo p;
    p.setIsPlaying(playing);
    p.setPpqPosition(ppq);
    p.setBpm(bpm);
    return p;
  }
};
juce::TextButton* button(juce::Component& parent, const juce::String& name) {
  for (auto* child : parent.getChildren()) {
    if (auto* b = dynamic_cast<juce::TextButton*>(child))
      if (b->getButtonText() == name) return b;
    if (auto* b = button(*child, name)) return b;
  }
  return nullptr;
}
void makeWave(const juce::File& file) {
  std::ofstream out(file.getFullPathName().toStdString(), std::ios::binary);
  const auto word = [&](std::uint32_t x, int bytes = 4) {
    for (int i = 0; i < bytes; ++i)
      out.put(static_cast<char>((x >> (8 * i)) & 255));
  };
  out.write("RIFF", 4);
  word(36 + 8192);
  out.write("WAVEfmt ", 8);
  word(16);
  word(1, 2);
  word(1, 2);
  word(48000);
  word(96000);
  word(2, 2);
  word(16, 2);
  out.write("data", 4);
  word(8192);
  for (int n = 0; n < 4096; ++n)
    word(static_cast<std::uint16_t>(
             static_cast<std::int16_t>(std::sin(n * .08) * 24000)),
         2);
}
int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI init;
  const auto folder =
      argc > 1 ? juce::File{argv[1]}
               : juce::File::getCurrentWorkingDirectory().getChildFile(
                     "click_editor_artifacts");
  folder.createDirectory();
  auto p = std::make_unique<Processor>();
  p->prepareToPlay(48000, 256);
  require(p->getParameters().size() == 2592, "stable parameter count");
  require(p->getLatencySamples() == 8, "latency");
  require(p->factoryIndex() == 0, "fresh starting point identity");
  for (int preset = 0; preset < 6; ++preset) {
    p->scene(preset);
    require(p->factoryIndex() == preset, "factory snapshot recognition");
  }
  p->scene(0);
  p->set("mode", 0);
  p->set("o1_tune", -12);
  p->set("o1_e1_count", 7);
  juce::MemoryBlock state;
  p->getStateInformation(state);
  p->set("o1_tune", 9);
  p->setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  require(std::abs(p->value(22 + 2) + 12) < .001F, "scalar recall");
  require(p->hasScene(0) && p->hasScene(3), "factory scene persistence");
  juce::MemoryBlock canonical;
  p->getStateInformation(canonical);
  auto xml = juce::AudioProcessor::getXmlFromBinary(
      canonical.getData(), static_cast<int>(canonical.getSize()));
  auto malformed = juce::ValueTree::fromXml(*xml);
  malformed.getChildWithProperty("id", "o1_tune")
      .setProperty("value", "NaN", nullptr);
  juce::MemoryBlock bad;
  juce::AudioProcessor::copyXmlToBinary(*malformed.createXml(), bad);
  p->setStateInformation(bad.getData(), static_cast<int>(bad.getSize()));
  juce::MemoryBlock unchanged;
  p->getStateInformation(unchanged);
  require(unchanged == canonical, "malformed restore is transactional");

  juce::AudioBuffer<float> b(2, 256);
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::noteOn(1, 36, 1.F), 71);
  p->processBlock(b, midi);
  for (int n = 0; n < 71; ++n)
    require(b.getSample(0, n) == 0, "exact MIDI dispatch");
  require(b.getMagnitude(0, 256) > 0, "MIDI synthesized audio");
  midi.clear();
  midi.addEvent(juce::MidiMessage::noteOn(1, 38, .7F), 3);
#ifdef ASTE_MAC_AUDIT
  asteRealtimeAuditReset();
  asteRealtimeAuditSetActive(1);
#endif
  audit = true;
  p->processBlock(b, midi);
  audit = false;
#ifdef ASTE_MAC_AUDIT
  asteRealtimeAuditSetActive(0);
  require(asteRealtimeAuditLockCalls() == 0 &&
              asteRealtimeAuditFileOpenCalls() == 0 &&
              asteRealtimeAuditWriteCalls() == 0,
          "callback lock/file/log audit");
#endif
  require(allocations == 0, "callback allocation/free audit");
  p->panic();
  midi.clear();
  p->processBlock(b, midi);
  require(b.getMagnitude(0, 256) == 0, "panic silence");
  const auto file = folder.getChildFile("test_sample.wav");
  makeWave(file);
  const auto wait = [&](const juce::String& status) {
    for (int attempt = 0; attempt < 2500; ++attempt) {
      p->processBlock(b, midi);
      if (p->sampleStatus(0).contains(status)) return true;
      juce::Thread::sleep(1);
    }
    return false;
  };
  p->loadSample(0, file.getFullPathName());
  require(wait("test_sample.wav"), "asynchronous WAV load");
  p->set("o1_source", 1);
  p->set("o1_loop", 1);
  p->set("o1_loop_end", .5F);
  p->audition(0);
  p->processBlock(b, midi);
  require(b.getMagnitude(0, 256) > 0, "sample audio");
  auto sound = p->soundPreset(0);
  p->set("o1_tune", 24);
  require(p->restoreSound(0, sound), "independent sound preset restore");
  require(std::abs(p->value(24) + 12) < .001F, "sound preset retains tune");
  for (int load = 0; load < 8; ++load) {
    p->loadSample(0, file.getFullPathName());
    require(wait("test_sample.wav"), "repeated sample handoff");
    p->audition(0);
    p->processBlock(b, midi);
  }
  p->storeScene(1);
  p->set("o1_tune", 24);
  require(p->recallScene(1), "user scene recall");
  require(std::abs(p->value(24) + 12) < .001F, "scene sound recall");
  p->getStateInformation(state);
  auto reloaded = std::make_unique<Processor>();
  reloaded->setStateInformation(state.getData(),
                                static_cast<int>(state.getSize()));
  reloaded->prepareToPlay(96000, 256);
  require(reloaded->samplePath(0) == file.getFullPathName(),
          "sample path persists across instance reload");
  juce::MemoryBlock recalled;
  reloaded->getStateInformation(recalled);
  require(recalled == state, "project bytes round trip");
  reloaded.reset();
  const auto moved = folder.getChildFile("moved_sample.wav");
  require(file.moveFileTo(moved), "move original sample away from saved path");
  auto portable = std::make_unique<Processor>();
  portable->setStateInformation(state.getData(),
                                static_cast<int>(state.getSize()));
  portable->prepareToPlay(48000, 256);
  for (int attempt = 0; attempt < 2500 && portable->sampleFrames(0) != 4096;
       ++attempt) {
    portable->processBlock(b, midi);
    juce::Thread::sleep(1);
  }
  const bool recovered = portable->sampleFrames(0) == 4096;
  require(portable->recallScene(1),
          "embedded scene restores while file is absent");
  for (int attempt = 0; attempt < 2500 && portable->sampleFrames(0) != 4096;
       ++attempt) {
    portable->processBlock(b, midi);
    juce::Thread::sleep(1);
  }
  const bool sceneRecovered = portable->sampleFrames(0) == 4096;
  portable->loadSample(0, {});
  require(portable->restoreSound(0, sound), "embedded sound preset restores");
  for (int attempt = 0; attempt < 2500 && portable->sampleFrames(0) != 4096;
       ++attempt) {
    portable->processBlock(b, midi);
    juce::Thread::sleep(1);
  }
  const bool soundRecovered = portable->sampleFrames(0) == 4096;
  require(moved.moveFileTo(file), "restore test fixture path");
  require(recovered,
          "project embeds sample audio independently of original path");
  require(sceneRecovered && soundRecovered,
          "scenes and individual sounds carry their samples too");
  portable->audition(0);
  portable->processBlock(b, midi);
  require(b.getMagnitude(0, 256) > 0, "embedded sample actually plays");
  portable.reset();
  p->getStateInformation(canonical);
  xml = juce::AudioProcessor::getXmlFromBinary(
      canonical.getData(), static_cast<int>(canonical.getSize()));
  malformed = juce::ValueTree::fromXml(*xml);
  malformed.getChildWithName("AUDIO0").setProperty("frames", 1048577, nullptr);
  juce::AudioProcessor::copyXmlToBinary(*malformed.createXml(), bad);
  p->setStateInformation(bad.getData(), static_cast<int>(bad.getSize()));
  p->getStateInformation(unchanged);
  require(unchanged == canonical,
          "oversized embedded audio rejects the whole state");
  p->loadSample(0, folder.getChildFile("missing_sample.wav").getFullPathName());
  require(wait("MISSING"), "missing file visible");
  p->scene(0);
  require(wait("Factory fragment"), "factory sample available for editor");
  auto editor = std::unique_ptr<juce::AudioProcessorEditor>(p->createEditor());
  require(
      writeMinimumEditorArtifact(*editor, folder.getChildFile("sequence.png")),
      "minimum sequence layout");
  auto* soundView = button(*editor, "SOUND");
  require(soundView && soundView->onClick, "sound view connected");
  soundView->onClick();
  require(writeMinimumEditorArtifact(*editor, folder.getChildFile("sound.png")),
          "minimum sound layout");
  auto* more = button(*editor, "ADVANCED SOUND");
  require(more && more->onClick, "advanced sound connected");
  more->setToggleState(true, juce::dontSendNotification);
  more->onClick();
  require(writeMinimumEditorArtifact(*editor,
                                     folder.getChildFile("sound_advanced.png")),
          "advanced layout");
  auto* perform = button(*editor, "PERFORM");
  require(perform && perform->onClick, "perform view connected");
  perform->onClick();
  require(
      writeMinimumEditorArtifact(*editor, folder.getChildFile("perform.png")),
      "minimum perform layout");
  PlayHead host;
  p->setPlayHead(&host);
  p->scene(3);
  p->set("master", 18);
  for (double rate : {22050., 44100., 48000., 96000., 192000.}) {
    p->prepareToPlay(rate, 256);
    host.ppq = 0;
    for (int block = 0; block < 64; ++block) {
      host.ppq = block * 256 * host.bpm / (60 * rate);
      p->set("pressure", block % 2 == 0 ? 100 : 0);
      p->set("dirt", block % 3 == 0 ? 100 : 0);
      p->processBlock(b, midi);
      for (int ch = 0; ch < 2; ++ch)
        for (int n = 0; n < 256; ++n)
          require(std::isfinite(b.getSample(ch, n)) &&
                      std::abs(b.getSample(ch, n)) <= .981F,
                  "automation/rate/final limiting");
    }
    host.playing = false;
    p->processBlock(b, midi);
    host.playing = true;
  }
  p->setPlayHead(nullptr);
  std::cout << "K01 plugin passed; " << state.getSize()
            << " state bytes; sample load, missing paths, reload, scenes, 3 "
               "views, RT audit, rates/automation\n";
}
