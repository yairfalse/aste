#include "core.hpp"
#include "sequence_processor.hpp"
#include "field_processor.hpp"
#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <complex>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace aste::r01;
constexpr double pi = std::numbers::pi, rate = 48000;
struct Study {
  const char* name;
  Architecture architecture{Architecture::Modal};
  Memory memory{Memory::Shared};
  Excitation excitation{Excitation::White};
  Controls controls{};
  bool external{}, fixed{}, release{}, midi{}, retuneOnly{};
};
const std::vector<Study> studies = [] {
  std::vector<Study> s;
  for (auto [name, type] : {std::pair{"modal_white", Excitation::White},
                            {"modal_pink", Excitation::Pink},
                            {"modal_shaped", Excitation::Shaped},
                            {"modal_impulse", Excitation::Impulse},
                            {"modal_granular", Excitation::Granular}})
    s.push_back({.name = name, .excitation = type});
  s.push_back({.name = "modal_external", .external = true});
  s.push_back(
      {.name = "waveguide_white", .architecture = Architecture::Waveguide});
  s.push_back({.name = "waveguide_impulse",
               .architecture = Architecture::Waveguide,
               .excitation = Excitation::Impulse});
  s.push_back({.name = "waveguide_no_dispersion",
               .architecture = Architecture::Waveguide,
               .controls = {.dispersion = 0}});
  s.push_back({.name = "waveguide_high_dispersion",
               .architecture = Architecture::Waveguide,
               .controls = {.dispersion = .8}});
  s.push_back({.name = "modal_stretched", .controls = {.stretch = .16}});
  s.push_back({.name = "modal_inharmonic", .controls = {.inharmonic = .8}});
  s.push_back({.name = "modal_coupled_3hz", .controls = {.couplingHz = 3}});
  s.push_back({.name = "modal_coupled_15hz", .controls = {.couplingHz = 15}});
  s.push_back({.name = "memory_shared", .controls = {.t60 = 8}});
  s.push_back({.name = "memory_reset",
               .memory = Memory::Reset,
               .controls = {.t60 = 8}});
  s.push_back({.name = "memory_partial",
               .memory = Memory::Partial,
               .controls = {.t60 = 8}});
  s.push_back({.name = "memory_overlap",
               .memory = Memory::Overlap,
               .controls = {.t60 = 8}});
  s.push_back(
      {.name = "memory_coupled", .controls = {.t60 = 8, .couplingHz = 3}});
  s.push_back({.name = "waveguide_overlap",
               .architecture = Architecture::Waveguide,
               .memory = Memory::Overlap});
  s.push_back({.name = "short_stabs", .controls = {.t60 = .18, .damping = .7}});
  s.push_back({.name = "long_memory", .controls = {.t60 = 120, .damping = .1}});
  s.push_back({.name = "persistent_then_release",
               .controls = {.persist = true},
               .release = true});
  s.push_back({.name = "repeated_spectral_mutation",
               .controls = {.mutationCents = 30},
               .fixed = true});
  Controls individual;
  for (std::size_t i = 0; i < kModes; ++i)
    individual.modeDecay[i] = i % 3 == 1 ? .15 : 1;
  s.push_back({.name = "individual_mode_damping", .controls = individual});
  s.push_back({.name = "midi_chord_replacement", .midi = true});
  s.push_back({.name = "retune_without_new_excitation",
               .controls = {.t60 = 8, .damping = .15},
               .retuneOnly = true});
  s.push_back({.name = "coupled_retune_without_new_excitation",
               .controls = {.t60 = 8, .damping = .15, .couplingHz = 8},
               .retuneOnly = true});
  return s;
}();
struct Audio {
  std::vector<float> input, left, right;
  std::vector<std::pair<std::size_t, Snapshot>> state;
  Snapshot final{};
};
std::vector<Event> score(double fs, bool fixed = false, bool release = false) {
  std::vector<Event> events;
  constexpr std::array roots{48., 53., 49., 46.};
  constexpr std::array<std::array<double, 4>, 4> intervals{
      {{0, 3, 7, 10}, {0, 5, 7, 12}, {0, 3, 7, 14}, {0, 2, 7, 11}}};
  for (std::size_t i = 0; i < roots.size(); ++i) {
    Chord chord;
    chord.root = roots[fixed ? 0 : i];
    chord.intervals = intervals[fixed ? 0 : i];
    chord.cents = {0, -4, 3, -2};
    events.push_back({static_cast<std::size_t>((.1 + i * 1.5) * fs),
                      EventType::Strike, chord});
  }
  if (release)
    events.push_back({static_cast<std::size_t>(6 * fs), EventType::Release});
  return events;
}
std::vector<float> excitation(const Study& s, double fs,
                              const std::vector<Event>& events) {
  std::vector<float> input(static_cast<std::size_t>(8 * fs));
  Exciter e;
  if (!e.prepare(fs)) throw std::runtime_error("exciter rate rejected");
  for (std::size_t i = 0; i < events.size(); ++i) {
    if (events[i].type != EventType::Strike || (s.retuneOnly && i > 0))
      continue;
    const auto length = static_cast<std::size_t>(fs * .04);
    std::vector<float> burst(length);
    e.trigger(s.excitation, 501 + i, length);
    double energy{};
    for (std::size_t n = 0; n < length; ++n) {
      const double t = n / fs;
      // Lawful synthetic audio input: an unpitched transient/chirp fixture, not
      // a recording.
      burst[n] =
          s.external
              ? static_cast<float>(std::exp(-t * 130) *
                                   std::sin(2 * pi * (800 * t + 30000 * t * t)))
              : e.next();
      energy += burst[n] * static_cast<double>(burst[n]);
    }
    if (energy == 0) throw std::runtime_error("empty excitation");
    const double gain =
        1 / std::sqrt(energy);  // Equal input energy across exciter types.
    for (std::size_t n = 0; n < length; ++n)
      input[events[i].offset + n] = static_cast<float>(burst[n] * gain);
  }
  return input;
}
std::vector<Event> performanceEvents(const Study& study, double fs) {
  auto snapshots = score(fs, study.fixed, study.release);
  if (study.retuneOnly) {
    bool initial = true;
    for (auto& event : snapshots)
      if (event.type == EventType::Strike) {
        if (!initial) event.type = EventType::Retune;
        initial = false;
      }
  }
  if (!study.midi) return snapshots;
  std::vector<Event> result;
  std::array<int, kTones> held{};
  std::size_t count{};
  for (const auto& event : snapshots) {
    if (event.type != EventType::Strike) {
      result.push_back(event);
      continue;
    }
    for (std::size_t i = 0; i < count; ++i)
      result.push_back({event.offset, EventType::NoteOff, {}, held[i]});
    count = event.chord.count;
    for (std::size_t i = 0; i < count; ++i) {
      held[i] = static_cast<int>(event.chord.root + event.chord.intervals[i]);
      result.push_back({event.offset, EventType::NoteOn, {}, held[i]});
    }
  }
  return result;
}
Audio render(const Study& s, std::size_t block = 127, double fs = rate) {
  const auto events = performanceEvents(s, fs);
  Audio a;
  a.input = excitation(s, fs, score(fs, s.fixed, s.release));
  a.left.resize(a.input.size());
  a.right.resize(a.input.size());
  Engine e;
  if (!e.prepare(fs, s.architecture, s.memory, s.controls))
    throw std::runtime_error("invalid study setup");
  std::size_t event{};
  for (std::size_t at = 0; at < a.input.size();) {
    const auto count = std::min(block, a.input.size() - at);
    std::array<Event, 64> batch{};
    std::size_t n{};
    while (event < events.size() && events[event].offset < at + count) {
      batch[n] = events[event++];
      batch[n++].offset -= at;
    }
    e.process(a.input.data() + at, a.left.data() + at, a.right.data() + at,
              count, s.controls, std::span(batch).first(n));
    at += count;
    if (at % (block * 8) == 0) a.state.emplace_back(at, e.snapshot());
  }
  a.final = e.snapshot();
  if (a.final.rejected || a.final.sanitized)
    throw std::runtime_error("incomplete or invalid study input");
  return a;
}

