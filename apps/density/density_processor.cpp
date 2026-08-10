#include "density_processor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace aste::density {
namespace {

constexpr float kPi = 3.14159265358979323846F;

float finiteOr(float value, float fallback) noexcept {
  return std::isfinite(value) ? value : fallback;
}

float bounded(float value, float low, float high, float fallback) noexcept {
  return std::clamp(finiteOr(value, fallback), low, high);
}

float dbToGain(float db) noexcept {
  return std::exp(db * 0.11512925464970229F);
}

float gainToDb(float gain) noexcept {
  return 8.6858896380650366F * std::log(std::max(gain, 1.0e-12F));
}

float cleanSample(float sample) noexcept {
  return std::clamp(finiteOr(sample, 0.0F), -16.0F, 16.0F);
}

float timeCoefficient(float milliseconds, double sampleRate) noexcept {
  const double seconds = static_cast<double>(milliseconds) * 0.001;
  return static_cast<float>(std::exp(-1.0 / (seconds * sampleRate)));
}

double besselI0(double value) noexcept {
  const double squared = 0.25 * value * value;
  double sum = 1.0;
  double term = 1.0;
  for (double order = 1.0; order <= 32.0; order += 1.0) {
    term *= squared / (order * order);
    sum += term;
    if (term < 1.0e-14 * sum) {
      break;
    }
  }
  return sum;
}

}  // namespace

float saturateSample(float sample, float drive) noexcept {
  sample = finiteOr(sample, 0.0F);
  drive = bounded(drive, 1.0e-3F, 16.0F, 1.0F);
  const float normalizer = std::tanh(drive);
  return std::tanh(sample * drive) / normalizer;
}

float controlledClipSample(float sample) noexcept {
  sample = finiteOr(sample, 0.0F);
  constexpr float knee = 0.9F;
  constexpr float end = 1.2F;
  const float magnitude = std::abs(sample);
  if (magnitude <= knee) {
    return sample;
  }
  if (magnitude >= end) {
    return std::copysign(1.0F, sample);
  }
  const float t = (magnitude - knee) / (end - knee);
  const float curved = knee + 0.1F * (1.0F - std::pow(1.0F - t, 3.0F));
  return std::copysign(curved, sample);
}

void CrushOversampler4x::prepare(std::size_t tapsPerPhase) noexcept {
  latency_ = std::clamp(tapsPerPhase, std::size_t{1}, std::size_t{64});
  tapCount_ = kFactor * latency_ + 1U;
  inputHistoryLength_ = latency_ + 1U;
  const std::size_t centre = tapCount_ / 2U;
  constexpr double cutoff = 0.5 / static_cast<double>(kFactor);
  coefficients_.fill(0.0F);
  double sum{};
  for (std::size_t tap = 0; tap < tapCount_; ++tap) {
    const double offset = static_cast<double>(tap) - centre;
    const double sinc =
        offset == 0.0
            ? 2.0 * cutoff
            : std::sin(2.0 * static_cast<double>(kPi) * cutoff * offset) /
                  (static_cast<double>(kPi) * offset);
    const double phase = 2.0 * static_cast<double>(kPi) * tap /
                         static_cast<double>(tapCount_ - 1U);
    const double blackman =
        0.42 - 0.5 * std::cos(phase) + 0.08 * std::cos(2.0 * phase);
    coefficients_[tap] = static_cast<float>(sinc * blackman);
    sum += coefficients_[tap];
  }
  for (std::size_t tap = 0; tap < tapCount_; ++tap) {
    coefficients_[tap] = static_cast<float>(coefficients_[tap] / sum);
  }
  reset();
}

void CrushOversampler4x::reset() noexcept {
  inputHistory_.fill(0.0F);
  outputHistory_.fill(0.0F);
  inputWrite_ = inputHistoryLength_ - 1U;
  outputWrite_ = tapCount_ - 1U;
}

