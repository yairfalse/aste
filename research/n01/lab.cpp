#include "core.hpp"
#include "impulse_processor.hpp"
#include "loop_processor.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace aste::n01;
constexpr double kRate = 48000;
constexpr std::size_t kFrames = 16 * 48000;
struct Audio {
  std::vector<float> left, right;
  std::vector<Hit> hits;
};
struct Study {
  const char* name;
  bool local{}, master{}, suspend{}, reverse{}, shorten{}, hard{};
  int source{-1};
  bool constant{}, single{};
  bool articulationStudy{};
  Articulation articulation{Articulation::Restart};
};
constexpr std::array studies{
    Study{.name = "sparse_single", .single = true},
    Study{.name = "event_constant", .constant = true},
    Study{.name = "event_acceleration"},
    Study{.name = "local_microfragment", .local = true},
    Study{.name = "master_continue", .master = true},
    Study{.name = "master_suspend", .master = true, .suspend = true},
    Study{.name = "nested_continue", .local = true, .master = true},
    Study{.name = "nested_suspend",
          .local = true,
          .master = true,
          .suspend = true},
    Study{.name = "reverse_soft", .master = true, .reverse = true},
    Study{.name = "shorten_soft", .master = true, .shorten = true},
    Study{.name = "nested_hard",
          .local = true,
          .master = true,
          .reverse = true,
          .shorten = true,
          .hard = true},
    Study{.name = "nested_soft",
          .local = true,
          .master = true,
          .reverse = true,
          .shorten = true},
    Study{.name = "pressure", .source = 0},
    Study{.name = "impact", .source = 1},
    Study{.name = "dust", .source = 2},
    Study{.name = "retrigger_restart", .source = 1, .articulationStudy = true},
    Study{.name = "retrigger_carry",
          .source = 1,
          .articulationStudy = true,
          .articulation = Articulation::Carry},
    Study{.name = "retrigger_crossfade",
          .source = 1,
          .articulationStudy = true,
          .articulation = Articulation::Crossfade}};

std::vector<Command> gestures(const Study& study) {
  std::vector<Command> result;
  for (std::size_t origin = 0; origin < kFrames; origin += 192000) {
    const auto mode = study.suspend ? Mode::Suspend : Mode::Continue;
    if (study.local)
      result.push_back({origin + 21600, 1, Action::Capture, 960, 0, 48000,
                        Mode::Continue, study.hard});
    if (study.master) {
      result.push_back(
          {origin + 40800, 3, Action::Capture, 7200, 0, 0, mode, study.hard});
      if (study.reverse) result.push_back({origin + 48000, 3, Action::Reverse});
      if (study.shorten)
        result.push_back({origin + 52800, 3, Action::Shorten, 1200});
      result.push_back(
          {origin + 69600, 3, Action::Release, 0, 0, 0, mode, study.hard});
      if (study.suspend) result.push_back({origin + 144000, 3, Action::Rejoin});
    }
  }
  return result;
}
Audio render(const Study& study, std::size_t block = 127) {
  auto pattern = studyPattern();
  for (std::size_t v = 0; v < kVoices; ++v)
    for (auto& e : pattern.tracks[v].events) {
      if (study.source >= 0 && v != static_cast<std::size_t>(study.source))
        e.enabled = false;
      if (study.constant) e.acceleration = 1;
      if (study.single) e.repeats = 1;
      if (study.articulationStudy) {
        e.articulation = study.articulation;
        e.repeats = 6;
        e.interval = .02;
        e.acceleration = .65;
        e.decay = .18F;
        e.probability = 1;
      }
    }
  Engine engine;
  if (!engine.prepare(kRate, pattern))
    throw std::runtime_error("invalid render pattern");
  Audio audio{std::vector<float>(kFrames), std::vector<float>(kFrames), {}};
  const auto actions = gestures(study);
  std::size_t action{};
  for (std::size_t at = 0; at < kFrames;) {
    const auto count = std::min(block, kFrames - at);
    std::array<Command, 64> batch{};
    std::size_t n{};
    while (action < actions.size() && actions[action].offset < at + count) {
      batch[n] = actions[action++];
      batch[n++].offset -= at;
    }
    std::array<Hit, 512> hits{};
    Trace trace{hits};
    engine.process(
        audio.left.data() + at, audio.right.data() + at, count,
        {true, true, false, static_cast<std::int64_t>(at), at / 24000.0, 120},
        &trace, std::span(batch).first(n));
    if (trace.dropped != 0 || engine.rejected() != 0)
      throw std::runtime_error("incomplete render trace/gesture");
    audio.hits.insert(audio.hits.end(), hits.begin(),
                      hits.begin() + trace.size);
    at += count;
  }
  return audio;
}