Audio reference(bool fieldOnly, bool current = false, std::size_t block = 127) {
  const auto events = score(rate);
  Audio a;
  a.input = excitation(studies[0], rate, events);
  a.left.resize(a.input.size());
  a.right.resize(a.input.size());
  auto field = std::make_unique<aste::field::Processor>();
  field->prepare(rate);
  aste::field::Parameters fp;
  fp.blend = 1;
  fp.mass = .8F;
  fp.pitch = .2F;
  fp.outputDb = -6;
  if (current) {
    fp.mass = .75F;
    fp.grain = .65F;
    fp.pitch = .55F;
    fp.motion = .55F;
    fp.distance = .20F;
    fp.blend = .65F;
    fp.outputDb = 0;
    fp.texture = .70F;
  }
  std::array<aste::sequence::Processor, 4> voices;
  aste::sequence::Processor polyphonic;
  aste::sequence::Parameters sp;
  sp.sequenceEnabled = false;
  sp.sustain = 0;
  sp.attackMs = 2;
  sp.decayMs = 180;
  sp.releaseMs = 120;
  sp.outputDb = -12;
  sp.glideMs = 0;
  if (current) {
    sp.voiceMode = 1;
    sp.pressure = .18F;
    sp.shape = .08F;
    sp.oscillatorMix = .5F;
    sp.detuneSemitones = .14F;
    sp.subLevel = .08F;
    sp.cutoffHz = 1600;
    sp.resonance = .18F;
    sp.filterMorph = .15F;
    sp.filterDrive = .25F;
    sp.envelopeAmount = .35F;
    sp.attackMs = 18;
    sp.decayMs = 650;
    sp.sustain = .78F;
    sp.releaseMs = 550;
    sp.outputDb = -8;
  }
  for (auto& voice : voices) voice.prepare(rate, sp);
  polyphonic.prepare(rate, sp);
  std::array<int, 4> held{};
  held.fill(-1);
  std::size_t event{};
  std::size_t off = a.left.size();
  for (std::size_t at = 0; at < a.left.size();) {
    auto count = std::min<std::size_t>(block, a.left.size() - at);
    if (event < events.size() && events[event].offset > at)
      count = std::min(count, events[event].offset - at);
    if (off > at) count = std::min(count, off - at);
    const bool onset = event < events.size() && events[event].offset == at;
    std::array<aste::sequence::MidiEvent, 8> polyMidi{};
    std::size_t polyCount{};
    for (std::size_t v = 0; v < voices.size() && !fieldOnly; ++v) {
      std::array<aste::sequence::MidiEvent, 2> midi{};
      std::size_t n{};
      if ((at == off || onset) && held[v] >= 0) {
        midi[n++] = {0, aste::sequence::MidiEventType::noteOff, held[v], 0};
        held[v] = -1;
      }
      if (onset) {
        held[v] = static_cast<int>(events[event].chord.root +
                                   events[event].chord.intervals[v]);
        midi[n++] = {0, aste::sequence::MidiEventType::noteOn, held[v], .75F};
      }
      if (current) {
        for (std::size_t i = 0; i < n; ++i) polyMidi[polyCount++] = midi[i];
        continue;
      }
      std::array<float, 2048> left{}, right{};
      voices[v].process(left.data(), right.data(), count, sp, {},
                        std::span(midi).first(n));
      for (std::size_t i = 0; i < count; ++i) {
        a.left[at + i] += left[i] * .25F;
        a.right[at + i] += right[i] * .25F;
      }
    }
    if (current && !fieldOnly) {
      std::sort(polyMidi.begin(),
                polyMidi.begin() + static_cast<std::ptrdiff_t>(polyCount),
                [](const auto& a, const auto& b) {
                  return a.type == aste::sequence::MidiEventType::noteOff &&
                         b.type != aste::sequence::MidiEventType::noteOff;
                });
      polyphonic.process(a.left.data() + at, a.right.data() + at, count, sp, {},
                         std::span(polyMidi).first(polyCount));
      // Reproduce the new plugin's independent +3 dB final master stage.
      constexpr float master = 1.4125375446F;
      for (std::size_t i = 0; i < count; ++i) {
        a.left[at + i] *= master;
        a.right[at + i] *= master;
      }
    }
    if (fieldOnly)
      for (std::size_t i = 0; i < count; ++i)
        a.left[at + i] = a.right[at + i] = a.input[at + i];
    field->process(a.left.data() + at, a.right.data() + at, count, fp);
    if (onset) {
      ++event;
      off = at + 4800;
    } else if (at == off)
      off = a.left.size();
    at += count;
  }
  return a;
}

