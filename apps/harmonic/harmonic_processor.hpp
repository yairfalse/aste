#pragma once

#include <array>
#include <cstddef>

namespace aste::harmonic {

struct Parameters {
  float inputDb{};
  float foundationGainDb{};
  float foundationFrequencyHz{80.0F};
  float bodyGainDb{};
  float bodyFrequencyHz{400.0F};
  float presenceGainDb{};
  float presenceFrequencyHz{2500.0F};
  float airGainDb{};
  float airFrequencyHz{12000.0F};
  float harmonic{0.35F};
  float outputDb{};
};

struct MeterSnapshot {
  float inputPeak{};
  float outputPeak{};
  float harmonicActivity{};
};

class Processor {
 public:
  void prepare(double sampleRate, const Parameters& initial = {}) noexcept;
  void reset() noexcept;
  void process(float* left, float* right, std::size_t frames,
               const Parameters& parameters) noexcept;

  [[nodiscard]] constexpr std::size_t latencySamples() const noexcept {
    return 0U;
  }
  [[nodiscard]] MeterSnapshot meters() const noexcept { return meters_; }

 private:
  struct Smoother {
    void prepare(double sampleRate, double seconds, float initial) noexcept;
    void setTarget(float target) noexcept { target_ = target; }
    [[nodiscard]] float next() noexcept;
    void reset() noexcept { current_ = target_; }

    float current_{};
    float target_{};
    float coefficient_{};
  };

  struct PeakFilter {
    void prepare(double sampleRate, float frequency, float q,
                 float gainDb) noexcept;
    void setTarget(double sampleRate, float frequency, float q,
                   float gainDb) noexcept;
    [[nodiscard]] float process(float input) noexcept;
    void reset() noexcept;

    struct Coefficients {
      float b0{1.0F};
      float b1{};
      float b2{};
      float a1{};
      float a2{};
    } current_{}, target_{};
    float smoothing_{};
    float z1_{};
    float z2_{};
  };

  struct StateVariableBand {
    void prepare(double sampleRate, float frequency, float q) noexcept;
    void setTarget(double sampleRate, float frequency, float q) noexcept;
    [[nodiscard]] float process(float input, float amount) noexcept;
    void reset() noexcept;

    struct Coefficients {
      float g{};
      float a1{};
      float a2{};
    } current_{}, target_{};
    float smoothing_{};
    float state1_{};
    float state2_{};
  };

  struct Band {
    void prepare(double sampleRate, float frequency, float gainDb) noexcept;
    void setTarget(double sampleRate, float frequency, float gainDb) noexcept;
    [[nodiscard]] float process(float input, float harmonic,
                                float& activity) noexcept;
    void reset() noexcept;

    PeakFilter contour_{};
    StateVariableBand reference_{};
    StateVariableBand nonlinear_{};
    Smoother boost_{};
  };

  [[nodiscard]] static float proportionalQ(float gainDb) noexcept;

  double sampleRate_{48000.0};
  Smoother input_{};
  Smoother harmonic_{};
  Smoother output_{};
  std::array<std::array<Band, 4>, 2> bands_{};
  MeterSnapshot meters_{};
};

}  // namespace aste::harmonic
