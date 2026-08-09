#include "harmonic_processor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace aste::harmonic {
namespace {

float finiteOr(float value, float fallback) noexcept {
  return std::isfinite(value) ? value : fallback;
}

float bounded(float value, float low, float high, float fallback) noexcept {
  return std::clamp(finiteOr(value, fallback), low, high);
}

float dbToGain(float db) noexcept {
  return std::exp(db * 0.11512925464970229F);
}

float cleanSample(float sample) noexcept {
  return std::clamp(finiteOr(sample, 0.0F), -16.0F, 16.0F);
}

float flush(float value) noexcept {
  if (!std::isfinite(value) ||
      std::abs(value) < std::numeric_limits<float>::min()) {
    return 0.0F;
  }
  return value;
}

float smooth(float current, float target, float coefficient) noexcept {
  const float result = target + coefficient * (current - target);
  return std::abs(result - target) < 1.0e-8F ? target : result;
}

}  // namespace

void Processor::Smoother::prepare(double sampleRate, double seconds,
                                  float initial) noexcept {
  coefficient_ = static_cast<float>(std::exp(-1.0 / (seconds * sampleRate)));
  current_ = target_ = initial;
}

float Processor::Smoother::next() noexcept {
  current_ = smooth(current_, target_, coefficient_);
  return current_;
}

void Processor::PeakFilter::prepare(double sampleRate, float frequency, float q,
                                    float gainDb) noexcept {
  smoothing_ = static_cast<float>(std::exp(-1.0 / (0.020 * sampleRate)));
  setTarget(sampleRate, frequency, q, gainDb);
  current_ = target_;
  reset();
}

void Processor::PeakFilter::setTarget(double sampleRate, float frequency,
                                      float q, float gainDb) noexcept {
  gainDb = bounded(gainDb, -12.0F, 12.0F, 0.0F);
  if (gainDb == 0.0F) {
    target_ = {};
    return;
  }
  frequency =
      bounded(frequency, 10.0F, static_cast<float>(sampleRate * 0.45), 1000.0F);
  q = bounded(q, 0.2F, 5.0F, 0.7F);
  const double amplitude = std::pow(10.0, gainDb / 40.0);
  const double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
  const double alpha = std::sin(omega) / (2.0 * q);
  const double cosine = std::cos(omega);
  const double a0 = 1.0 + alpha / amplitude;
  target_ = {
      .b0 = static_cast<float>((1.0 + alpha * amplitude) / a0),
      .b1 = static_cast<float>((-2.0 * cosine) / a0),
      .b2 = static_cast<float>((1.0 - alpha * amplitude) / a0),
      .a1 = static_cast<float>((-2.0 * cosine) / a0),
      .a2 = static_cast<float>((1.0 - alpha / amplitude) / a0),
  };
}

float Processor::PeakFilter::process(float input) noexcept {
  current_.b0 = smooth(current_.b0, target_.b0, smoothing_);
  current_.b1 = smooth(current_.b1, target_.b1, smoothing_);
  current_.b2 = smooth(current_.b2, target_.b2, smoothing_);
  current_.a1 = smooth(current_.a1, target_.a1, smoothing_);
  current_.a2 = smooth(current_.a2, target_.a2, smoothing_);
  const float output = current_.b0 * input + z1_;
  z1_ = flush(current_.b1 * input - current_.a1 * output + z2_);
  z2_ = flush(current_.b2 * input - current_.a2 * output);
  return flush(output);
}

void Processor::PeakFilter::reset() noexcept { z1_ = z2_ = 0.0F; }

void Processor::StateVariableBand::prepare(double sampleRate, float frequency,
                                           float q) noexcept {
  smoothing_ = static_cast<float>(std::exp(-1.0 / (0.020 * sampleRate)));
  setTarget(sampleRate, frequency, q);
  current_ = target_;
  reset();
}