Audio reference(const Audio& events, bool withLoop) {
  aste::impulse::Parameters p;
  p.sequenceEnabled = false;
  p.variation = p.mutation = 0;
  p.outputDb = -6;
  aste::impulse::Processor impulse;
  impulse.prepare(kRate, p);
  Audio audio{std::vector<float>(kFrames), std::vector<float>(kFrames),
              events.hits};
  constexpr std::array<std::size_t, 3> mapping{
      4, 3, 2};  // Low / Body / Burst, no Kick.
  std::size_t event{};
  for (std::size_t at = 0; at < kFrames; ++at) {
    std::array<aste::impulse::MidiEvent, 3> midi{};
    std::size_t n{};
    while (event < events.hits.size() &&
           events.hits[event].sample == static_cast<std::int64_t>(at)) {
      const auto& h = events.hits[event++];
      const auto v = mapping[h.voice];
      auto& track = p.tracks[v];
      track.pitchHz = h.pitch;
      track.tone = h.color;
      track.level = h.gain;
      track.decayMs =
          studyPattern().tracks[h.voice].events[h.event].decay * 1000;
      midi[n++] = {0, static_cast<int>(36 + v), 1, true};
    }
    impulse.process(audio.left.data() + at, audio.right.data() + at, 1, p, {},
                    std::span(midi).first(n));
  }
  if (withLoop) {
    aste::loop::Processor loop;
    loop.prepare(kRate, 1, false);
    aste::loop::Parameters lp;
    lp.loopLengthSeconds = .5F;
    lp.wow = lp.flutter = lp.drift = 0;
    lp.degradation = lp.amplifier = 0;
    lp.outputDb = -6;
    lp.splice = .02F;
    for (std::size_t at = 0; at < kFrames;) {
      // Explicit split at capture and reverse boundaries; not a host
      // experiment.
      auto count = std::min<std::size_t>(127, kFrames - at);
      for (const auto boundary : {24000U, 48000U, 96000U})
        if (at < boundary) count = std::min(count, boundary - at);
      lp.capture = at < 24000;
      lp.reverse = at >= 48000 && at < 96000;
      lp.mix = at < 24000 || at >= 96000 ? 0 : 1;
      loop.process(audio.left.data() + at, audio.right.data() + at, count, lp);
      at += count;
    }
  }
  return audio;
}

struct Metrics {
  double peak{}, rms{}, delta{}, zeroFraction{};
  std::uint64_t hash{14695981039346656037ULL};
};
Metrics measure(const Audio& a) {
  Metrics m;
  double sum{};
  std::size_t zero{};
  for (std::size_t i = 0; i < a.left.size(); ++i) {
    for (const auto x : {a.left[i], a.right[i]}) {
      if (!std::isfinite(x)) throw std::runtime_error("nonfinite output");
      m.peak = std::max(m.peak, static_cast<double>(std::abs(x)));
      sum += x * static_cast<double>(x);
      auto bits = std::bit_cast<std::uint32_t>(x);
      for (unsigned b = 0; b < 4; ++b) {
        m.hash ^= (bits >> (8 * b)) & 255;
        m.hash *= 1099511628211ULL;
      }
    }
    if (a.left[i] == 0 && a.right[i] == 0) ++zero;
    if (i)
      m.delta = std::max(
          {m.delta, static_cast<double>(std::abs(a.left[i] - a.left[i - 1])),
           static_cast<double>(std::abs(a.right[i] - a.right[i - 1]))});
  }
  m.rms = std::sqrt(sum / (2 * a.left.size()));
  m.zeroFraction = static_cast<double>(zero) / a.left.size();
  return m;
}
void u16(std::ostream& out, std::uint16_t value) {
  out.put(static_cast<char>(value));
  out.put(static_cast<char>(value >> 8));
}
void u32(std::ostream& out, std::uint32_t value) {
  for (unsigned b = 0; b < 4; ++b) out.put(static_cast<char>(value >> (8 * b)));
}
void wave(const std::filesystem::path& path, const Audio& a, double gain) {
  std::ofstream out(path, std::ios::binary);
  out.exceptions(std::ios::failbit | std::ios::badbit);
  const auto size = static_cast<std::uint32_t>(a.left.size() * 8);
  out.write("RIFF", 4);
  u32(out, 48 + size);
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
    for (float x : {a.left[i], a.right[i]}) {
      const auto scaled = static_cast<float>(x * gain);
      if (!std::isfinite(scaled) || std::abs(scaled) > 1)
        throw std::runtime_error("unsafe WAV sample");
      u32(out, std::bit_cast<std::uint32_t>(scaled));
    }
}
std::ofstream csv(const std::filesystem::path& path) {
  std::ofstream out(path);
  out.exceptions(std::ios::failbit | std::ios::badbit);
  out << std::setprecision(12);
  return out;
}