float CrushOversampler4x::processSample(float input,
                                        float saturationDrive) noexcept {
  inputWrite_ = inputWrite_ + 1U == inputHistoryLength_ ? 0U : inputWrite_ + 1U;
  inputHistory_[inputWrite_] = finiteOr(input, 0.0F);
  float output{};
  for (std::size_t phase = 0; phase < kFactor; ++phase) {
    double interpolated{};
    std::size_t history = inputWrite_;
    for (std::size_t tap = phase; tap < tapCount_; tap += kFactor) {
      interpolated += inputHistory_[history] * coefficients_[tap];
      history = history == 0U ? inputHistoryLength_ - 1U : history - 1U;
    }
    const float shaped = controlledClipSample(saturateSample(
        static_cast<float>(kFactor * interpolated), saturationDrive));
    outputWrite_ = outputWrite_ + 1U == tapCount_ ? 0U : outputWrite_ + 1U;
    outputHistory_[outputWrite_] = shaped;
    if (phase == 0U) {
      double decimated{};
      std::size_t highRateHistory = outputWrite_;
      for (std::size_t tap = 0; tap < tapCount_; ++tap) {
        decimated += outputHistory_[highRateHistory] * coefficients_[tap];
        highRateHistory =
            highRateHistory == 0U ? tapCount_ - 1U : highRateHistory - 1U;
      }
      output = static_cast<float>(decimated);
    }
  }
  return std::isfinite(output) ? output : 0.0F;
}

void CrushOversampler4x::process(float* samples, std::size_t frames,
                                 float saturationDrive) noexcept {
  if (samples == nullptr) {
    return;
  }
  for (std::size_t sample = 0; sample < frames; ++sample) {
    samples[sample] = processSample(samples[sample], saturationDrive);
  }
}

void CrushOversampler4xHalfBand::Stage::prepare(std::size_t taps,
                                                float kaiserBeta) noexcept {
  taps = std::clamp(taps, std::size_t{5}, std::size_t{129});
  kaiserBeta = finiteOr(kaiserBeta, -1.0F);
  const double beta = std::clamp(static_cast<double>(kaiserBeta), 0.0, 16.0);
  const double kaiserDenominator = kaiserBeta >= 0.0F ? besselI0(beta) : 1.0;
  tapCount = taps - (taps - 1U) % 4U;
  centre = tapCount / 2U;
  oddCount = (tapCount - 1U) / 2U;
  oddCoefficients.fill(0.0F);
  constexpr double cutoff = 0.25;
  double sum{};
  for (std::size_t tap = 0; tap < tapCount; ++tap) {
    const double offset = static_cast<double>(tap) - centre;
    const double sinc =
        offset == 0.0
            ? 2.0 * cutoff
            : std::sin(2.0 * static_cast<double>(kPi) * cutoff * offset) /
                  (static_cast<double>(kPi) * offset);
    double window{};
    if (kaiserBeta >= 0.0F) {
      const double position =
          2.0 * static_cast<double>(tap) / static_cast<double>(tapCount - 1U) -
          1.0;
      window =
          besselI0(beta * std::sqrt(std::max(0.0, 1.0 - position * position))) /
          kaiserDenominator;
    } else {
      const double phase = 2.0 * static_cast<double>(kPi) * tap /
                           static_cast<double>(tapCount - 1U);
      window = 0.42 - 0.5 * std::cos(phase) + 0.08 * std::cos(2.0 * phase);
    }
    const float coefficient = static_cast<float>(sinc * window);
    if (tap == centre) {
      centreCoefficient = coefficient;
    } else if ((tap & 1U) != 0U) {
      oddCoefficients[tap / 2U] = coefficient;
    }
    sum += coefficient;
  }
  centreCoefficient = static_cast<float>(centreCoefficient / sum);
  for (std::size_t coefficient = 0; coefficient < oddCount; ++coefficient) {
    oddCoefficients[coefficient] =
        static_cast<float>(oddCoefficients[coefficient] / sum);
  }
  reset();
}

void CrushOversampler4xHalfBand::Stage::reset() noexcept {
  inputHistory.fill(0.0F);
  outputHistory.fill(0.0F);
  inputWrite = oddCount - 1U;
  outputWrite = tapCount - 1U;
}

std::array<float, 2> CrushOversampler4xHalfBand::Stage::interpolate(
    float input) noexcept {
  inputWrite = inputWrite + 1U == oddCount ? 0U : inputWrite + 1U;
  inputHistory[inputWrite] = finiteOr(input, 0.0F);
  const std::size_t centreDelay = centre / 2U;
  const std::size_t centreIndex = inputWrite >= centreDelay
                                      ? inputWrite - centreDelay
                                      : inputWrite + oddCount - centreDelay;
  double oddOutput{};
  std::size_t history = inputWrite;
  for (std::size_t coefficient = 0; coefficient < oddCount; ++coefficient) {
    oddOutput += inputHistory[history] * oddCoefficients[coefficient];
    history = history == 0U ? oddCount - 1U : history - 1U;
  }
  return {2.0F * centreCoefficient * inputHistory[centreIndex],
          static_cast<float>(2.0 * oddOutput)};
}