void Processor::StateVariableBand::setTarget(double sampleRate, float frequency,
                                             float q) noexcept {
  frequency =
      bounded(frequency, 10.0F, static_cast<float>(sampleRate * 0.45), 1000.0F);
  q = bounded(q, 0.2F, 5.0F, 0.9F);
  const double g = std::tan(std::numbers::pi * frequency / sampleRate);
  const double k = 1.0 / q;
  const double a1 = 1.0 / (1.0 + g * (g + k));
  target_ = {
      .g = static_cast<float>(g),
      .a1 = static_cast<float>(a1),
      .a2 = static_cast<float>(g * a1),
  };
}

float Processor::StateVariableBand::process(float input,
                                            float amount) noexcept {
  current_.g = smooth(current_.g, target_.g, smoothing_);
  current_.a1 = smooth(current_.a1, target_.a1, smoothing_);
  current_.a2 = smooth(current_.a2, target_.a2, smoothing_);
  amount = bounded(amount, 0.0F, 1.0F, 0.0F);
  const float v3 = input - state2_;
  const float linearBand = current_.a1 * state1_ + current_.a2 * v3;
  const float drive = 1.0F + 2.0F * amount;
  const float boundedBand = std::tanh(drive * linearBand) / drive;
  const float band = linearBand + amount * (boundedBand - linearBand);
  const float low = state2_ + current_.g * band;
  state1_ = flush(2.0F * band - state1_);
  state2_ = flush(2.0F * low - state2_);
  return flush(band);
}

void Processor::StateVariableBand::reset() noexcept {
  state1_ = state2_ = 0.0F;
}

float Processor::proportionalQ(float gainDb) noexcept {
  return 0.7F * (1.0F + 0.7F * std::abs(gainDb) / 12.0F);
}

void Processor::Band::prepare(double sampleRate, float frequency,
                              float gainDb) noexcept {
  gainDb = bounded(gainDb, -12.0F, 12.0F, 0.0F);
  const float q = Processor::proportionalQ(gainDb);
  contour_.prepare(sampleRate, frequency, q, gainDb);
  reference_.prepare(sampleRate, frequency, 0.9F);
  nonlinear_.prepare(sampleRate, frequency, 0.9F);
  boost_.prepare(sampleRate, 0.020, std::max(gainDb, 0.0F) / 12.0F);
}

void Processor::Band::setTarget(double sampleRate, float frequency,
                                float gainDb) noexcept {
  gainDb = bounded(gainDb, -12.0F, 12.0F, 0.0F);
  contour_.setTarget(sampleRate, frequency, Processor::proportionalQ(gainDb),
                     gainDb);
  reference_.setTarget(sampleRate, frequency, 0.9F);
  nonlinear_.setTarget(sampleRate, frequency, 0.9F);
  boost_.setTarget(std::max(gainDb, 0.0F) / 12.0F);
}

float Processor::Band::process(float input, float harmonic,
                               float& activity) noexcept {
  const float linear = contour_.process(input);
  const float reference = reference_.process(input, 0.0F);
  const float coloured = nonlinear_.process(input, harmonic);
  const float contribution =
      0.08F * harmonic * boost_.next() * (coloured - reference);
  activity = std::max(activity, std::abs(contribution));
  return flush(linear + contribution);
}

void Processor::Band::reset() noexcept {
  contour_.reset();
  reference_.reset();
  nonlinear_.reset();
  boost_.reset();
}