void pack(const std::filesystem::path& path) {
  if (std::filesystem::exists(path))
    throw std::runtime_error(
        "pack destination exists; use a new directory to preserve listening "
        "responses");
  std::filesystem::create_directories(path / "audio");
  std::filesystem::create_directory(path / "source");
  auto metrics = csv(path / "measurements.csv");
  metrics << "id,variant,frames,peak,rms,max_adjacent_delta,exact_stereo_zero_"
             "fraction,matched_gain,fnv1a64_raw\n";
  auto key = csv(path / "answer-key.csv");
  key << "id,variant\n";
  auto responses = csv(path / "responses.csv");
  responses << "id,transient_character,space,return_predictability,repeat_"
               "control,preference,confidence_0_to_3,notes\n";
  auto trace = csv(path / "events.csv");
  trace << "variant,sample,voice,event,cycle,repeat,gain,pitch,color\n";
  auto commands = csv(path / "gestures.csv");
  commands
      << "variant,sample,target,action,length,lookback,duration,suspend,hard\n";
  auto edits = csv(path / "edits.csv");
  edits << "variant,sample,action,delta_at_action,max_delta_next_64\n";
  // Deterministic obscured order, not a statistical randomization claim.
  constexpr auto ids = [] {
    std::array<unsigned, studies.size() + 2> result{};
    for (std::size_t i = 0; i < result.size(); ++i)
      result[i] = static_cast<unsigned>((i * 7 + 3) % result.size() + 1);
    return result;
  }();
  auto articulationEdges = csv(path / "retrigger-edges.csv");
  articulationEdges << "variant,sample,repeat,left_delta,max_delta_next_64\n";
  const auto base = render(studies[2]);
  for (std::size_t i = 0; i < ids.size(); ++i) {
    const std::string name =
        i < studies.size() ? studies[i].name
                           : (i == studies.size() ? "impulse_matched_events"
                                                  : "impulse_into_loop");
    const auto audio = i < studies.size()
                           ? render(studies[i])
                           : reference(base, i == studies.size() + 1);
    const auto m = measure(audio);
    if (i < studies.size() && studies[i].articulationStudy) {
      for (const auto& hit : audio.hits) {
        const auto at = static_cast<std::size_t>(hit.sample);
        double maximum = 0;
        for (std::size_t n = std::max<std::size_t>(1, at);
             n < std::min(at + 64, kFrames); ++n)
          maximum = std::max(
              maximum,
              static_cast<double>(std::abs(audio.left[n] - audio.left[n - 1])));
        articulationEdges << name << ',' << at << ',' << hit.repeat << ','
                          << (at ? std::abs(audio.left[at] - audio.left[at - 1])
                                 : 0)
                          << ',' << maximum << '\n';
      }
    }
    if (m.rms == 0)
      throw std::runtime_error("unexpected empty listening fixture");
    const double gain = std::min(.06 / m.rms, .85 / m.peak);
    const auto id =
        std::string(ids[i] < 10 ? "0" : "") + std::to_string(ids[i]);
    wave(path / "audio" / (id + ".wav"), audio, gain);
    wave(path / "source" / (name + ".wav"), audio, 1);
    metrics << id << ',' << name << ',' << kFrames << ',' << m.peak << ','
            << m.rms << ',' << m.delta << ',' << m.zeroFraction << ',' << gain
            << ',' << std::hex << m.hash << std::dec << '\n';
    key << id << ',' << name << '\n';
    responses << id << ",,,,,,,\n";
    for (const auto& h : audio.hits)
      trace << name << ',' << h.sample << ',' << h.voice << ',' << h.event
            << ',' << h.cycle << ',' << h.repeat << ',' << h.gain << ','
            << h.pitch << ',' << h.color << '\n';
    if (i < studies.size()) {
      for (const auto& c : gestures(studies[i])) {
        double maximum{};
        for (std::size_t j = c.offset; j < std::min(c.offset + 64, kFrames);
             ++j)
          if (j)
            maximum = std::max(
                maximum, static_cast<double>(
                             std::abs(audio.left[j] - audio.left[j - 1])));
        edits << name << ',' << c.offset << ',' << static_cast<int>(c.action)
              << ','
              << (c.offset ? std::abs(audio.left[c.offset] -
                                      audio.left[c.offset - 1])
                           : 0)
              << ',' << maximum << '\n';
      }
      for (const auto& c : gestures(studies[i]))
        commands << name << ',' << c.offset << ',' << c.target << ','
                 << static_cast<int>(c.action) << ',' << c.length << ','
                 << c.lookback << ',' << c.duration << ','
                 << (c.mode == Mode::Suspend) << ',' << c.hard << '\n';
    }
  }
  auto manifest = csv(path / "MANIFEST.txt");
  manifest
      << "N-01 research listening pack; not a plugin "
         "release\ncommit_at_configure="
      << N01_COMMIT << "\ncompiler=" << N01_COMPILER
      << "\nrate=48000\nseconds=16\nseed=1701\nblock=127\n"
         "WAV: stereo IEEE float32, little endian; no sample clipping.\n"
         "audio/: blind IDs, RMS target .06 with .85 peak cap; source/: raw.\n"
         "RMS matching is not perceptual loudness matching, especially with "
         "silence.\n"
         "FNV is a reproducibility fingerprint, not a security hash; use "
         "shasum externally.\n"
         "Compiler math can differ across architectures; exact hashes cover "
         "this build only.\n"
         "Listen quietly first. No normalization or dynamics. Use "
         "monitors/headphones/mono.\n"
         "Fill responses before opening answer-key or source. No preference is "
         "valid.\n"
         "Compare return predictability and controllability separately from "
         "sound preference.\n"
         "Impulse maps identical N01 hit traces to Low/Body/Burst. Loop chain "
         "is a capability\n"
         "comparison (half-second capture/reverse/release), not an "
         "identical-control null test.\n"
         "No imported material, musician responses, Cubase or usability "
         "evidence included.\n";
  std::cout << "pack: " << ids.size() << " variants, " << ids.size() * 2
            << " WAVs, 48000 Hz stereo, 16 seconds each\n";
}