float CrushOversampler4xHalfBand::Stage::decimate(
    const std::array<float, 2>& input) noexcept {
  float result{};
  for (std::size_t phase = 0; phase < input.size(); ++phase) {
    outputWrite = outputWrite + 1U == tapCount ? 0U : outputWrite + 1U;
    outputHistory[outputWrite] = finiteOr(input[phase], 0.0F);
    if (phase == 0U) {
      const std::size_t centreIndex = outputWrite >= centre
                                          ? outputWrite - centre
                                          : outputWrite + tapCount - centre;
      double output = outputHistory[centreIndex] * centreCoefficient;
      std::size_t history =
          outputWrite == 0U ? tapCount - 1U : outputWrite - 1U;
      for (std::size_t coefficient = 0; coefficient < oddCount; ++coefficient) {
        output += outputHistory[history] * oddCoefficients[coefficient];
        history = history < 2U ? history + tapCount - 2U : history - 2U;
      }
      result = static_cast<float>(output);
    }
  }
  return result;
}

void CrushOversampler4xHalfBand::prepare(std::size_t firstStageTaps,
                                         std::size_t secondStageTaps,
                                         float firstStageKaiserBeta) noexcept {
  firstStage_.prepare(firstStageTaps, firstStageKaiserBeta);
  secondStage_.prepare(secondStageTaps);
  latency_ =
      (firstStage_.tapCount - 1U) / 2U + (secondStage_.tapCount - 1U) / 4U;
}

void CrushOversampler4xHalfBand::reset() noexcept {
  firstStage_.reset();
  secondStage_.reset();
}

float CrushOversampler4xHalfBand::processLinearSample(float input) noexcept {
  const auto firstHigh = firstStage_.interpolate(input);
  std::array<float, 2> secondLow{};
  for (std::size_t sample = 0; sample < firstHigh.size(); ++sample) {
    secondLow[sample] =
        secondStage_.decimate(secondStage_.interpolate(firstHigh[sample]));
  }
  const float output = firstStage_.decimate(secondLow);
  return std::isfinite(output) ? output : 0.0F;
}

float CrushOversampler4xHalfBand::processSample(
    float input, float saturationDrive) noexcept {
  const auto firstHigh = firstStage_.interpolate(input);
  std::array<float, 2> secondLow{};
  for (std::size_t sample = 0; sample < firstHigh.size(); ++sample) {
    auto secondHigh = secondStage_.interpolate(firstHigh[sample]);
    for (float& highSample : secondHigh) {
      highSample =
          controlledClipSample(saturateSample(highSample, saturationDrive));
    }
    secondLow[sample] = secondStage_.decimate(secondHigh);
  }
  const float output = firstStage_.decimate(secondLow);
  return std::isfinite(output) ? output : 0.0F;
}

void CrushOversampler4xHalfBand::process(float* samples, std::size_t frames,
                                         float saturationDrive) noexcept {
  if (samples == nullptr) {
    return;
  }
  for (std::size_t sample = 0; sample < frames; ++sample) {
    samples[sample] = processSample(samples[sample], saturationDrive);
  }
}

DensityMapping mapDensity(float density) noexcept {
  const float d = bounded(density, 0.0F, 1.0F, 0.5F);
  return {
      .thresholdDb = -2.0F - 28.0F * d,
      .ratio = 3.0F + 57.0F * d,
      .saturationDrive = 1.0F + 8.0F * d,
      .releaseCurve = 1.0F + 4.0F * d,
      .crushMakeupDb = 10.0F * d,
  };
}

float mapCrushSaturation(float density, float crush, float driveDb) noexcept {
  const auto mapping = mapDensity(density);
  crush = bounded(crush, 0.0F, 1.0F, 0.65F);
  driveDb = bounded(driveDb, -12.0F, 24.0F, 0.0F);
  const float pushed = 1.0F + 0.75F * std::max(driveDb, 0.0F) / 24.0F;
  return 1.0e-3F + crush * (mapping.saturationDrive * pushed - 1.0e-3F);
}

