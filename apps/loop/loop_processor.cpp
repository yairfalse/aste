#include "loop_processor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace aste::loop {
namespace {

float finiteOr(float value, float fallback) noexcept {
  return std::isfinite(value) ? value : fallback;
}

float bounded(float value, float low, float high, float fallback) noexcept {
  return std::clamp(finiteOr(value, fallback), low, high);
}

float clean(float value) noexcept {
  if (!std::isfinite(value) ||
      std::abs(value) < std::numeric_limits<float>::min()) {
    return 0.0F;
  }
  return std::clamp(value, -8.0F, 8.0F);
}

float dbToGain(float db) noexcept {
  return std::exp(bounded(db, -24.0F, 12.0F, -3.0F) * 0.11512925464970229F);
}

double wrap(double position, std::size_t length) noexcept {
  if (length == 0U || !std::isfinite(position)) {
    return 0.0;
  }
  const double size = static_cast<double>(length);
  position = std::fmod(position, size);
  return position < 0.0 ? position + size : position;
}

}  // namespace

void Processor::prepare(double sampleRate, double maximumSeconds) noexcept {
  sampleRate_ = std::clamp(std::isfinite(sampleRate) ? sampleRate : 48000.0,
                           8000.0, 384000.0);
  maximumSeconds = std::clamp(
      std::isfinite(maximumSeconds) ? maximumSeconds : 30.0, 1.0, 120.0);
  const auto capacity =
      static_cast<std::size_t>(std::ceil(sampleRate_ * maximumSeconds));
  try {
    left_.assign(capacity, 0.0F);
    right_.assign(capacity, 0.0F);
  } catch (...) {
    left_.clear();
    right_.clear();
  }
  smoothingCoefficient_ =
      static_cast<float>(1.0 - std::exp(-1.0 / (0.005 * sampleRate_)));
  reset();
}

void Processor::reset() noexcept {
  writePosition_ = 0U;
  capturedSamples_ = 0U;
  readPosition_ = 0.0;
  modulationPhase_ = 0.0;
  pitchPhase_ = 0.0;
  driftState_ = 0.0F;
  smoothingInitialized_ = false;
  wasCapturing_ = false;
  meters_ = {};
}

void Processor::clear() noexcept {
  std::fill(left_.begin(), left_.end(), 0.0F);
  std::fill(right_.begin(), right_.end(), 0.0F);
  reset();
}

void Processor::discard() noexcept { reset(); }

float Processor::read(const std::vector<float>& channel, double position,
                      std::size_t length) const noexcept {
  if (length == 0U || channel.size() < length) {
    return 0.0F;
  }
  position = wrap(position, length);
  const auto first = static_cast<std::size_t>(position);
  const auto second = (first + 1U) % length;
  const float fraction = static_cast<float>(position - first);
  return clean(channel[first] + fraction * (channel[second] - channel[first]));
}

float Processor::playback(const std::vector<float>& channel,
                          const Parameters& parameters,
                          std::size_t length) const noexcept {
  const double start = bounded(parameters.start, 0.0F, 1.0F, 0.0F) * length;
  const double spliceSamples = std::max(
      1.0, static_cast<double>(bounded(parameters.splice, 0.0F, 0.25F, 0.03F)) *
               static_cast<double>(length));
  const auto spliced = [&](double position) {
    const double wrapped = wrap(position + start, length);
    const float primary = read(channel, wrapped, length);
    if (wrapped < static_cast<double>(length) - spliceSamples) {
      return primary;
    }
    const float fade = static_cast<float>(
        (wrapped - (static_cast<double>(length) - spliceSamples)) /
        spliceSamples);
    const float beginning =
        read(channel, wrapped - (static_cast<double>(length) - spliceSamples),
             length);
    return primary + fade * (beginning - primary);
  };
  const float pitch = bounded(parameters.pitchSemitones, -12.0F, 12.0F, 0.0F);
  if (std::abs(pitch) < 0.001F) {
    return spliced(readPosition_);
  }
  const double ratio = std::exp2(static_cast<double>(pitch) / 12.0);
  const double grain = std::min(1024.0, static_cast<double>(length) * 0.5);
  const double direction = ratio >= 1.0 ? 1.0 : -1.0;
  const auto head = [&](double phase) {
    phase -= std::floor(phase);
    const double window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * phase);
    const double offset = direction * phase * grain;
    return static_cast<float>(window) * spliced(readPosition_ + offset);
  };
  const float first = head(pitchPhase_);
  const float second = head(pitchPhase_ + 0.5);
  return clean(first + second);
}

