#pragma once

#include <array>
#include <cstddef>

namespace aste::density {

struct Parameters {
  float driveDb{0.0F};
  float crush{0.65F};
  float attackMs{1.0F};
  float releaseMs{180.0F};
  float density{0.5F};
  float blend{0.5F};
  float stereoLink{1.0F};
  float outputDb{0.0F};
  float detectorHpfHz{90.0F};
  bool protection{true};
};

struct DensityMapping {
  float thresholdDb;
  float ratio;
  float saturationDrive;
  float releaseCurve;
  float crushMakeupDb;
};

struct MeterSnapshot {
  float inputPeak{};
  float outputPeak{};
  float gainReductionDb{};
};

[[nodiscard]] DensityMapping mapDensity(float density) noexcept;

class Processor {
 public:
  void prepare(double sampleRate, const Parameters& initial = {}) noexcept;
  void reset() noexcept;
  void process(float* left, float* right, std::size_t frames,
               const Parameters& parameters) noexcept;

  [[nodiscard]] constexpr std::size_t latencySamples() const noexcept { return 0; }
  [[nodiscard]] MeterSnapshot meters() const noexcept { return meters_; }

 private:
  struct Smoother {
    void prepare(double sampleRate, double seconds, float initial) noexcept;
    void setTarget(float target) noexcept { target_ = target; }
    [[nodiscard]] float next() noexcept;
    void reset(float value) noexcept { current_ = target_ = value; }

    float current_{};
    float target_{};
    float coefficient_{};
  };

  struct HighPass {
    [[nodiscard]] float process(float input, float coefficient) noexcept;
    void reset() noexcept { previousInput_ = previousOutput_ = 0.0F; }

    float previousInput_{};
    float previousOutput_{};
  };

  double sampleRate_{48000.0};
  std::array<float, 2> envelopes_{};
  std::array<HighPass, 2> detectorHpf_{};
  Smoother drive_{};
  Smoother crush_{};
  Smoother density_{};
  Smoother blend_{};
  Smoother stereoLink_{};
  Smoother output_{};
  MeterSnapshot meters_{};
};

}  // namespace aste::density