void Processor::Smoother::prepare(double sampleRate, double seconds,
                                  float initial) noexcept {
  coefficient_ = static_cast<float>(std::exp(-1.0 / (seconds * sampleRate)));
  reset(initial);
}

float Processor::Smoother::next() noexcept {
  current_ = target_ + coefficient_ * (current_ - target_);
  if (std::abs(current_ - target_) < 1.0e-8F) {
    current_ = target_;
  }
  return current_;
}

float Processor::HighPass::process(float input, float coefficient) noexcept {
  float output = coefficient * (previousOutput_ + input - previousInput_);
  previousInput_ = input;
  if (!std::isfinite(output) ||
      std::abs(output) < std::numeric_limits<float>::min()) {
    output = 0.0F;
  }
  previousOutput_ = output;
  return output;
}

void Processor::prepare(double sampleRate, const Parameters& initial) noexcept {
  oversamplingPrototype_ = false;
  driveCascade_ = true;
  attackSmoothing_ = true;
  attackCascade_ = false;
  blendCascade_ = true;
  sampleRate_ = std::clamp(finiteOr(static_cast<float>(sampleRate), 48000.0F),
                           8000.0F, 384000.0F);
  const float initialDrive = bounded(initial.driveDb, -12.0F, 24.0F, 0.0F);
  drive_.prepare(sampleRate_, 0.003, initialDrive);
  driveStage2_.prepare(sampleRate_, 0.003, initialDrive);
  const float initialAttack = bounded(initial.attackMs, 0.02F, 30.0F, 1.0F);
  attackLog_.prepare(sampleRate_, 0.005, std::log(initialAttack));
  crush_.prepare(sampleRate_, 0.010, bounded(initial.crush, 0.0F, 1.0F, 0.65F));
  density_.prepare(sampleRate_, 0.010,
                   bounded(initial.density, 0.0F, 1.0F, 0.5F));
  const float initialBlend = bounded(initial.blend, 0.0F, 1.0F, 0.5F);
  blend_.prepare(sampleRate_, 0.003, initialBlend);
  blendStage2_.prepare(sampleRate_, 0.003, initialBlend);
  stereoLink_.prepare(sampleRate_, 0.010,
                      bounded(initial.stereoLink, 0.0F, 1.0F, 1.0F));
  const float initialOutput = bounded(initial.outputDb, -24.0F, 12.0F, 0.0F);
  output_.prepare(sampleRate_, 0.003, initialOutput);
  outputStage2_.prepare(sampleRate_, 0.003, initialOutput);
  reset();
}

void Processor::prepareOversamplingPrototype(
    double sampleRate, const Parameters& initial) noexcept {
  prepare(sampleRate, initial);
  oversamplingPrototype_ = true;
  for (auto& oversampler : crushOversamplers_) {
    oversampler.prepare(73U, 33U, 5.0F);
  }
  reset();
}

void Processor::prepareDriveSmoothingPrototype(
    double sampleRate, double stageSeconds, bool cascade,
    const Parameters& initial) noexcept {
  prepare(sampleRate, initial);
  const float initialDrive = bounded(initial.driveDb, -12.0F, 24.0F, 0.0F);
  const double seconds = std::clamp(
      static_cast<double>(finiteOr(static_cast<float>(stageSeconds), 0.005F)),
      0.0001, 1.0);
  drive_.prepare(sampleRate_, seconds, initialDrive);
  driveStage2_.prepare(sampleRate_, seconds, initialDrive);
  driveCascade_ = cascade;
  reset();
}

void Processor::prepareAttackSmoothingPrototype(
    double sampleRate, double stageSeconds, bool cascade,
    const Parameters& initial) noexcept {
  prepare(sampleRate, initial);
  if (std::isfinite(stageSeconds) && stageSeconds <= 0.0) {
    attackSmoothing_ = false;
    reset();
    return;
  }
  const float initialAttack = bounded(initial.attackMs, 0.02F, 30.0F, 1.0F);
  const double seconds = std::clamp(
      static_cast<double>(finiteOr(static_cast<float>(stageSeconds), 0.005F)),
      0.0001, 1.0);
  const float initialLog = std::log(initialAttack);
  attackLog_.prepare(sampleRate_, seconds, initialLog);
  attackLogStage2_.prepare(sampleRate_, seconds, initialLog);
  attackSmoothing_ = true;
  attackCascade_ = cascade;
  reset();
}