std::vector<double> spectrum(const std::vector<float>& signal,
                             std::size_t begin, std::size_t n) {
  if (begin + n > signal.size() || !std::has_single_bit(n))
    throw std::runtime_error("FFT bounds");
  std::vector<std::complex<double>> x(n);
  for (std::size_t i = 0; i < n; ++i)
    x[i] = signal[begin + i] * (.5 - .5 * std::cos(2 * pi * i / n));
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    auto bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(x[i], x[j]);
  }
  for (std::size_t length = 2; length <= n; length *= 2) {
    const auto step = std::polar(1.0, -2 * pi / length);
    for (std::size_t start = 0; start < n; start += length) {
      std::complex<double> phase{1};
      for (std::size_t i = 0; i < length / 2; ++i) {
        const auto a = x[start + i], b = phase * x[start + i + length / 2];
        x[start + i] = a + b;
        x[start + i + length / 2] = a - b;
        phase *= step;
      }
    }
  }
  std::vector<double> power(n / 2 + 1);
  for (std::size_t i = 0; i < power.size(); ++i) power[i] = std::norm(x[i]);
  return power;
}
double peakFrequency(const std::vector<double>& power, double fs,
                     double target) {
  const double step = fs / ((power.size() - 1) * 2);
  auto low = static_cast<std::size_t>(target * .9 / step),
       high = static_cast<std::size_t>(target * 1.1 / step);
  low = std::max<std::size_t>(1, low);
  high = std::min(high, power.size() - 2);
  const auto k = static_cast<std::size_t>(
      std::max_element(power.begin() + low, power.begin() + high + 1) -
      power.begin());
  const double a = std::log(std::max(power[k - 1], 1e-300)),
               b = std::log(std::max(power[k], 1e-300)),
               c = std::log(std::max(power[k + 1], 1e-300));
  const double denominator = a - 2 * b + c;
  const double correction = denominator == 0 ? 0 : .5 * (a - c) / denominator;
  return (k + correction) * step;
}
struct Metrics {
  double peak{}, rms{}, tail{}, delta{};
  std::uint64_t hash{14695981039346656037ULL};
};
Metrics measure(const Audio& a, double fs = rate) {
  Metrics m;
  double sum{}, tail{};
  for (std::size_t i = 0; i < a.left.size(); ++i) {
    for (float x : {a.left[i], a.right[i]}) {
      if (!std::isfinite(x))
        throw std::runtime_error("nonfinite rendered audio");
      m.peak = std::max(m.peak, static_cast<double>(std::abs(x)));
      sum += x * static_cast<double>(x);
      if (i >= a.left.size() - static_cast<std::size_t>(fs))
        tail += x * static_cast<double>(x);
      const auto bits = std::bit_cast<std::uint32_t>(x);
      for (unsigned b = 0; b < 4; ++b) {
        m.hash ^= (bits >> (8 * b)) & 255;
        m.hash *= 1099511628211ULL;
      }
    }
    if (i)
      m.delta = std::max(
          {m.delta, static_cast<double>(std::abs(a.left[i] - a.left[i - 1])),
           static_cast<double>(std::abs(a.right[i] - a.right[i - 1]))});
  }
  m.rms = std::sqrt(sum / (a.left.size() * 2));
  m.tail = std::sqrt(tail / (fs * 2));
  return m;
}
std::ofstream file(const std::filesystem::path& path, bool binary = false) {
  std::ofstream out(path, binary ? std::ios::binary : std::ios::out);
  out.exceptions(std::ios::failbit | std::ios::badbit);
  out << std::setprecision(12);
  return out;
}
void u16(std::ostream& o, std::uint16_t x) {
  o.put(static_cast<char>(x));
  o.put(static_cast<char>(x >> 8));
}
void u32(std::ostream& o, std::uint32_t x) {
  for (unsigned b = 0; b < 4; ++b) o.put(static_cast<char>(x >> (b * 8)));
}
void wave(const std::filesystem::path& path, const Audio& a, double gain) {
  auto out = file(path, true);
  const auto size = static_cast<std::uint32_t>(a.left.size() * 8);
  out.write("RIFF", 4);
  u32(out, size + 48);
  out.write("WAVEfmt ", 8);
  u32(out, 16);
  u16(out, 3);
  u16(out, 2);
  u32(out, 48000);
  u32(out, 384000);
  u16(out, 8);
  u16(out, 32);
  out.write("fact", 4);
  u32(out, 4);
  u32(out, static_cast<std::uint32_t>(a.left.size()));
  out.write("data", 4);
  u32(out, size);
  for (std::size_t i = 0; i < a.left.size(); ++i)
    for (auto x : {a.left[i], a.right[i]}) {
      const auto value = static_cast<float>(x * gain);
      if (!std::isfinite(value) || std::abs(value) > 1)
        throw std::runtime_error("unsafe WAV export");
      u32(out, std::bit_cast<std::uint32_t>(value));
    }
}