void Processor::process(float* left, float* right, std::size_t frames,
                        const Parameters& parameters) noexcept {
  if (left == nullptr || frames == 0U || left_.empty()) {
    meters_ = {};
    return;
  }
  const std::size_t requested = static_cast<std::size_t>(
      std::clamp(static_cast<double>(bounded(parameters.loopLengthSeconds,
                                             0.05F, 30.0F, 2.0F)) *
                     sampleRate_,
                 64.0, static_cast<double>(left_.size())));
  const std::size_t length = std::max<std::size_t>(64U, requested);
  const bool stereo = right != nullptr;
  const float overdub = bounded(parameters.overdub, 0.0F, 1.0F, 0.5F);
  const float feedback = bounded(parameters.feedback, 0.0F, 1.0F, 0.85F);
  const float targetMix = bounded(parameters.mix, 0.0F, 1.0F, 1.0F);
  const float targetAmplifier =
      bounded(parameters.amplifier, 0.0F, 1.0F, 0.25F);
  const float targetDegradation =
      bounded(parameters.degradation, 0.0F, 1.0F, 0.08F);
  const float targetOutputGain = dbToGain(parameters.outputDb);
  const float targetSpeed = bounded(parameters.speed, 0.125F, 4.0F, 1.0F);
  if (!smoothingInitialized_) {
    smoothedMix_ = targetMix;
    smoothedAmplifier_ = targetAmplifier;
    smoothedDegradation_ = targetDegradation;
    smoothedOutputGain_ = targetOutputGain;
    smoothedSpeed_ = targetSpeed;
    smoothingInitialized_ = true;
  }
  if (parameters.capture && !wasCapturing_ && capturedSamples_ == 0U) {
    writePosition_ = 0U;
    readPosition_ = 0.0;
  }
  MeterSnapshot measured{};
  for (std::size_t sample = 0; sample < frames; ++sample) {
    const auto smooth = [this](float& state, float target) {
      state += smoothingCoefficient_ * (target - state);
    };
    smooth(smoothedMix_, targetMix);
    smooth(smoothedAmplifier_, targetAmplifier);
    smooth(smoothedDegradation_, targetDegradation);
    smooth(smoothedOutputGain_, targetOutputGain);
    smooth(smoothedSpeed_, targetSpeed);
    const float inputLeft = clean(left[sample]);
    const float inputRight = stereo ? clean(right[sample]) : inputLeft;
    measured.inputPeak =
        std::max(measured.inputPeak,
                 std::max(std::abs(inputLeft), std::abs(inputRight)));
    float wetLeft =
        capturedSamples_ > 1U
            ? playback(left_, parameters, std::min(length, capturedSamples_))
            : 0.0F;
    float wetRight =
        capturedSamples_ > 1U
            ? playback(right_, parameters, std::min(length, capturedSamples_))
            : 0.0F;
    if (capturedSamples_ < 2U) {
      wetLeft = inputLeft;
      wetRight = inputRight;
    }
    if (parameters.capture) {
      const float existingLeft = left_[writePosition_];
      const float existingRight = right_[writePosition_];
      left_[writePosition_] = clean(inputLeft * (1.0F - overdub) +
                                    existingLeft * feedback * overdub);
      right_[writePosition_] = clean(inputRight * (1.0F - overdub) +
                                     existingRight * feedback * overdub);
      writePosition_ = (writePosition_ + 1U) % length;
      capturedSamples_ = std::min(length, capturedSamples_ + 1U);
    }
    const float drive = 1.0F + 4.0F * smoothedAmplifier_;
    wetLeft = std::tanh(drive * wetLeft) / drive;
    wetRight = std::tanh(drive * wetRight) / drive;
    const float quantization = 32768.0F / (1.0F + 63.0F * smoothedDegradation_);
    wetLeft = std::round(wetLeft * quantization) / quantization;
    wetRight = std::round(wetRight * quantization) / quantization;
    float outputLeft = inputLeft + smoothedMix_ * (wetLeft - inputLeft);
    float outputRight = inputRight + smoothedMix_ * (wetRight - inputRight);
    outputLeft = clean(outputLeft * smoothedOutputGain_);
    outputRight = clean(outputRight * smoothedOutputGain_);
    if (parameters.bypass) {
      outputLeft = inputLeft;
      outputRight = inputRight;
    }
    left[sample] = outputLeft;
    if (stereo) {
      right[sample] = outputRight;
    }
    measured.outputPeak =
        std::max(measured.outputPeak,
                 std::max(std::abs(outputLeft), std::abs(outputRight)));
    modulationPhase_ += 1.0 / sampleRate_;
    modulationPhase_ -= std::floor(modulationPhase_);
    const float wow = bounded(parameters.wow, 0.0F, 1.0F, 0.08F);
    const float flutter = bounded(parameters.flutter, 0.0F, 1.0F, 0.03F);
    const float drift = bounded(parameters.drift, 0.0F, 1.0F, 0.02F);
    driftState_ = clean(
        0.99997F * driftState_ +
        0.00003F * std::sin(static_cast<float>(2.0 * std::numbers::pi *
                                               modulationPhase_ * 0.173)));
    const double modulation =
        1.0 +
        0.012 * wow *
            std::sin(2.0 * std::numbers::pi * modulationPhase_ * 0.47) +
        0.002 * flutter *
            std::sin(2.0 * std::numbers::pi * modulationPhase_ * 6.71) +
        0.006 * drift * driftState_;
    double speed = smoothedSpeed_;
    if (parameters.reverse) {
      speed = -speed;
    }
    readPosition_ = wrap(readPosition_ + speed * modulation, length);
    const double pitchRatio = std::exp2(
        bounded(parameters.pitchSemitones, -12.0F, 12.0F, 0.0F) / 12.0F);
    pitchPhase_ = wrap(pitchPhase_ + std::abs(pitchRatio - 1.0) / 1024.0, 1U);
  }
  wasCapturing_ = parameters.capture;
  measured.position =
      length > 0U ? static_cast<float>(readPosition_ / length) : 0.0F;
  measured.captured = static_cast<float>(capturedSamples_) /
                      static_cast<float>(std::max<std::size_t>(1U, length));
  meters_ = measured;
}

}  // namespace aste::loop
