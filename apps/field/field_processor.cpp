#include "field_processor.hpp"

#include <algorithm>
#include <cmath>
#include <numbers>

namespace aste::field {
namespace {

constexpr float kPi = std::numbers::pi_v<float>;
constexpr std::array<float, 8> kDelaySeconds{
    0.0311F, 0.0377F, 0.0439F, 0.0521F, 0.0593F, 0.0677F, 0.0739F, 0.0821F};
constexpr std::array<float, 8> kInputSigns{1.0F,  1.0F,  -1.0F, 1.0F,
                                           -1.0F, -1.0F, 1.0F,  -1.0F};
constexpr std::array<float, 8> kLeftSigns{1.0F,  -1.0F, 1.0F,  1.0F,
                                          -1.0F, 1.0F,  -1.0F, -1.0F};
constexpr std::array<float, 8> kRightSigns{-1.0F, 1.0F, 1.0F,  -1.0F,
                                           1.0F,  1.0F, -1.0F, -1.0F};

float finiteOr(float value, float fallback) noexcept {
  return std::isfinite(value) ? value : fallback;
}

float unit(float value, float fallback) noexcept {
  return std::clamp(finiteOr(value, fallback), 0.0F, 1.0F);
}

float linearGain(float decibels) noexcept {
  return std::pow(10.0F,
                  std::clamp(finiteOr(decibels, 0.0F), -60.0F, 24.0F) / 20.0F);
}

float interpolateCircular(const float* samples, std::size_t capacity,
                          std::size_t writePosition,
                          float delaySamples) noexcept {
  const float bounded =
      std::clamp(delaySamples, 1.0F, static_cast<float>(capacity - 2U));
  float position = static_cast<float>(writePosition) - bounded;
  if (position < 0.0F) position += static_cast<float>(capacity);
  const auto first = static_cast<std::size_t>(position);
  const auto second = first + 1U == capacity ? 0U : first + 1U;
  const float fraction = position - static_cast<float>(first);
  return samples[first] + fraction * (samples[second] - samples[first]);
}

}  // namespace

float Processor::DelayLine::read(float delaySamples) const noexcept {
  return interpolateCircular(samples.data(), samples.size(), writePosition,
                             delaySamples);
}

void Processor::DelayLine::write(float sample) noexcept {
  samples[writePosition] = sample;
  writePosition = (writePosition + 1U) % samples.size();
}

void Processor::DelayLine::clear() noexcept {
  samples.fill(0.0F);
  writePosition = 0U;
}

float Processor::PitchVoice::process(float input, float ratio,
                                     float windowSamples) noexcept {
  const float firstPhase = phase;
  float secondPhase = phase + 0.5F;
  if (secondPhase >= 1.0F) secondPhase -= 1.0F;
  const float firstWindow = std::sin(kPi * firstPhase);
  const float secondWindow = std::sin(kPi * secondPhase);
  const float first =
      interpolateCircular(samples.data(), samples.size(), writePosition,
                          32.0F + firstPhase * windowSamples);
  const float second =
      interpolateCircular(samples.data(), samples.size(), writePosition,
                          32.0F + secondPhase * windowSamples);
  samples[writePosition] = input;
  writePosition = (writePosition + 1U) % samples.size();
  phase += (1.0F - ratio) / windowSamples;
  while (phase < 0.0F) phase += 1.0F;
  while (phase >= 1.0F) phase -= 1.0F;
  const float firstGain = firstWindow * firstWindow;
  const float secondGain = secondWindow * secondWindow;
  return first * firstGain + second * secondGain;
}

void Processor::PitchVoice::clear() noexcept {
  samples.fill(0.0F);
  writePosition = 0U;
  phase = 0.25F;
}

std::uint32_t Processor::randomStep(std::uint32_t state) noexcept {
  state ^= state << 13U;
  state ^= state >> 17U;
  state ^= state << 5U;
  return state == 0U ? 0x46a31d2bU : state;
}

void Processor::prepare(double sampleRate) noexcept {
  sampleRate_ = std::clamp(std::isfinite(sampleRate) ? sampleRate : 48000.0,
                           8000.0, 384000.0);
  smoothingCoefficient_ =
      1.0F - std::exp(-1.0F / static_cast<float>(sampleRate_ * 0.035));
  exciterDecay_ = std::exp(-1.0F / static_cast<float>(sampleRate_ * 0.035));
  for (std::size_t line = 0; line < kLineCount; ++line) {
    baseDelaySamples_[line] =
        std::min(kDelaySeconds[line] * static_cast<float>(sampleRate_),
                 static_cast<float>(kDelayCapacity - 4096U));
  }
  reset();
}

void Processor::reset() noexcept {
  for (auto& delay : delays_) delay.clear();
  for (auto& pitch : pitchVoices_) pitch.clear();
  dampingState_.fill(0.0F);
  grainTarget_.fill(0.0F);
  grainState_.fill(0.0F);
  lineOutput_.fill(0.0F);
  feedback_.fill(0.0F);
  randomState_ = 0x46a31d2bU;
  grainCountdown_ = 0U;
  motionPhase_ = 0.0;
  exciterPhase_ = 0.0;
  exciterIncrement_ = 0.0;
  exciterEnvelope_ = 0.0F;
  inputFilter_ = 0.0F;
  energyState_ = 0.0F;
  smoothingInitialized_ = false;
  meters_ = {};
}

void Processor::noteOn(int midiNote, float velocity) noexcept {
  if (midiNote < 0 || midiNote > 127 || !std::isfinite(velocity) ||
      velocity <= 0.0F)
    return;
  const double frequency =
      440.0 * std::pow(2.0, (static_cast<double>(midiNote) - 69.0) / 12.0);
  exciterIncrement_ = 2.0 * std::numbers::pi * frequency / sampleRate_;
  exciterEnvelope_ = std::clamp(velocity, 0.0F, 1.0F);
}

float Processor::renderExciter() noexcept {
  const float sample =
      static_cast<float>(std::sin(exciterPhase_)) * exciterEnvelope_ * 0.32F;
  exciterPhase_ += exciterIncrement_;
  if (exciterPhase_ >= 2.0 * std::numbers::pi)
    exciterPhase_ -= 2.0 * std::numbers::pi;
  exciterEnvelope_ *= exciterDecay_;
  if (exciterEnvelope_ < 1.0e-8F) exciterEnvelope_ = 0.0F;
  return sample;
}

void Processor::process(float* left, float* right, std::size_t frames,
                        const Parameters& parameters) noexcept {
  if (left == nullptr || frames == 0U) {
    meters_ = {};
    return;
  }
  const float targetForever = parameters.forever ? 1.0F : 0.0F;
  const float targetMass = unit(parameters.mass, 0.62F);
  const float targetGrain = unit(parameters.grain, 0.34F);
  const float targetPitch = unit(parameters.pitch, 0.28F);
  const float targetMotion = unit(parameters.motion, 0.24F);
  const float targetDistance = unit(parameters.distance, 0.45F);
  const float targetBlend = unit(parameters.blend, 0.48F);
  const float targetOutput = linearGain(parameters.outputDb);
  if (!smoothingInitialized_) {
    smoothedForever_ = targetForever;
    smoothedMass_ = targetMass;
    smoothedGrain_ = targetGrain;
    smoothedPitch_ = targetPitch;
    smoothedMotion_ = targetMotion;
    smoothedDistance_ = targetDistance;
    smoothedBlend_ = targetBlend;
    smoothedOutputGain_ = targetOutput;
    smoothingInitialized_ = true;
  }
  float inputPeak{};
  float outputPeak{};
  float lastRetention{};
  for (std::size_t sample = 0; sample < frames; ++sample) {
    float dryLeft = finiteOr(left[sample], 0.0F);
    float dryRight = right == nullptr ? dryLeft : finiteOr(right[sample], 0.0F);
    inputPeak =
        std::max(inputPeak, std::max(std::abs(dryLeft), std::abs(dryRight)));
    if (parameters.bypass) {
      left[sample] = dryLeft;
      if (right != nullptr) right[sample] = dryRight;
      outputPeak =
          std::max(outputPeak, std::max(std::abs(dryLeft), std::abs(dryRight)));
      continue;
    }
    const auto smooth = [this](float& value, float target) {
      value += smoothingCoefficient_ * (target - value);
    };
    smooth(smoothedForever_, targetForever);
    smooth(smoothedMass_, targetMass);
    smooth(smoothedGrain_, targetGrain);
    smooth(smoothedPitch_, targetPitch);
    smooth(smoothedMotion_, targetMotion);
    smooth(smoothedDistance_, targetDistance);
    smooth(smoothedBlend_, targetBlend);
    smooth(smoothedOutputGain_, targetOutput);

    const float heldInfluence = 0.14F * smoothedForever_;
    const float effectiveGrain = std::min(1.0F, smoothedGrain_ + heldInfluence);
    const float effectivePitch = std::min(1.0F, smoothedPitch_ + heldInfluence);
    const float effectiveMotion =
        std::min(1.0F, smoothedMotion_ + heldInfluence);
    const float ordinaryRetention =
        0.70F + 0.285F * std::pow(smoothedMass_, 1.35F);
    const float retention =
        ordinaryRetention + smoothedForever_ * (0.99935F - ordinaryRetention);
    lastRetention = retention;
    const float pitchReturn = 0.095F * effectivePitch;
    const float feedbackGain = retention * (1.0F - 0.11F * effectivePitch);

    if (grainCountdown_ == 0U) {
      const float updatesPerSecond = 7.0F + 36.0F * effectiveMotion;
      grainCountdown_ = std::max<std::size_t>(
          1U, static_cast<std::size_t>(sampleRate_ / updatesPerSecond));
      for (auto& target : grainTarget_) {
        randomState_ = randomStep(randomState_);
        target = static_cast<float>(randomState_ & 0xffffU) / 32767.5F - 1.0F;
      }
    }
    --grainCountdown_;
    const float grainSlew =
        1.0F - std::exp(-1.0F / static_cast<float>(sampleRate_ * 0.012));
    for (std::size_t line = 0; line < kLineCount; ++line) {
      grainState_[line] += grainSlew * (grainTarget_[line] - grainState_[line]);
      const float phase =
          static_cast<float>(motionPhase_ + static_cast<double>(line) * 0.743);
      const float smoothOffset =
          std::sin(phase) * (0.0004F + 0.0038F * effectiveMotion);
      const float grainOffset = grainState_[line] * effectiveGrain * 0.014F;
      const float delay =
          baseDelaySamples_[line] +
          (smoothOffset + grainOffset) * static_cast<float>(sampleRate_);
      lineOutput_[line] = delays_[line].read(delay);
    }
    motionPhase_ +=
        2.0 * std::numbers::pi * (0.025 + 0.58 * effectiveMotion) / sampleRate_;
    if (motionPhase_ >= 2.0 * std::numbers::pi)
      motionPhase_ -= 2.0 * std::numbers::pi;

    float sum{};
    for (float value : lineOutput_) sum += value;
    const float householder = 2.0F * sum / static_cast<float>(kLineCount);
    for (std::size_t line = 0; line < kLineCount; ++line)
      feedback_[line] = householder - lineOutput_[line];

    const float fieldMono = sum / static_cast<float>(kLineCount);
    const float window =
        std::clamp(static_cast<float>(sampleRate_ * 0.072), 512.0F,
                   static_cast<float>(kPitchCapacity - 64U));
    const float shiftedFifth = pitchVoices_[0].process(
        fieldMono, std::pow(2.0F, 7.0F / 12.0F), window);
    const float shiftedOctave =
        pitchVoices_[1].process(fieldMono, 2.0F, window);
    const float pitched = 0.62F * shiftedFifth + 0.38F * shiftedOctave;

    const float monoInput = 0.5F * (dryLeft + dryRight) + renderExciter();
    const float sideInput = 0.5F * (dryLeft - dryRight);
    const float inputCutoff = 15000.0F - 12500.0F * smoothedDistance_;
    const float inputAlpha =
        inputCutoff /
        (inputCutoff + static_cast<float>(sampleRate_ / (2.0 * kPi)));
    inputFilter_ += inputAlpha * (monoInput - inputFilter_);
    const float injection = 0.24F - 0.10F * smoothedForever_;
    const float dampingCutoff =
        14500.0F - 11200.0F * smoothedDistance_ - 1600.0F * smoothedMass_;
    const float dampingAlpha =
        dampingCutoff /
        (dampingCutoff + static_cast<float>(sampleRate_ / (2.0 * kPi)));
    for (std::size_t line = 0; line < kLineCount; ++line) {
      dampingState_[line] +=
          dampingAlpha * (feedback_[line] - dampingState_[line]);
      const float stereoInjection =
          inputFilter_ + kLeftSigns[line] * sideInput * 0.35F;
      const float write = dampingState_[line] * feedbackGain +
                          pitched * pitchReturn * kRightSigns[line] +
                          stereoInjection * injection * kInputSigns[line];
      delays_[line].write(std::tanh(std::clamp(write, -4.0F, 4.0F)));
    }

    float wetLeft{};
    float wetRight{};
    for (std::size_t line = 0; line < kLineCount; ++line) {
      wetLeft += lineOutput_[line] * kLeftSigns[line];
      wetRight += lineOutput_[line] * kRightSigns[line];
    }
    wetLeft *= 0.42F;
    wetRight *= 0.42F;
    const float dryGain = std::cos(smoothedBlend_ * kPi * 0.5F);
    const float wetGain = std::sin(smoothedBlend_ * kPi * 0.5F);
    float outputLeft =
        (dryLeft * dryGain + wetLeft * wetGain) * smoothedOutputGain_;
    float outputRight =
        (dryRight * dryGain + wetRight * wetGain) * smoothedOutputGain_;
    outputLeft = finiteOr(outputLeft, 0.0F);
    outputRight = finiteOr(outputRight, 0.0F);
    left[sample] = outputLeft;
    if (right != nullptr) right[sample] = outputRight;
    outputPeak = std::max(
        outputPeak, std::max(std::abs(outputLeft), std::abs(outputRight)));
    const float instantaneous = fieldMono * fieldMono;
    energyState_ += 0.001F * (instantaneous - energyState_);
  }
  meters_ = {inputPeak, outputPeak, std::sqrt(std::max(0.0F, energyState_)),
             lastRetention};
}

}  // namespace aste::field
