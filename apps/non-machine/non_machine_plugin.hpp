#pragma once
#include "experimental_instrument.hpp"
#include "non_machine_processor.hpp"

namespace aste::nonmachine::plugin {
class NonMachineAudioProcessor final : public aste::experimental::Instrument {
 public:
  enum Global {
    sequence,
    seed,
    phrase,
    voidStart,
    voidLength,
    timeMode,
    fragment,
    lookback,
    hard,
    hold,
    reverse,
    globalCount
  };
  static constexpr std::size_t trackBase = globalCount,
                               eventBase = trackBase + 12;
  static constexpr std::size_t fields = 16;
  NonMachineAudioProcessor();
  juce::AudioProcessorEditor* createEditor() override;
  void audition(std::size_t voice, std::size_t event = 0) noexcept {
    if (voice < 3 && event < 8) auditions_.fetch_or(1U << (voice * 8 + event));
  }
  void clear();
  void rejoin() noexcept { rejoin_.store(true); }
  void scene(int index);
  void emptyPattern();
  void lockHitSound(std::size_t voice, std::size_t event, bool enabled);
  void applyHitSoundToObject(std::size_t voice, std::size_t event);
  void generateRhythm(bool advanceSeed = true);
  bool limitedRepeats() const noexcept { return limited_.load(); }
  unsigned heldTargets() const noexcept { return held_.load(); }
  float phrasePosition() const noexcept { return phrase_.load(); }
  bool invalidPattern() const noexcept { return invalid_.load(); }
  bool waiting() const noexcept { return waiting_.load(); }
  bool running() const noexcept { return running_.load(); }
  static juce::String eventId(std::size_t voice, std::size_t event,
                              const char* field);
  static juce::String trackId(std::size_t voice, const char* field);
  static const std::array<const char*, fields> fieldIds;
  static constexpr std::size_t materialBase = eventBase + 3 * 8 * fields;
  static constexpr std::size_t materialIndex(std::size_t voice) {
    return materialBase + voice * 5;
  }
  static constexpr std::size_t lockBase = materialBase + 15;
  bool hitLocked(std::size_t voice, std::size_t event) const noexcept {
    return event > 0 && event < 8 && voice < 3 &&
           value(lockBase + voice * 7 + event - 1) >= .5F;
  }
  unsigned soundOwner() const noexcept { return soundOwner_.load(); }
  unsigned ignoredMidi() const noexcept { return ignoredMidi_.load(); }
  static constexpr std::size_t eventIndex(std::size_t voice,
                                          std::size_t event) {
    return eventBase + (voice * 8 + event) * fields;
  }

 private:
  static std::vector<aste::experimental::ControlSpec> controls();
  Pattern pattern() const noexcept;
  bool prepareEngine(double, int) override;
  void render(juce::AudioBuffer<float>&,
              const juce::MidiBuffer&) noexcept override;
  void onRestore() noexcept override;
  Engine engine_, preview_;
  std::size_t previewRemaining_{}, previewElapsed_{};
  std::int64_t previewSample_{};
  std::atomic<unsigned> soundOwner_{}, ignoredMidi_{};
  std::int64_t samples_{};
  bool wasRunning_{}, reversed_{};
  std::array<std::size_t, 4> capturedLength_{};
  std::atomic<unsigned> auditions_{}, held_{};
  std::atomic<bool> editing_{}, limited_{};
  std::atomic<bool> rejoin_{}, invalid_{}, waiting_{}, running_{};
  std::atomic<float> phrase_{};
};
}  // namespace aste::nonmachine::plugin