void pack(const std::filesystem::path& path) {
  if (std::filesystem::exists(path))
    throw std::runtime_error(
        "use a new pack directory; responses are never overwritten");
  std::filesystem::create_directories(path / "audio");
  std::filesystem::create_directory(path / "source");
  auto metrics = file(path / "measurements.csv"),
       responses = file(path / "responses.csv"),
       key = file(path / "answer-key.csv");
  auto energy = file(path / "energy.csv"), events = file(path / "events.csv");
  metrics << "id,variant,peak,rms,last_second_rms,max_delta,final_state_energy,"
             "safety_interventions,stolen_banks,matched_gain,raw_fnv1a64\n";
  responses << "id,physical_character,chord_legibility,memory_control,identity,"
               "preference,confidence,notes\n";
  key << "id,variant\n";
  energy << "variant,sample,bank,mode,frequency,energy\n";
  events
      << "variant,sample,event,midi_note,root,interval_1,interval_2,interval_3,"
         "interval_4\n";
  for (std::size_t i = 0; i < studies.size() + 4; ++i) {
    const std::array referenceNames{"four_sequence_into_field",
                                    "same_noise_into_field",
                                    "current_poly_sequence_into_textured_field",
                                    "same_noise_into_textured_field"};
    const auto referenceIndex = i >= studies.size() ? i - studies.size() : 0;
    const std::string name =
        i < studies.size() ? studies[i].name : referenceNames[referenceIndex];
    const auto a = i < studies.size() ? render(studies[i])
                                      : reference(referenceIndex % 2 != 0,
                                                  referenceIndex >= 2);
    const auto m = measure(a);
    if (m.rms == 0) throw std::runtime_error("unexpected empty study");
    const double gain = std::min(.05 / m.rms, .8 / m.peak);
    // Reversed cyclic order is bijective, not a claim of randomization.
    const auto number = (studies.size() + 13 - i) % (studies.size() + 4) + 1;
    const std::string id = (number < 10 ? "0" : "") + std::to_string(number);
    wave(path / "audio" / (id + ".wav"), a, gain);
    wave(path / "source" / (name + ".wav"), a, 1);
    metrics << id << ',' << name << ',' << m.peak << ',' << m.rms << ','
            << m.tail << ',' << m.delta << ',';
    if (i < studies.size())
      metrics << a.final.energy << ',' << a.final.safetyInterventions << ','
              << a.final.stolen;
    else
      metrics << "unknown,unknown,unknown";  // Product state metrics are not
                                             // comparable.
    metrics << ',' << gain << ',' << std::hex << m.hash << std::dec << '\n';
    responses << id << ",,,,,,,\n";
    key << id << ',' << name << '\n';
    for (const auto& [sample, state] : a.state)
      for (std::size_t b = 0; b < 2; ++b)
        for (std::size_t k = 0; k < kModes; ++k)
          energy << name << ',' << sample << ',' << b << ',' << k << ','
                 << state.bankFrequencies[b][k] << ','
                 << state.bankEnergies[b][k] << '\n';
    const auto scoreEvents =
        i < studies.size() ? performanceEvents(studies[i], rate) : score(rate);
    for (const auto& event : scoreEvents) {
      events << name << ',' << event.offset << ','
             << static_cast<int>(event.type) << ',' << event.note << ','
             << event.chord.root;
      for (double interval : event.chord.intervals) events << ',' << interval;
      events << '\n';
    }
  }
  auto manifest = file(path / "MANIFEST.txt");
  manifest
      << "R-01 isolated research, source commit at configure: " << R01_COMMIT
      << "\ncompiler: " << R01_COMPILER
      << "\n48000 Hz stereo float32 WAV, 8 seconds each; no clipped "
         "export samples.\n"
         "Each synthetic excitation burst has unit squared-sample energy. "
         "Identical streams enter modal/waveguide comparisons.\n"
         "Four-chord score: minor7, suspended, minor9 and ambiguous. "
         "Exact cents and generators live in lab.cpp.\n"
         "audio/: obscured IDs, RMS target .05 with peak cap .8. source/: "
         "unchanged raw outputs.\n"
         "RMS matching is not perceptual loudness matching. FNV is only a "
         "build-specific fingerprint.\n"
         "Listen quietly first; disable normalization/dynamics. Use "
         "monitors, headphones and mono.\n"
         "Complete responses before revealing answer-key. No preference "
         "is a valid result.\n"
         "No human listening or musical-success claim; generated audio "
         "and numerical assessment only.\n"
         "External excitation is a generated transient/chirp passed "
         "through the audio-input API, not a file importer.\n"
         "Sequence comparison uses four independent monophonic voices "
         "into one Field, not a crippled one-note baseline.\n"
         "Two further references use one current eight-voice Sequence "
         "with Warm Brass values and +3 dB Master into Open Field/Texture 70, "
         "and identical unpitched noise into that Field.\n"
         "Field-only uses the same unpitched input. Neither reference is "
         "an equivalent-control null test.\n";
  std::cout << "pack: " << studies.size() + 4 << " variants, "
            << (studies.size() + 4) * 2 << " WAVs\n";
}

