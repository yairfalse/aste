#pragma once

#include "master_output.hpp"
#include "material_revision.hpp"

#include "sequence_processor.hpp"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>

namespace aste::sequence::plugin {

class SequenceAudioProcessor final : public juce::AudioProcessor {
 public:
  SequenceAudioProcessor();

  void prepareToPlay(double sampleRate, int maximumBlockSize) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  void processBlockBypassed(juce::AudioBuffer<float>&,
                            juce::MidiBuffer&) override;
  [[nodiscard]] bool isBusesLayoutSupported(const BusesLayout&) const override;

  [[nodiscard]] juce::AudioProcessorEditor* createEditor() override;
  [[nodiscard]] bool hasEditor() const override { return true; }
  [[nodiscard]] const juce::String getName() const override {
    return "Sequence S-01";
  }
  [[nodiscard]] bool acceptsMidi() const override { return true; }
  [[nodiscard]] bool producesMidi() const override { return false; }
  [[nodiscard]] bool isMidiEffect() const override { return false; }
  [[nodiscard]] double getTailLengthSeconds() const override { return 15.0; }
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
  [[nodiscard]] float envelopeLevel() const noexcept;
  [[nodiscard]] int currentStep() const noexcept;
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
  enum MainParameter : std::size_t {
    pressure,
    shape,
    oscillatorMix,
    detune,
    sub,
    cutoff,
    resonance,
    filterMorph,
    envelopeAmount,
    attack,
    decay,
    sustain,
    release,
    glide,
    output,
    root,
    division,
    sequenceEnabled,
    bypass,
    pulseWidth,
    filterDrive,
    voiceMode,
    mainParameterCount
  };

  [[nodiscard]] static juce::AudioProcessorValueTreeState::ParameterLayout
  createParameterLayout();
  [[nodiscard]] Parameters currentParameters() const noexcept;
  [[nodiscard]] Transport currentTransport() const noexcept;
  [[nodiscard]] std::span<const MidiEvent> readMidi(const juce::MidiBuffer&,
                                                    int frames) noexcept;
  void publishMeters(const MeterSnapshot&) noexcept;

  std::array<std::atomic<float>*, 3> filterValues_{};
  aste::parameters::MaterialRevision materialRevision_;
  Processor processor_{};
  std::atomic<bool> legacyBehavior_{};
  juce::AudioProcessorValueTreeState state_;
  aste::ui::MasterGain masterGain_;
  std::array<std::atomic<float>*, mainParameterCount> mainValues_{};
  std::array<std::atomic<float>*, kStepCount> noteValues_{};
  std::array<std::atomic<float>*, kStepCount> gateValues_{};
  std::array<std::atomic<float>*, kStepCount> accentValues_{};
  std::array<std::atomic<float>*, kStepCount> slideValues_{};
  std::array<MidiEvent, 256> midiScratch_{};
  std::size_t midiCount_{};
  std::atomic<float> outputPeak_{};
  std::atomic<float> envelopeLevel_{};
  std::atomic<float> currentStep_{-1.0F};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SequenceAudioProcessor)
};

static_assert(std::atomic<float>::is_always_lock_free,
              "Sequence meter publication must remain lock-free");
static_assert(std::atomic<bool>::is_always_lock_free);

}  // namespace aste::sequence::plugin
