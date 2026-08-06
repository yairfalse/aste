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

float dbToGain(float db) noexcept { return std::exp(db * 0.11512925464970229F); }

float gainToDb(float gain) noexcept {
  return 8.6858896380650366F * std::log(std::max(gain, 1.0e-12F));
}

float cleanSample(float sample) noexcept {
  return std::clamp(finiteOr(sample, 0.0F), -16.0F, 16.0F);
}

float saturate(float sample, float drive) noexcept {
  const float normalizer = std::tanh(drive);
  return std::tanh(sample * drive) / normalizer;
}

float controlledClip(float sample) noexcept {
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

float timeCoefficient(float milliseconds, double sampleRate) noexcept {
  const double seconds = static_cast<double>(milliseconds) * 0.001;
  return static_cast<float>(std::exp(-1.0 / (seconds * sampleRate)));
}

}  // namespace

DensityMapping mapDensity(float density) noexcept {
  const float d = bounded(density, 0.0F, 1.0F, 0.5F);
  return {
      .thresholdDb = -3.0F - 21.0F * d,
      .ratio = 4.0F + 36.0F * d,
      .saturationDrive = 1.0F + 4.0F * d,
      .releaseCurve = 1.0F + 2.0F * d,
      .crushMakeupDb = 9.0F * d,
  };
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
  if (!std::isfinite(output) || std::abs(output) < std::numeric_limits<float>::min()) {
    output = 0.0F;
  }
  previousOutput_ = output;
  return output;
}

void Processor::prepare(double sampleRate, const Parameters& initial) noexcept {
  sampleRate_ = std::clamp(finiteOr(static_cast<float>(sampleRate), 48000.0F),
                           8000.0F, 384000.0F);
  drive_.prepare(sampleRate_, 0.005, bounded(initial.driveDb, -12.0F, 24.0F, 0.0F));
  crush_.prepare(sampleRate_, 0.010, bounded(initial.crush, 0.0F, 1.0F, 0.65F));
  density_.prepare(sampleRate_, 0.010, bounded(initial.density, 0.0F, 1.0F, 0.5F));
  blend_.prepare(sampleRate_, 0.005, bounded(initial.blend, 0.0F, 1.0F, 0.5F));
  stereoLink_.prepare(sampleRate_, 0.010,
                      bounded(initial.stereoLink, 0.0F, 1.0F, 1.0F));
  output_.prepare(sampleRate_, 0.005, bounded(initial.outputDb, -24.0F, 12.0F, 0.0F));
  reset();
}

void Processor::reset() noexcept {
  envelopes_ = {};
  for (auto& filter : detectorHpf_) {
    filter.reset();
  }
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
  const float attackCoefficient = timeCoefficient(attackMs, sampleRate_);
  const float hpfCoefficient =
      std::exp(-2.0F * kPi * hpfHz / static_cast<float>(sampleRate_));
  meters_ = {};

  for (std::size_t i = 0; i < frames; ++i) {
    const float dryLeft = cleanSample(left[i]);
    const float dryRight = right == nullptr ? dryLeft : cleanSample(right[i]);
    const float driveGain = dbToGain(drive_.next());
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
    float crushedLeft = saturate(drivenLeft * compressedGain[0] * makeupGain,
                                 mapping.saturationDrive);
    float crushedRight = saturate(drivenRight * compressedGain[1] * makeupGain,
                                  mapping.saturationDrive);
    crushedLeft = controlledClip(crushedLeft);
    crushedRight = controlledClip(crushedRight);

    const float blend = blend_.next();
    const float outputGain = dbToGain(output_.next());
    float outputLeft = ((1.0F - blend) * dryLeft + blend * crushedLeft) * outputGain;
    float outputRight = ((1.0F - blend) * dryRight + blend * crushedRight) * outputGain;
    if (parameters.protection) {
      outputLeft = controlledClip(outputLeft);
      outputRight = controlledClip(outputRight);
    }
    left[i] = cleanSample(outputLeft);
    if (right != nullptr) {
      right[i] = cleanSample(outputRight);
    }

    meters_.inputPeak = std::max(meters_.inputPeak, std::max(std::abs(dryLeft),
                                                             std::abs(dryRight)));
    meters_.outputPeak = std::max(meters_.outputPeak, std::max(std::abs(left[i]),
                                                               std::abs(outputRight)));
    meters_.gainReductionDb =
        std::max(meters_.gainReductionDb, std::max(reductionDb[0], reductionDb[1]));
  }
}

}  // namespace aste::density
