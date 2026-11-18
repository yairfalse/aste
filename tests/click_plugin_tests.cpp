#include "click_plugin.hpp"
#include "audio_state.hpp"
#include "editor_artifact.hpp"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <new>
#include <iomanip>
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
juce::Component* titled(juce::Component& parent, const juce::String& title) {
  if (parent.getTitle() == title) return &parent;
  for (auto* child : parent.getChildren())
    if (auto* found = titled(*child, title)) return found;
  return nullptr;
}
juce::MouseEvent mouse(juce::Component& component, juce::Point<float> at,
                       juce::Point<float> origin, bool dragged = false) {
  return {juce::Desktop::getInstance().getMainMouseSource(),
          at,
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
void writeAudio(const juce::File& folder, const char* name,
                const juce::AudioBuffer<float>& audio) {
  juce::WavAudioFormat wav;
  auto output =
      folder.getChildFile(juce::String(name) + ".wav").createOutputStream();
  require(output && output->setPosition(0) && output->truncate().wasOk(),
          "WAV output opens");
  std::unique_ptr<juce::OutputStream> stream = std::move(output);
  auto writer = wav.createWriterFor(stream, juce::AudioFormatWriterOptions{}
                                                .withSampleRate(48000)
                                                .withNumChannels(2)
                                                .withBitsPerSample(24));
  require(writer && writer->writeFromAudioSampleBuffer(audio, 0,
                                                       audio.getNumSamples()),
          "actual plugin workflow WAV");
}
void performanceWorkflows(const juce::File& folder) {
  auto p = std::make_unique<Processor>();
  PlayHead host;
  p->setPlayHead(&host);
  p->prepareToPlay(48000, 256);
  p->scene(0);
  require(p->currentControls().object[0].speed == 1 &&
              p->memoryControls().generation[0].speed == 1,
          "neutral playback speed is exactly one");
  juce::AudioBuffer<float> block(2, 256);
  juce::MidiBuffer midi;
  std::uint64_t frames = 0;
  std::ofstream metrics(folder.getChildFile("performance_metrics.csv")
                            .getFullPathName()
                            .toStdString());
  metrics << "example,frames,rms,peak,mean_cpu_fraction\n";
  const auto render = [&](const char* name, int length) {
    juce::AudioBuffer<float> audio(2, length);
    const auto begin = std::chrono::steady_clock::now();
    for (int at = 0; at < length;) {
      const int n = std::min(256, length - at);
      block.setSize(2, n, false, false, true);
      host.ppq = frames / 24000.;
      p->processBlock(block, midi);
      for (int ch = 0; ch < 2; ++ch) audio.copyFrom(ch, at, block, ch, 0, n);
      at += n;
      frames += n;
    }
    const double cpu =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - begin)
            .count() /
        (length / 48000.);
    double energy = 0, peak = 0;
    for (int ch = 0; ch < 2; ++ch)
      for (int n = 0; n < length; ++n) {
        const double x = audio.getSample(ch, n);
        energy += x * x;
        peak = std::max(peak, std::abs(x));
      }
    metrics << name << ',' << length << ',' << std::sqrt(energy / (2 * length))
            << ',' << peak << ',' << cpu << '\n';
    writeAudio(folder, name, audio);
    return audio;
  };
  render("01_straight", 192000);
  p->set("skew", 1);
  render("02_skew", 192000);
  p->set("skew", 0);
  p->set("o1_e1_count", 12);
  p->set("o1_e1_curve", .8F);
  p->set("o1_e1_end_pitch", 12);
  p->set("o1_e1_end_velocity", .2F);
  render("03_accelerating_roll", 192000);
  p->memoryAction(aste::click::MasterMemory::Capture);
  render("04_capture", 256);
  require(p->memoryCount() == 1, "workflow master capture");
  p->set("memory_repeat", 1);
  p->set("repeat_division", 4);
  render("05_master_64", 48000);
  p->set("memory_repeat", 0);
  render("06_release", 48000);
  p->set("memory_play", 1);
  p->set("m0_pitch", -12);
  p->set("m0_cutoff", 1500);
  p->set("m0_resonance", .72F);
  p->set("m0_morph", .4F);
  p->set("m0_playback", 1);
  render("07_transpose_filter", 96000);
  p->memoryAction(aste::click::MasterMemory::Resample);
  render("08_resample_generation_2", 240000);
  require(p->memoryCount() == 2 && p->memorySelection() == 1,
          "first resample publishes playable generation");
  p->syncMemorySelection();
  p->undoEdit();
  render("09_undo_generation", 256);
  require(p->memorySelection() == 0, "undo returns previous generation");
  p->redoEdit();
  render("10_redo_generation", 256);
  require(p->memorySelection() == 1, "redo restores printed generation");
  p->set("m1_pitch", 7);
  p->set("m1_reverse", 1);
  p->set("m1_loop_start", .2F);
  p->set("m1_cutoff", 4000);
  p->set("m1_morph", .8F);
  p->memoryAction(aste::click::MasterMemory::Resample);
  render("11_resample_generation_3", 144000);
  require(p->memoryCount() == 3 && p->memorySelection() == 2,
          "second actual resample generation");
  juce::MemoryBlock saved;
  p->getStateInformation(saved);
  auto xml = juce::AudioProcessor::getXmlFromBinary(
      saved.getData(), static_cast<int>(saved.getSize()));
  const auto tree = juce::ValueTree::fromXml(*xml);
  const auto captures = tree.getChildWithName("MASTER_MEMORY");
  require(captures.getNumChildren() == 3, "project embeds every generation");
  require(captures.getChild(0).getProperty("audio") !=
                  captures.getChild(1).getProperty("audio") &&
              captures.getChild(1).getProperty("audio") !=
                  captures.getChild(2).getProperty("audio"),
          "generations contain different PCM");
  p->setPlayHead(nullptr);
  p.reset();
  p = std::make_unique<Processor>();
  p->setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));
  p->prepareToPlay(48000, 256);
  p->setPlayHead(&host);
  auto recalled = render("12_reopened_project", 96000);
  require(p->memoryCount() == 3 && p->memorySelection() == 2,
          "close/reopen preserves history and selection");
  require(recalled.getMagnitude(0, recalled.getNumSamples()) > .001F,
          "reopened captured audio actually plays");
  const auto selectedAudio =
      aste::parameters::decodeAudio(captures.getChild(2), 192000 * 8);
  require(selectedAudio.has_value(), "selected PCM decodes");
  float capturedPeak = 0;
  for (float x : selectedAudio->stereo)
    capturedPeak = std::max(capturedPeak, std::abs(x));
  require(recalled.getMagnitude(0, recalled.getNumSamples()) <=
              capturedPeak + .000001F,
          "reopened capture does not leak an underlying live hit");
  juce::MemoryBlock roundtrip;
  p->getStateInformation(roundtrip);
  auto roundXml = juce::AudioProcessor::getXmlFromBinary(
      roundtrip.getData(), static_cast<int>(roundtrip.getSize()));
  const auto roundTree = juce::ValueTree::fromXml(*roundXml);
  if (!roundTree.getChildWithName("MASTER_MEMORY").isEquivalentTo(captures)) {
    const auto restored = roundTree.getChildWithName("MASTER_MEMORY");
    std::cerr << "memory selection "
              << captures.getProperty("selected").toString() << " / "
              << restored.getProperty("selected").toString() << '\n';
    for (int n = 0; n < 3; ++n) {
      const auto a = captures.getChild(n), b = restored.getChild(n);
      std::cerr << "generation " << n << " rate "
                << a.getProperty("rate").toString() << '/'
                << b.getProperty("rate").toString() << " frames "
                << a.getProperty("frames").toString() << '/'
                << b.getProperty("frames").toString() << " PCM hashes "
                << a.getProperty("audio").toString().hashCode64() << '/'
                << b.getProperty("audio").toString().hashCode64() << '\n';
    }
  }
  require(roundTree.getChildWithName("MASTER_MEMORY").isEquivalentTo(captures),
          "generation PCM bit exact through host state");
  auto malformed = roundTree.createCopy();
  malformed.getChildWithName("MASTER_MEMORY")
      .getChild(1)
      .setProperty("frames", 192000 * 8 + 1, nullptr);
  juce::MemoryBlock invalid;
  juce::AudioProcessor::copyXmlToBinary(*malformed.createXml(), invalid);
  p->setStateInformation(invalid.getData(),
                         static_cast<int>(invalid.getSize()));
  juce::MemoryBlock afterInvalid;
  p->getStateInformation(afterInvalid);
  require(afterInvalid == roundtrip,
          "malformed generation rejects transactionally");
  auto editor = std::unique_ptr<juce::AudioProcessorEditor>(p->createEditor());
  require(writeMinimumEditorArtifact(
              *editor, folder.getChildFile("captured_generations.png")),
          "captured waveform and generation controls visible");
  auto* wave = titled(*editor, "Captured master waveform");
  require(wave != nullptr, "captured waveform is accessible");
  const juce::Point<float> a{wave->getWidth() * .2F, 25},
      z{wave->getWidth() * .6F, 25};
  wave->mouseDown(mouse(*wave, a, a));
  wave->mouseDrag(mouse(*wave, z, a, true));
  wave->mouseUp(mouse(*wave, z, a, true));
  require(std::abs(p->memoryControls().generation[2].loopStart - .2F) < .001F &&
              std::abs(p->memoryControls().generation[2].loopEnd - .6F) < .001F,
          "waveform drag edits real loop bounds");
  juce::Component* division = button(*editor, "1/64");
  require(division != nullptr, "master division directly accessible");
  division->mouseDown(mouse(*division, {5, 5}, {5, 5}));
  require(p->memoryControls().repeat && p->memoryControls().division == 4,
          "one-gesture master repeat");
  division->mouseUp(mouse(*division, {5, 5}, {5, 5}));
  require(!p->memoryControls().repeat, "release returns from momentary repeat");
  auto* grid = titled(*editor, "Four sound object event grid");
  require(grid != nullptr, "event grid accessible");
  const juce::Point<float> edge{grid->getWidth() / 32.F - 2, 20},
      roll{edge.x + 50, edge.y - 50};
  grid->mouseDown(mouse(*grid, edge, edge));
  grid->mouseDrag(mouse(*grid, roll, edge, true));
  grid->mouseUp(mouse(*grid, roll, edge, true));
  const auto event = p->currentControls().object[0].step[0];
  require(event.count > 1 && event.curve > .5F,
          "one edge drag creates an accelerating roll");
  require(writeMinimumEditorArtifact(*editor,
                                     folder.getChildFile("instant_roll.png")),
          "roll extent feedback");
  auto* originalButton = button(*editor, "ORIGINAL");
  require(originalButton && originalButton->onClick,
          "original material action connected");
  originalButton->onClick();
  require(p->memoryControls().generation[0].pitch == 0 &&
              p->memoryControls().generation[0].cutoff == 20000 &&
              p->memoryControls().generation[0].playback == 0,
          "Original restores unprocessed material in one action");
  p->undoEdit();
  require(p->memoryControls().generation[0].pitch == -12 &&
              p->memoryControls().generation[0].playback == 1,
          "original-material reset is undoable");
  editor.reset();
  p->clearMemory();
  p->scene(1);
  p->set("pressure", 85);
  p->set("recovery", 300);
  p->set("dirt", 65);
  p->set("parallel", 75);
  p->set("compressor_mode", 0);
  p->prepareToPlay(48000, 256);
  frames = 0;
  auto original = render("13_original_dynamics", 192000);
  p->set("compressor_mode", 1);
  p->prepareToPlay(48000, 256);
  frames = 0;
  auto impact = render("14_impact_dynamics", 192000);
  const double aRms = original.getRMSLevel(0, 0, 192000),
               bRms = impact.getRMSLevel(0, 0, 192000);
  require(aRms > 0 && bRms > 0, "both compressor candidates produce audio");
  const double target = std::min(aRms, bRms);
  original.applyGain(static_cast<float>(target / aRms));
  impact.applyGain(static_cast<float>(target / bRms));
  require(std::abs(original.getRMSLevel(0, 0, 192000) -
                   impact.getRMSLevel(0, 0, 192000)) < 1e-6,
          "compressor comparisons RMS matched");
  writeAudio(folder, "15_original_dynamics_matched", original);
  writeAudio(folder, "16_impact_dynamics_matched", impact);
  std::cout << "Compressor RMS matched to " << target
            << "; peaks original=" << original.getMagnitude(0, 192000)
            << " impact=" << impact.getMagnitude(0, 192000) << '\n';
  auto legacy = roundTree.createCopy();
  legacy.removeChild(legacy.getChildWithName("MASTER_MEMORY"), nullptr);
  for (const auto& spec : p->specs())
    if (spec.appended)
      legacy.removeChild(legacy.getChildWithProperty("id", spec.id), nullptr);
  juce::MemoryBlock oldState;
  juce::AudioProcessor::copyXmlToBinary(*legacy.createXml(), oldState);
  p->setStateInformation(oldState.getData(),
                         static_cast<int>(oldState.getSize()));
  p->processBlock(block, midi);
  require(p->currentControls().skew == 0 &&
              p->currentControls().compressorMode == 0 &&
              p->currentControls().object[0].speed == 1 &&
              p->memoryCount() == 0 && !p->memoryControls().play,
          "legacy state restores neutral additions and Original dynamics");
  p->setPlayHead(nullptr);
  std::cout << "Actual plugin workflows: capture, 1/64, transformed PCM x2, "
               "undo/redo, destruction/reopen, malformed state: PASS\n";
}
int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI init;
  const auto folder =
      argc > 1 ? juce::File{argv[1]}
               : juce::File::getCurrentWorkingDirectory().getChildFile(
                     "click_editor_artifacts");
  folder.createDirectory();
  performanceWorkflows(folder);
  auto p = std::make_unique<Processor>();
  p->prepareToPlay(48000, 256);
  require(p->getParameters().size() == 2697, "stable parameter count");
  require(dynamic_cast<juce::AudioProcessorParameterWithID*>(
              p->getParameters()[2590])
                      ->paramID == "master" &&
              dynamic_cast<juce::AudioProcessorParameterWithID*>(
                  p->getParameters()[2591])
                      ->paramID == "bypass",
          "original host positions retained before appended controls");
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
  p->undoEdit();
  require(wait("Factory fragment"),
          "sample-load undo restores previous source");
  p->redoEdit();
  require(wait("Embedded"),
          "sample-load redo uses captured PCM rather than rereading the path");
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
