#include "../../apps/impulse/impulse_starting_points.hpp"
#include "../quality/audio.hpp"

#include <iostream>
#include <numeric>

using namespace aste::impulse;
using namespace power;

void require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}

Parameters space() {
  Parameters p;
  p.energy = .3F;
  p.variation = 0;
  p.tracks[0].decayMs = 320;
  for (auto& track : p.tracks) {
    track.length = 16;
    track.probability = 1;
    track.rotation = 0;
    track.pattern.fill(0);
  }
  p.tracks[0].pattern[0] = p.tracks[0].pattern[9] = p.tracks[5].pattern[6] = 1;
  return p;
}

Audio render(Parameters p, double rate = 48000, int solo = -1,
             std::size_t block = 127) {
  Audio a(static_cast<std::size_t>(rate * (solo < 0 ? 8 : 4)), rate);
  Processor engine;
  engine.prepare(rate, p);
  p.sequenceEnabled = solo < 0;
  for (std::size_t at = 0; at < a.left.size();) {
    const auto stop = static_cast<std::size_t>(rate * 4.8);
    auto n = std::min(block, a.left.size() - at);
    if (at < stop) n = std::min(n, stop - at);
    const MidiEvent hit{0, 36 + solo, 1, true};
    engine.process(a.left.data() + at, a.right.data() + at, n, p,
                   {solo < 0 && at < stop, 100, at / rate * (100. / 60)},
                   solo >= 0 && at == 0 ? std::span{&hit, 1}
                                        : std::span<const MidiEvent>{});
    at += n;
  }
  validate(a);
  return a;
}

double time95(const Audio& a) {
  double total{};
  for (float x : a.left) total += double(x) * x;
  require(total > 0, "Silent sound object");
  double sum{};
  for (std::size_t n = 0; n < a.left.size(); ++n) {
    sum += double(a.left[n]) * a.left[n];
    if (sum >= .95 * total) return 1000. * n / a.rate;
  }
  throw std::runtime_error("Energy extent");
}

struct SubMetrics {
  double peakHz, bandFraction;
};
SubMetrics subMetrics(const Audio& a) {
  constexpr std::size_t size = 32768;
  const auto bins = spectrum(a, static_cast<std::size_t>(a.rate * .04), size);
  const double total = std::accumulate(bins.begin(), bins.end(), 0.);
  require(total > 0, "Silent sub spectrum");
  double low{};
  for (std::size_t n = 0; n < bins.size(); ++n) {
    const double hz = n * a.rate / size;
    if (hz >= 20 && hz <= 100) low += bins[n];
  }
  const auto peak = std::max_element(bins.begin(), bins.end()) - bins.begin();
  return {peak * a.rate / size, low / total};
}

void check(bool fault) {
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
    for (auto choice : {SparseStartingPoint::clicksAndSub,
                        SparseStartingPoint::cutsAndPressure}) {
      auto p = sparseStartingPoint(choice);
      if (fault) p.tracks[4].pitchHz = 160;
      const auto sub = render(p, rate, 4);
      const auto edge =
          render(p, rate, choice == SparseStartingPoint::clicksAndSub ? 1 : 7);
      const auto m = subMetrics(sub);
      require(m.peakHz >= 30 && m.peakHz <= 48 && m.bandFraction > .90,
              "Sub must concentrate its settled energy below 100 Hz");
      require(time95(sub) > 150 && time95(sub) < 800,
              "Sub needs a body, not a click or an indefinite pad");
      require(time95(edge) < 20, "Click/cut must concentrate in under 20 ms");
      const auto split = render(p, rate, 4, 511);
      require(sub.left == split.left && sub.right == split.right,
              "Sub MIDI rendering must not depend on block size");
      require(sub.left == sub.right && edge.left == edge.right,
              "No random stereo motion in the starting objects");
    }
  std::cout << "12 scene/rate cases: sub spectrum, duration, click extent, "
               "mono and MIDI partition checks passed\n";
}

void pack(const std::filesystem::path& directory) {
  require(!std::filesystem::exists(directory), "Use a new output directory");
  std::filesystem::create_directories(directory);
  const auto click = sparseStartingPoint(SparseStartingPoint::clicksAndSub);
  const auto cut = sparseStartingPoint(SparseStartingPoint::cutsAndPressure);
  auto previousSameRhythm = space();
  previousSameRhythm.tracks[0].pattern = click.tracks[4].pattern;
  previousSameRhythm.tracks[5].pattern = click.tracks[1].pattern;
  const std::array names{"01_previous_space", "02_clicks_sub",
                         "03_cuts_pressure", "04_previous_voices_same_rhythm"};
  const std::array parameters{space(), click, cut, previousSameRhythm};
  std::vector<Audio> patterns;
  std::vector<double> scales;
  for (const auto& p : parameters) {
    patterns.push_back(render(p));
    const auto level = loudness(patterns.back());
    require(level.has_value(), "Pattern loudness unknown");
    scales.push_back(gain(-23 - *level));
  }
  double headroom = 1;
  for (std::size_t i = 0; i < patterns.size(); ++i)
    headroom = std::min(headroom, .8 / (measure(patterns[i]).peak * scales[i]));
  auto csv = output(directory / "patterns.csv");
  csv << "name,raw_peak,raw_lufs,matched_lufs,scale,raw_hash\n";
  for (std::size_t i = 0; i < patterns.size(); ++i) {
    const auto raw = measure(patterns[i]);
    const double scale = scales[i] * headroom;
    for (std::size_t n = 0; n < patterns[i].left.size(); ++n) {
      patterns[i].left[n] *= static_cast<float>(scale);
      patterns[i].right[n] *= static_cast<float>(scale);
    }
    const auto matched = measure(patterns[i]);
    require(matched.lufs && std::abs(*matched.lufs -
                                     (-23 + 20 * std::log10(headroom))) < .05,
            "Matched pattern loudness differs beyond 0.05 LU");
    wav(directory / (std::string(names[i]) + ".wav"), patterns[i]);
    csv << names[i] << ',' << raw.peak << ',' << *raw.lufs << ','
        << *matched.lufs << ',' << scale << ',' << raw.hash << '\n';
  }
  auto objects = output(directory / "objects.csv");
  objects
      << "scene,object,peak,t95_ms,settled_peak_hz,settled_20_100_fraction\n";
  for (int scene = 0; scene < 2; ++scene)
    for (int track : {4, scene ? 7 : 1}) {
      const auto a = render(scene ? cut : click, 48000, track);
      const auto m = measure(a);
      const auto name = std::string(scene ? "cuts" : "clicks") +
                        (track == 4 ? "_sub" : "_edge");
      wav(directory / (name + ".wav"), a);
      objects << scene << ',' << name << ',' << m.peak << ',' << time95(a)
              << ',';
      if (track == 4) {
        const auto sub = subMetrics(a);
        objects << sub.peakHz << ',' << sub.bandFraction;
      } else
        objects << "not_applicable,not_applicable";
      objects << '\n';
    }
  std::cout << "8 WAVs rendered; patterns matched, isolated objects at actual "
               "levels\n";
}

int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--check")
      check(false);
    else if (argc == 2 && std::string(argv[1]) == "--fault")
      check(true);
    else if (argc == 3 && std::string(argv[1]) == "--pack")
      pack(argv[2]);
    else
      throw std::runtime_error("Use --check, --fault, or --pack NEW_DIRECTORY");
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
