#include "contamination_plugin.hpp"
#include <cstdlib>
#include <iostream>
#include <new>
namespace {
thread_local bool audit{};
thread_local unsigned allocations{};
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
#ifdef C01_MAC_AUDIT
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif
using aste::contamination::plugin::Processor;
void require(bool b, const char* m) {
  if (!b) {
    std::cerr << m << '\n';
    std::exit(1);
  }
}
struct ReprepareHost final : juce::AudioProcessorListener {
  bool pending{true};
  unsigned changes{};
  void audioProcessorParameterChanged(juce::AudioProcessor*, int,
                                      float) override {}
  void audioProcessorChanged(juce::AudioProcessor* processor,
                             const ChangeDetails& details) override {
    if (details.latencyChanged) {
      pending = true;
      ++changes;
      std::cout << "latency notification: " << processor->getLatencySamples()
                << '\n';
    }
  }
};
class ZeroLatencyProbe final : public aste::experimental::Instrument {
 public:
  ZeroLatencyProbe()
      : Instrument{"Zero latency probe", "zero-latency-probe", {}} {}
  juce::AudioProcessorEditor* createEditor() override { return nullptr; }

 private:
  bool prepareEngine(double, int) override { return true; }
  void render(juce::AudioBuffer<float>& b,
              const juce::MidiBuffer&) noexcept override {
    b.clear();
  }
};
void latencyRestartRegression() {
  Processor p;
  ReprepareHost host;
  p.addListener(&host);
  unsigned preparations = 0;
  while (host.pending && preparations < 8) {
    host.pending = false;
    ++preparations;
    p.prepareToPlay(48000, 256);
    p.releaseResources();
  }
  for (double rate : {44100., 48000., 96000., 192000.})
    for (int block : {64, 127, 512}) {
      p.prepareToPlay(rate, block);
      require(p.getLatencySamples() == 16,
              "latency stable across rates and block sizes");
      p.releaseResources();
    }
  p.removeListener(&host);
  std::cout << "Latency-driven host: preparations=" << preparations
            << " changes=" << host.changes << " pending=" << host.pending
            << '\n';
  require(
      !host.pending,
      "Repeated preparation must not create another latency restart request");
  require(p.getLatencySamples() == 16,
          "Fixed latency remains 16 after repeat prepare");
  require(host.changes == 0,
          "fixed-latency prepare sends no restart notifications");
  ZeroLatencyProbe zero;
  ReprepareHost zeroHost;
  zero.addListener(&zeroHost);
  zeroHost.pending = false;
  for (int i = 0; i < 8; ++i) {
    zero.prepareToPlay(48000, 127);
    zero.releaseResources();
  }
  zero.removeListener(&zeroHost);
  require(zero.getLatencySamples() == 0 && zeroHost.changes == 0,
          "zero-latency products retain zero without restart requests");
}
void circuitCompatibility() {
  Processor classic;
  classic.scene(0);
  require(!classic.usesDenseCircuit(),
          "historic snapshots select classic circuit");
  juce::MemoryBlock old;
  classic.getStateInformation(old);
  auto xml = juce::AudioProcessor::getXmlFromBinary(
      old.getData(), static_cast<int>(old.getSize()));
  auto tree = juce::ValueTree::fromXml(*xml);
  tree.removeProperty("circuit", nullptr);
  juce::MemoryBlock historic;
  juce::AudioProcessor::copyXmlToBinary(*tree.createXml(), historic);
  Processor restored;
  restored.setStateInformation(historic.getData(),
                               static_cast<int>(historic.getSize()));
  require(!restored.usesDenseCircuit(),
          "pre-revision sessions retain classic sound even with material=1");
  std::array<float, 12> params{};
  for (int i = 0; i < 12; ++i)
    params[static_cast<std::size_t>(i)] =
        restored.getParameters()[i]->getValue();
  restored.adoptDenseCircuit();
  require(restored.usesDenseCircuit(), "explicit dense adoption");
  for (int i = 0; i < 12; ++i)
    require(restored.getParameters()[i]->getValue() ==
                params[static_cast<std::size_t>(i)],
            "adoption preserves all parameter values");
  juce::MemoryBlock dense, again;
  restored.getStateInformation(dense);
  Processor copy;
  copy.setStateInformation(dense.getData(), static_cast<int>(dense.getSize()));
  copy.getStateInformation(again);
  require(copy.usesDenseCircuit() && dense == again,
          "dense revision recalls byte stably");
  auto broken =
      juce::ValueTree::fromXml(*juce::AudioProcessor::getXmlFromBinary(
          dense.getData(), static_cast<int>(dense.getSize())));
  broken.setProperty("circuit", "bad", nullptr);
  broken.getChildWithProperty("id", "memory").setProperty("value", 1, nullptr);
  juce::MemoryBlock malformed;
  juce::AudioProcessor::copyXmlToBinary(*broken.createXml(), malformed);
  copy.setStateInformation(malformed.getData(),
                           static_cast<int>(malformed.getSize()));
  copy.getStateInformation(again);
  require(again == dense,
          "invalid revision rejects the complete state transaction");
  // The selected classic path must remain sample-identical to the original
  // core.
  classic.prepareToPlay(48000, 127);
  aste::contamination::Engine reference;
  reference.prepare(48000, 4);
  aste::contamination::Controls c;
  c.pressure = classic.value(0) * .01;
  c.contamination = classic.value(1) * .01;
  c.instability = classic.value(2) * .01;
  c.memory = classic.value(3) * .01;
  c.motion = classic.value(4) * .01;
  c.colour = classic.value(5);
  c.attack = classic.value(6) * .001;
  c.decay = classic.value(7) * .001;
  c.sustain = classic.value(8) * .01;
  c.release = classic.value(9) * .001;
  juce::AudioBuffer<float> audio{2, 127};
  std::array<float, 127> left{}, right{};
  for (int block = 0; block < 30; ++block) {
    juce::MidiBuffer midi;
    std::array<aste::contamination::Event, 1> events{};
    std::size_t count = 0;
    if (block == 0) {
      auto on = juce::MidiMessage::noteOn(1, 48, .8F);
      midi.addEvent(on, 23);
      events[count++] = {23, aste::contamination::Kind::On, 48,
                         on.getVelocity() / 127.};
    }
    if (block == 20) {
      midi.addEvent(juce::MidiMessage::noteOff(1, 48), 11);
      events[count++] = {11, aste::contamination::Kind::Off, 48, 0};
    }
    classic.processBlock(audio, midi);
    reference.process(left.data(), right.data(), 127, c,
                      std::span(events).first(count));
    for (int i = 0; i < 127; ++i)
      require(audio.getSample(0, i) == left[static_cast<std::size_t>(i)] &&
                  audio.getSample(1, i) == right[static_cast<std::size_t>(i)],
              "classic audio is sample-identical");
  }
  std::cout << "Circuit compatibility and exact classic audio passed\n";
}
int main() {
  juce::ScopedJuceInitialiser_GUI init;
  latencyRestartRegression();
  circuitCompatibility();
  Processor p;
  p.prepareToPlay(48000, 256);
  require(p.getParameters().size() == 12, "parameter count");
  require(p.getLatencySamples() == 16, "latency");
  p.set("memory", 87);
  p.set("colour", 1789);
  juce::MemoryBlock state;
  p.getStateInformation(state);
  Processor q;
  q.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  q.prepareToPlay(48000, 256);
  require(q.value(3) == 87 && q.value(5) == 1789, "state recall");
  auto xml = juce::AudioProcessor::getXmlFromBinary(
      state.getData(), static_cast<int>(state.getSize()));
  auto tree = juce::ValueTree::fromXml(*xml);
  tree.getChildWithProperty("id", "memory")
      .setProperty("value", "NaN", nullptr);
  juce::MemoryBlock bad;
  juce::AudioProcessor::copyXmlToBinary(*tree.createXml(), bad);
  q.setStateInformation(bad.getData(), static_cast<int>(bad.getSize()));
  require(q.value(3) == 87, "malformed restore rejected");
  juce::AudioBuffer<float> audio(2, 256);
  juce::MidiBuffer midi;
  midi.addEvent(juce::MidiMessage::noteOn(1, 48, .8F), 73);
  q.processBlock(audio, midi);
  for (int i = 0; i < 73; ++i)
    require(audio.getSample(0, i) == 0, "MIDI offset");
  require(audio.getMagnitude(0, 256) > 0, "MIDI sounding");
  midi.clear();
  midi.addEvent(juce::MidiMessage::noteOn(1, 60, .8F), 2);
#ifdef C01_MAC_AUDIT
  asteRealtimeAuditReset();
  asteRealtimeAuditSetActive(1);
#endif
  audit = true;
  q.processBlock(audio, midi);
  audit = false;
#ifdef C01_MAC_AUDIT
  asteRealtimeAuditSetActive(0);
  require(asteRealtimeAuditLockCalls() == 0 &&
              asteRealtimeAuditFileOpenCalls() == 0 &&
              asteRealtimeAuditWriteCalls() == 0,
          "JUCE callback RT audit");
#endif
  require(allocations == 0, "JUCE callback allocation/free audit");
  midi.clear();
  q.panic();
  q.processBlock(audio, midi);
  require(audio.getMagnitude(0, 256) == 0, "panic");
  Processor fresh;
  auto editor =
      std::unique_ptr<juce::AudioProcessorEditor>(fresh.createEditor());
  editor->setSize(900, 560);
  require(editor->getWidth() == 900, "editor minimum");
  for (int i = 0; i < 5; ++i) {
    juce::Thread::sleep(55);
    juce::Timer::callPendingTimersSynchronously();
  }
  std::cout << "Editor timer ticks completed\n";
  auto image = editor->createComponentSnapshot(editor->getLocalBounds());
  auto out = juce::File::getCurrentWorkingDirectory()
                 .getChildFile("c01-editor.png")
                 .createOutputStream();
  if (out) {
    out->setPosition(0);
    out->truncate();
  }
  require(out && juce::PNGImageFormat{}.writeImageToStream(image, *out),
          "editor artifact");
  std::cout << "C01 plugin contracts passed\n";
}