void benchmark() {
  std::cout << "commit,compiler,workload,rate,block,run,cpu_percent,max_block_"
               "us,audio_"
               "bytes,state_bytes,checksum\n";
  for (const bool dense : {false, true}) {
    for (double rate : {48000., 96000.})
      for (std::size_t block : {127U, 128U, 511U}) {
        auto p = studyPattern();
        if (dense)
          for (auto& t : p.tracks) {
            t.length = 4;
            for (std::size_t i = 0; i < kEvents; ++i)
              t.events[i] = {.enabled = true,
                             .beat = i * .5,
                             .repeats = 8,
                             .interval = .03125,
                             .mutation = 1,
                             .gain = .5F,
                             .pitch = 700,
                             .decay = .2F};
          }
        for (int run = 0; run < 5; ++run) {
          Engine e;
          if (!e.prepare(rate, p))
            throw std::runtime_error("invalid stress pattern");
          std::array<float, 511> l{}, r{};
          double maximum{}, checksum{};
          const auto begin = std::chrono::steady_clock::now();
          const auto frames = static_cast<std::size_t>(rate * 8);
          for (std::size_t at = 0; at < frames;) {
            const auto count = std::min(block, frames - at);
            std::array<Command, 4> commands{};
            for (std::size_t v = 0; v < commands.size(); ++v)
              commands[v] = {0, v,  Action::Capture, 64,
                             0, 64, Mode::Continue,  false};
            const auto start = std::chrono::steady_clock::now();
            e.process(l.data(), r.data(), count,
                      {true, true, false, static_cast<std::int64_t>(at),
                       at * 3.0 / rate, 180},
                      nullptr,
                      at == 0 || !dense ? std::span<const Command>{}
                                        : std::span<const Command>(commands));
            maximum =
                std::max(maximum, std::chrono::duration<double, std::micro>(
                                      std::chrono::steady_clock::now() - start)
                                      .count());
            checksum += l[0] + r[0];
            at += count;
          }
          const auto elapsed = std::chrono::duration<double>(
                                   std::chrono::steady_clock::now() - begin)
                                   .count();
          if (e.rejected() != 0 || !std::isfinite(checksum))
            throw std::runtime_error("benchmark invalid");
          std::cout << N01_COMMIT << ',' << N01_COMPILER << ','
                    << (dense ? "dense_nested" : "sparse") << ',' << rate << ','
                    << block << ',' << run << ',' << elapsed / 8 * 100 << ','
                    << maximum << ',' << e.audioBytes() << ',' << sizeof(e)
                    << ',' << checksum << '\n';
        }
      }
  }
}
}  // namespace
int main(int argc, char** argv) {
  try {
    if (argc == 3 && std::string(argv[1]) == "--pack")
      pack(argv[2]);
    else if (argc == 2 && std::string(argv[1]) == "--benchmark")
      benchmark();
    else if (argc == 2 && std::string(argv[1]) == "--check") {
      for (const auto& s : studies) {
        const auto a = render(s, 127), b = render(s, 511);
        if (a.left != b.left || a.right != b.right || a.hits != b.hits)
          throw std::runtime_error(std::string("partition mismatch: ") +
                                   s.name);
        if (measure(a).rms == 0) throw std::runtime_error("empty fixture");
      }
      std::cout << studies.size()
                << " studies: identical stereo audio and event traces at "
                   "127/511 frames\n";
    } else {
      std::cerr
          << "usage: n01_lab --pack NEW_DIRECTORY | --check | --benchmark\n";
      return 2;
    }
  } catch (const std::exception& e) {
    std::cerr << "n01_lab: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
