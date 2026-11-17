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
  diffuse_.prepare(sampleRate_);
  memory_.prepare(sampleRate_);
  reset();
}

void Processor::reset() noexcept {
  clearFrom_ = lastDiffuse_;
  clearFade_ = int(sampleRate_ * .005);
  diffuse_.clear();
  diffuseTone_ = {};
  diffuseBass_ = {};
  lastDiffuse_ = {};
  memory_.reset();
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
  movingGrainPhase_ = 0.0;
  lowFeedback_.fill(0.F);
  lowInput_ = 0.F;
  exciterPhase_ = 0.0;
  exciterIncrement_ = 0.0;
  exciterEnvelope_ = 0.0F;
  inputFilter_ = 0.0F;
  energyState_ = 0.0F;
  materialStrain_ = 0.F;
  materialLow_.fill(0.F);
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
  if (parameters.diffuseBehavior) {
    processDiffuse(left, right, frames, parameters);
    return;
  }
  const float targetLoopMix = unit(parameters.loopMix, 0.F);
  const NestedMemory::Controls memoryControls{
      parameters.loopHold,
      parameters.innerRepeat,
      parameters.loopReverse,
      std::clamp(finiteOr(parameters.loopMs, 1500.F), 20.F, 4000.F),
      std::clamp(finiteOr(parameters.innerMs, 90.F), 4.F, 1000.F),
      std::clamp(finiteOr(parameters.loopSpeed, 1.F), .25F, 2.F)};
  const float targetFocus = unit(parameters.focus, 0.F);
  const float targetTexture = unit(parameters.texture, 0.F);
  const float targetForever = parameters.forever ? 1.0F : 0.0F;
  const float targetMass = unit(parameters.mass, 0.62F);
  const float targetGrain = unit(parameters.grain, 0.34F);
  const float targetPitch = unit(parameters.pitch, 0.28F);
  const float targetMotion = unit(parameters.motion, 0.24F);
  const float targetDistance = unit(parameters.distance, 0.45F);
  const float targetBlend = unit(parameters.blend, 0.48F);
  const float targetOutput = linearGain(parameters.outputDb);
  if (!smoothingInitialized_) {
    smoothedLoopMix_ = 0.F;
    smoothedFocus_ = targetFocus;
    smoothedTexture_ = targetTexture;
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
    smooth(smoothedLoopMix_, memory_.active() ? targetLoopMix : 0.F);
    smooth(smoothedFocus_, targetFocus);
    smooth(smoothedTexture_, targetTexture);
    smooth(smoothedForever_, targetForever);
    smooth(smoothedMass_, targetMass);
    smooth(smoothedGrain_, targetGrain);
    smooth(smoothedPitch_, targetPitch);
    smooth(smoothedMotion_, targetMotion);
    smooth(smoothedDistance_, targetDistance);
    smooth(smoothedBlend_, targetBlend);
    smooth(smoothedOutputGain_, targetOutput);

    const float heldInfluence =
        parameters.legacyBehavior ? 0.14F * smoothedForever_ : 0.0F;
    const float effectiveGrain = std::min(1.0F, smoothedGrain_ + heldInfluence);
    const float effectivePitch = std::min(1.0F, smoothedPitch_ + heldInfluence);
    const float effectiveMotion =
        std::min(1.0F, smoothedMotion_ + heldInfluence);
    const float strainTarget = energyState_ / (energyState_ + .0025F);
    if (parameters.materialBehavior)
      materialStrain_ +=
          static_cast<float>(1. - std::exp(-1. / (sampleRate_ * .35))) *
          (strainTarget - materialStrain_);
    const float material = parameters.materialBehavior ? smoothedTexture_ : 0.F;
    const float ordinaryRetention =
        0.70F + 0.285F * std::pow(smoothedMass_, 1.35F);
    const float retention =
        ordinaryRetention +
        smoothedForever_ * ((parameters.materialBehavior ? .999995F : .99935F) -
                            ordinaryRetention);
    lastRetention = retention;
    const float pitchReturn =
        (0.095F + .145F * smoothedTexture_) * effectivePitch;
    const float feedbackGain =
        retention * (1.0F - (0.11F + .24F * smoothedTexture_) * effectivePitch);

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
          (smoothOffset + grainOffset +
           material * .006F * materialStrain_ * std::sin(phase)) *
              static_cast<float>(sampleRate_);
      lineOutput_[line] = delays_[line].read(delay);
      if (parameters.materialBehavior)
        undisturbed_[line] =
            delays_[line].read(std::round(baseDelaySamples_[line]));
      if (smoothedTexture_ > 0.F && effectiveGrain > 0.F) {
        // Two overlapping Hann-windowed heads expose discrete moving grains
        // directly, rather than relying on tiny offsets deep in feedback.
        const float oldCycle = static_cast<float>(motionPhase_ / (2.0 * kPi)) *
                               (3.F + 5.F * effectiveMotion);
        const float span =
            static_cast<float>(sampleRate_) * (.018F + .065F * effectiveGrain);
        const float offset =
            .012F * static_cast<float>(sampleRate_) * grainState_[line];
        const auto heads = [&](float cycle) {
          cycle += line * .137F;
          const float phaseA = cycle - std::floor(cycle);
          const float phaseB = phaseA < .5F ? phaseA + .5F : phaseA - .5F;
          const float windowA = .5F - .5F * std::cos(2.F * kPi * phaseA);
          const float a = delays_[line].read(baseDelaySamples_[line] + offset +
                                             span * phaseA);
          const float b = delays_[line].read(baseDelaySamples_[line] + offset +
                                             span * phaseB);
          return windowA * a + (1.F - windowA) * b;
        };
        const float original = heads(oldCycle);
        const float granular =
            original +
            smoothedFocus_ *
                (heads(static_cast<float>(movingGrainPhase_)) - original);
        lineOutput_[line] +=
            smoothedTexture_ * effectiveGrain * (granular - lineOutput_[line]);
      }
    }
    movingGrainPhase_ +=
        (.12 + 3.8 * effectiveMotion * effectiveMotion) / sampleRate_;
    if (movingGrainPhase_ >= 1.) movingGrainPhase_ -= 1.;
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
        fieldMono,
        std::pow(2.0F, (7.0F + smoothedFocus_ * effectiveMotion * 5.F *
                                   static_cast<float>(std::sin(motionPhase_))) /
                           12.0F),
        window);
    const float shiftedOctave = pitchVoices_[1].process(
        fieldMono,
        2.0F * std::exp2(-smoothedFocus_ * effectiveMotion * 4.F *
                         static_cast<float>(std::cos(motionPhase_)) / 12.F),
        window);
    const float pitched = 0.62F * shiftedFifth + 0.38F * shiftedOctave;
    // Allpass-like rotation redistributes existing stored energy, rather than
    // relying only on a dry shifted layer to suggest movement.
    if (material > 0.F) {
      const float angle =
          material * (.12F + .38F * effectiveMotion + .2F * materialStrain_);
      const float cosine = std::cos(angle), sine = std::sin(angle);
      for (std::size_t line = 0; line < kLineCount; line += 2) {
        const float a = feedback_[line], b = feedback_[line + 1];
        feedback_[line] = cosine * a - sine * b;
        feedback_[line + 1] = sine * a + cosine * b;
      }
    }

    const float excitation = renderExciter();
    const auto memoryAudio = memory_.process(
        dryLeft + excitation, dryRight + excitation, memoryControls);
    const float monoInput = 0.5F * (dryLeft + dryRight) + excitation;
    const float sideInput = 0.5F * (dryLeft - dryRight);
    const float inputCutoff =
        15000.0F - (12500.0F - 6500.F * smoothedFocus_) * smoothedDistance_;
    const float inputAlpha =
        inputCutoff /
        (inputCutoff + static_cast<float>(sampleRate_ / (2.0 * kPi)));
    inputFilter_ += inputAlpha * (monoInput - inputFilter_);
    const float bassAlpha =
        1.F - std::exp(-2.F * kPi * (80.F + 240.F * smoothedFocus_) /
                       static_cast<float>(sampleRate_));
    lowInput_ += bassAlpha * (inputFilter_ - lowInput_);
    const float focusedInput = inputFilter_ - smoothedFocus_ * lowInput_;
    const float injection = parameters.materialBehavior
                                ? .3F - .04F * smoothedForever_
                                : 0.24F - 0.10F * smoothedForever_;
    const float dampingCutoff =
        14500.0F - (11200.0F - 7000.F * smoothedFocus_) * smoothedDistance_ -
        1600.0F * smoothedMass_;
    const float dampingAlpha =
        dampingCutoff /
        (dampingCutoff + static_cast<float>(sampleRate_ / (2.0 * kPi)));
    if (parameters.materialBehavior) {
      std::array<float, kLineCount> returns{}, injections{};
      double readPower = 0, returnPower = 0;
      for (std::size_t line = 0; line < kLineCount; ++line) {
        dampingState_[line] +=
            dampingAlpha * (feedback_[line] - dampingState_[line]);
        float value = dampingState_[line] * feedbackGain +
                      pitched * pitchReturn * kRightSigns[line];
        lowFeedback_[line] += bassAlpha * (value - lowFeedback_[line]);
        value -= smoothedFocus_ * lowFeedback_[line];
        const float hpAlpha = static_cast<float>(
            1. -
            std::exp(-2. * kPi * (70. + 120. * smoothedMass_) / sampleRate_));
        materialLow_[line] += hpAlpha * (value - materialLow_[line]);
        value -= material * .45F * materialLow_[line];
        returns[line] = value;
        injections[line] =
            (focusedInput + kLeftSigns[line] * sideInput * .35F) * injection *
            kInputSigns[line];
        readPower += undisturbed_[line] * undisturbed_[line];
        returnPower += value * value;
      }
      // Hold conserves the returning network energy before admitting new input.
      // Bound the correction; never amplify an empty/denormal tail into noise.
      const float correction =
          readPower > 1.e-20 && returnPower > 1.e-24
              ? static_cast<float>(std::min(
                    8.0,
                    std::sqrt(readPower * retention * retention / returnPower)))
              : 1.F;
      const float balance = 1.F + smoothedForever_ * (correction - 1.F);
      for (std::size_t line = 0; line < kLineCount; ++line)
        delays_[line].write(std::tanh(
            std::clamp(returns[line] * balance + injections[line], -4.F, 4.F)));
    } else {
      for (std::size_t line = 0; line < kLineCount; ++line) {
        dampingState_[line] +=
            dampingAlpha * (feedback_[line] - dampingState_[line]);
        const float stereoInjection =
            focusedInput + kLeftSigns[line] * sideInput * 0.35F;
        const float unfilteredWrite =
            dampingState_[line] * feedbackGain +
            pitched * pitchReturn * kRightSigns[line] +
            stereoInjection * injection * kInputSigns[line];
        lowFeedback_[line] +=
            bassAlpha * (unfilteredWrite - lowFeedback_[line]);
        const float focused =
            unfilteredWrite - smoothedFocus_ * lowFeedback_[line];
        const float hpAlpha = static_cast<float>(
            1. -
            std::exp(-2. * kPi * (70. + 120. * smoothedMass_) / sampleRate_));
        materialLow_[line] += hpAlpha * (focused - materialLow_[line]);
        const float write = focused - material * .45F * materialLow_[line];
        delays_[line].write(std::tanh(std::clamp(write, -4.0F, 4.0F)));
      }
    }

    float wetLeft{};
    float wetRight{};
    for (std::size_t line = 0; line < kLineCount; ++line) {
      wetLeft += lineOutput_[line] * kLeftSigns[line];
      wetRight += lineOutput_[line] * kRightSigns[line];
    }
    wetLeft *= 0.42F;
    wetRight *= 0.42F;
    const float pitchLayer = 1.8F * smoothedTexture_ * effectivePitch;
    wetLeft += pitchLayer * (.8F * shiftedFifth + .2F * shiftedOctave);
    wetRight += pitchLayer * (.2F * shiftedFifth + .8F * shiftedOctave);
    const float textureMakeup = 1.F + 1.8F * smoothedTexture_;
    wetLeft *= textureMakeup;
    wetRight *= textureMakeup;
    wetLeft += smoothedLoopMix_ * (memoryAudio[0] - wetLeft);
    wetRight += smoothedLoopMix_ * (memoryAudio[1] - wetRight);
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
  meters_ = {inputPeak,
             outputPeak,
             std::sqrt(std::max(0.0F, energyState_)),
             lastRetention,
             memory_.position(),
             memory_.active(),
             memory_.innerActive(),
             parameters.loopHold && !memory_.active()};
}

