#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace aste::field {

struct Parameters {
  bool forever{};
  float mass{0.62F};
  float grain{0.34F};
  float pitch{0.28F};
  float motion{0.24F};
  float distance{0.45F};
  float blend{0.48F};
  float outputDb{-3.0F};
  bool bypass{};
};

struct MeterSnapshot {
  float inputPeak{};
  float outputPeak{};
  float fieldEnergy{};
  float retention{};
};

class Processor {
 public:
  void prepare(double sampleRate) noexcept;
  void reset() noexcept;
  void noteOn(int midiNote, float velocity) noexcept;
  void process(float* left, float* right, std::size_t frames,
               const Parameters& parameters) noexcept;

  [[nodiscard]] constexpr std::size_t latencySamples() const noexcept {
    return 0U;
  }
  [[nodiscard]] MeterSnapshot meters() const noexcept { return meters_; }

 private:
  static constexpr std::size_t kLineCount = 8U;
  static constexpr std::size_t kDelayCapacity = 32768U;
  static constexpr std::size_t kPitchCapacity = 16384U;

  struct DelayLine {
    std::array<float, kDelayCapacity> samples{};
    std::size_t writePosition{};

    [[nodiscard]] float read(float delaySamples) const noexcept;
    void write(float sample) noexcept;
    void clear() noexcept;
  };

  struct PitchVoice {
    std::array<float, kPitchCapacity> samples{};
    std::size_t writePosition{};
    float phase{0.25F};

    [[nodiscard]] float process(float input, float ratio,
                                float windowSamples) noexcept;
    void clear() noexcept;
  };

  [[nodiscard]] float renderExciter() noexcept;
  [[nodiscard]] static std::uint32_t randomStep(std::uint32_t state) noexcept;

  double sampleRate_{48000.0};
  std::array<DelayLine, kLineCount> delays_{};
  std::array<float, kLineCount> baseDelaySamples_{};
  std::array<float, kLineCount> dampingState_{};
  std::array<float, kLineCount> grainTarget_{};
  std::array<float, kLineCount> grainState_{};
  std::array<float, kLineCount> lineOutput_{};
  std::array<float, kLineCount> feedback_{};
  std::array<PitchVoice, 2> pitchVoices_{};
  std::uint32_t randomState_{0x46a31d2bU};
  std::size_t grainCountdown_{};
  double motionPhase_{};
  double exciterPhase_{};
  double exciterIncrement_{};
  float exciterEnvelope_{};
  float exciterDecay_{0.99F};
  float inputFilter_{};
  float smoothedForever_{};
  float smoothedMass_{0.62F};
  float smoothedGrain_{0.34F};
  float smoothedPitch_{0.28F};
  float smoothedMotion_{0.24F};
  float smoothedDistance_{0.45F};
  float smoothedBlend_{0.48F};
  float smoothedOutputGain_{0.7079458F};
  float smoothingCoefficient_{1.0F};
  float energyState_{};
  bool smoothingInitialized_{};
  MeterSnapshot meters_{};
};

}  // namespace aste::field