void tuning(const std::filesystem::path& path) {
  auto out = file(path);
  out << "rate,architecture,target_hz,measured_hz,error_cents,energy_at_1s,"
         "energy_at_2s,heap_bytes,state_bytes\n";
  for (double fs : {44100., 48000., 88200., 96000., 176400., 192000.})
    for (auto architecture : {Architecture::Modal, Architecture::Waveguide}) {
      Controls c;
      c.t60 = 2;
      c.damping = .1;
      Engine e;
      if (!e.prepare(fs, architecture, Memory::Shared, c))
        throw std::runtime_error("tuning setup");
      Chord chord;
      chord.root = 48;
      chord.count = 1;
      chord.cents = {};
      std::vector<float> input(static_cast<std::size_t>(fs * 3)),
          output(input.size());
      input[0] = .1F;
      const std::array event{Event{0, EventType::Strike, chord}};
      e.process(input.data(), output.data(), nullptr,
                static_cast<std::size_t>(fs), c, event);
      const double first = e.snapshot().energy;
      e.process(nullptr, output.data() + static_cast<std::size_t>(fs), nullptr,
                static_cast<std::size_t>(fs), c);
      const double second = e.snapshot().energy;
      const auto n = fs <= 48000 ? 65536U : 131072U;
      const auto power = spectrum(output, static_cast<std::size_t>(.1 * fs), n);
      const double target = 440 * std::exp2((48. - 69) / 12),
                   measured = peakFrequency(power, fs, target);
      out << fs << ','
          << (architecture == Architecture::Modal ? "modal" : "waveguide")
          << ',' << target << ',' << measured << ','
          << 1200 * std::log2(measured / target) << ',' << first << ','
          << second << ',' << e.heapBytes() << ',' << sizeof(e) << '\n';
    }
}