void Processor::prepareBlendSmoothingPrototype(
    double sampleRate, double stageSeconds, bool cascade,
    const Parameters& initial) noexcept {
  prepare(sampleRate, initial);
  const float initialBlend = bounded(initial.blend, 0.0F, 1.0F, 0.5F);
  const double seconds = std::clamp(
      static_cast<double>(finiteOr(static_cast<float>(stageSeconds), 0.005F)),
      0.0001, 1.0);
  blend_.prepare(sampleRate_, seconds, initialBlend);
  blendStage2_.prepare(sampleRate_, seconds, initialBlend);
  blendCascade_ = cascade;
  reset();
}

void Processor::prepareAutomationReferencePrototype(
    double sampleRate, const Parameters& initial) noexcept {
  prepare(sampleRate, initial);
  const float initialDrive = bounded(initial.driveDb, -12.0F, 24.0F, 0.0F);
  drive_.prepare(sampleRate_, 0.005, initialDrive);
  driveCascade_ = false;
  attackSmoothing_ = false;
  const float initialBlend = bounded(initial.blend, 0.0F, 1.0F, 0.5F);
  blend_.prepare(sampleRate_, 0.005, initialBlend);
  blendCascade_ = false;
  reset();
}

void Processor::reset() noexcept {
  envelopes_ = {};
  for (auto& filter : detectorHpf_) {
    filter.reset();
  }
  for (auto& oversampler : crushOversamplers_) {
    oversampler.reset();
  }
  for (auto& delay : dryDelay_) {
    delay.fill(0.0F);
  }
  dryDelayWrite_ = 0U;
  meters_ = {};
}

