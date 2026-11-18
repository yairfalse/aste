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

void Processor::prepare(double sampleRate, double maximumSeconds,
                        bool enableSnapshots) noexcept {
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
    snapshots_.prepare(enableSnapshots ? capacity : 0);
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

void Processor::publishTape() noexcept {
  snapshots_.publish(
      sampleRate_, activeDeck_,
      {decks_[0].length, decks_[1].length, decks_[2].length},
      {decks_[0].generation, decks_[1].generation, decks_[2].generation});
}

void Processor::restoreTape(const TapeState& state) {
  reset();
  const double ratio = sampleRate_ / state.sampleRate;
  for (std::size_t deck = 0; deck < decks_.size(); ++deck) {
    auto& target = decks_[deck];
    const auto& audio = state.audio[deck];
    const auto frames = audio.size() / 2;
    target.length =
        std::min(target.left.size(),
                 static_cast<std::size_t>(std::round(frames * ratio)));
    target.generation = target.length ? state.generations[deck] : 0;
    generationCounter_ = std::max(generationCounter_, target.generation);
    for (std::size_t frame = 0; frame < target.length; ++frame) {
      const double position =
          std::min(static_cast<double>(frames - 1), frame / ratio);
      const auto first = static_cast<std::size_t>(position);
      const auto next = std::min(first + 1, frames - 1);
      const float mix = static_cast<float>(position - first);
      target.left[frame] =
          mix == 0 ? audio[first * 2]
                   : clean(audio[first * 2] +
                           mix * (audio[next * 2] - audio[first * 2]));
      target.right[frame] =
          mix == 0 ? audio[first * 2 + 1]
                   : clean(audio[first * 2 + 1] +
                           mix * (audio[next * 2 + 1] - audio[first * 2 + 1]));
      snapshots_.write(deck, frame, target.left[frame], target.right[frame]);
    }
  }
  activeDeck_ = std::min(state.active, decks_.size() - 1);
  publishTape();
}

void Processor::continuePlaybackFrom(const Processor& source) noexcept {
  if (decks_[activeDeck_].generation == 0 ||
      decks_[activeDeck_].generation !=
          source.decks_[source.activeDeck_].generation) {
    transportGain_ = 0;
    return;
  }
  readPosition_ = source.readPosition_;
  generationCounter_ = std::max(generationCounter_, source.generationCounter_);
  repeatPosition_ = source.repeatPosition_;
  pitchPhase_ = source.pitchPhase_;
  modulationPhase_ = source.modulationPhase_;
  driftState_ = source.driftState_;
  smoothedMix_ = source.smoothedMix_;
  smoothedOutputGain_ = source.smoothedOutputGain_;
  smoothedSpeed_ = source.smoothedSpeed_;
  smoothedAmplifier_ = source.smoothedAmplifier_;
  smoothedDegradation_ = source.smoothedDegradation_;
  smoothingInitialized_ = source.smoothingInitialized_;
  previousParameters_ = source.previousParameters_;
  repeatTimeline_ = source.repeatTimeline_;
  wasRepeating_ = source.wasRepeating_;
  captureLatched_ = source.captureLatched_;
  transport_ = source.transport_;
  transportGain_ = source.transportGain_;
  lastWetLeft_ = source.lastWetLeft_;
  lastWetRight_ = source.lastWetRight_;
  toneLeft_ = source.toneLeft_;
  toneRight_ = source.toneRight_;
  transitionLeft_ = source.transitionLeft_;
  transitionRight_ = source.transitionRight_;
  transitionRemaining_ = source.transitionRemaining_;
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
  recording_ = captureLatched_ = wasRepeating_ = false;
  repeatPosition_ = 0;
  toneLeft_ = toneRight_ = 0;
  previousParameters_ = {};
  transitionRemaining_ = 0;
  transport_ = Transport::Play;
  transportGain_ = 1;
  lastWetLeft_ = lastWetRight_ = 0;
  meters_ = {};
  publishTape();
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

void Processor::selectGeneration(std::uint32_t generation) noexcept {
  if (printing_ || recording_) return;
  for (std::size_t i = 0; i < decks_.size(); ++i)
    if (generation != 0 && decks_[i].generation == generation) {
      activeDeck_ = i;
      readPosition_ = repeatPosition_ = pitchPhase_ = 0;
      toneLeft_ = toneRight_ = 0;
      transportGain_ = 0;
      return;
    }
}

void Processor::removeGeneration(std::uint32_t generation) noexcept {
  if (printing_ || recording_) return;
  for (std::size_t i = 0; i < decks_.size(); ++i)
    if (i != activeDeck_ && decks_[i].generation == generation) {
      decks_[i].generation = 0;
      decks_[i].length = 0;
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
  const double regionStart = std::min(start, static_cast<double>(length - 1));
  const double regionEnd =
      std::clamp(static_cast<double>(bounded(parameters.end, 0, 1, 1)) * length,
                 regionStart + 1, static_cast<double>(length));
  const double regionLength = parameters.safeWorkflow
                                  ? regionEnd - regionStart
                                  : static_cast<double>(length);
  const double fragment =
      parameters.safeWorkflow && parameters.repeat &&
              parameters.repeatSeconds > 0
          ? std::min(regionLength, parameters.repeatSeconds * sampleRate_)
          : regionLength;
  const double spliceSamples = std::max(
      1.0, static_cast<double>(bounded(parameters.splice, 0.0F, 0.25F, 0.03F)) *
               static_cast<double>(length));
  const auto spliced = [&](double position) {
    if (parameters.safeWorkflow) {
      const double size = parameters.repeat ? fragment : regionLength;
      double local = std::fmod(position, size);
      if (local < 0) local += size;
      const float primary = read(channel, regionStart + local, length);
      const double fadeLength = std::min(spliceSamples, size * .25);
      if (parameters.splice <= 0 || local < size - fadeLength) return primary;
      const float fade =
          static_cast<float>((local - size + fadeLength) / fadeLength);
      return std::lerp(
          primary,
          read(channel, regionStart + local - size + fadeLength, length), fade);
    }
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
  const double position = parameters.safeWorkflow && parameters.repeat
                              ? repeatPosition_
                              : readPosition_;
  if (std::abs(pitch) < 0.001F) {
    return spliced(position);
  }
  const double ratio = std::exp2(static_cast<double>(pitch) / 12.0);
  const double grain = std::min(1024.0, static_cast<double>(length) * 0.5);
  const double direction = ratio >= 1.0 ? 1.0 : -1.0;
  const auto head = [&](double phase) {
    phase -= std::floor(phase);
    const double window = 0.5 - 0.5 * std::cos(2.0 * std::numbers::pi * phase);
    const double offset = direction * phase * grain;
    return static_cast<float>(window) * spliced(position + offset);
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

void Processor::beginReloop(std::size_t length, bool preserveHistory) noexcept {
  if (printing_ || recording_ || length < 2U ||
      decks_[activeDeck_].generation == 0U)
    return;
  const auto sourceGeneration = decks_[activeDeck_].generation;
  for (std::size_t deck = 0; deck < decks_.size(); ++deck) {
    if (!preserveHistory && deck != activeDeck_ &&
        decks_[deck].generation > sourceGeneration) {
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
  if (destination == decks_.size() && !preserveHistory) {
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
                        const Parameters& parameters, bool publish) noexcept {
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
  const auto playbackLength =
      parameters.safeWorkflow ? decks_[activeDeck_].length
                              : std::min(length, decks_[activeDeck_].length);
  const bool playing = parameters.transport == Transport::Play;
  const bool stopped = parameters.transport == Transport::Stop;
  if (parameters.transport == Transport::Release &&
      transport_ != Transport::Release) {
    printing_ = false;
    recording_ = false;
  }
  transport_ = parameters.transport;
  if (parameters.safeWorkflow) {
    if (parameters.start != previousParameters_.start ||
        parameters.end != previousParameters_.end ||
        parameters.reverse != previousParameters_.reverse ||
        parameters.repeat != previousParameters_.repeat ||
        parameters.repeatSeconds != previousParameters_.repeatSeconds) {
      transitionLeft_ = lastWetLeft_;
      transitionRight_ = lastWetRight_;
      transitionRemaining_ = static_cast<std::size_t>(sampleRate_ * .005);
    }
    previousParameters_ = parameters;
    if (!parameters.capture) {
      captureLatched_ = false;
      recording_ = false;
    }
    if (parameters.capture && !captureLatched_ && playing && !printing_) {
      captureLatched_ = true;
      for (std::size_t deck = 0; deck < decks_.size(); ++deck)
        if (decks_[deck].generation == 0) {
          activeDeck_ = deck;
          decks_[deck].length = 0;
          captureLength_ = length;
          writePosition_ = 0;
          recording_ = true;
          break;
        }
    }
    if (parameters.repeat != wasRepeating_) {
      repeatPosition_ = 0;
      if (parameters.repeat)
        repeatTimeline_ = parameters;
      else if (parameters.speed == repeatTimeline_.speed)
        smoothedSpeed_ = bounded(repeatTimeline_.speed, .125F, 4, 1);
    }
    wasRepeating_ = parameters.repeat;
  } else {
    // The original overdub graph does not own a one-shot capture or repeat.
    // A preset/mode change must not leave that ownership blocking RELOOP.
    recording_ = captureLatched_ = wasRepeating_ = false;
  }
  if (parameters.reloop && playing) {
    auto printFrames = playbackLength;
    if (parameters.safeWorkflow) {
      const double region =
          std::max(0.0F, bounded(parameters.end, 0, 1, 1) -
                             bounded(parameters.start, 0, 1, 0));
      double framesToPrint = playbackLength * region;
      if (parameters.repeat && parameters.repeatSeconds > 0)
        framesToPrint =
            std::min(framesToPrint, parameters.repeatSeconds * sampleRate_);
      framesToPrint /= bounded(parameters.speed, .125F, 4, 1);
      printFrames = static_cast<std::size_t>(
          std::clamp(std::ceil(framesToPrint), 0.0,
                     static_cast<double>(decks_[0].left.size())));
    }
    beginReloop(printFrames, parameters.safeWorkflow);
  }
  const bool stereo = right != nullptr;
  const float overdub = bounded(parameters.overdub, 0.0F, 1.0F, 0.5F);
  const float feedback = bounded(parameters.feedback, 0.0F, 1.0F, 0.85F);
  // Feedback controls a bounded release time (T60), even at 100%.
  const float release = static_cast<float>(std::exp(
      -std::log(1000.0) / (sampleRate_ * (0.05 + 7.95 * feedback * feedback))));
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
  if (playing && parameters.capture && !wasCapturing_ &&
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
    const bool recordThisSample = playing && parameters.capture && !printing_ &&
                                  (!parameters.safeWorkflow || recording_);
    const auto currentLength = parameters.safeWorkflow
                                   ? active.length
                                   : std::min(length, active.length);
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
    if (recordThisSample) {
      if (active.generation == 0U) {
        active.generation = ++generationCounter_;
      }
      const auto recordLength =
          parameters.safeWorkflow ? captureLength_ : length;
      writePosition_ %= recordLength;
      const bool hasRecordedSample = writePosition_ < active.length;
      const float existingLeft =
          hasRecordedSample ? active.left[writePosition_] : 0.0F;
      const float existingRight =
          hasRecordedSample ? active.right[writePosition_] : 0.0F;
      active.left[writePosition_] =
          parameters.safeWorkflow ? inputLeft
                                  : clean(inputLeft * (1.0F - overdub) +
                                          existingLeft * feedback * overdub);
      active.right[writePosition_] =
          parameters.safeWorkflow ? inputRight
                                  : clean(inputRight * (1.0F - overdub) +
                                          existingRight * feedback * overdub);
      snapshots_.write(activeDeck_, writePosition_, active.left[writePosition_],
                       active.right[writePosition_]);
      writePosition_ = (writePosition_ + 1U) % recordLength;
      active.length = std::min(recordLength, active.length + 1U);
      if (parameters.safeWorkflow) {
        wetLeft = inputLeft;
        wetRight = inputRight;
        if (writePosition_ == 0) {
          recording_ = false;
          readPosition_ = 0;
          transportGain_ = 0;
        }
      }
    }
    if (parameters.safeWorkflow) {
      Parameters tone = parameters;
      tone.amplifier = smoothedAmplifier_;
      tone.degradation = smoothedDegradation_;
      if (tone.amplifier > .00001F || tone.degradation > .00001F) {
        wetLeft = printSample(wetLeft, toneLeft_, tone);
        wetRight = printSample(wetRight, toneRight_, tone);
      } else {
        toneLeft_ = wetLeft;
        toneRight_ = wetRight;
      }
    } else {
      const float drive = 1.0F + 4.0F * smoothedAmplifier_;
      wetLeft = std::tanh(drive * wetLeft) / drive;
      wetRight = std::tanh(drive * wetRight) / drive;
      const float quantization =
          32768.0F / (1.0F + 63.0F * smoothedDegradation_);
      wetLeft = std::round(wetLeft * quantization) / quantization;
      wetRight = std::round(wetRight * quantization) / quantization;
    }
    if (parameters.safeWorkflow && transitionRemaining_ > 0) {
      const float fade = static_cast<float>(transitionRemaining_) /
                         static_cast<float>(std::max(1.0, sampleRate_ * .005));
      wetLeft = std::lerp(wetLeft, transitionLeft_, fade);
      wetRight = std::lerp(wetRight, transitionRight_, fade);
      --transitionRemaining_;
    }
    if (playing && printing_ && printPosition_ < printLength_) {
      auto& destination = decks_[printingDeck_];
      destination.left[printPosition_] =
          parameters.safeWorkflow
              ? wetLeft
              : printSample(wetLeft, printStateLeft_, parameters);
      destination.right[printPosition_] =
          parameters.safeWorkflow
              ? wetRight
              : printSample(wetRight, printStateRight_, parameters);
      snapshots_.write(printingDeck_, printPosition_,
                       destination.left[printPosition_],
                       destination.right[printPosition_]);
      ++printPosition_;
      destination.length = printPosition_;
      if (printPosition_ >= printLength_) {
        destination.generation = ++generationCounter_;
        if (!parameters.safeWorkflow) {
          activeDeck_ = printingDeck_;
          readPosition_ = 0.0;
          pitchPhase_ = 0.0;
          writePosition_ = 0U;
        }
        printing_ = false;
      }
    }
    if (stopped) {
      wetLeft = lastWetLeft_;
      wetRight = lastWetRight_;
      smooth(transportGain_, 0);
    } else {
      lastWetLeft_ = wetLeft;
      lastWetRight_ = wetRight;
      if (playing)
        smooth(transportGain_, 1);
      else
        transportGain_ *= release;
    }
    if (transportGain_ < 1.e-8F) transportGain_ = 0;
    if (!playing && currentLength < 2U) wetLeft = wetRight = 0;
    wetLeft *= transportGain_;
    wetRight *= transportGain_;
    float outputLeft = inputLeft + smoothedMix_ * (wetLeft - inputLeft);
    float outputRight = inputRight + smoothedMix_ * (wetRight - inputRight);
    if (parameters.safeWorkflow && parameters.monitor && currentLength > 1 &&
        !recordThisSample) {
      outputLeft += inputLeft * smoothedMix_;
      outputRight += inputRight * smoothedMix_;
    }
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
    if (stopped) continue;
    modulationPhase_ += 1.0 / sampleRate_;
    const double period = parameters.legacyBehavior ? 1.0 : 1000.0;
    modulationPhase_ -= period * std::floor(modulationPhase_ / period);
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
    const auto advancingLength =
        parameters.safeWorkflow ? decks_[activeDeck_].length
                                : std::min(length, decks_[activeDeck_].length);
    if (parameters.safeWorkflow && advancingLength > 0) {
      const double first =
          std::min(static_cast<double>(advancingLength - 1),
                   static_cast<double>(bounded(parameters.start, 0, 1, 0)) *
                       advancingLength);
      const double region =
          std::clamp(static_cast<double>(bounded(parameters.end, 0, 1, 1)) *
                         advancingLength,
                     first + 1, static_cast<double>(advancingLength)) -
          first;
      const double fragment =
          parameters.repeatSeconds > 0
              ? std::min(region, parameters.repeatSeconds * sampleRate_)
              : region;
      const auto advance = [speed, modulation](double position, double size) {
        position = std::fmod(position + speed * modulation, size);
        return position < 0 ? position + size : position;
      };
      if (parameters.repeat) {
        const double baseFirst = std::min(
            static_cast<double>(advancingLength - 1),
            static_cast<double>(bounded(repeatTimeline_.start, 0, 1, 0)) *
                advancingLength);
        const double baseLength =
            std::clamp(
                static_cast<double>(bounded(repeatTimeline_.end, 0, 1, 1)) *
                    advancingLength,
                baseFirst + 1, static_cast<double>(advancingLength)) -
            baseFirst;
        const double baseSpeed = bounded(repeatTimeline_.speed, .125F, 4, 1) *
                                 (repeatTimeline_.reverse ? -1 : 1);
        readPosition_ =
            std::fmod(readPosition_ + baseSpeed * modulation, baseLength);
        if (readPosition_ < 0) readPosition_ += baseLength;
      } else {
        readPosition_ = advance(readPosition_, region);
      }
      repeatPosition_ = advance(repeatPosition_, fragment);
    } else {
      readPosition_ = wrap(readPosition_ + speed * modulation, advancingLength);
    }
    const double pitchRatio = std::exp2(
        bounded(parameters.pitchSemitones, -12.0F, 12.0F, 0.0F) / 12.0F);
    pitchPhase_ = wrap(pitchPhase_ + std::abs(pitchRatio - 1.0) / 1024.0, 1U);
  }
  wasCapturing_ = playing && parameters.capture;
  const auto& active = decks_[activeDeck_];
  const auto activeLength =
      parameters.safeWorkflow ? active.length : std::min(length, active.length);
  measured.position = activeLength > 0U
                          ? static_cast<float>(readPosition_ / activeLength)
                          : 0.0F;
  if (parameters.safeWorkflow && activeLength > 0) {
    const double first = std::min(
        static_cast<double>(activeLength - 1),
        static_cast<double>(bounded(parameters.start, 0, 1, 0)) * activeLength);
    measured.position = static_cast<float>(
        (first + (parameters.repeat ? repeatPosition_ : readPosition_)) /
        activeLength);
  }
  measured.captured = static_cast<float>(activeLength) /
                      static_cast<float>(std::max<std::size_t>(1U, length));
  if (parameters.safeWorkflow)
    measured.captured =
        recording_
            ? static_cast<float>(activeLength) /
                  static_cast<float>(std::max<std::size_t>(1, captureLength_))
        : activeLength > 0 ? 1.F
                           : 0.F;
  measured.printing = printing_ && printLength_ > 0U
                          ? static_cast<float>(printPosition_) /
                                static_cast<float>(printLength_)
                          : 0.0F;
  measured.generation = active.generation;
  measured.retainedGenerations =
      static_cast<std::uint32_t>(retainedGenerations());
  measured.activeDeck = activeDeck_;
  measured.recording =
      parameters.safeWorkflow ? recording_ : playing && parameters.capture;
  measured.busy = printing_;
  measured.historyFull =
      parameters.safeWorkflow && retainedGenerations() == decks_.size();
  for (const auto& deck : decks_) {
    if (deck.generation == 0) continue;
    if (measured.oldestGeneration == 0 ||
        deck.generation < measured.oldestGeneration)
      measured.oldestGeneration = deck.generation;
    measured.newestGeneration =
        std::max(measured.newestGeneration, deck.generation);
  }
  meters_ = measured;
  if (publish) publishTape();
}

}  // namespace aste::loop