void Processor::prepare(double sampleRate, const Parameters& initial) noexcept {
  sampleRate_ = std::clamp(
      static_cast<double>(finiteOr(static_cast<float>(sampleRate), 48000.0F)),
      8000.0, 384000.0);
  input_.prepare(sampleRate_, 0.005,
                 bounded(initial.inputDb, -18.0F, 18.0F, 0.0F));
  harmonic_.prepare(sampleRate_, 0.010,
                    bounded(initial.harmonic, 0.0F, 1.0F, 0.35F));
  output_.prepare(sampleRate_, 0.005,
                  bounded(initial.outputDb, -18.0F, 18.0F, 0.0F));
  const std::array<float, 4> frequencies{
      bounded(initial.foundationFrequencyHz, 35.0F, 160.0F, 80.0F),
      bounded(initial.bodyFrequencyHz, 160.0F, 1000.0F, 400.0F),
      bounded(initial.presenceFrequencyHz, 800.0F, 7000.0F, 2500.0F),
      bounded(initial.airFrequencyHz, 6000.0F, 20000.0F, 12000.0F),
  };
  const std::array<float, 4> gains{
      bounded(initial.foundationGainDb, -12.0F, 12.0F, 0.0F),
      bounded(initial.bodyGainDb, -12.0F, 12.0F, 0.0F),
      bounded(initial.presenceGainDb, -12.0F, 12.0F, 0.0F),
      bounded(initial.airGainDb, -12.0F, 12.0F, 0.0F),
  };
  for (auto& channel : bands_) {
    for (std::size_t band = 0; band < channel.size(); ++band) {
      channel[band].prepare(sampleRate_, frequencies[band], gains[band]);
    }
  }
  reset();
}

void Processor::reset() noexcept {
  input_.reset();
  harmonic_.reset();
  output_.reset();
  for (auto& channel : bands_) {
    for (auto& band : channel) {
      band.reset();
    }
  }
  meters_ = {};
}

void Processor::process(float* left, float* right, std::size_t frames,
                        const Parameters& parameters) noexcept {
  if (left == nullptr || frames == 0U) {
    meters_ = {};
    return;
  }
  input_.setTarget(bounded(parameters.inputDb, -18.0F, 18.0F, 0.0F));
  harmonic_.setTarget(bounded(parameters.harmonic, 0.0F, 1.0F, 0.35F));
  output_.setTarget(bounded(parameters.outputDb, -18.0F, 18.0F, 0.0F));
  const std::array<float, 4> frequencies{
      bounded(parameters.foundationFrequencyHz, 35.0F, 160.0F, 80.0F),
      bounded(parameters.bodyFrequencyHz, 160.0F, 1000.0F, 400.0F),
      bounded(parameters.presenceFrequencyHz, 800.0F, 7000.0F, 2500.0F),
      bounded(parameters.airFrequencyHz, 6000.0F, 20000.0F, 12000.0F),
  };
  const std::array<float, 4> gains{
      bounded(parameters.foundationGainDb, -12.0F, 12.0F, 0.0F),
      bounded(parameters.bodyGainDb, -12.0F, 12.0F, 0.0F),
      bounded(parameters.presenceGainDb, -12.0F, 12.0F, 0.0F),
      bounded(parameters.airGainDb, -12.0F, 12.0F, 0.0F),
  };
  for (auto& channel : bands_) {
    for (std::size_t band = 0; band < channel.size(); ++band) {
      channel[band].setTarget(sampleRate_, frequencies[band], gains[band]);
    }
  }

  MeterSnapshot measured{};
  const std::size_t channelCount = right == nullptr ? 1U : 2U;
  const std::array<float*, 2> channels{left, right};
  for (std::size_t sample = 0; sample < frames; ++sample) {
    const float inputGain = dbToGain(input_.next());
    const float harmonic = harmonic_.next();
    const float outputGain = dbToGain(output_.next());
    for (std::size_t channel = 0; channel < channelCount; ++channel) {
      const float input = cleanSample(channels[channel][sample]);
      measured.inputPeak = std::max(measured.inputPeak, std::abs(input));
      float processed = input * inputGain;
      float activity = 0.0F;
      for (auto& band : bands_[channel]) {
        processed = band.process(processed, harmonic, activity);
      }
      processed = cleanSample(processed * outputGain);
      channels[channel][sample] = processed;
      measured.outputPeak = std::max(measured.outputPeak, std::abs(processed));
      measured.harmonicActivity = std::max(measured.harmonicActivity, activity);
    }
  }
  meters_ = measured;
}

}  // namespace aste::harmonic
