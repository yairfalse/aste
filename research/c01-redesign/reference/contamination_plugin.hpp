#pragma once
#include "experimental_instrument.hpp"
#include "core.hpp"
namespace aste::contamination::plugin {
class Processor final : public aste::experimental::Instrument {
 public:
  Processor();
  void prepareToPlay(double rate,int block) override {
    Instrument::prepareToPlay(rate,block);setLatencySamples(Engine::latency);
  }
  double getTailLengthSeconds() const override {return 12;}
  juce::AudioProcessorEditor* createEditor() override;
  void clearMemory() noexcept {memoryRequested_.store(true);}
  void panic() noexcept {clearRequested_.store(true);}
  void scene(int);
  float memoryLevel() const noexcept {return memoryLevel_.load();}
  float interaction() const noexcept {return interaction_.load();}
 private:
  static std::vector<aste::experimental::ControlSpec> controls();
  bool prepareEngine(double,int) override;
  void render(juce::AudioBuffer<float>&,const juce::MidiBuffer&) noexcept override;
  Engine engine_;
  std::array<float,512> monoRight_{};
  std::atomic<bool> memoryRequested_{};
  std::atomic<float> memoryLevel_{},interaction_{};
  bool playing_{};
};
}
