#pragma once
#include "audio.hpp"
#include "apps/density/density_processor.hpp"
#include "apps/harmonic/harmonic_processor.hpp"
#include "apps/sequence/sequence_processor.hpp"
#include "apps/loop/loop_processor.hpp"
#include "apps/impulse/impulse_processor.hpp"
#include "apps/field/field_processor.hpp"
#include "candidate/sequence/sequence_processor.hpp"
#include "candidate/loop/loop_processor.hpp"
#include <memory>
#include <span>

namespace power {
inline constexpr std::array productNames{"density", "harmonic", "sequence",
                                         "loop",    "impulse",  "field"};
inline constexpr std::array settingNames{
    "default", "conservative",    "character_candidate",
    "extreme", "fast_automation", "interacting_controls"};
inline aste::impulse::Parameters impulseStarter() {
  aste::impulse::Parameters p;
  p.variation = 0;
  for (auto& t : p.tracks) {
    t.length = 16;
    t.probability = 1;
    t.rotation = 0;
    t.pulses = 0;
    t.pattern.fill(0);
  }
  p.tracks[0].pattern[0] = 2;
  p.tracks[0].pattern[8] = 1;
  p.tracks[0].pulses = 2;
  for (int n : {2, 6, 10, 14})
    p.tracks[1].pattern[static_cast<std::size_t>(n)] = 1;
  p.tracks[1].pulses = 4;
  for (int n : {4, 12}) p.tracks[5].pattern[static_cast<std::size_t>(n)] = 1;
  p.tracks[5].pulses = 2;
  return p;
}
inline std::size_t chunk(std::size_t at, std::size_t frames, std::size_t block,
                         double fs) {
  if (!block) throw std::runtime_error("zero block");
  const double tick = std::floor(at * 40.0 / fs + 1e-9);
  const auto next =
      static_cast<std::size_t>(std::ceil((tick + 1) * fs / 40.0 - 1e-9));
  return std::min({block, frames - at, next - at});
}
inline aste::density::Parameters densityParameters(int setting, double t) {
  aste::density::Parameters p;
  if (setting == 1) {
    p.density = .2F;
    p.crush = .3F;
    p.blend = .5F;
    p.outputDb = 0;
  }
  if (setting == 2) {
    p.density = .8F;
    p.crush = .9F;
    p.driveDb = 6;
    p.attackMs = 10;
    p.releaseMs = 120;
    p.blend = .8F;
  }
  if (setting == 3) {
    p.density = 1;
    p.crush = 1;
    p.driveDb = 24;
    p.attackMs = .1F;
    p.releaseMs = 30;
    p.blend = 1;
  }
  if (setting == 4) {
    const bool high = static_cast<int>(t * 40 + 1e-9) % 2;
    p.density = high ? 1 : 0;
    p.driveDb = high ? 12 : 0;
    p.blend = high ? 1 : .2F;
  }
  if (setting == 5) {
    p.density = .9F;
    p.crush = 1;
    p.driveDb = 12;
    p.attackMs = 25;
    p.releaseMs = 600;
    p.blend = .5F;
    p.detectorHpfHz = 250;
    p.stereoLink = 0;
  }
  return p;
}
inline aste::harmonic::Parameters harmonicParameters(int setting, double t,
                                                     bool linked = false) {
  aste::harmonic::Parameters p;
  if (setting == 1) {
    p.foundationGainDb = 2;
    p.presenceGainDb = 2;
    p.harmonic = .2F;
  }
  if (setting == 2) {
    p.foundationGainDb = 6;
    p.bodyGainDb = -3;
    p.presenceGainDb = 6;
    p.airGainDb = 3;
    p.harmonic = 1;
  }
  if (setting == 3) {
    p.foundationGainDb = 12;
    p.bodyGainDb = 12;
    p.presenceGainDb = 12;
    p.airGainDb = 12;
    p.harmonic = 1;
  }
  if (setting == 4) {
    const bool high = static_cast<int>(t * 40 + 1e-9) % 2;
    p.bodyGainDb = high ? 12 : -12;
    p.presenceGainDb = high ? -12 : 12;
    p.harmonic = high ? 1 : 0;
  }
  if (setting == 5) {
    p.foundationGainDb = -9;
    p.bodyGainDb = 9;
    p.presenceGainDb = -9;
    p.airGainDb = 9;
    p.inputDb = 6;
    p.outputDb = -6;
    p.harmonic = 1;
  }
  if (linked) {
    p.inputDb = 12;
    p.outputDb = -12;
  }
  return p;
}
template <class P>
P sequenceParameters(int setting, double t, int phrase) {
  P p;
  p.sequenceEnabled = phrase == 0;
  if (setting == 1) {
    p.pressure = 0;
    p.filterDrive = 0;
    p.resonance = .1F;
    p.cutoffHz = 500;
    p.subLevel = .6F;
  }
  if (setting == 2) {
    p.pressure = .7F;
    p.resonance = .65F;
    p.cutoffHz = 1500;
    p.filterDrive = .5F;
    p.filterMorph = .8F;
    p.decayMs = 100;
    p.sustain = .3F;
  }
  if (setting == 3) {
    p.pressure = 1;
    p.resonance = 1;
    p.cutoffHz = 6500;
    p.filterDrive = 1;
    p.filterMorph = 1;
    p.detuneSemitones = 7;
    p.subLevel = 1;
    p.shape = 1;
  }
  if (setting == 4) {
    const bool high = static_cast<int>(t * 40 + 1e-9) % 2;
    p.cutoffHz = high ? 12000 : 120;
    p.pressure = high ? 1 : 0;
    p.filterMorph = high ? 1 : 0;
  }
  if (setting == 5) {
    p.shape = .8F;
    p.pulseWidth = .15F;
    p.detuneSemitones = -7;
    p.oscillatorMix = .8F;
    p.subLevel = .7F;
    p.cutoffHz = 450;
    p.resonance = .8F;
    p.filterDrive = .8F;
    p.filterMorph = .1F;
    p.envelopeAmount = 1;
  }
  return p;
}
template <class E>
std::vector<E> phraseEvents(double fs, int phrase) {
  std::vector<E> result;
  using T = decltype(E{}.type);
  if (!phrase) return result;
  for (int i = 0; i < 24; ++i) {
    const double t = .1 + i * .24;
    const int note =
        36 +
        std::array{0, 3, 7, 12, 7, -5, 3, 10}[static_cast<std::size_t>(i) % 8];
    result.push_back(E{static_cast<std::size_t>(t * fs), T::noteOn, note,
                       i % 4 == 0 ? .95F : .6F});
    result.push_back(
        E{static_cast<std::size_t>((t + (phrase == 1 ? .10 : .28)) * fs),
          T::noteOff, note, 0});
  }
  std::stable_sort(result.begin(), result.end(), [](const E& a, const E& b) {
    return a.sampleOffset < b.sampleOffset;
  });
  return result;
}
template <class Engine, class P, class E, class T>
Audio sequenceRender(int setting, int phrase, double fs, std::size_t block) {
  Audio a(static_cast<std::size_t>(fs * 8), fs);
  Engine e;
  e.prepare(fs, sequenceParameters<P>(setting, 0, phrase));
  const auto events = phraseEvents<E>(fs, phrase);
  std::size_t event{};
  for (std::size_t at = 0; at < a.left.size();) {
    auto n = chunk(at, a.left.size(), block, fs);
    std::array<E, 64> batch{};
    std::size_t count{};
    while (event < events.size() && events[event].sampleOffset < at + n) {
      batch[count] = events[event++];
      batch[count++].sampleOffset -= at;
    }
    const T transport{true, at < static_cast<std::size_t>(6 * fs), 120,
                      at / fs * 2};
    e.process(a.left.data() + at, a.right.data() + at, n,
              sequenceParameters<P>(setting, at / fs, phrase), transport,
              std::span(batch).first(count));
    at += n;
  }
  return a;
}
template <class P>
P loopParameters(int setting, double t) {
  P p;
  p.capture = t < 2;
  p.loopLengthSeconds = 2;
  if (setting == 1) {
    p.amplifier = 0;
    p.degradation = 0;
    p.wow = p.flutter = p.drift = 0;
    p.overdub = 0;
  }
  if (setting == 2) {
    p.amplifier = .6F;
    p.degradation = .3F;
    p.wow = .5F;
    p.flutter = .2F;
    p.pitchSemitones = -5;
    p.reloop = t >= 4;
  }
  if (setting == 3) {
    p.amplifier = 1;
    p.degradation = 1;
    p.wow = 1;
    p.flutter = 1;
    p.drift = 1;
    p.pitchSemitones = 12;
    p.reloop = t >= 4;
  }
  if (setting == 4) {
    const bool high = static_cast<int>(t * 40 + 1e-9) % 2;
    p.speed = high ? 2 : .5F;
    p.reverse = high;
    p.pitchSemitones = high ? 7 : -7;
  }
  if (setting == 5) {
    p.wow = 1;
    p.flutter = 0;
    p.drift = 0;
    p.amplifier = 0;
    p.degradation = 0;
    p.splice = .03F;
  }
  return p;
}
template <class Engine, class P>
Audio loopRender(const Audio& input, int setting, std::size_t block) {
  Audio a = input;
  auto e = std::make_unique<Engine>();
  e->prepare(a.rate, 16);
  for (std::size_t at = 0; at < a.left.size();) {
    const auto n = chunk(at, a.left.size(), block, a.rate);
    e->process(a.left.data() + at, a.right.data() + at, n,
               loopParameters<P>(setting, at / a.rate));
    at += n;
  }
  return a;
}
inline aste::impulse::Parameters impulseParameters(int setting, double t,
                                                   int object = -1) {
  auto p = impulseStarter();
  if (setting == 1) {
    p.energy = .1F;
    for (auto& v : p.tracks) {
      v.drive = 0;
      v.decayMs *= .6F;
    }
  }
  if (setting == 2) {
    p.energy = .8F;
    p.variation = .2F;
    for (auto& v : p.tracks) {
      v.drive = .7F;
      v.tone = .7F;
    }
  }
  if (setting == 3) {
    p.energy = 1;
    p.variation = 1;
    p.mutation = 1;
    for (auto& v : p.tracks) {
      v.drive = 1;
      v.decayMs = 1500;
      v.tone = 1;
      v.ratchet = 4;
    }
  }
  if (setting == 4) {
    const bool high = static_cast<int>(t * 40 + 1e-9) % 2;
    p.energy = high ? 1 : 0;
    for (auto& v : p.tracks) {
      v.pitchHz *= high ? 2 : .5F;
      v.tone = high ? 1 : 0;
    }
  }
  if (setting == 5) {
    p.energy = .7F;
    p.variation = .4F;
    p.mutation = .4F;
    for (auto& v : p.tracks) {
      v.length = 13;
      v.probability = .8F;
      v.ratchet = 2;
      v.timing = .2F;
    }
  }
  if (object >= 0) {
    p.sequenceEnabled = false;
    p.variation = 0;
    p.mutation = 0;
    auto& v = p.tracks[static_cast<std::size_t>(object)];
    if (setting == 1) {
      v.tone = 0;
      v.drive = 0;
      v.decayMs *= .5F;
    }
    if (setting == 2) {
      v.tone = 1;
      v.drive = 1;
      v.decayMs *= 2;
    }
  }
  return p;
}
inline Audio impulseRender(int setting, int phrase, int object, double fs,
                           std::size_t block) {
  Audio a(static_cast<std::size_t>(fs * 8), fs);
  auto initial = impulseParameters(setting, 0, object);
  initial.sequenceEnabled = phrase == 0 && object < 0;
  aste::impulse::Processor e;
  e.prepare(fs, initial);
  std::vector<aste::impulse::MidiEvent> events;
  if (phrase || object >= 0)
    for (int i = 0; i < 16; ++i)
      events.push_back({static_cast<std::size_t>((.1037 + i * .3517) * fs),
                        36 + (object < 0 ? i % 8 : object),
                        i % 4 == 0 ? 1.F : .65F, true});
  std::size_t event{};
  for (std::size_t at = 0; at < a.left.size();) {
    const auto n = chunk(at, a.left.size(), block, fs);
    std::array<aste::impulse::MidiEvent, 64> batch{};
    std::size_t count{};
    while (event < events.size() && events[event].offset < at + n) {
      batch[count] = events[event++];
      batch[count++].offset -= at;
    }
    auto p = impulseParameters(setting, at / fs, object);
    p.sequenceEnabled = initial.sequenceEnabled;
    e.process(a.left.data() + at, a.right.data() + at, n, p,
              {at < static_cast<std::size_t>(6 * fs), 120, at / fs * 2},
              std::span(batch).first(count));
    at += n;
  }
  return a;
}
inline aste::field::Parameters fieldParameters(int setting, double t) {
  aste::field::Parameters p;
  if (setting == 1) {
    p.mass = .2F;
    p.grain = p.pitch = p.motion = 0;
    p.distance = .2F;
    p.blend = .25F;
  }
  if (setting == 2) {
    p.mass = .8F;
    p.grain = .5F;
    p.pitch = .5F;
    p.motion = .6F;
    p.distance = .6F;
    p.blend = .7F;
  }
  if (setting == 3) {
    p.mass = p.grain = p.pitch = p.motion = p.distance = p.blend = 1;
    p.forever = t >= 2 && t < 6;
  }
  if (setting == 4) {
    const bool high = static_cast<int>(t * 40 + 1e-9) % 2;
    p.mass = high ? 1 : 0;
    p.pitch = high ? 1 : 0;
    p.motion = high ? 1 : 0;
    p.blend = 1;
  }
  if (setting == 5) {
    p.mass = 1;
    p.grain = p.pitch = p.motion = 0;
    p.blend = 1;
    p.forever = t >= 2 && t < 6;
  }
  return p;
}
inline Audio render(int product, const Audio& input, int setting = 0,
                    bool candidate = false, int phrase = 0, int object = -1,
                    std::size_t block = 127) {
  if (product == 2) {
    if (candidate)
      return sequenceRender<aste::sequence_candidate::Processor,
                            aste::sequence_candidate::Parameters,
                            aste::sequence_candidate::MidiEvent,
                            aste::sequence_candidate::Transport>(
          setting, phrase, input.rate, block);
    return sequenceRender<aste::sequence::Processor, aste::sequence::Parameters,
                          aste::sequence::MidiEvent, aste::sequence::Transport>(
        setting, phrase, input.rate, block);
  }
  if (product == 3) {
    if (candidate)
      return loopRender<aste::loop_candidate::Processor,
                        aste::loop_candidate::Parameters>(input, setting,
                                                          block);
    return loopRender<aste::loop::Processor, aste::loop::Parameters>(
        input, setting, block);
  }
  if (product == 4)
    return impulseRender(setting, phrase, object, input.rate, block);
  Audio a = input;
  aste::density::Processor density;
  aste::harmonic::Processor harmonic;
  auto field = std::make_unique<aste::field::Processor>();
  if (product == 0) density.prepare(a.rate, densityParameters(setting, 0));
  if (product == 1)
    harmonic.prepare(a.rate, harmonicParameters(setting, 0, candidate));
  if (product == 5) field->prepare(a.rate);
  for (std::size_t at = 0; at < a.left.size();) {
    const auto n = chunk(at, a.left.size(), block, a.rate);
    if (product == 0)
      density.process(a.left.data() + at, a.right.data() + at, n,
                      densityParameters(setting, at / a.rate));
    else if (product == 1)
      harmonic.process(a.left.data() + at, a.right.data() + at, n,
                       harmonicParameters(setting, at / a.rate, candidate));
    else if (product == 5)
      field->process(a.left.data() + at, a.right.data() + at, n,
                     fieldParameters(setting, at / a.rate));
    else
      throw std::runtime_error("unknown product");
    at += n;
  }
  return a;
}
}  // namespace power
