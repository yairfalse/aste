#pragma once

#include "harmonic_processor.hpp"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>

namespace aste::harmonic::plugin {

class HarmonicAudioProcessor final : public juce::AudioProcessor {
 public:
  HarmonicAudioProcessor();

  void prepareToPlay(double sampleRate, int maximumBlockSize) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  void processBlockBypassed(juce::AudioBuffer<float>&,
                            juce::MidiBuffer&) override;
  [[nodiscard]] bool isBusesLayoutSupported(const BusesLayout&) const override;

  [[nodiscard]] juce::AudioProcessorEditor* createEditor() override;
  [[nodiscard]] bool hasEditor() const override { return true; }
  [[nodiscard]] const juce::String getName() const override {
    return "Harmonic H-01";
  }
  [[nodiscard]] bool acceptsMidi() const override { return false; }
  [[nodiscard]] bool producesMidi() const override { return false; }
  [[nodiscard]] bool isMidiEffect() const override { return false; }
  [[nodiscard]] double getTailLengthSeconds() const override { return 0.0; }
  [[nodiscard]] juce::AudioProcessorParameter* getBypassParameter()
      const override;

  [[nodiscard]] int getNumPrograms() override { return 1; }
  [[nodiscard]] int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  [[nodiscard]] const juce::String getProgramName(int) override {
    return "Default";
  }
  void changeProgramName(int, const juce::String&) override {}

  void getStateInformation(juce::MemoryBlock&) override;
  void setStateInformation(const void*, int) override;

  [[nodiscard]] juce::AudioProcessorValueTreeState& state() noexcept {
    return state_;
  }
  [[nodiscard]] float inputPeak() const noexcept;
  [[nodiscard]] float outputPeak() const noexcept;
  [[nodiscard]] float harmonicActivity() const noexcept;
  [[nodiscard]] static int factoryPresetCount() noexcept;
  [[nodiscard]] static juce::String factoryPresetName(int index);
  void loadFactoryPreset(int index);

 private:
  enum ParameterIndex : std::size_t {
    input,
    foundationGain,
    foundationFrequency,
    bodyGain,
    bodyFrequency,
    presenceGain,
    presenceFrequency,
    airGain,
    airFrequency,
    harmonic,
    output,
    bypass,
    parameterCount
  };

  [[nodiscard]] static juce::AudioProcessorValueTreeState::ParameterLayout
  createParameterLayout();
  [[nodiscard]] Parameters currentParameters() const noexcept;
  void publishMeters(const MeterSnapshot&) noexcept;

  Processor processor_{};
  juce::AudioProcessorValueTreeState state_;
  std::array<std::atomic<float>*, parameterCount> parameterValues_{};
  std::atomic<float> inputPeak_{};
  std::atomic<float> outputPeak_{};
  std::atomic<float> harmonicActivity_{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmonicAudioProcessor)
};

static_assert(std::atomic<float>::is_always_lock_free,
              "Meter publication must remain lock-free on supported targets");

}  // namespace aste::harmonic::plugin
