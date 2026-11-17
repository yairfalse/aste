#include "grain_plugin.hpp"
#include "../research/quality/audio.hpp"
#include "editor_artifact.hpp"
#include "master_output_checks.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
#ifdef GRAIN_MAC_AUDIT
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
  if (auto* p = std::malloc(std::max(n, std::size_t{1}))) return p;
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
using Processor = aste::grain::plugin::GrainAudioProcessor;
void require(bool b, const char* text) {
  if (!b) {
    ++failures;
    std::cerr << "FAIL: " << text << '\n';
  }
}
int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI juce;
  {
    Processor source;
    source.set("mix", 1);
    source.set("n1_capture_seconds", .02F);
    source.prepareToPlay(48000, 1024);
    juce::AudioBuffer<float> captured(2, 1024);
    juce::MidiBuffer midi;
    for (int ch = 0; ch < 2; ++ch)
      for (int i = 0; i < 1024; ++i)
        captured.setSample(ch, i, .3F * std::sin(i * .13F));
    source.processBlock(captured, midi);
    source.set("freeze_all", 1);
    source.processBlock(captured, midi);
    const auto storedFrames = source.nodeMeter(0, 6);
    require(storedFrames > 0, "source has captured audio before save");
    juce::MemoryBlock state;
    source.getStateInformation(state);
    auto xml = juce::AudioProcessor::getXmlFromBinary(
        state.getData(), static_cast<int>(state.getSize()));
    auto malformed = juce::ValueTree::fromXml(*xml);
    malformed.getChildWithName("MEMORY").getChild(0).setProperty(
        "audio", "invalid", nullptr);
    juce::MemoryBlock bad;
    juce::AudioProcessor::copyXmlToBinary(*malformed.createXml(), bad);
    source.setStateInformation(bad.getData(), static_cast<int>(bad.getSize()));
    juce::MemoryBlock unchanged;
    source.getStateInformation(unchanged);
    require(state == unchanged,
            "malformed audio leaves parameters and memory unchanged");
    for (double rate : {48000., 96000.}) {
      Processor restored;
      restored.setStateInformation(state.getData(),
                                   static_cast<int>(state.getSize()));
      restored.prepareToPlay(rate, 1024);
      float peak = 0;
      for (int block = 0; block < 20; ++block) {
        captured.clear();
        restored.processBlock(captured, midi);
        peak = std::max(peak, captured.getMagnitude(0, 1024));
      }
      require(peak > .01F,
              "Frozen captured audio survives state-before-prepare recall");
      require(
          restored.nodeMeter(0, 6) == std::round(storedFrames * rate / 48000),
          "Recall preserves captured duration across sample rates");
      restored.releaseResources();
      restored.prepareToPlay(rate, 1024);
      captured.clear();
      restored.processBlock(captured, midi);
      require(restored.nodeMeter(0, 6) > 0,
              "host reprepare retains frozen audio");
      allocations = 0;
      restored.setStateInformation(state.getData(),
                                   static_cast<int>(state.getSize()));
      allocations = 0;
      audit = true;
      restored.processBlock(captured, midi);
      audit = false;
      require(allocations == 0,
              "restored engine handoff never allocates or frees on audio");
      require(restored.nodeMeter(0, 6) > 0,
              "state-after-prepare restores captured audio");
    }
  }
#ifdef GRAIN_MAC_AUDIT
  pthread_mutex_t lock;
  pthread_mutex_init(&lock, nullptr);
  asteRealtimeAuditReset();
  asteRealtimeAuditSetActive(1);
  pthread_mutex_lock(&lock);
  pthread_mutex_unlock(&lock);
  auto* f = std::fopen("/dev/null", "r");
  if (f) std::fclose(f);
  (void)::write(-1, "x", 1);
  asteRealtimeAuditSetActive(0);
  pthread_mutex_destroy(&lock);
  require(asteRealtimeAuditLockCalls() > 0 &&
              asteRealtimeAuditFileOpenCalls() > 0 &&
              asteRealtimeAuditWriteCalls() > 0,
          "OS audit calibrated");
