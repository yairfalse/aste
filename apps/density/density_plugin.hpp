#pragma once

#include "master_output.hpp"
#include "material_revision.hpp"

#include "density_processor.hpp"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>

namespace aste::density::plugin {

class DensityAudioProcessor final : public juce::AudioProcessor {
 public:
  DensityAudioProcessor();

  void prepareToPlay(double sampleRate, int maximumBlockSize) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  void processBlockBypassed(juce::AudioBuffer<float>&,
                            juce::MidiBuffer&) override;
  [[nodiscard]] bool isBusesLayoutSupported(const BusesLayout&) const override;

  [[nodiscard]] juce::AudioProcessorEditor* createEditor() override;
  [[nodiscard]] bool hasEditor() const override { return true; }
  [[nodiscard]] const juce::String getName() const override {
    return "Density D-01";
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
  [[nodiscard]] float gainReductionDb() const noexcept;
  bool usesMaterialResponse() const noexcept {
    return materialRevision_.active();
  }
  [[nodiscard]] static int factoryPresetCount() noexcept;
  [[nodiscard]] static juce::String factoryPresetName(int index);
  void loadFactoryPreset(int index);
  void useCurrentResponse() {
    materialRevision_.adopt();
    legacyBehavior_.store(false);
    updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
  }
  [[nodiscard]] bool usesLegacyBehavior() const noexcept {
    return legacyBehavior_.load(std::memory_order_relaxed);
  }

 private:
  enum ParameterIndex : std::size_t {
    drive,
    crush,
    attack,
    release,
    density,
    blend,
    output,
    stereo,
    detectorHpf,
    protection,
    bypass,
    character,
    parameterCount
  };

  [[nodiscard]] static juce::AudioProcessorValueTreeState::ParameterLayout
  createParameterLayout();
  [[nodiscard]] Parameters currentParameters() const noexcept;
  void publishMeters(const MeterSnapshot&) noexcept;

  aste::parameters::MaterialRevision materialRevision_;
  Processor processor_{};
  std::atomic<bool> legacyBehavior_{};
  juce::AudioProcessorValueTreeState state_;
  aste::ui::MasterGain masterGain_;
  std::array<std::atomic<float>*, parameterCount> parameterValues_{};
  std::atomic<float> inputPeak_{};
  std::atomic<float> outputPeak_{};
  std::atomic<float> gainReduction_{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DensityAudioProcessor)
};

static_assert(std::atomic<float>::is_always_lock_free,
              "Meter publication must remain lock-free on supported targets");
static_assert(std::atomic<bool>::is_always_lock_free);

}  // namespace aste::density::plugin
