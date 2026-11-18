#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>
#include "tape_snapshots.hpp"

namespace aste::loop {

enum class Transport { Play, Stop, Release };

struct Parameters {
  bool capture{};
  bool reloop{};
  float overdub{0.5F};
  float feedback{0.85F};
  float loopLengthSeconds{2.0F};
  float start{0.0F};
  float speed{1.0F};
  bool reverse{};
  float pitchSemitones{};
  float splice{0.03F};
  float wow{0.08F};
  float flutter{0.03F};
  float drift{0.02F};
  float degradation{0.08F};
  float amplifier{0.25F};
  float tapeSpeed{1.0F};
  float mix{1.0F};
  float outputDb{-3.0F};
  bool bypass{};
  bool legacyBehavior{};  // Saved with the tape engine, not automated.
  Transport transport{Transport::Play};
  bool safeWorkflow{};
  float end{1};
  bool repeat{};
  float repeatSeconds{};  // Zero: use the selected region.
  bool monitor{};
};

struct MeterSnapshot {
  float inputPeak{};
  float outputPeak{};
  float position{};
  float captured{};
  float printing{};
  std::uint32_t generation{};
  std::uint32_t retainedGenerations{};
  std::size_t activeDeck{};
  std::uint32_t oldestGeneration{};
  std::uint32_t newestGeneration{};
  bool recording{}, busy{}, historyFull{};
};

class Processor {
 public:
  void prepare(double sampleRate, double maximumSeconds = 30.0,
               bool enableSnapshots = false) noexcept;
  void reset() noexcept;
  void clear() noexcept;
  void discard() noexcept;
  void previousGeneration() noexcept;
  void nextGeneration() noexcept;
  void selectGeneration(std::uint32_t generation) noexcept;
  void removeGeneration(std::uint32_t generation) noexcept;
  [[nodiscard]] TapeState tapeState() const { return snapshots_.read(); }
  void publishTape() noexcept;
  // Non-audio setup only: restore into a freshly prepared processor.
  void restoreTape(const TapeState&);
  void continuePlaybackFrom(const Processor&) noexcept;
  void process(float* left, float* right, std::size_t frames,
               const Parameters& parameters, bool publish = true) noexcept;

  [[nodiscard]] constexpr std::size_t latencySamples() const noexcept {
    return 0U;
  }
  [[nodiscard]] MeterSnapshot meters() const noexcept { return meters_; }
  [[nodiscard]] std::size_t capacitySamples() const noexcept {
    return decks_[0].left.size();
  }

 private:
  static constexpr std::size_t kDeckCount = 3U;
  TapeSnapshots snapshots_;
  struct Deck {
    std::vector<float> left{};
    std::vector<float> right{};
    std::size_t length{};
    std::uint32_t generation{};
  };

  [[nodiscard]] float read(const std::vector<float>& channel, double position,
                           std::size_t length) const noexcept;
  [[nodiscard]] float playback(const std::vector<float>& channel,
                               const Parameters& parameters,
                               std::size_t length) const noexcept;
  void beginReloop(std::size_t length, bool preserveHistory = false) noexcept;
  [[nodiscard]] float printSample(float input, float& state,
                                  const Parameters& parameters) const noexcept;
  [[nodiscard]] std::size_t retainedGenerations() const noexcept;

  double sampleRate_{48000.0};
  std::array<Deck, kDeckCount> decks_{};
  std::size_t activeDeck_{};
  std::size_t printingDeck_{};
  std::size_t writePosition_{};
  std::size_t printPosition_{};
  std::size_t printLength_{};
  std::uint32_t generationCounter_{};
  double readPosition_{};
  double modulationPhase_{};
  double pitchPhase_{};
  float driftState_{};
  float smoothingCoefficient_{1.0F};
  float smoothedMix_{1.0F};
  float smoothedOutputGain_{1.0F};
  float smoothedSpeed_{1.0F};
  float smoothedAmplifier_{};
  float smoothedDegradation_{};
  float printStateLeft_{};
  float printStateRight_{};
  bool smoothingInitialized_{};
  bool wasCapturing_{};
  bool printing_{};
  bool recording_{}, captureLatched_{}, wasRepeating_{};
  std::size_t captureLength_{};
  double repeatPosition_{};
  float toneLeft_{}, toneRight_{};
  Parameters previousParameters_{};
  Parameters repeatTimeline_{};
  float transitionLeft_{}, transitionRight_{};
  std::size_t transitionRemaining_{};
  Transport transport_{Transport::Play};
  float transportGain_{1.0F};
  float lastWetLeft_{}, lastWetRight_{};
  MeterSnapshot meters_{};
};

}  // namespace aste::loop