void Processor::processDiffuse(float* left, float* right, std::size_t frames,
                               const Parameters& p) noexcept {
  const float mass = unit(p.mass, .62F), distance = unit(p.distance, .45F),
              transform = unit(p.texture, 0), pitch = unit(p.pitch, 0),
              focus = unit(p.focus, 0);
  const DiffuseSpace::Controls c{.25 * std::pow(64., mass),
                                 .3 + .65 * (1 - distance),
                                 unit(p.motion, 0),
                                 transform,
                                 unit(p.grain, 0),
                                 p.forever ? 1. : 0.,
                                 focus};
  const NestedMemory::Controls mc{
      p.loopHold,
      p.innerRepeat,
      p.loopReverse,
      std::clamp(finiteOr(p.loopMs, 1500), 20.F, 4000.F),
      std::clamp(finiteOr(p.innerMs, 90), 4.F, 1000.F),
      std::clamp(finiteOr(p.loopSpeed, 1), .25F, 2.F)};
  const float targetBlend = unit(p.blend, .48F),
              targetGain = linearGain(p.outputDb),
              tone = 1 - std::exp(-2 * kPi * (1800 + 12000 * (1 - distance)) /
                                  float(sampleRate_));
  if (!smoothingInitialized_) {
    smoothedBlend_ = targetBlend;
    smoothedOutputGain_ = targetGain;
    smoothedPitch_ = pitch;
    smoothedTexture_ = transform;
    smoothingInitialized_ = true;
  }
  float inputPeak = 0, outputPeak = 0;
  for (std::size_t i = 0; i < frames; ++i) {
    float dl = finiteOr(left[i], 0), dr = right ? finiteOr(right[i], 0) : dl;
    inputPeak = std::max({inputPeak, std::abs(dl), std::abs(dr)});
    if (p.bypass) {
      left[i] = dl;
      if (right) right[i] = dr;
      continue;
    }
    smoothedBlend_ += smoothingCoefficient_ * (targetBlend - smoothedBlend_);
    smoothedOutputGain_ +=
        smoothingCoefficient_ * (targetGain - smoothedOutputGain_);
    smoothedPitch_ += smoothingCoefficient_ * (pitch - smoothedPitch_);
    smoothedTexture_ += smoothingCoefficient_ * (transform - smoothedTexture_);
    const float excitation = renderExciter();
    auto wet = diffuse_.tick(dl + excitation, dr + excitation, c);
    const float window = std::clamp(float(sampleRate_ * .072), 512.F,
                                    float(kPitchCapacity - 64));
    float shiftedL = pitchVoices_[0].process(
              wet[0], std::exp2((7.F + 5.F * focus) / 12.F), window),
          shiftedR = pitchVoices_[1].process(
              wet[1], std::exp2((7.F + 5.F * focus) / 12.F), window);
    const float amount = smoothedTexture_ * smoothedPitch_ * .65F;
    wet[0] += amount * (shiftedL - wet[0]);
    wet[1] += amount * (shiftedR - wet[1]);
    for (int ch = 0; ch < 2; ++ch) {
      wet[ch] *=
          2.5F;  // Calibrate the wet return independently of the dry path.
      diffuseTone_[ch] += tone * (wet[ch] - diffuseTone_[ch]);
      wet[ch] = diffuseTone_[ch];
      diffuseBass_[ch] += (1 - std::exp(-2 * kPi * 160 / float(sampleRate_))) *
                          (wet[ch] - diffuseBass_[ch]);
      wet[ch] -= focus * .75F * diffuseBass_[ch];
    }
    auto memoryAudio = memory_.process(dl + excitation, dr + excitation, mc);
    const float targetLoop = memory_.active() ? unit(p.loopMix, 0) : 0;
    smoothedLoopMix_ += smoothingCoefficient_ * (targetLoop - smoothedLoopMix_);
    for (int ch = 0; ch < 2; ++ch) {
      wet[ch] += smoothedLoopMix_ * (memoryAudio[ch] - wet[ch]);
      if (clearFade_ > 0)
        wet[ch] += clearFrom_[ch] * float(clearFade_) /
                   float(std::max(1, int(sampleRate_ * .005)));
      lastDiffuse_[ch] = wet[ch];
    }
    if (clearFade_ > 0) --clearFade_;
    const float dry = smoothedBlend_ >= 1
                          ? 0
                          : std::cos(smoothedBlend_ * kPi * .5F),
                gain = std::sin(smoothedBlend_ * kPi * .5F);
    float l = (dl * dry + wet[0] * gain) * smoothedOutputGain_,
          r = (dr * dry + wet[1] * gain) * smoothedOutputGain_;
    left[i] = finiteOr(right ? l : (l + r) * .5F, 0);
    if (right) right[i] = finiteOr(r, 0);
    outputPeak = std::max({outputPeak, std::abs(l), std::abs(r)});
  }
  meters_ = {inputPeak,
             outputPeak,
             float(diffuse_.energy()),
             float(diffuse_.retention()),
             memory_.position(),
             memory_.active(),
             memory_.innerActive(),
             p.loopHold && !memory_.active()};
}

}  // namespace aste::field