#endif
  if (argc == 3 && juce::String{argv[1]} == "--render") {
    const juce::File folder{juce::String::fromUTF8(argv[2])};
    folder.createDirectory();
    Processor render;
    render.set("mix", 1);
    render.prepareToPlay(48000, 511);
    power::Audio output(48000 * 8, 48000);
    juce::MidiBuffer midi;
    for (std::size_t at = 0; at < output.left.size();) {
      auto n = std::min({std::size_t{511}, output.left.size() - at,
                         std::size_t{48000} - at % 48000});
      if (at == 48000) render.set("n1_freeze", 1);
      juce::AudioBuffer<float> b{2, static_cast<int>(n)};
      for (std::size_t i = 0; i < n; ++i) {
        double t = (at + i) / 48000.;
        float x = 0;
        if (t < 2)
          for (int note : {48, 51, 55, 62})
            x += static_cast<float>(.06 *
                                    std::sin(2 * power::pi * 440 *
                                             std::exp2((note - 69) / 12.) * t) *
                                    std::exp(-t * 2));
        b.setSample(0, static_cast<int>(i), x);
        b.setSample(1, static_cast<int>(i), x);
      }
      render.processBlock(b, midi);
      for (std::size_t i = 0; i < n; ++i) {
        output.left[at + i] = b.getSample(0, static_cast<int>(i));
        output.right[at + i] = b.getSample(1, static_cast<int>(i));
      }
      at += n;
    }
    power::validate(output);
    power::wav(folder.getChildFile("grain-g01-playable.wav")
                   .getFullPathName()
                   .toStdString(),
               output);
    std::cout << "Rendered playable G-01: peak=" << power::measure(output).peak
              << " rms=" << power::measure(output).rms << '\n';
    return 0;
  }
  Processor p;
  require(p.getParameters().size() == 94, "parameter identity count");
  require(p.getTotalNumInputChannels() == 2 && !p.acceptsMidi(),
          "stereo audio effect, not MIDI instrument");
  juce::MemoryBlock state, copy;
  p.getStateInformation(state);
  p.set("n2_pitch", -11);
  p.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  p.getStateInformation(copy);
  require(state == copy, "byte stable recall");
  p.setStateInformation("bad", 3);
  p.getStateInformation(copy);
  require(state == copy, "malformed state rejected");
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    p.prepareToPlay(rate, 511);
    juce::AudioBuffer<float> b{2, 511};
    juce::MidiBuffer midi;
    float peak = 0;
    for (int block = 0; block < 80; ++block) {
      for (int i = 0; i < 511; ++i) {
        float v =
            block < 50
                ? .2F * std::sin(static_cast<float>((block * 511 + i) * .07))
                : 0;
        b.setSample(0, i, v);
        b.setSample(1, i, v);
      }
      allocations = 0;
#ifdef GRAIN_MAC_AUDIT
      asteRealtimeAuditReset();
      asteRealtimeAuditSetActive(1);
#endif
      audit = true;
      p.processBlock(b, midi);
      audit = false;
#ifdef GRAIN_MAC_AUDIT
      asteRealtimeAuditSetActive(0);
      require(asteRealtimeAuditLockCalls() == 0 &&
                  asteRealtimeAuditFileOpenCalls() == 0 &&
                  asteRealtimeAuditWriteCalls() == 0,
              "callback OS audit");
#endif
      require(allocations == 0, "callback no allocations or frees");
      for (int ch = 0; ch < 2; ++ch)
        for (int i = 0; i < 511; ++i)
          require(std::isfinite(b.getSample(ch, i)), "finite output");
      peak = std::max(peak, b.getMagnitude(0, 511));
    }
    require(peak > .01F, "input produces effect output");
    p.set("freeze_all", 1);
    p.processBlock(b, midi);
    require(p.nodeMeter(0, 6) > 0 && p.nodeMeter(0, 5) > .5F,
            "freeze retains source bank");
    p.set("bypass", 1);
    b.clear();
    b.setSample(0, 17, .25F);
    p.processBlock(b, midi);
    require(b.getSample(0, 17) == .25F, "effect bypass passes dry");
    p.set("bypass", 0);
    p.set("freeze_all", 0);
    p.clearAll();
    p.processBlock(b, midi);
  }
  auto before = p.currentControls();
  p.scene(3);
  auto after = p.currentControls();
  require(before.node == after.node, "routing scene preserves sounds");
  p.mutate(0);
  juce::AudioBuffer<float> finalAudio{2, 127};
  finalAudio.clear();
  juce::MidiBuffer finalMidi;
  allocations = 0;
  audit = true;
  p.clearAll();
  p.processBlock(finalAudio, finalMidi);
  audit = false;
  require(allocations == 0, "clear and mutate stay allocation-free");
  Processor masterCheck;
  masterCheck.set("mix", 0);
  masterCheck.set("master", -6);
  masterCheck.prepareToPlay(48000, 511);
  juce::AudioBuffer<float> dry{2, 511};
  for (int ch = 0; ch < 2; ++ch)
    for (int i = 0; i < 511; ++i) dry.setSample(ch, i, .2F);
  masterCheck.processBlock(dry, finalMidi);
  require(std::abs(dry.getSample(0, 200) - .2F * std::pow(10.F, -6.F / 20)) <
              1.e-5F,
          "Master controls dry mix too");
  require(std::isinf(masterCheck.getTailLengthSeconds()),
          "frozen memory advertises infinite tail");
  Processor mono;
  auto layout = mono.getBusesLayout();
  layout.inputBuses.set(0, juce::AudioChannelSet::mono());
  layout.outputBuses.set(0, juce::AudioChannelSet::mono());
  require(mono.setBusesLayout(layout), "mono effect layout accepted");
  mono.prepareToPlay(48000, 511);
  juce::AudioBuffer<float> one{1, 511};
  for (int i = 0; i < 511; ++i) one.setSample(0, i, .2F);
  mono.processBlock(one, finalMidi);
  require(one.getMagnitude(0, 511) > .01F, "mono effect folds stereo grains");
  Processor armed;
  armed.set("freeze_all", 1);
  armed.prepareToPlay(48000, 511);
  juce::AudioBuffer<float> waiting{2, 511};
  for (int block = 0; block < 60; ++block) {
    waiting.clear();
    armed.processBlock(waiting, finalMidi);
  }
  require(armed.nodeMeter(0, 6) == 0,
          "empty recalled freeze waits through silence");
  for (int block = 0; block < 60; ++block) {
    for (int ch = 0; ch < 2; ++ch)
      for (int i = 0; i < 511; ++i) waiting.setSample(ch, i, .2F);
    armed.processBlock(waiting, finalMidi);
  }
  require(armed.nodeMeter(0, 6) > 0 && armed.nodeMeter(0, 5) > .5F,
          "armed freeze captures new material then holds");
  if (argc == 2) {
    juce::File folder{juce::String::fromUTF8(argv[1])};
    folder.createDirectory();
    Processor ui;
    std::unique_ptr<juce::AudioProcessorEditor> editor{ui.createEditor()};
    require(
        writeMinimumEditorArtifact(*editor, folder.getChildFile("minimum.png")),
        "minimum editor bounds");
  }
  std::cout << "grain_plugin_tests: " << failures << " failures\n";
  return failures ? 1 : 0;
}
