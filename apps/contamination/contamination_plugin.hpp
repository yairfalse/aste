#pragma once
#include "experimental_instrument.hpp"
#include "core.hpp"
#include "revised_circuit.hpp"
namespace aste::contamination::plugin {
class Processor final : public aste::experimental::Instrument {
 public:
  Processor();
  void prepareToPlay(double rate, int block) override {
    Instrument::prepareToPlay(rate, block);
    setLatencySamples(Engine::latency);
  }
  double getTailLengthSeconds() const override {
    return usesDenseCircuit() ? 60 : 12;
  }
  bool usesDenseCircuit() const noexcept {
    return circuit_.load(std::memory_order_acquire) == 2;
  }
  void adoptDenseCircuit();
  float spaceLevel() const noexcept { return spaceLevel_.load(); }
  juce::AudioProcessorEditor* createEditor() override;
  void clearMemory() noexcept { memoryRequested_.store(true); }
  void panic() noexcept { clearRequested_.store(true); }
  void scene(int);
  float memoryLevel() const noexcept { return memoryLevel_.load(); }
  float interaction() const noexcept { return interaction_.load(); }

 private:
  static std::vector<aste::experimental::ControlSpec> controls();
  bool prepareEngine(double, int) override;
  void render(juce::AudioBuffer<float>&,
              const juce::MidiBuffer&) noexcept override;
  void writeProductMetadata(juce::ValueTree&) const override;
  bool validateProductMetadata(const juce::ValueTree&) const override;
  void restoreProductMetadata(const juce::ValueTree&) override;
  Engine engine_;
  revised::Engine revised_;
  std::atomic<int> circuit_{2};
  bool activeDense_{true};
  std::atomic<float> spaceLevel_{};
  std::array<float, 512> monoRight_{};
  std::atomic<bool> memoryRequested_{};
  std::atomic<float> memoryLevel_{}, interaction_{};
  bool playing_{};
};
}  // namespace aste::contamination::plugin
