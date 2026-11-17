#include "circuit.hpp"
#include "score.hpp"
#include "research/quality/audio.hpp"
#include <chrono>
#include <iostream>
#include <memory>
using namespace aste::contamination;
using power::Audio;
namespace {
void require(bool b, const char* text) {
  if (!b) throw std::runtime_error(text);
}
Audio render(int architecture, const Controls& c, int score = 0,
             bool wet = true, double rate = 48000, std::size_t block = 127,
             int factor = 0) {
  Audio a(static_cast<std::size_t>(rate * 18), rate);
  auto events = aste::c01_study::phrase(rate, score);
  std::size_t next = 0;
  if (!factor) factor = Engine::recommendedFactor(rate);
  Engine old;
  revised::Engine updated;
  if (architecture < 0)
    require(old.prepare(rate, factor), "baseline prepare");
  else
    require(updated.prepare(rate, factor,
                            static_cast<revised::Architecture>(architecture)),
            "candidate prepare");
  for (std::size_t at = 0; at < a.left.size(); at += block) {
    auto n = std::min(block, a.left.size() - at);
    std::array<Event, 16> local{};
    std::size_t count = 0;
    while (next < events.size() && events[next].offset < at + n) {
      local[count] = events[next++];
      local[count++].offset -= at;
    }
    if (architecture < 0)
      old.process(a.left.data() + at, a.right.data() + at, n, c,
                  std::span(local).first(count));
    else
      updated.process(a.left.data() + at, a.right.data() + at, n, c,
                      std::span(local).first(count), wet);
  }
  power::validate(a);
  return a;
}
Audio matched(const Audio& raw, double target) {
  auto level = power::loudness(raw);
  require(level.has_value(), "silent comparison");
  double scale = power::gain(target - *level);
  Audio out = raw;
  for (int i = 0; i < 10; ++i) {
    out = raw;
    for (auto* ch : {&out.left, &out.right})
      for (auto& v : *ch) v = static_cast<float>(v * scale);
    auto measured = power::loudness(out);
    require(measured.has_value(), "invalid matched loudness");
    if (std::abs(*measured - target) < .005) break;
    scale *= power::gain(target - *measured);
  }
  return out;
}
void group(const std::filesystem::path& folder, std::ostream& csv,
           const std::vector<std::pair<std::string, Audio>>& items) {
  double target = -27;
  std::vector<Audio> out;
  for (int iteration = 0; iteration < 10; ++iteration) {
    out.clear();
    double peak = 0;
    for (auto& [name, a] : items) {
      (void)name;
      out.push_back(matched(a, target));
      peak = std::max(peak, power::measure(out.back()).peak);
    }
    if (peak <= .85) break;
    target -= 20 * std::log10(peak / .85) + .01;
  }
  for (std::size_t i = 0; i < items.size(); ++i) {
    auto raw = power::measure(items[i].second), m = power::measure(out[i]);
    require(m.peak <= .851 && m.lufs && std::abs(*m.lufs - target) < .05,
            "match/headroom");
    double tail = 0;
    for (std::size_t n = static_cast<std::size_t>(12 * out[i].rate);
         n < static_cast<std::size_t>(14 * out[i].rate); ++n)
      tail += .5 * (out[i].left[n] * out[i].left[n] +
                    out[i].right[n] * out[i].right[n]);
    csv << items[i].first << ',' << raw.peak << ',' << raw.rms << ','
        << (raw.lufs ? *raw.lufs : -999) << ',' << m.peak << ',' << *m.lufs
        << ',' << std::sqrt(tail / (out[i].rate * 2)) << '\n';
    power::wav(folder / (items[i].first + ".wav"), out[i]);
  }
}
void benchmark(const std::filesystem::path& folder) {
  auto out = power::output(folder / "benchmark.csv");
  out << "architecture,rate,factor,p50_us,p99_us,max_us,deadline_us,heap_bytes,"
         "safety\n";
  for (int arch = 0; arch < 4; ++arch)
    for (double rate : {48000., 96000., 192000.}) {
      revised::Engine e;
      int factor = Engine::recommendedFactor(rate);
      require(e.prepare(rate, factor, static_cast<revised::Architecture>(arch)),
              "bench prepare");
      auto c = aste::c01_study::patch(3);
      c.pressure = c.contamination = c.instability = c.memory = c.motion = 1;
      c.sustain = 1;
      for (int note = 48; note < 60; ++note) e.event({0, Kind::On, note, .8});
      std::array<float, 128> l{}, r{};
      std::vector<double> elapsed;
      for (int block = 0; block < 250; ++block) {
        auto start = std::chrono::steady_clock::now();
        e.process(l.data(), r.data(), l.size(), c);
        double us = std::chrono::duration<double, std::micro>(
                        std::chrono::steady_clock::now() - start)
                        .count();
        if (block >= 50) elapsed.push_back(us);
      }
      std::sort(elapsed.begin(), elapsed.end());
      out << arch << ',' << rate << ',' << factor << ','
          << elapsed[elapsed.size() / 2] << ',' << elapsed[elapsed.size() - 3]
          << ',' << elapsed.back() << ',' << 128 / rate * 1e6 << ','
          << e.heapBytes() << ',' << e.observation().safety << '\n';
    }
}
}  // namespace
int main(int argc, char** argv) {
  try {
    require(argc == 2 || argc == 3,
            "usage: c01_redesign folder | --benchmark fresh-folder");
    bool bench = argc == 3 && std::string(argv[1]) == "--benchmark";
    std::filesystem::path folder = argv[bench ? 2 : 1];
    require(!std::filesystem::exists(folder), "fresh folder");
    std::filesystem::create_directories(folder);
    if (bench) {
      benchmark(folder);
      return 0;
    }
    auto csv = power::output(folder / "measurements.csv");
    csv << "name,raw_peak,raw_rms,raw_lufs,matched_peak,matched_lufs,tail_rms_"
           "12_14s\n";
    auto events = power::output(folder / "midi.csv");
    events << "sample,kind,note,value\n";
    for (const auto& e : aste::c01_study::phrase(48000))
      events << e.offset << ',' << static_cast<int>(e.kind) << ',' << e.note
             << ',' << e.value << '\n';
    const std::array<const char*, 5> names{"original", "dense-source",
                                           "nonlinear-filter", "dirty-return",
                                           "interacting"};
    std::vector<std::pair<std::string, Audio>> central;
    for (int arch = -1; arch < 4; ++arch)
      for (bool wet : {false, true}) {
        auto name = "compare-" +
                    std::string(names[static_cast<std::size_t>(arch + 1)]) +
                    (wet ? "-processed" : "-dry");
        central.emplace_back(name,
                             render(arch, aste::c01_study::patch(4), 0, wet));
      }
    group(folder, csv, central);
    // A second comparison holds the foreground chord loudness fixed. Long
    // tails otherwise alter the integrated gate and can bias onset levels.
    std::vector<Audio> anchors;
    double onsetTarget = -27;
    for (int attempt = 0; attempt < 8; ++attempt) {
      anchors.clear();
      double peak = 0;
      for (std::size_t item = 1; item < central.size(); item += 2) {
        const auto& raw = central[item].second;
        Audio onset(19200, 48000);
        std::copy_n(raw.left.begin() + 24000, 19200, onset.left.begin());
        std::copy_n(raw.right.begin() + 24000, 19200, onset.right.begin());
        auto level = power::loudness(onset);
        require(level.has_value(), "foreground loudness");
        double scale = power::gain(onsetTarget - *level);
        auto matched = raw;
        for (auto* ch : {&matched.left, &matched.right})
          for (auto& v : *ch) v = static_cast<float>(v * scale);
        peak = std::max(peak, power::measure(matched).peak);
        anchors.push_back(std::move(matched));
      }
      if (peak <= .85) break;
      onsetTarget -= 20 * std::log10(peak / .85) + .01;
    }
    auto anchorLog = power::output(folder / "foreground-match.txt");
    anchorLog << "Same first chord window .5-.9s matched to " << onsetTarget
              << " LUFS; full-file integrated levels intentionally differ.\n";
    for (std::size_t i = 0; i < anchors.size(); ++i)
      power::wav(folder / ("foreground-" + std::string(names[i]) + ".wav"),
                 anchors[i]);

    const std::array<const char*, 5> patches{
        "01-submerged-chord", "02-corroded-stab", "03-feedback-ghost",
        "04-unstable-mass", "05-dub-residue"};
    for (int patch = 0; patch < 5; ++patch) {
      int shape = patch == 2 ? 1 : patch == 3 ? 2 : patch == 4 ? 3 : 0;
      auto scoreFile = power::output(
          folder / (std::string(patches[static_cast<std::size_t>(patch)]) +
                    "-midi.csv"));
      scoreFile << "sample,kind,note,value\n";
      for (const auto& e : aste::c01_study::phrase(48000, shape))
        scoreFile << e.offset << ',' << static_cast<int>(e.kind) << ','
                  << e.note << ',' << e.value << '\n';
      std::vector<std::pair<std::string, Audio>> pair;
      pair.emplace_back(std::string(patches[patch]) + "-old",
                        render(-1, aste::c01_study::patch(patch), shape));
      pair.emplace_back(std::string(patches[patch]) + "-new-dry",
                        render(3, aste::c01_study::patch(patch), shape, false));
      pair.emplace_back(std::string(patches[patch]) + "-new",
                        render(3, aste::c01_study::patch(patch), shape));
      group(folder, csv, pair);
    }
    // Isolated nonlinear alias probe: no noise, drift, memory, detune or
    // movement.
    auto alias = power::output(folder / "alias.csv");
    alias << "note,factor,nonharmonic_fraction\n";
    for (int note : {93, 105})
      for (int factor : {1, 2, 4, 8}) {
        revised::Engine e;
        e.prepare(48000, factor, revised::Architecture::Resonant);
        Controls c;
        c.pressure = 1;
        c.contamination = c.instability = c.memory = c.motion = c.detune = 0;
        c.colour = 1800;
        c.sustain = 1;
        e.event({0, Kind::On, note, .8});
        Audio a(48000 * 2);
        e.process(a.left.data(), a.right.data(), a.left.size(), c);
        power::wav(folder / ("alias-" + std::to_string(note) + "-" +
                             std::to_string(factor) + "x.wav"),
                   a);
        alias << note << ',' << factor << ",-1\n";
      }
    std::cout
        << "Rendered 25 integrated-matched files, 5 foreground-matched files "
           "and 8 raw alias probes; no subjective success claimed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
