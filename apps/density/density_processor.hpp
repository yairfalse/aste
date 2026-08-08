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
[[nodiscard]] float saturateSample(float sample, float drive) noexcept;
[[nodiscard]] float controlledClipSample(float sample) noexcept;

class CrushOversampler4x {
 public:
  void prepare(std::size_t tapsPerPhase = 64U) noexcept;
  void reset() noexcept;
  [[nodiscard]] float processSample(float input, float saturationDrive) noexcept;
  void process(float* samples, std::size_t frames,
               float saturationDrive) noexcept;

  [[nodiscard]] std::size_t latencySamples() const noexcept { return latency_; }

 private:
  static constexpr std::size_t kFactor = 4U;
  static constexpr std::size_t kMaximumTaps = 257U;
  static constexpr std::size_t kMaximumInputHistory = 65U;

  std::array<float, kMaximumTaps> coefficients_{};
  std::array<float, kMaximumInputHistory> inputHistory_{};
  std::array<float, kMaximumTaps> outputHistory_{};
  std::size_t tapCount_{kMaximumTaps};
  std::size_t inputHistoryLength_{kMaximumInputHistory};
  std::size_t latency_{64U};
  std::size_t inputWrite_{kMaximumInputHistory - 1U};
  std::size_t outputWrite_{kMaximumTaps - 1U};
};

class CrushOversampler4xHalfBand {
 public:
  void prepare(std::size_t firstStageTaps = 97U,
               std::size_t secondStageTaps = 33U,
               float firstStageKaiserBeta = -1.0F) noexcept;
  void reset() noexcept;
  [[nodiscard]] float processLinearSample(float input) noexcept;
  [[nodiscard]] float processSample(float input, float saturationDrive) noexcept;
  void process(float* samples, std::size_t frames,
               float saturationDrive) noexcept;

  [[nodiscard]] std::size_t latencySamples() const noexcept { return latency_; }

 private:
  struct Stage {
    void prepare(std::size_t taps, float kaiserBeta = -1.0F) noexcept;
    void reset() noexcept;
    [[nodiscard]] std::array<float, 2> interpolate(float input) noexcept;
    [[nodiscard]] float decimate(const std::array<float, 2>& input) noexcept;

    std::array<float, 64> oddCoefficients{};
    std::array<float, 64> inputHistory{};
    std::array<float, 129> outputHistory{};
    float centreCoefficient{};
    std::size_t oddCount{16U};
    std::size_t tapCount{33U};
    std::size_t centre{16U};
    std::size_t inputWrite{15U};
    std::size_t outputWrite{32U};
  };

  Stage firstStage_{};
  Stage secondStage_{};
  std::size_t latency_{56U};
};

class Processor {
 public:
  void prepare(double sampleRate, const Parameters& initial = {}) noexcept;
  void prepareOversamplingPrototype(double sampleRate,
                                    const Parameters& initial = {}) noexcept;
  void prepareDriveSmoothingPrototype(
      double sampleRate, double stageSeconds, bool cascade,
      const Parameters& initial = {}) noexcept;
  void prepareAttackSmoothingPrototype(
      double sampleRate, double stageSeconds, bool cascade,
      const Parameters& initial = {}) noexcept;
  void prepareBlendSmoothingPrototype(
      double sampleRate, double stageSeconds, bool cascade,
      const Parameters& initial = {}) noexcept;
  void prepareAutomationReferencePrototype(
      double sampleRate, const Parameters& initial = {}) noexcept;
  void reset() noexcept;
  void process(float* left, float* right, std::size_t frames,
               const Parameters& parameters) noexcept;

  [[nodiscard]] std::size_t latencySamples() const noexcept {
    return oversamplingPrototype_ ? 44U : 0U;
  }
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
  Smoother driveStage2_{};
  Smoother attackLog_{};
  Smoother attackLogStage2_{};
  Smoother crush_{};
  Smoother density_{};
  Smoother blend_{};
  Smoother blendStage2_{};
  Smoother stereoLink_{};
  Smoother output_{};
  Smoother outputStage2_{};
  std::array<CrushOversampler4xHalfBand, 2> crushOversamplers_{};
  std::array<std::array<float, 44>, 2> dryDelay_{};
  std::size_t dryDelayWrite_{};
  bool oversamplingPrototype_{};
  bool driveCascade_{};
  bool attackSmoothing_{};
  bool attackCascade_{};
  bool blendCascade_{};
  MeterSnapshot meters_{};
};

}  // namespace aste::density
