#include "sequence_processor.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace aste::sequence {
namespace {

float finiteOr(float value, float fallback) noexcept {
  return std::isfinite(value) ? value : fallback;
}

float bounded(float value, float low, float high, float fallback) noexcept {
  return std::clamp(finiteOr(value, fallback), low, high);
}

float flush(float value) noexcept {
  return !std::isfinite(value) ||
                 std::abs(value) < std::numeric_limits<float>::min()
             ? 0.0F
             : value;
}

float midiFrequency(int note) noexcept {
  return 440.0F *
         std::exp2((static_cast<float>(std::clamp(note, 0, 127)) - 69.0F) /
                   12.0F);
}

float dbToGain(float db) noexcept {
  return std::exp(bounded(db, -24.0F, 6.0F, -6.0F) * 0.11512925464970229F);
}

float polyBlep(float phase, float increment) noexcept {
  if (increment <= 0.0F) {
    return 0.0F;
  }
  if (phase < increment) {
    const float x = phase / increment;
    return x + x - x * x - 1.0F;
  }
  if (phase > 1.0F - increment) {
    const float x = (phase - 1.0F) / increment;
    return x * x + x + x + 1.0F;
  }
  return 0.0F;
}

float smooth(float current, float target, float coefficient) noexcept {
  const float result = target + coefficient * (current - target);
  return std::abs(result - target) < 1.0e-8F ? target : result;
}

double stepLengthPpq(int division) noexcept {
  switch (division) {
    case 8:
      return 0.5;
    case 32:
      return 0.125;
    default:
      return 0.25;
  }
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

void Processor::Envelope::noteOn(bool retrigger) noexcept {
  if (retrigger || stage_ == Stage::idle) {
    stage_ = Stage::attack;
  }
}

void Processor::Envelope::noteOff() noexcept {
  if (stage_ != Stage::idle) {
    stage_ = Stage::release;
  }
}

float Processor::Envelope::process(double sampleRate, float attackMs,
                                   float decayMs, float sustain,
                                   float releaseMs) noexcept {
  attackMs = bounded(attackMs, 0.2F, 2000.0F, 3.0F);
  decayMs = bounded(decayMs, 5.0F, 4000.0F, 180.0F);
  sustain = bounded(sustain, 0.0F, 1.0F, 0.55F);
  releaseMs = bounded(releaseMs, 5.0F, 5000.0F, 120.0F);
  const auto approach = [sampleRate](float current, float target,
                                     float milliseconds) {
    const double seconds = std::max(0.0001, milliseconds * 0.001);
    const float coefficient =
        static_cast<float>(std::exp(-1.0 / (seconds * sampleRate)));
    return target + coefficient * (current - target);
  };
  switch (stage_) {
    case Stage::idle:
      value_ = 0.0F;
      break;
    case Stage::attack:
      value_ = approach(value_, 1.0F, attackMs * 0.25F);
      if (value_ > 0.999F) {
        value_ = 1.0F;
        stage_ = Stage::decay;
      }
      break;
    case Stage::decay:
      value_ = approach(value_, sustain, decayMs * 0.25F);
      if (std::abs(value_ - sustain) < 0.001F) {
        value_ = sustain;
        stage_ = Stage::sustain;
      }
      break;
    case Stage::sustain:
      value_ = sustain;
      break;
    case Stage::release:
      value_ = approach(value_, 0.0F, releaseMs * 0.25F);
      if (value_ < 1.0e-5F) {
        value_ = 0.0F;
        stage_ = Stage::idle;
      }
      break;
  }
  return flush(value_);
}

void Processor::Envelope::reset() noexcept {
  stage_ = Stage::idle;
  value_ = 0.0F;
}

float Processor::CharacterFilter::process(float input, float cutoff,
                                          float resonance, float weight,
                                          float drive, float loading,
                                          double sampleRate) noexcept {
  cutoff =
      bounded(cutoff, 20.0F, static_cast<float>(sampleRate * 0.42), 900.0F);
  resonance = bounded(resonance, 0.0F, 1.0F, 0.35F);
  weight = bounded(weight, 0.0F, 1.0F, 0.45F);
  drive = bounded(drive, 0.0F, 1.0F, 0.25F);
  loading = bounded(loading, 0.0F, 1.0F, 0.0F);
  const float coefficient =
      1.0F - static_cast<float>(
                 std::exp(-2.0 * std::numbers::pi * cutoff / sampleRate));
  const float feedback = (2.35F + 1.25F * weight + 0.25F * loading) * resonance;
  const float inputGain = 1.0F + 5.0F * drive + 1.25F * loading;
  float stage = std::tanh(inputGain * (input - feedback * state_[3]));
  for (auto& state : state_) {
    state = flush(state + coefficient * (stage - state));
    stage = state;
  }
  const float twoPole = state_[1];
  const float fourPole = state_[3];
  const float response = twoPole + weight * (fourPole - twoPole);
  const float compensation =
      (1.0F + resonance * (0.45F + 0.30F * weight)) / std::sqrt(inputGain);
  return flush(response * compensation);
}

PressureMapping Processor::mapPressure(float amount) noexcept {
  amount = bounded(amount, 0.0F, 1.0F, 0.35F);
  return {
      .mixerDrive = 1.0F + 4.5F * amount,
      .envelopeDepth = 1.0F + 0.65F * amount,
      .accentGain = 0.12F + 0.38F * amount,
      .filterLoading = amount,
  };
}

void Processor::prepare(double sampleRate, const Parameters& initial) noexcept {
  sampleRate_ = std::clamp(std::isfinite(sampleRate) ? sampleRate : 48000.0,
                           8000.0, 384000.0);
  pressure_.prepare(sampleRate_, 0.010,
                    bounded(initial.pressure, 0.0F, 1.0F, 0.35F));
  shape_.prepare(sampleRate_, 0.010, bounded(initial.shape, 0.0F, 1.0F, 0.25F));
  pulseWidth_.prepare(sampleRate_, 0.010,
                      bounded(initial.pulseWidth, 0.1F, 0.9F, 0.5F));
  mix_.prepare(sampleRate_, 0.010,
               bounded(initial.oscillatorMix, 0.0F, 1.0F, 0.45F));
  sub_.prepare(sampleRate_, 0.010,
               bounded(initial.subLevel, 0.0F, 1.0F, 0.25F));
  cutoff_.prepare(sampleRate_, 0.015,
                  bounded(initial.cutoffHz, 30.0F, 18000.0F, 900.0F));
  resonance_.prepare(sampleRate_, 0.015,
                     bounded(initial.resonance, 0.0F, 1.0F, 0.35F));
  filterMorph_.prepare(sampleRate_, 0.020,
                       bounded(initial.filterMorph, 0.0F, 1.0F, 0.45F));
  filterDrive_.prepare(sampleRate_, 0.010,
                       bounded(initial.filterDrive, 0.0F, 1.0F, 0.25F));
  output_.prepare(sampleRate_, 0.005,
                  bounded(initial.outputDb, -24.0F, 6.0F, -6.0F));
  frequency_.prepare(sampleRate_, 0.001, midiFrequency(initial.rootNote));
  reset();
}

void Processor::reset() noexcept {
  phase1_ = 0.0;
  phase2_ = 0.17;
  subPhase_ = 0.0;
  envelope_.reset();
  characterFilter_.reset();
  heldNotes_.fill(false);
  velocity_ = 1.0F;
  accented_ = false;
  lastMidiNote_ = -1;
  absoluteStep_ = -1;
  currentStep_ = -1;
  sequenceGate_ = false;
  wasPlaying_ = false;
  meters_ = {};
  meters_.currentStep = -1.0F;
}

int Processor::heldNote() const noexcept {
  for (int note = 127; note >= 0; --note) {
    if (heldNotes_[static_cast<std::size_t>(note)]) {
      return note;
    }
  }
  return -1;
}

void Processor::startNote(int note, float velocity, bool accent, bool slide,
                          float glideMs) noexcept {
  const float target = midiFrequency(note);
  if (!slide || !envelope_.active()) {
    frequency_.snap(target);
  } else {
    const double seconds = bounded(glideMs, 0.0F, 1000.0F, 70.0F) * 0.001;
    frequency_.coefficient_ = static_cast<float>(
        seconds <= 0.0 ? 0.0 : std::exp(-1.0 / (seconds * sampleRate_)));
    frequency_.setTarget(target);
  }
  velocity_ = bounded(velocity, 0.0F, 1.0F, 1.0F);
  accented_ = accent;
  envelope_.noteOn(!slide);
}

void Processor::stopNote() noexcept { envelope_.noteOff(); }

void Processor::handleMidi(const MidiEvent& event, bool sequencerRunning,
                           float glideMs) noexcept {
  const int note = std::clamp(event.note, 0, 127);
  switch (event.type) {
    case MidiEventType::noteOn:
      if (event.velocity <= 0.0F) {
        heldNotes_[static_cast<std::size_t>(note)] = false;
        break;
      }
      heldNotes_[static_cast<std::size_t>(note)] = true;
      lastMidiNote_ = note;
      if (!sequencerRunning) {
        startNote(note, event.velocity, event.velocity > 0.9F,
                  envelope_.active(), glideMs);
      }
      break;
    case MidiEventType::noteOff:
      heldNotes_[static_cast<std::size_t>(note)] = false;
      if (lastMidiNote_ == note) {
        lastMidiNote_ = heldNote();
        if (!sequencerRunning) {
          if (lastMidiNote_ >= 0) {
            startNote(lastMidiNote_, 1.0F, false, true, glideMs);
          } else {
            stopNote();
          }
        }
      }
      break;
    case MidiEventType::allNotesOff:
      heldNotes_.fill(false);
      lastMidiNote_ = -1;
      if (!sequencerRunning) {
        stopNote();
      }
      break;
  }
}

void Processor::updateSequencer(const Parameters& parameters,
                                const Transport& transport,
                                std::size_t sample) noexcept {
  const bool running = parameters.sequenceEnabled && transport.valid &&
                       transport.playing && std::isfinite(transport.bpm) &&
                       transport.bpm > 0.0 &&
                       std::isfinite(transport.ppqPosition);
  if (!running) {
    if (wasPlaying_) {
      stopNote();
    }
    absoluteStep_ = -1;
    currentStep_ = -1;
    sequenceGate_ = false;
    wasPlaying_ = false;
    return;
  }
  const double stepLength = stepLengthPpq(parameters.division);
  const double ppq = transport.ppqPosition + static_cast<double>(sample) *
                                                 transport.bpm /
                                                 (60.0 * sampleRate_);
  const double stepPosition = ppq / stepLength;
  const auto absolute = static_cast<long long>(std::floor(stepPosition));
  int step = static_cast<int>(absolute % static_cast<long long>(kStepCount));
  if (step < 0) {
    step += static_cast<int>(kStepCount);
  }
  const auto& programmed = parameters.steps[static_cast<std::size_t>(step)];
  if (absolute != absoluteStep_) {
    const bool slideFromPrevious =
        wasPlaying_ && currentStep_ >= 0 && sequenceGate_ &&
        parameters.steps[static_cast<std::size_t>(currentStep_)].slide;
    absoluteStep_ = absolute;
    currentStep_ = step;
    sequenceGate_ = programmed.gate;
    if (programmed.gate) {
      const int transpose = lastMidiNote_ >= 0 ? lastMidiNote_ - 60 : 0;
      startNote(
          std::clamp(parameters.rootNote + programmed.note + transpose, 0, 127),
          programmed.accent ? 1.0F : 0.78F, programmed.accent,
          slideFromPrevious, parameters.glideMs);
    } else {
      stopNote();
    }
  }
  const double phase = stepPosition - std::floor(stepPosition);
  if (sequenceGate_ && phase >= 0.78 && !programmed.slide) {
    stopNote();
    sequenceGate_ = false;
  }
  wasPlaying_ = true;
}

float Processor::oscillator(double& phase, float frequency, float shape,
                            float pulseWidth) noexcept {
  const float increment =
      bounded(frequency / static_cast<float>(sampleRate_), 0.0F, 0.45F, 0.0F);
  const float current = static_cast<float>(phase);
  const float saw = 2.0F * current - 1.0F - polyBlep(current, increment);
  pulseWidth = bounded(pulseWidth, 0.1F, 0.9F, 0.5F);
  const float wrapped = current >= pulseWidth ? current - pulseWidth
                                              : current + 1.0F - pulseWidth;
  const float pulse = (current < pulseWidth ? 1.0F : -1.0F) +
                      polyBlep(current, increment) -
                      polyBlep(wrapped, increment);
  phase += increment;
  phase -= std::floor(phase);
  const float sine = std::sin(2.0F * std::numbers::pi_v<float> * current);
  shape = bounded(shape, 0.0F, 1.0F, 0.25F);
  if (shape < 0.5F) {
    return saw + (shape * 2.0F) * (pulse - saw);
  }
  return pulse + ((shape - 0.5F) * 2.0F) * (sine - pulse);
}

void Processor::process(float* left, float* right, std::size_t frames,
                        const Parameters& parameters,
                        const Transport& transport,
                        std::span<const MidiEvent> midi) noexcept {
  if (left == nullptr || frames == 0U) {
    meters_ = {};
    meters_.currentStep = static_cast<float>(currentStep_);
    return;
  }
  pressure_.setTarget(bounded(parameters.pressure, 0.0F, 1.0F, 0.35F));
  shape_.setTarget(bounded(parameters.shape, 0.0F, 1.0F, 0.25F));
  pulseWidth_.setTarget(bounded(parameters.pulseWidth, 0.1F, 0.9F, 0.5F));
  mix_.setTarget(bounded(parameters.oscillatorMix, 0.0F, 1.0F, 0.45F));
  sub_.setTarget(bounded(parameters.subLevel, 0.0F, 1.0F, 0.25F));
  cutoff_.setTarget(bounded(parameters.cutoffHz, 30.0F, 18000.0F, 900.0F));
  resonance_.setTarget(bounded(parameters.resonance, 0.0F, 1.0F, 0.35F));
  filterMorph_.setTarget(bounded(parameters.filterMorph, 0.0F, 1.0F, 0.45F));
  filterDrive_.setTarget(bounded(parameters.filterDrive, 0.0F, 1.0F, 0.25F));
  output_.setTarget(bounded(parameters.outputDb, -24.0F, 6.0F, -6.0F));
  const bool sequencerRunning =
      parameters.sequenceEnabled && transport.valid && transport.playing;
  std::size_t midiIndex = 0;
  MeterSnapshot measured{};
  measured.currentStep = static_cast<float>(currentStep_);
  for (std::size_t sample = 0; sample < frames; ++sample) {
    while (midiIndex < midi.size() && midi[midiIndex].sampleOffset <= sample) {
      handleMidi(midi[midiIndex], sequencerRunning, parameters.glideMs);
      ++midiIndex;
    }
    updateSequencer(parameters, transport, sample);
    const float envelope =
        envelope_.process(sampleRate_, parameters.attackMs, parameters.decayMs,
                          parameters.sustain, parameters.releaseMs);
    const float frequency = frequency_.next();
    const float pressure = pressure_.next();
    const auto mapping = mapPressure(pressure);
    const float shape = shape_.next();
    const float pulseWidth = pulseWidth_.next();
    const float osc1 = oscillator(phase1_, frequency, shape, pulseWidth);
    const float detune =
        bounded(parameters.detuneSemitones, -12.0F, 12.0F, 0.08F);
    const float osc2 =
        oscillator(phase2_, frequency * std::exp2(detune / 12.0F), shape,
                   1.0F - pulseWidth);
    const float mix = mix_.next();
    const float subFrequency = frequency * 0.5F;
    const float subOsc = oscillator(subPhase_, subFrequency, 1.0F, 0.5F);
    float source =
        (1.0F - mix) * osc1 + mix * osc2 + 0.55F * sub_.next() * subOsc;
    source = std::tanh(mapping.mixerDrive * source) /
             std::max(1.0F, mapping.mixerDrive * 0.72F);
    const float envelopeAmount =
        bounded(parameters.envelopeAmount, 0.0F, 1.0F, 0.55F);
    const float cutoff =
        cutoff_.next() *
        std::exp2(5.0F * envelope * envelopeAmount * mapping.envelopeDepth);
    const float resonance = resonance_.next();
    const float outputFromFilter = characterFilter_.process(
        source, cutoff, resonance, filterMorph_.next(), filterDrive_.next(),
        mapping.filterLoading, sampleRate_);
    float output = outputFromFilter;
    const float accent = accented_ ? mapping.accentGain : 0.0F;
    output *= envelope * velocity_ * (1.0F + accent) * dbToGain(output_.next());
    output = flush(std::tanh(1.35F * output) / std::tanh(1.35F));
    if (parameters.bypass) {
      output = 0.0F;
    }
    left[sample] = output;
    if (right != nullptr) {
      right[sample] = output;
    }
    measured.outputPeak = std::max(measured.outputPeak, std::abs(output));
    measured.envelope = std::max(measured.envelope, envelope);
    measured.currentStep = static_cast<float>(currentStep_);
  }
  meters_ = measured;
}

}  // namespace aste::sequence
