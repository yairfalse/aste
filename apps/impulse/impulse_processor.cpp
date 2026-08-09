#include "impulse_processor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace aste::impulse {
namespace {
float finite(float value, float fallback) noexcept {
  return std::isfinite(value) ? value : fallback;
}
float bounded(float value, float low, float high, float fallback) noexcept {
  return std::clamp(finite(value, fallback), low, high);
}
float clean(float value) noexcept {
  if (!std::isfinite(value) ||
      std::abs(value) < std::numeric_limits<float>::min())
    return 0.0F;
  return std::clamp(value, -4.0F, 4.0F);
}
float dbGain(float value) noexcept {
  return std::exp(bounded(value, -24.0F, 12.0F, -6.0F) * 0.11512925464970229F);
}
double divisionPpq(int value) noexcept {
  constexpr std::array divisions{0.5, 0.25, 0.125};
  return divisions[static_cast<std::size_t>(std::clamp(value, 0, 2))];
}
}  // namespace

void Processor::prepare(double sampleRate, const Parameters&) noexcept {
  sampleRate_ = std::clamp(std::isfinite(sampleRate) ? sampleRate : 48000.0,
                           8000.0, 384000.0);
  outputSmoothing_ =
      static_cast<float>(1.0 - std::exp(-1.0 / (0.005 * sampleRate_)));
  reset();
}

void Processor::reset() noexcept {
  voices_ = {};
  lastTick_.fill(-1);
  expectedPpq_ = 0.0;
  transportKnown_ = false;
  smoothedOutputGain_ = 1.0F;
  meters_ = {};
}

std::uint32_t Processor::hash(std::uint32_t value) noexcept {
  value ^= value >> 16U;
  value *= 0x7feb352dU;
  value ^= value >> 15U;
  value *= 0x846ca68bU;
  return value ^ (value >> 16U);
}

float Processor::random01(std::uint32_t value) noexcept {
  return static_cast<float>(hash(value) >> 8U) * (1.0F / 16777216.0F);
}

bool Processor::euclidean(int step, int pulses, int length,
                          int rotation) noexcept {
  length = std::clamp(length, 1, 32);
  pulses = std::clamp(pulses, 0, length);
  if (pulses == 0) return false;
  const int rotated = ((step + rotation) % length + length) % length;
  return (rotated * pulses) % length < pulses;
}

void Processor::trigger(std::size_t track, float velocity,
                        const TrackParameters& parameters, float energy,
                        float variation, std::uint32_t random) noexcept {
  auto& voice = voices_[track];
  const float bipolar = random01(random) * 2.0F - 1.0F;
  const float spread = bounded(variation, 0.0F, 1.0F, 0.12F);
  voice.phase = 0.0;
  voice.secondPhase = 0.0;
  voice.amplitude = bounded(velocity, 0.0F, 1.0F, 1.0F);
  voice.pitchEnvelope = track == 0 ? 1.0F : 0.0F;
  voice.noiseState = bipolar;
  voice.filterState = 0.0F;
  voice.level = bounded(parameters.level, 0.0F, 1.0F, 0.75F);
  voice.pitch = bounded(parameters.pitchHz, 25.0F, 10000.0F, 60.0F) *
                std::exp2(bipolar * spread * 0.08F);
  voice.decay = std::exp(
      -1.0F / (0.001F * bounded(parameters.decayMs, 5.0F, 3000.0F, 300.0F) *
               sampleRate_));
  voice.tone = bounded(parameters.tone, 0.0F, 1.0F, 0.5F);
  voice.drive = bounded(parameters.drive, 0.0F, 1.0F, 0.25F) +
                0.35F * bounded(energy, 0.0F, 1.0F, 0.45F);
  voice.pan =
      std::clamp(bipolar * spread * (track == 0 ? 0.05F : 0.55F), -0.8F, 0.8F);
}

float Processor::renderVoice(std::size_t track) noexcept {
  auto& voice = voices_[track];
  if (voice.amplitude < 1.0e-7F) return 0.0F;
  const double pitch =
      std::clamp(static_cast<double>(voice.pitch) *
                     (track == 0 ? 1.0 + 2.8 * voice.pitchEnvelope : 1.0),
                 20.0, 0.45 * sampleRate_);
  voice.phase += pitch / sampleRate_;
  voice.phase -= std::floor(voice.phase);
  voice.secondPhase += pitch * (track == 3 ? 1.618 : 1.37) / sampleRate_;
  voice.secondPhase -= std::floor(voice.secondPhase);
  voice.noiseState = clean(voice.noiseState * 3.9876543F + 0.1234567F);
  voice.noiseState -= std::floor(voice.noiseState);
  voice.noiseState = voice.noiseState * 2.0F - 1.0F;
  const float sine =
      std::sin(static_cast<float>(voice.phase * 2.0 * std::numbers::pi));
  const float second =
      std::sin(static_cast<float>(voice.secondPhase * 2.0 * std::numbers::pi));
  float sample{};
  if (track == 0) {
    const float click = voice.pitchEnvelope > 0.72F ? voice.noiseState : 0.0F;
    sample = sine + 0.18F * voice.tone * click;
    voice.pitchEnvelope *= 0.992F;
  } else if (track == 1) {
    sample =
        (voice.phase < 0.08 ? 1.0F : -0.12F) * (0.35F + 0.65F * voice.tone);
  } else if (track == 2) {
    const float coefficient = 0.03F + 0.35F * voice.tone;
    voice.filterState += coefficient * (voice.noiseState - voice.filterState);
    sample = voice.filterState + 0.25F * second;
  } else {
    sample = sine + (0.25F + 0.45F * voice.tone) * second;
  }
  voice.amplitude *= voice.decay;
  const float driven = std::tanh(sample * (1.0F + 5.0F * voice.drive));
  return clean(driven * voice.amplitude * voice.level);
}

