#pragma once
#include "click_processor.hpp"
#include "experimental_instrument.hpp"
#include <mutex>

namespace aste::click::plugin {
class Processor final : public aste::experimental::Instrument,
                        private juce::Thread {
 public:
  Processor();
  ~Processor() override;
  juce::AudioProcessorEditor* createEditor() override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  void setStateInformation(const void*, int) override;
  double getTailLengthSeconds() const override { return 16; }
  static constexpr int globals = 22, objectFields = 34, eventFields = 19;
  static juce::String objectId(int, const char*);
  static juce::String eventId(int, int, const char*);
  static const std::array<const char*, objectFields> objectIds;
  static const std::array<const char*, eventFields> eventIds;
  Controls currentControls() const noexcept;
  void scene(int);
  int factoryIndex() const;
  juce::ValueTree soundPreset(int);
  bool restoreSound(int, const juce::ValueTree&);
  void replaceSound(int, int);
  void moveEvent(int, int, int);
  void duplicateSound(int, int);
  void loadSample(int, const juce::String&);
  juce::String sampleStatus(int) const;
  juce::String samplePath(int) const;
  std::size_t sampleFrames(int) const;
  double sampleRate(int) const;
  std::array<float, 256> waveform(int) const;
  void audition(int n) noexcept {
    if (n >= 0 && n < 4) audition_.fetch_or(1U << n);
  }
  void panic() noexcept { clearRequested_.store(true); }
  void storeScene(int);
  bool recallScene(int);
  bool hasScene(int) const;
  float meter(int index) const noexcept {
    return meters_[static_cast<std::size_t>(index)].load();
  }
  float outputPeak() const noexcept { return finalPeak_.load(); }

 private:
  static std::vector<aste::experimental::ControlSpec> controls();
  bool prepareEngine(double, int) override;
  void render(juce::AudioBuffer<float>&,
              const juce::MidiBuffer&) noexcept override;
  void run() override;
  void writeProductMetadata(juce::ValueTree&) const override;
  bool validateProductMetadata(const juce::ValueTree&) const override;
  void restoreProductMetadata(const juce::ValueTree&) override;
  void applySound(int, const Object&);
  void loadStoredSample(int, const juce::String&, const juce::ValueTree&);
  void writeSamples(juce::ValueTree&) const;  // metadata_ held by caller.
  static bool validSamples(const juce::ValueTree&);
  Engine engine_;
  std::array<float, 512> mono_{};
  std::atomic<unsigned> audition_{};
  std::array<std::atomic<float>, 20> meters_{};
  std::atomic<float> finalPeak_{};
  juce::AudioFormatManager formats_;
  mutable std::mutex metadata_;
  std::array<juce::String, 4> paths_, status_;
  std::array<std::array<float, 256>, 4> waves_{};
  std::array<std::size_t, 4> frames_{};
  std::array<double, 4> sampleRates_{};
  std::array<std::uint64_t, 4> requested_{}, completed_{};
  std::array<juce::ValueTree, 4> scenes_;
  std::array<juce::ValueTree, 4> audio_;
};
}  // namespace aste::click::plugin
