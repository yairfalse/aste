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
    for (auto& deck : decks_) {
      deck.left.assign(capacity, 0.0F);
      deck.right.assign(capacity, 0.0F);
    }
  } catch (...) {
    for (auto& deck : decks_) {
      deck.left.clear();
      deck.right.clear();
    }
  }
  smoothingCoefficient_ =
      static_cast<float>(1.0 - std::exp(-1.0 / (0.005 * sampleRate_)));
  reset();
}

void Processor::reset() noexcept {
  for (auto& deck : decks_) {
    deck.length = 0U;
    deck.generation = 0U;
  }
  activeDeck_ = 0U;
  printingDeck_ = 0U;
  writePosition_ = 0U;
  printPosition_ = 0U;
  printLength_ = 0U;
  generationCounter_ = 0U;
  readPosition_ = 0.0;
  modulationPhase_ = 0.0;
  pitchPhase_ = 0.0;
  driftState_ = 0.0F;
  printStateLeft_ = 0.0F;
  printStateRight_ = 0.0F;
  smoothingInitialized_ = false;
  wasCapturing_ = false;
  printing_ = false;
  meters_ = {};
}

void Processor::clear() noexcept {
  for (auto& deck : decks_) {
    std::fill(deck.left.begin(), deck.left.end(), 0.0F);
    std::fill(deck.right.begin(), deck.right.end(), 0.0F);
  }
  reset();
}

void Processor::discard() noexcept { reset(); }

void Processor::previousGeneration() noexcept {
  if (printing_ || decks_[activeDeck_].generation == 0U) return;
  std::size_t candidate = activeDeck_;
  std::uint32_t nearest{};
  const auto current = decks_[activeDeck_].generation;
  for (std::size_t deck = 0; deck < decks_.size(); ++deck) {
    const auto generation = decks_[deck].generation;
    if (generation < current && generation > nearest) {
      nearest = generation;
      candidate = deck;
    }
  }
  if (candidate != activeDeck_) {
    activeDeck_ = candidate;
    writePosition_ = 0U;
    readPosition_ = 0.0;
    pitchPhase_ = 0.0;
  }
}

void Processor::nextGeneration() noexcept {
  if (printing_ || decks_[activeDeck_].generation == 0U) return;
  std::size_t candidate = activeDeck_;
  auto nearest = std::numeric_limits<std::uint32_t>::max();
  const auto current = decks_[activeDeck_].generation;
  for (std::size_t deck = 0; deck < decks_.size(); ++deck) {
    const auto generation = decks_[deck].generation;
    if (generation > current && generation < nearest) {
      nearest = generation;
      candidate = deck;
    }
  }
  if (candidate != activeDeck_) {
    activeDeck_ = candidate;
    writePosition_ = 0U;
    readPosition_ = 0.0;
    pitchPhase_ = 0.0;
  }
}

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

std::size_t Processor::retainedGenerations() const noexcept {
  return static_cast<std::size_t>(
      std::count_if(decks_.begin(), decks_.end(),
                    [](const Deck& deck) { return deck.generation != 0U; }));
}

void Processor::beginReloop(std::size_t length) noexcept {
  if (printing_ || length < 2U || decks_[activeDeck_].generation == 0U) return;
  const auto sourceGeneration = decks_[activeDeck_].generation;
  for (std::size_t deck = 0; deck < decks_.size(); ++deck) {
    if (deck != activeDeck_ && decks_[deck].generation > sourceGeneration) {
      decks_[deck].length = 0U;
      decks_[deck].generation = 0U;
    }
  }
  std::size_t destination = decks_.size();
  for (std::size_t deck = 0; deck < decks_.size(); ++deck) {
    if (deck != activeDeck_ && decks_[deck].generation == 0U) {
      destination = deck;
      break;
    }
  }
  if (destination == decks_.size()) {
    auto oldest = std::numeric_limits<std::uint32_t>::max();
    for (std::size_t deck = 0; deck < decks_.size(); ++deck) {
      if (deck != activeDeck_ && decks_[deck].generation < oldest) {
        oldest = decks_[deck].generation;
        destination = deck;
      }
    }
  }
  if (destination == decks_.size()) return;
  printingDeck_ = destination;
  printLength_ = std::min(length, decks_[destination].left.size());
  printPosition_ = 0U;
  printStateLeft_ = 0.0F;
  printStateRight_ = 0.0F;
  decks_[destination].length = 0U;
  decks_[destination].generation = 0U;
  printing_ = true;
}