void Processor::process(float* left, float* right, std::size_t frames,
                        const Parameters& parameters) noexcept {
  if (left == nullptr || frames == 0) {
    meters_ = {};
    return;
  }

  drive_.setTarget(bounded(parameters.driveDb, -12.0F, 24.0F, 0.0F));
  crush_.setTarget(bounded(parameters.crush, 0.0F, 1.0F, 0.65F));
  density_.setTarget(bounded(parameters.density, 0.0F, 1.0F, 0.5F));
  blend_.setTarget(bounded(parameters.blend, 0.0F, 1.0F, 0.5F));
  stereoLink_.setTarget(bounded(parameters.stereoLink, 0.0F, 1.0F, 1.0F));
  output_.setTarget(bounded(parameters.outputDb, -24.0F, 12.0F, 0.0F));

  const float attackMs = bounded(parameters.attackMs, 0.02F, 30.0F, 1.0F);
  const float releaseMs = bounded(parameters.releaseMs, 20.0F, 1200.0F, 180.0F);
  const float hpfHz = bounded(parameters.detectorHpfHz, 20.0F, 300.0F, 90.0F);
  const float blockAttackCoefficient =
      attackSmoothing_ ? 0.0F : timeCoefficient(attackMs, sampleRate_);
  if (attackSmoothing_) {
    attackLog_.setTarget(std::log(attackMs));
  }
  const float hpfCoefficient =
      std::exp(-2.0F * kPi * hpfHz / static_cast<float>(sampleRate_));
  meters_ = {};

  for (std::size_t i = 0; i < frames; ++i) {
    const float dryLeft = cleanSample(left[i]);
    const float dryRight = right == nullptr ? dryLeft : cleanSample(right[i]);
    float driveDb = drive_.next();
    if (driveCascade_) {
      driveStage2_.setTarget(driveDb);
      driveDb = driveStage2_.next();
    }
    const float driveGain = dbToGain(driveDb);
    const float drivenLeft = dryLeft * driveGain;
    const float drivenRight = dryRight * driveGain;
    const float detectorLeft =
        std::abs(detectorHpf_[0].process(drivenLeft, hpfCoefficient));
    const float detectorRight =
        std::abs(detectorHpf_[1].process(drivenRight, hpfCoefficient));
    const float linkedDetector = std::max(detectorLeft, detectorRight);
    const float stereoLink = stereoLink_.next();
    const std::array<float, 2> detectors{
        detectorLeft + stereoLink * (linkedDetector - detectorLeft),
        detectorRight + stereoLink * (linkedDetector - detectorRight)};
    const float density = density_.next();
    const float crush = crush_.next();
    const DensityMapping mapping = mapDensity(density);

    const float threshold = mapping.thresholdDb + 12.0F * (1.0F - crush);
    const float ratio = 1.0F + crush * (mapping.ratio - 1.0F);
    float attackCoefficient = blockAttackCoefficient;
    if (attackSmoothing_) {
      float attackLog = attackLog_.next();
      if (attackCascade_) {
        attackLogStage2_.setTarget(attackLog);
        attackLog = attackLogStage2_.next();
      }
      attackCoefficient = timeCoefficient(std::exp(attackLog), sampleRate_);
    }
    std::array<float, 2> reductionDb{};
    std::array<float, 2> compressedGain{};
    for (std::size_t channel = 0; channel < envelopes_.size(); ++channel) {
      const float detector = detectors[channel];
      float& envelope = envelopes_[channel];
      if (detector > envelope) {
        envelope = detector + attackCoefficient * (envelope - detector);
      } else {
        const float programme = 1.0F + (mapping.releaseCurve - 1.0F) *
                                           std::clamp(envelope, 0.0F, 1.0F);
        const float releaseCoefficient =
            timeCoefficient(releaseMs * programme, sampleRate_);
        envelope = detector + releaseCoefficient * (envelope - detector);
      }
      if (!std::isfinite(envelope) ||
          envelope < std::numeric_limits<float>::min()) {
        envelope = 0.0F;
      }
      const float overDb = std::max(0.0F, gainToDb(envelope) - threshold);
      reductionDb[channel] = overDb * (1.0F - 1.0F / ratio);
      compressedGain[channel] = dbToGain(-reductionDb[channel]);
    }
    const float makeupGain = dbToGain(mapping.crushMakeupDb * crush);
    const float saturationDrive = mapCrushSaturation(density, crush, driveDb);
    float crushedLeft = drivenLeft * compressedGain[0] * makeupGain;
    float crushedRight = drivenRight * compressedGain[1] * makeupGain;
    float alignedDryLeft = dryLeft;
    float alignedDryRight = dryRight;
    if (oversamplingPrototype_) {
      crushedLeft =
          crushOversamplers_[0].processSample(crushedLeft, saturationDrive);
      crushedRight =
          crushOversamplers_[1].processSample(crushedRight, saturationDrive);
      alignedDryLeft = dryDelay_[0][dryDelayWrite_];
      alignedDryRight = dryDelay_[1][dryDelayWrite_];
      dryDelay_[0][dryDelayWrite_] = dryLeft;
      dryDelay_[1][dryDelayWrite_] = dryRight;
      dryDelayWrite_ =
          dryDelayWrite_ + 1U == dryDelay_[0].size() ? 0U : dryDelayWrite_ + 1U;
    } else {
      crushedLeft =
          controlledClipSample(saturateSample(crushedLeft, saturationDrive));
      crushedRight =
          controlledClipSample(saturateSample(crushedRight, saturationDrive));
    }

    float blend = blend_.next();
    if (blendCascade_) {
      blendStage2_.setTarget(blend);
      blend = blendStage2_.next();
    }
    outputStage2_.setTarget(output_.next());
    const float outputGain = dbToGain(outputStage2_.next());
    float outputLeft =
        ((1.0F - blend) * alignedDryLeft + blend * crushedLeft) * outputGain;
    float outputRight =
        ((1.0F - blend) * alignedDryRight + blend * crushedRight) * outputGain;
    if (parameters.protection) {
      outputLeft = controlledClipSample(outputLeft);
      outputRight = controlledClipSample(outputRight);
    }
    left[i] = cleanSample(outputLeft);
    if (right != nullptr) {
      right[i] = cleanSample(outputRight);
    }

    meters_.inputPeak = std::max(
        meters_.inputPeak, std::max(std::abs(dryLeft), std::abs(dryRight)));
    meters_.outputPeak = std::max(
        meters_.outputPeak, std::max(std::abs(left[i]), std::abs(outputRight)));
    meters_.gainReductionDb = std::max(
        meters_.gainReductionDb, std::max(reductionDb[0], reductionDb[1]));
  }
}

}  // namespace aste::density
