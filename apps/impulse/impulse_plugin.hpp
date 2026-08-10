#pragma once

#include "impulse_processor.hpp"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>

namespace aste::impulse::plugin {

class ImpulseAudioProcessor final : public juce::AudioProcessor {
 public:
  ImpulseAudioProcessor();
  void prepareToPlay(double sampleRate, int maximumBlockSize) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  void processBlockBypassed(juce::AudioBuffer<float>&,
                            juce::MidiBuffer&) override;
  [[nodiscard]] bool isBusesLayoutSupported(const BusesLayout&) const override;
  [[nodiscard]] juce::AudioProcessorEditor* createEditor() override;
  [[nodiscard]] bool hasEditor() const override { return true; }
  [[nodiscard]] const juce::String getName() const override {
    return "Impulse I-01";
  }
  [[nodiscard]] bool acceptsMidi() const override { return true; }
  [[nodiscard]] bool producesMidi() const override { return false; }
  [[nodiscard]] bool isMidiEffect() const override { return false; }
  [[nodiscard]] double getTailLengthSeconds() const override { return 3.0; }
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
  [[nodiscard]] float outputPeak() const noexcept;
  [[nodiscard]] int currentStep(std::size_t track) const noexcept;
  [[nodiscard]] static int factoryPresetCount() noexcept;
  [[nodiscard]] static juce::String factoryPresetName(int index);
  void loadFactoryPreset(int index);

 private:
  static constexpr std::size_t kGlobalCount = 8;
  static constexpr std::size_t kTrackParameterCount = 13;
  [[nodiscard]] static juce::AudioProcessorValueTreeState::ParameterLayout
  createParameterLayout();
  [[nodiscard]] Parameters currentParameters() const noexcept;
  [[nodiscard]] Transport currentTransport() const noexcept;
  [[nodiscard]] std::span<const MidiEvent> readMidi(const juce::MidiBuffer&,
                                                    int frames) noexcept;
  void publishMeters(const MeterSnapshot&) noexcept;

  Processor processor_{};
  juce::AudioProcessorValueTreeState state_;
  std::array<std::atomic<float>*, kGlobalCount> globals_{};
  std::array<std::array<std::atomic<float>*, kTrackParameterCount>, kTrackCount>
      tracks_{};
  std::array<std::array<std::atomic<float>*, kPatternSteps>, kTrackCount>
      patterns_{};
  std::array<MidiEvent, 256> midiScratch_{};
  std::size_t midiCount_{};
  std::atomic<float> outputPeak_{};
  std::array<std::atomic<float>, kTrackCount> currentSteps_{};
  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ImpulseAudioProcessor)
};

static_assert(std::atomic<float>::is_always_lock_free,
              "Impulse meter publication must remain lock-free");

}  // namespace aste::impulse::plugin