void Processor::process(float* left, float* right, std::size_t frames,
                        const Parameters& parameters,
                        const Transport& transport,
                        std::span<const MidiEvent> midi) noexcept {
  if (left == nullptr || frames == 0U) {
    meters_ = {};
    return;
  }
  MeterSnapshot measured{};
  const double bpm = std::clamp(
      std::isfinite(transport.bpm) ? transport.bpm : 120.0, 20.0, 400.0);
  const double ppqIncrement = bpm / (60.0 * sampleRate_);
  const double stepDuration = divisionPpq(parameters.division);
  const bool discontinuity =
      !transportKnown_ || std::abs(transport.ppq - expectedPpq_) >
                              std::max(0.02, ppqIncrement * frames * 2.0);
  if (discontinuity) lastTick_.fill(-1);
  transportKnown_ = transport.playing;
  std::size_t midiIndex{};
  for (std::size_t sampleIndex = 0; sampleIndex < frames; ++sampleIndex) {
    while (midiIndex < midi.size() && midi[midiIndex].offset <= sampleIndex) {
      const auto& event = midi[midiIndex++];
      if (event.noteOn && event.velocity > 0.0F && event.note >= 36 &&
          event.note < 40) {
        const auto track = static_cast<std::size_t>(event.note - 36);
        trigger(track, event.velocity, parameters.tracks[track],
                parameters.energy, parameters.variation,
                parameters.seed ^ static_cast<std::uint32_t>(sampleIndex));
        measured.triggered[track] = true;
      }
    }
    const double ppq = transport.ppq + ppqIncrement * sampleIndex;
    if (transport.playing && parameters.sequenceEnabled) {
      for (std::size_t track = 0; track < kTrackCount; ++track) {
        const auto& trackParameters = parameters.tracks[track];
        const int length = std::clamp(trackParameters.length, 1, 32);
        const int ratchet = std::clamp(trackParameters.ratchet, 1, 4);
        const double shifted =
            ppq -
            bounded(trackParameters.timing, -0.49F, 0.49F, 0.0F) * stepDuration;
        const auto tick = static_cast<std::int64_t>(
            std::floor(shifted / (stepDuration / ratchet)));
        const auto absoluteStep = tick >= 0 ? tick / ratchet : -1;
        const int step =
            absoluteStep >= 0 ? static_cast<int>(absoluteStep % length) : -1;
        measured.currentStep[track] = step;
        if (tick == lastTick_[track] || step < 0) continue;
        lastTick_[track] = tick;
        const int cycle = static_cast<int>(absoluteStep / length);
        const int condition = std::clamp(trackParameters.condition, 1, 4);
        bool active = euclidean(step, trackParameters.pulses, length,
                                trackParameters.rotation) &&
                      cycle % condition == 0;
        const std::uint32_t eventKey =
            parameters.seed ^ static_cast<std::uint32_t>(track * 0x9e3779b9U) ^
            static_cast<std::uint32_t>(absoluteStep * 0x85ebca6bU) ^
            static_cast<std::uint32_t>(tick);
        if (random01(eventKey ^ 0xa341316cU) <
            bounded(parameters.mutation, 0.0F, 1.0F, 0.0F) * 0.35F)
          active = !active;
        if (active && random01(eventKey) <= bounded(trackParameters.probability,
                                                    0.0F, 1.0F, 1.0F)) {
          const float accent =
              step == 0 ? bounded(trackParameters.accent, 0.0F, 1.0F, 0.35F)
                        : 0.0F;
          trigger(track, 0.7F + 0.3F * accent, trackParameters,
                  parameters.energy, parameters.variation, eventKey);
          measured.triggered[track] = true;
        }
      }
    }
    float mono{};
    float side{};
    for (std::size_t track = 0; track < kTrackCount; ++track) {
      const float signal = renderVoice(track);
      mono += signal;
      side += signal * voices_[track].pan;
    }
    const float energy = bounded(parameters.energy, 0.0F, 1.0F, 0.45F);
    const float protectedSignal =
        std::tanh(mono * (1.0F + 2.5F * energy)) / (1.0F + 0.7F * energy);
    const float targetGain = dbGain(parameters.outputDb);
    smoothedOutputGain_ +=
        outputSmoothing_ * (targetGain - smoothedOutputGain_);
    float outputLeft =
        clean((protectedSignal - 0.35F * side) * smoothedOutputGain_);
    float outputRight =
        clean((protectedSignal + 0.35F * side) * smoothedOutputGain_);
    if (parameters.bypass) outputLeft = outputRight = 0.0F;
    left[sampleIndex] = outputLeft;
    if (right != nullptr) right[sampleIndex] = outputRight;
    measured.outputPeak =
        std::max(measured.outputPeak,
                 std::max(std::abs(outputLeft), std::abs(outputRight)));
  }
  expectedPpq_ = transport.ppq + ppqIncrement * frames;
  meters_ = measured;
}

}  // namespace aste::impulse
