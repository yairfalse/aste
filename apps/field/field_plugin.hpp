#pragma once

#include "master_output.hpp"
#include "material_revision.hpp"

#include "field_processor.hpp"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>

namespace aste::field::plugin {

class FieldAudioProcessor final : public juce::AudioProcessor {
 public:
  FieldAudioProcessor();

  void prepareToPlay(double sampleRate, int maximumBlockSize) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  void processBlockBypassed(juce::AudioBuffer<float>&,
                            juce::MidiBuffer&) override;
  [[nodiscard]] bool isBusesLayoutSupported(const BusesLayout&) const override;

  [[nodiscard]] juce::AudioProcessorEditor* createEditor() override;
  [[nodiscard]] bool hasEditor() const override { return true; }
  [[nodiscard]] const juce::String getName() const override {
    return "Field F-01";
  }
  [[nodiscard]] bool acceptsMidi() const override { return true; }
  [[nodiscard]] bool producesMidi() const override { return false; }
  [[nodiscard]] bool isMidiEffect() const override { return false; }
  [[nodiscard]] double getTailLengthSeconds() const override { return 120.0; }
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
  [[nodiscard]] float fieldEnergy() const noexcept;
  [[nodiscard]] float retention() const noexcept;
  float loopPosition() const noexcept { return loopPosition_.load(); }
  unsigned loopStatus() const noexcept { return loopStatus_.load(); }
  void clearField() noexcept { clearRequested_.store(true); }
  bool usesDiffuseSpace() const noexcept {
    return engineRevision_.load(std::memory_order_relaxed) == 2;
  }
  bool usesMaterialResponse() const noexcept {
    return materialRevision_.active();
  }
  [[nodiscard]] static int factoryPresetCount() noexcept;
  [[nodiscard]] static juce::String factoryPresetName(int index);
  void loadFactoryPreset(int index);
  void useCurrentResponse() {
    materialRevision_.adopt();
    if (engineRevision_.exchange(2) != 2 || usesLegacyBehavior())
      clearRequested_.store(true);
    legacyBehavior_.store(false);
    updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
  }
  [[nodiscard]] bool usesLegacyBehavior() const noexcept {
    return legacyBehavior_.load(std::memory_order_relaxed);
  }

 private:
  enum ParameterIndex : std::size_t {
    forever,
    mass,
    grain,
    pitch,
    motion,
    distance,
    blend,
    output,
    bypass,
    texture,
    focus,
    loopHold,
    loopMs,
    innerRepeat,
    innerMs,
    loopSpeed,
    loopReverse,
    loopMix,
    parameterCount
  };

  [[nodiscard]] static juce::AudioProcessorValueTreeState::ParameterLayout
  createParameterLayout();
  [[nodiscard]] Parameters currentParameters() const noexcept;
  void processRange(juce::AudioBuffer<float>&, int start, int frames,
                    const Parameters&) noexcept;
  void publishMeters(const MeterSnapshot&) noexcept;

  std::atomic<int> engineRevision_{2};
  std::atomic<float> loopPosition_{};
  std::atomic<unsigned> loopStatus_{};
  aste::parameters::MaterialRevision materialRevision_;
  Processor processor_{};
  std::atomic<bool> legacyBehavior_{};
  std::atomic<bool> clearRequested_{};
  juce::AudioProcessorValueTreeState state_;
  aste::ui::MasterGain masterGain_;
  std::array<std::atomic<float>*, parameterCount> parameterValues_{};
  std::atomic<float> inputPeak_{};
  std::atomic<float> outputPeak_{};
  std::atomic<float> fieldEnergy_{};
  std::atomic<float> retention_{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FieldAudioProcessor)
};

static_assert(std::atomic<float>::is_always_lock_free,
              "Field meter publication must remain lock-free");
static_assert(std::atomic<bool>::is_always_lock_free,
              "Field clear request must remain lock-free");

}  // namespace aste::field::plugin
