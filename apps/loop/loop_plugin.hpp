#pragma once

#include "loop_processor.hpp"

#include <juce_audio_utils/juce_audio_utils.h>

#include <array>
#include <atomic>
#include <mutex>
#include <limits>

namespace aste::loop::plugin {

class LoopAudioProcessor final : public juce::AudioProcessor {
 public:
  LoopAudioProcessor();
  ~LoopAudioProcessor() override;

  void prepareToPlay(double sampleRate, int maximumBlockSize) override;
  void releaseResources() override;
  void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
  void processBlockBypassed(juce::AudioBuffer<float>&,
                            juce::MidiBuffer&) override;
  [[nodiscard]] bool isBusesLayoutSupported(const BusesLayout&) const override;

  [[nodiscard]] juce::AudioProcessorEditor* createEditor() override;
  [[nodiscard]] bool hasEditor() const override { return true; }
  [[nodiscard]] const juce::String getName() const override {
    return "Loop L-01";
  }
  [[nodiscard]] bool acceptsMidi() const override { return true; }
  [[nodiscard]] bool producesMidi() const override { return false; }
  [[nodiscard]] bool isMidiEffect() const override { return false; }
  [[nodiscard]] double getTailLengthSeconds() const override {
    // Tape capacity is not a tail limit: playback loops until explicitly
    // cleared.
    return std::numeric_limits<double>::infinity();
  }
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
  [[nodiscard]] float loopPosition() const noexcept;
  [[nodiscard]] float capturedAmount() const noexcept;
  [[nodiscard]] float printingProgress() const noexcept;
  [[nodiscard]] int generation() const noexcept;
  [[nodiscard]] int retainedGenerations() const noexcept;
  [[nodiscard]] int activeDeck() const noexcept;
  [[nodiscard]] int oldestGeneration() const noexcept {
    return oldestGeneration_.load();
  }
  [[nodiscard]] int newestGeneration() const noexcept {
    return newestGeneration_.load();
  }
  [[nodiscard]] bool isBypassed() const noexcept { return bypassed_.load(); }
  [[nodiscard]] bool isCapturing() const noexcept {
    return captureActive_.load();
  }
  void clearLoop() noexcept;
  void reloop() noexcept;
  void previousGeneration() noexcept;
  void nextGeneration() noexcept;
  [[nodiscard]] static int factoryPresetCount() noexcept;
  [[nodiscard]] static juce::String factoryPresetName(int index);
  void loadFactoryPreset(int index);

 private:
  enum ParameterIndex : std::size_t {
    capture,
    overdub,
    feedback,
    sync,
    lengthBeats,
    freeLength,
    start,
    speed,
    reverse,
    pitch,
    splice,
    wow,
    flutter,
    drift,
    degradation,
    amplifier,
    tapeSpeed,
    mix,
    output,
    bypass,
    parameterCount
  };

  [[nodiscard]] static juce::AudioProcessorValueTreeState::ParameterLayout
  createParameterLayout();
  [[nodiscard]] Parameters currentParameters() const noexcept;
  void processRange(juce::AudioBuffer<float>&, int start, int frames,
                    Parameters) noexcept;
  void publishMeters(const MeterSnapshot&) noexcept;

  struct Engine {
    Processor processor;
    Engine* retiredNext{};
  };
  void adoptPending() noexcept;
  void collectRetired();
  std::atomic<Engine*> engine_{new Engine};
  std::atomic<Engine*> pending_{};
  std::atomic<Engine*> retired_{};
  std::mutex lifecycle_;  // Only prepare/save/restore; never acquired by audio.
  double hostRate_{48000};
  bool hostPrepared_{};  // Guarded by lifecycle_, like hostRate_.
  juce::AudioProcessorValueTreeState state_;
  std::array<std::atomic<float>*, parameterCount> parameterValues_{};
  bool midiCapture_{};
  std::atomic<bool> captureActive_{};
  std::atomic<bool> clearRequested_{};
  std::atomic<bool> reloopRequested_{};
  std::atomic<int> generationNavigation_{};
  std::atomic<float> inputPeak_{};
  std::atomic<float> outputPeak_{};
  std::atomic<float> loopPosition_{};
  std::atomic<float> capturedAmount_{};
  std::atomic<float> printingProgress_{};
  std::atomic<int> generation_{};
  std::atomic<int> retainedGenerations_{};
  std::atomic<int> activeDeck_{};
  std::atomic<int> oldestGeneration_{}, newestGeneration_{};
  std::atomic<bool> bypassed_{};

  JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(LoopAudioProcessor)
};

static_assert(std::atomic<float>::is_always_lock_free,
              "Loop meter publication must remain lock-free");
static_assert(std::atomic<bool>::is_always_lock_free,
              "Loop clear request must remain lock-free");
static_assert(std::atomic<int>::is_always_lock_free,
              "Loop generation requests must remain lock-free");
static_assert(std::atomic<void*>::is_always_lock_free,
              "Loop engine handoff must remain lock-free");

}  // namespace aste::loop::plugin
