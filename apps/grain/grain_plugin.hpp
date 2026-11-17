#pragma once
#include "experimental_instrument.hpp"
#include "grain_processor.hpp"
namespace aste::grain::plugin {
class GrainAudioProcessor final : public aste::experimental::Instrument {
 public:
  static constexpr std::size_t globalCount = 8, liveBase = 8, routeBase = 11,
                               nodeBase = 20, nodeFields = 24;
  GrainAudioProcessor();
  juce::AudioProcessorEditor* createEditor() override;
  double getTailLengthSeconds() const override {
    return std::numeric_limits<double>::infinity();
  }
  static juce::String nodeId(std::size_t node, const char* field);
  static juce::String routeId(std::size_t destination, std::size_t source);
  Controls currentControls() const noexcept;
  void scene(int);
  void clearNode(std::size_t n) noexcept {
    if (n < 3) clearMask_.fetch_or(1U << n);
  }
  void clearAll() noexcept { clearMask_.fetch_or(8U); }
  void mutate(std::size_t n);
  float nodeMeter(std::size_t n, std::size_t field) const noexcept {
    return meters_[n * 8 + field].load(std::memory_order_relaxed);
  }
  static const std::array<const char*, nodeFields> fieldIds;

 private:
  static std::vector<aste::experimental::ControlSpec> controls();
  bool prepareEngine(double, int) override;
  void render(juce::AudioBuffer<float>&,
              const juce::MidiBuffer&) noexcept override;
  Engine engine_;
  std::atomic<unsigned> clearMask_{};
  std::array<std::atomic<float>, 24> meters_{};
};
}  // namespace aste::grain::plugin