std::vector<float> driven(double fs, double drive, Architecture architecture,
                          bool persist, std::size_t& guards) {
  Controls c;
  c.persist = persist;
  c.t60 = .03;
  c.damping = 0;
  c.dispersion = 0;
  Engine e;
  if (!e.prepare(fs, architecture, Memory::Shared, c))
    throw std::runtime_error("alias setup");
  Chord chord;
  chord.root = 100;
  chord.count = 1;
  chord.intervals[0] = 69 + 12 * std::log2(4500. / 440) - 100;
  const std::array event{Event{0, EventType::Strike, chord}};
  std::vector<float> input(static_cast<std::size_t>(fs * 2)),
      output(input.size());
  for (std::size_t i = 0; i < input.size(); ++i)
    input[i] = static_cast<float>(drive * std::cos(2 * pi * 4500 * i / fs));
  e.process(input.data(), output.data(), nullptr, input.size(), c, event);
  guards = e.snapshot().safetyInterventions;
  return output;
}
std::vector<float> decimate(const std::vector<float>& high) {
  std::array<double, 129> filter{};
  double sum{};
  for (int i = -64; i <= 64; ++i) {
    const double w =
        .42 + .5 * std::cos(pi * i / 64) + .08 * std::cos(2 * pi * i / 64);
    const double sinc = i == 0 ? .225 : std::sin(2 * pi * .1125 * i) / (pi * i);
    filter[static_cast<std::size_t>(i + 64)] = sinc * w;
    sum += sinc * w;
  }
  std::vector<float> low(high.size() / 4);
  for (std::size_t i = 16; i + 16 < low.size(); ++i) {
    double sample{};
    for (std::size_t k = 0; k < filter.size(); ++k)
      sample += filter[k] / sum * high[i * 4 + k - 64];
    low[i] = static_cast<float>(sample);
  }
  return low;
}
double foldedDb(const std::vector<float>& signal) {
  constexpr std::size_t n = 32768;
  const auto power = spectrum(signal, 48000, n);
  double folded{}, total{};
  for (double p : power) total += p;
  for (double frequency : {1500., 7500., 10500., 16500., 19500.}) {
    const auto bin =
        static_cast<std::size_t>(std::round(frequency * n / 48000));
    for (std::size_t k = bin - 2; k <= bin + 2; ++k) folded += power[k];
  }
  if (!(total > 0)) throw std::runtime_error("fold-band ratio has no signal");
  return 10 * std::log10(std::max(folded / total, 1e-30));
}
void alias(const std::filesystem::path& path) {
  auto out = file(path);
  out << "architecture,drive,persist,base_guard_interventions,high_guard_"
         "interventions,"
         "base_fold_band_db,filtered_4x_fold_band_db\n";
  for (auto architecture : {Architecture::Modal, Architecture::Waveguide})
    for (bool strong : {false, true}) {
      std::size_t baseGuards{}, highGuards{};
      const double drive = strong ? 1 : .0001;
      const auto base = driven(48000, drive, architecture, strong, baseGuards);
      const auto high =
          driven(192000, drive / 4, architecture, strong, highGuards);
      out << (architecture == Architecture::Modal ? "modal" : "waveguide")
          << ',' << drive << ',' << strong << ',' << baseGuards << ','
          << highGuards << ',' << foldedDb(base) << ','
          << foldedDb(decimate(high)) << '\n';
    }
}
void benchmark(const std::filesystem::path& path) {
  auto out = file(path);
  out << "commit,compiler,architecture,rate,block,trial,cpu_percent,max_block_"
         "us,heap_bytes,state_bytes,guard_interventions,output_energy_probe\n";
  for (auto architecture : {Architecture::Modal, Architecture::Waveguide})
    for (double fs : {48000., 96000.})
      for (auto block : {128U, 511U})
        for (int trial = 0; trial < 5; ++trial) {
          Controls c;
          c.persist = true;
          c.couplingHz = architecture == Architecture::Modal ? 20 : 0;
          Engine e;
          if (!e.prepare(fs, architecture, Memory::Overlap, c))
            throw std::runtime_error("benchmark setup");
          std::array<float, 511> input{}, left{}, right{};
          input.fill(.0001F);
          double maximum{}, checksum{};
          const auto start = std::chrono::steady_clock::now();
          for (std::size_t at = 0; at < static_cast<std::size_t>(fs * 5);) {
            const auto count = std::min<std::size_t>(
                block, static_cast<std::size_t>(fs * 5) - at);
            c.damping = (at / block) % 2 ? .1 : .7;
            Chord chord;
            chord.root = (at / block / 16) % 2 ? 48 : 53;
            const std::array event{Event{0, EventType::Strike, chord}};
            // Keep both banks sounding long enough for feedback to return;
            // clearing every block can benchmark a silent short-delay prefix.
            const auto events =
                std::span(event).first((at / block) % 16 == 0 ? 1 : 0);
            const auto begin = std::chrono::steady_clock::now();
            e.process(input.data(), left.data(), right.data(), count, c,
                      events);
            maximum =
                std::max(maximum, std::chrono::duration<double, std::micro>(
                                      std::chrono::steady_clock::now() - begin)
                                      .count());
            checksum += left[0] * static_cast<double>(left[0]) +
                        right[0] * static_cast<double>(right[0]);
            at += count;
          }
          const double elapsed = std::chrono::duration<double>(
                                     std::chrono::steady_clock::now() - start)
                                     .count();
          if (!(checksum > 0) || !std::isfinite(checksum))
            throw std::runtime_error(
                "benchmark did not exercise sounding state");
          out << R01_COMMIT << ',' << R01_COMPILER << ','
              << (architecture == Architecture::Modal ? "modal" : "waveguide")
              << ',' << fs << ',' << block << ',' << trial << ','
              << elapsed / 5 * 100 << ',' << maximum << ',' << e.heapBytes()
              << ',' << sizeof(e) << ',' << e.snapshot().safetyInterventions
              << ',' << checksum << '\n';
        }
}
void stress(const std::filesystem::path& path) {
  auto out = file(path);
  out << "architecture,rate,seconds,peak,final_energy,guard_interventions,"
         "rejected,sanitized\n";
  for (auto architecture : {Architecture::Modal, Architecture::Waveguide})
    for (double fs : {44100., 48000., 88200., 96000., 176400., 192000.}) {
      Controls c;
      c.persist = true;
      Engine e;
      if (!e.prepare(fs, architecture, Memory::Shared, c))
        throw std::runtime_error("stress setup");
      std::array<float, 128> input{}, left{}, right{};
      const auto end = static_cast<std::size_t>(fs * 60);
      const auto releaseAt = static_cast<std::size_t>(fs * 55);
      for (std::size_t at = 0; at < end;) {
        const auto frames = std::min(input.size(), end - at);
        const bool high = (at / 128) % 2;
        c.t60 = high ? 120 : .03;
        c.damping = high ? .8 : 0;
        c.dispersion = high ? .8 : 0;
        c.couplingHz = architecture == Architecture::Modal && high ? 20 : 0;
        for (std::size_t i = 0; i < frames; ++i)
          input[i] = at + i < fs * 50
                         ? static_cast<float>(
                               std::cos(2 * pi * 130.81278265 * (at + i) / fs))
                         : 0;
        Chord chord;
        chord.root = high ? 36 : 84;
        std::array<Event, 2> events{Event{0, EventType::Retune, chord}};
        std::size_t count = 1;
        if (at <= releaseAt && releaseAt < at + frames)
          events[count++] = Event{releaseAt - at, EventType::Release};
        e.process(input.data(), left.data(), right.data(), frames, c,
                  std::span(events).first(count));
        for (std::size_t i = 0; i < frames; ++i)
          if (!std::isfinite(left[i]) || !std::isfinite(right[i]) ||
              std::abs(left[i]) > 1.60001 || std::abs(right[i]) > 1.60001)
            throw std::runtime_error("stress output escaped bound");
        at += frames;
      }
      const auto s = e.snapshot();
      out << (architecture == Architecture::Modal ? "modal" : "waveguide")
          << ',' << fs << ",60," << s.peak << ',' << s.energy << ','
          << s.safetyInterventions << ',' << s.rejected << ',' << s.sanitized
          << '\n';
      if (s.rejected || s.sanitized || !std::isfinite(s.energy) ||
          s.energy > 1e-12)
        throw std::runtime_error("stress failed validation or release decay");
    }
}
}  // namespace
int main(int argc, char** argv) {
  try {
    if (argc == 3 && std::string(argv[1]) == "--pack")
      pack(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--tuning")
      tuning(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--alias")
      alias(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--benchmark")
      benchmark(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--stress")
      stress(argv[2]);
    else if (argc == 2 && std::string(argv[1]) == "--check") {
      std::vector<float> sine(4096);
      for (std::size_t i = 0; i < sine.size(); ++i)
        sine[i] = static_cast<float>(std::sin(2 * pi * 128 * i / sine.size()));
      const auto power = spectrum(sine, 0, sine.size());
      if (std::max_element(power.begin(), power.end()) - power.begin() != 128 ||
          std::abs(peakFrequency(power, 48000, 1500) - 1500) > 1e-6 ||
          std::abs(power[128] / (1024. * 1024.) - 1) > 1e-6)
        throw std::runtime_error("FFT independent sinusoid calibration failed");
      for (const auto& s : studies) {
        const auto a = render(s, 127), b = render(s, 511);
        if (s.retuneOnly &&
            !std::all_of(
                a.input.begin() + static_cast<std::ptrdiff_t>(rate * .15),
                a.input.end(), [](float x) { return x == 0; }))
          throw std::runtime_error(
              "retuning memory study received fresh excitation");
        if (a.left != b.left || a.right != b.right || measure(a).rms == 0)
          throw std::runtime_error(std::string("render mismatch: ") + s.name);
      }
      for (bool current : {false, true})
        for (bool fieldOnly : {false, true}) {
          const auto a = reference(fieldOnly, current, 127);
          const auto b = reference(fieldOnly, current, 511);
          if (a.left != b.left || a.right != b.right || measure(a).rms == 0)
            throw std::runtime_error("family reference partition mismatch");
        }
      if (render(studies[0]).input != render(studies[6]).input)
        throw std::runtime_error("architecture comparison changed excitation");
      std::cout << studies.size()
                << " studies: stereo samples identical at 127/511 frames; "
                   "architecture inputs identical\n";
    } else {
      std::cerr << "usage: r01_lab --pack NEW_DIR | --tuning CSV | --alias CSV "
                   "| --benchmark CSV | --stress CSV | --check\n";
      return 2;
    }
  } catch (const std::exception& e) {
    std::cerr << "r01_lab: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