float Processor::printSample(float input, float& state,
                             const Parameters& parameters) const noexcept {
  const float record = bounded(parameters.amplifier, 0.0F, 1.0F, 0.25F);
  const float loss = bounded(parameters.degradation, 0.0F, 1.0F, 0.08F);
  const float tapeSpeed = bounded(parameters.tapeSpeed, 0.5F, 2.0F, 1.0F);
  const float drive = 1.0F + 5.0F * record;
  const float saturated = std::tanh(input * drive) / std::tanh(drive);
  const double cutoff = std::clamp(19000.0 * tapeSpeed * (1.0 - 0.72 * loss),
                                   1800.0, sampleRate_ * 0.45);
  const float coefficient = static_cast<float>(
      1.0 - std::exp(-2.0 * std::numbers::pi * cutoff / sampleRate_));
  state = clean(state + coefficient * (saturated - state));
  const float resolution = 32768.0F / (1.0F + 95.0F * loss);
  const float printed = std::round(state * resolution) / resolution;
  return clean(printed * (1.0F - 0.11F * loss));
}

void Processor::process(float* left, float* right, std::size_t frames,
                        const Parameters& parameters) noexcept {
  if (left == nullptr || frames == 0U || decks_[0].left.empty()) {
    meters_ = {};
    return;
  }
  const std::size_t requested = static_cast<std::size_t>(
      std::clamp(static_cast<double>(bounded(parameters.loopLengthSeconds,
                                             0.05F, 30.0F, 2.0F)) *
                     sampleRate_,
                 64.0, static_cast<double>(decks_[0].left.size())));
  const std::size_t length = std::max<std::size_t>(64U, requested);
  const auto playbackLength = std::min(length, decks_[activeDeck_].length);
  if (parameters.reloop) beginReloop(playbackLength);
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
  if (parameters.capture && !wasCapturing_ &&
      decks_[activeDeck_].length == 0U) {
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
    auto& active = decks_[activeDeck_];
    const auto currentLength = std::min(length, active.length);
    float wetLeft = currentLength > 1U
                        ? playback(active.left, parameters, currentLength)
                        : 0.0F;
    float wetRight = currentLength > 1U
                         ? playback(active.right, parameters, currentLength)
                         : 0.0F;
    if (currentLength < 2U) {
      wetLeft = inputLeft;
      wetRight = inputRight;
    }
    if (parameters.capture && !printing_) {
      if (active.generation == 0U) {
        active.generation = ++generationCounter_;
      }
      writePosition_ %= length;
      const bool hasRecordedSample = writePosition_ < active.length;
      const float existingLeft =
          hasRecordedSample ? active.left[writePosition_] : 0.0F;
      const float existingRight =
          hasRecordedSample ? active.right[writePosition_] : 0.0F;
      active.left[writePosition_] = clean(inputLeft * (1.0F - overdub) +
                                          existingLeft * feedback * overdub);
      active.right[writePosition_] = clean(inputRight * (1.0F - overdub) +
                                           existingRight * feedback * overdub);
      writePosition_ = (writePosition_ + 1U) % length;
      active.length = std::min(length, active.length + 1U);
    }
    const float drive = 1.0F + 4.0F * smoothedAmplifier_;
    wetLeft = std::tanh(drive * wetLeft) / drive;
    wetRight = std::tanh(drive * wetRight) / drive;
    const float quantization = 32768.0F / (1.0F + 63.0F * smoothedDegradation_);
    wetLeft = std::round(wetLeft * quantization) / quantization;
    wetRight = std::round(wetRight * quantization) / quantization;
    if (printing_ && printPosition_ < printLength_) {
      auto& destination = decks_[printingDeck_];
      destination.left[printPosition_] =
          printSample(wetLeft, printStateLeft_, parameters);
      destination.right[printPosition_] =
          printSample(wetRight, printStateRight_, parameters);
      ++printPosition_;
      destination.length = printPosition_;
      if (printPosition_ >= printLength_) {
        destination.generation = ++generationCounter_;
        activeDeck_ = printingDeck_;
        readPosition_ = 0.0;
        pitchPhase_ = 0.0;
        writePosition_ = 0U;
        printing_ = false;
      }
    }
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
    const auto advancingLength = std::min(length, decks_[activeDeck_].length);
    readPosition_ = wrap(readPosition_ + speed * modulation, advancingLength);
    const double pitchRatio = std::exp2(
        bounded(parameters.pitchSemitones, -12.0F, 12.0F, 0.0F) / 12.0F);
    pitchPhase_ = wrap(pitchPhase_ + std::abs(pitchRatio - 1.0) / 1024.0, 1U);
  }
  wasCapturing_ = parameters.capture;
  const auto& active = decks_[activeDeck_];
  const auto activeLength = std::min(length, active.length);
  measured.position = activeLength > 0U
                          ? static_cast<float>(readPosition_ / activeLength)
                          : 0.0F;
  measured.captured = static_cast<float>(activeLength) /
                      static_cast<float>(std::max<std::size_t>(1U, length));
  measured.printing = printing_ && printLength_ > 0U
                          ? static_cast<float>(printPosition_) /
                                static_cast<float>(printLength_)
                          : 0.0F;
  measured.generation = active.generation;
  measured.retainedGenerations =
      static_cast<std::uint32_t>(retainedGenerations());
  measured.activeDeck = activeDeck_;
  meters_ = measured;
}

}  // namespace aste::loop
