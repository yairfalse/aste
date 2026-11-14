#include "render.hpp"
#include "benchmark.hpp"
#include "meter_checks.hpp"
#include "space_diagnostics.hpp"
#include "colour_diagnostics.hpp"
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <new>

namespace {
thread_local bool auditing{};
thread_local std::size_t allocations{}, frees{};
}  // namespace
void* operator new(std::size_t n) {
  if (auditing) ++allocations;
  if (auto* p = std::malloc(std::max<std::size_t>(n, 1))) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  if (auditing) ++frees;
  std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

namespace power {
void require(bool ok, const char* message) {
  if (!ok) throw std::runtime_error(message);
}
Audio sine(double hz, double db, double fs = 48000, double seconds = 4) {
  Audio a(static_cast<std::size_t>(fs * seconds), fs);
  for (std::size_t i = 0; i < a.left.size(); ++i)
    a.left[i] = a.right[i] =
        static_cast<float>(gain(db) * std::sin(2 * pi * hz * i / fs));
  return a;
}
template <class Engine, class P, class E, class T>
double secondAttack(double fs) {
  Engine e;
  P p;
  p.sequenceEnabled = false;
  p.sustain = .8F;
  p.releaseMs = 120;
  e.prepare(fs, p);
  using Type = decltype(E{}.type);
  std::array<float, 1> l{}, r{};
  double peak{};
  for (std::size_t i = 0; i < static_cast<std::size_t>(fs * .25); ++i) {
    std::array<E, 1> event{};
    std::size_t count{};
    if (i == 0 || i == static_cast<std::size_t>(fs * .12))
      event[count++] = {0, Type::noteOn, 48, .8F};
    if (i == static_cast<std::size_t>(fs * .1))
      event[count++] = {0, Type::noteOff, 48, 0};
    e.process(l.data(), r.data(), 1, p, T{}, std::span(event).first(count));
    if (i >= static_cast<std::size_t>(fs * .12))
      peak = std::max(peak, static_cast<double>(e.meters().envelope));
  }
  return peak;
}
void callbackCheck() {
  aste::sequence_candidate::Processor seq;
  aste::sequence_candidate::Parameters sp;
  sp.sequenceEnabled = false;
  seq.prepare(48000, sp);
  auto loop = std::make_unique<aste::loop_candidate::Processor>();
  loop->prepare(48000, 2, true);
  aste::loop_candidate::Parameters lp;
  lp.capture = true;
  std::array<float, 128> l{}, r{};
  const std::array event{aste::sequence_candidate::MidiEvent{
      0, aste::sequence_candidate::MidiEventType::noteOn, 48, .8F}};
  auditing = true;
  auto* probe = ::operator new(8);
  ::operator delete(probe);
  auditing = false;
  require(allocations && frees, "allocation guard positive control");
  allocations = frees = 0;
  auditing = true;
  for (int n = 0; n < 64; ++n) {
    seq.process(l.data(), r.data(), l.size(), sp, {}, event);
    loop->process(l.data(), r.data(), l.size(), lp);
  }
  auditing = false;
  require(allocations == 0 && frees == 0, "candidate callback allocated/freed");
}
void checks() {
  const auto tone = sine(1000, -20);
  require(std::abs(*loudness(tone) + 19.9933) < .02,
          "analytic 1kHz K-weighting oracle");
  auto mono = tone;
  std::fill(mono.right.begin(), mono.right.end(), 0);
  require(std::abs(*loudness(mono) - *loudness(tone) + 3.01029995664) < 1e-7,
          "stereo channel weighting");
  auto inverted = tone;
  for (auto& x : inverted.right) x = -x;
  require(std::abs(*loudness(inverted) - *loudness(tone)) < 1e-9,
          "anti-phase stereo loudness preserved");
  require(!loudness(Audio{}), "silent loudness must be unknown");
  require(!loudness(sine(1000, -90)), "below-gate loudness must be unknown");
  const auto fft = spectrum(sine(1500, -20), 0, 4096);
  require(std::max_element(fft.begin(), fft.end()) - fft.begin() == 128,
          "FFT bin-centred frequency oracle");
  bool invalid{};
  try {
    Audio bad(0);
    (void)measure(bad);
  } catch (const std::runtime_error&) {
    invalid = true;
  }
  require(invalid, "empty measurements must fail");
  invalid = false;
  try {
    auto bad = tone;
    bad.left[3] = std::numeric_limits<float>::quiet_NaN();
    (void)loudness(bad);
  } catch (const std::runtime_error&) {
    invalid = true;
  }
  require(invalid, "nonfinite metering must fail");
  for (double fs : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    const double peak = secondAttack<aste::sequence_candidate::Processor,
                                     aste::sequence_candidate::Parameters,
                                     aste::sequence_candidate::MidiEvent,
                                     aste::sequence_candidate::Transport>(fs);
    require(peak > .95,
            "new separated MIDI note must attack during prior release");
  }
  const auto input = fixture(0);
  const auto seq1 = render(2, input, 0, true, 1, -1, 127),
             seq2 = render(2, input, 0, true, 1, -1, 511);
  require(seq1.left == seq2.left && seq1.right == seq2.right,
          "candidate Sequence block identity");
  const auto loop1 = render(3, input, 5, true, 0, -1, 127),
             loop2 = render(3, input, 5, true, 0, -1, 511);
  require(loop1.left == loop2.left && loop1.right == loop2.right,
          "candidate Loop block identity");
  const auto legato = render(2, input, 0, false, 2),
             legatoCandidate = render(2, input, 0, true, 2);
  require(legato.left == legatoCandidate.left &&
              legato.right == legatoCandidate.right,
          "genuinely overlapping notes retain original legato behavior");
  callbackCheck();
  meterChecks();
  colourChecks();
  spaceChecks();
  std::cout << "power measurement and candidate contracts passed\n";
}
void hostAutomationCheck() {
  for (double fs : {44100., 48000.}) {
    const auto source = fixture(0, fs);
    for (int product = 0; product < 6; ++product) {
      const auto a = render(product, source, 4, false, 0, -1, 127),
                 b = render(product, source, 4, false, 0, -1, 511);
      if (a.left != b.left || a.right != b.right)
        throw std::runtime_error(
            "automation partition mismatch: " +
            std::string(productNames[static_cast<std::size_t>(product)]) +
            " at " + std::to_string(fs) + " Hz; RMS difference " +
            std::to_string(difference(a, b)));
    }
  }
}
struct Record {
  std::string name;
  Audio audio;
  Metrics metrics;
};
struct Match {
  Audio audio;
  Metrics metrics;
  double scale;
};
std::vector<Match> matchGroup(const std::vector<Record>& records,
                              double target) {
  // Absolute gating can change after scaling. Re-measure and solve rather
  // than pretending integrated loudness always shifts by exactly the gain.
  for (int headroom = 0; headroom < 8; ++headroom) {
    std::vector<Match> matches;
    double nextTarget = target;
    for (const auto& r : records) {
      double scale = r.metrics.lufs       ? gain(target - *r.metrics.lufs)
                     : r.metrics.peak > 0 ? .1 / r.metrics.peak
                                          : 1;
      Match match{r.audio, {}, scale};
      for (int iteration = 0; iteration < 8; ++iteration) {
        match.audio = r.audio;
        for (std::size_t n = 0; n < match.audio.left.size(); ++n) {
          match.audio.left[n] *= static_cast<float>(scale);
          match.audio.right[n] *= static_cast<float>(scale);
        }
        match.metrics = measure(match.audio);
        match.scale = scale;
        if (!match.metrics.lufs ||
            std::abs(*match.metrics.lufs - target) < .0001)
          break;
        scale *= gain(target - *match.metrics.lufs);
      }
      if (match.metrics.lufs)
        require(std::abs(*match.metrics.lufs - target) < .05,
                "integrated matching solve failed");
      if (match.metrics.peak > .8)
        nextTarget = std::min(
            nextTarget,
            target - 20 * std::log10(match.metrics.peak / .8) - .00001);
      matches.push_back(std::move(match));
    }
    if (nextTarget == target) return matches;
    target = nextTarget;
  }
  throw std::runtime_error("common comparison headroom solve failed");
}
void number(std::ostream& out, std::optional<double> value) {
  if (value)
    out << *value;
  else
    out << "unknown";
}
void group(const std::filesystem::path& dir, int id,
           const std::string& fixtureName, std::vector<Record>& records,
           std::ostream& measurements, std::ostream& responses,
           std::ostream& key) {
  double target = -23;
  for (auto& r : records) {
    r.metrics = measure(r.audio);
    if (r.metrics.lufs && r.metrics.peak > 0)
      target = std::min(target,
                        *r.metrics.lufs + 20 * std::log10(.8 / r.metrics.peak));
  }
  const auto matches = matchGroup(records, target);
  for (std::size_t i = 0; i < records.size(); ++i) {
    auto& r = records[i];
    const auto& m = r.metrics;
    const std::string token =
        "G" + std::to_string(id) + "_" +
        std::to_string((records.size() + 3 - i) % records.size() + 1);
    const auto& matched = matches[i].audio;
    const auto& final = matches[i].metrics;
    const auto scale = matches[i].scale;
    require(final.peak <= .80001, "matched export sample peak exceeded");
    wav(dir / "audio" / (token + ".wav"), matched);
    wav(dir / "source" / ("G" + std::to_string(id) + "_" + r.name + ".wav"),
        r.audio);
    key << token << ',' << fixtureName << ',' << r.name << '\n';
    responses << token << ",,,,,,,,\n";
    measurements << token << ',' << fixtureName << ',' << r.name << ','
                 << m.peak << ',' << m.rms << ',';
    number(measurements, m.lufs);
    measurements << ',';
    number(measurements, final.lufs);
    measurements << ',' << scale << ',';
    number(measurements, m.rms > 0
                             ? std::optional(20 * std::log10(m.peak / m.rms))
                             : std::nullopt);
    measurements << ',' << m.tail << ',' << m.delta << ',';
    number(measurements, m.correlation);
    measurements << ',';
    number(measurements,
           m.midEnergy + m.sideEnergy > 0
               ? std::optional(m.sideEnergy / (m.midEnergy + m.sideEnergy))
               : std::nullopt);
    measurements << ',' << std::hex << m.hash << std::dec << '\n';
  }
}
void pack(const std::filesystem::path& root) {
  require(!std::filesystem::exists(root),
          "pack path exists; do not overwrite responses");
  std::filesystem::create_directories(root);
  std::size_t total{};
  for (int product = 0; product < 6; ++product) {
    const auto dir = root / productNames[static_cast<std::size_t>(product)];
    std::filesystem::create_directories(dir / "audio");
    std::filesystem::create_directory(dir / "source");
    auto metrics = output(dir / "measurements.csv"),
         responses = output(dir / "responses.csv"),
         key = output(dir / "answer-key.csv");
    metrics << "id,fixture,variant,raw_peak,raw_rms,raw_lufs,matched_lufs,"
               "matched_gain,crest_db,last_second_rms,max_sample_delta,"
               "correlation,side_fraction,raw_fnv1a64\n";
    responses << "id,range,character,responsiveness,depth,control,preference,"
                 "confidence,notes\n";
    key << "id,fixture,variant\n";
    int id{};
    const bool synth = product == 2 || product == 4;
    const int count = synth ? (product == 2 ? 3 : 2) : 7;
    for (int source = 0; source < count; ++source) {
      const auto input = fixture(synth ? 0 : source);
      std::vector<Record> records;
      if (!synth) records.push_back({"dry", input, {}});
      for (int setting = 0; setting < 6; ++setting)
        records.push_back({settingNames[static_cast<std::size_t>(setting)],
                           render(product, input, setting, false, source),
                           {}});
      if (product == 1)
        records.push_back(
            {"linked_12db_gesture", render(product, input, 2, true), {}});
      if (product == 2)
        records.push_back({"release_aware_default",
                           render(product, input, 0, true, source),
                           {}});
      if (product == 3)
        records.push_back(
            {"continuous_wow_clock", render(product, input, 5, true), {}});
      group(dir, ++id,
            synth ? (source == 0   ? "host_pattern"
                     : source == 1 ? "gapped_MIDI"
                                   : "overlapping_MIDI")
                  : fixtureNames[static_cast<std::size_t>(source)],
            records, metrics, responses, key);
      total += records.size();
    }
    if (product == 4)
      for (int object = 0; object < 8; ++object) {
        std::vector<Record> records;
        for (int setting = 0; setting < 3; ++setting)
          records.push_back({settingNames[static_cast<std::size_t>(setting)],
                             impulseRender(setting, 1, object, 48000, 127),
                             {}});
        group(dir, ++id, "object_" + std::to_string(object), records, metrics,
              responses, key);
        total += records.size();
      }
  }
  auto manifest = output(root / "MANIFEST.txt");
  manifest
      << "Aste musical-power audit; source commit " << POWER_COMMIT
      << "; compiler " << POWER_COMPILER << "\n"
      << total << " render variants; " << total * 2
      << " WAVs; 48k stereo float32, 8 seconds.\n"
      << "All material is deterministic procedural diagnostic audio, including "
         "a layered generated passage, NOT supplied recordings or real project "
         "excerpts.\n"
      << "audio/: 48k BS.1770-style K-weighted integrated matching, -70LUFS "
         "absolute and -10LU relative gating; target at most -23LUFS.\n"
      << "ONE common lower target per comparison group preserves matching and "
         "keeps sample peaks <=0.8. Actual matched LUFS recorded, "
         "tolerance0.05LU. Not certified metering or true-peak limiting.\n"
      << "source/: unmodified float diagnostic outputs; can exceed0dBFS. Use "
         "matched audio/ for audition. No product output was changed for "
         "matching.\n"
      << "Exceptions are NAMED settings: Harmonic linked +12/-12 "
         "operating-point gesture is the experiment, not the matching "
         "algorithm.\n"
      << "Six settings are explicit lab recipes in render.hpp, not "
         "artist-approved sweet spots or factory preset names. Impulse default "
         "copies current adapter Steady Start, not old core defaults.\n"
      << "Read response sheet before revealing key. IDs use a deterministic "
         "reversed rotation, not randomization or a claim of blinding a study "
         "operator.\n"
      << "Rate character/control separately from preference; no reliable "
         "difference is valid. Repeat on monitors/headphones/mono in a real "
         "project.\n"
      << "No new human listening, Cubase validation or musical improvement is "
         "claimed. Raw hashes are build-local fingerprints, not portable audio "
         "guarantees.\n";
  std::cout << total << " variants, " << total * 2 << " WAVs written\n";
}
void noteDiagnostics(const std::filesystem::path& dir) {
  auto partitions = output(dir / "host_partition.csv");
  partitions
      << "product,rate,block_a,block_b,difference_rms,first_different_sample\n";
  for (double fs : {44100., 48000.})
    for (int product = 0; product < 6; ++product) {
      const auto input = fixture(0, fs);
      const auto a = render(product, input, 4, false, 0, -1, 127),
                 b = render(product, input, 4, false, 0, -1, 511);
      std::size_t first{};
      while (first < a.left.size() && a.left[first] == b.left[first] &&
             a.right[first] == b.right[first])
        ++first;
      partitions << productNames[static_cast<std::size_t>(product)] << ',' << fs
                 << ",127,511," << difference(a, b) << ',';
      if (first < a.left.size())
        partitions << first;
      else
        partitions << "none";
      partitions << '\n';
    }
  auto out = output(dir / "note_response.csv");
  out << "test,rate,production,candidate_or_comparison,unit\n";
  for (double fs : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    const auto a =
        secondAttack<aste::sequence::Processor, aste::sequence::Parameters,
                     aste::sequence::MidiEvent, aste::sequence::Transport>(fs);
    const auto b = secondAttack<aste::sequence_candidate::Processor,
                                aste::sequence_candidate::Parameters,
                                aste::sequence_candidate::MidiEvent,
                                aste::sequence_candidate::Transport>(fs);
    out << "sequence_second_attack," << fs << ',' << a << ',' << b
        << ",peak_envelope\n";
    auto p = impulseStarter();
    p.sequenceEnabled = false;
    p.variation = 0;
    for (std::size_t object = 0; object < 8; ++object) {
      const auto renderOne = [&](float tone) {
        auto q = p;
        q.tracks[object].tone = tone;
        Audio a(static_cast<std::size_t>(fs), fs);
        aste::impulse::Processor engine;
        engine.prepare(fs, q);
        const std::array event{aste::impulse::MidiEvent{
            17, 36 + static_cast<int>(object), .8F, true}};
        engine.process(a.left.data(), a.right.data(), a.left.size(), q, {},
                       event);
        return a;
      };
      const auto low = renderOne(0), high = renderOne(1);
      out << "impulse_tone_object_" << object << ',' << fs << ','
          << rms(low, 0, 1) << ',' << difference(low, high)
          << ",low_rms_then_endpoint_difference_rms\n";
    }
  }
  const auto a = impulseRender(2, 1, -1, 48000, 127),
             b = impulseRender(2, 1, -1, 48000, 511);
  out << "impulse_MIDI_partition,48000," << measure(a).rms << ','
      << difference(a, b) << ",reference_rms_then_difference_rms\n";
  const auto c = impulseRender(0, 0, -1, 48000, 127),
             d = impulseRender(0, 0, -1, 48000, 511);
  out << "impulse_host_partition,48000," << measure(c).rms << ','
      << difference(c, d) << ",reference_rms_then_difference_rms\n";
  auto alias = output(dir / "impulse_click_fold.csv");
  alias << "rate,fundamental_hz,fold_hz,fold_dbc\n";
  for (double fs : {48000., 192000.}) {
    auto p = impulseStarter();
    p.sequenceEnabled = false;
    p.energy = 0;
    p.variation = 0;
    p.tracks[1].pitchHz = 7000;
    p.tracks[1].decayMs = 1500;
    p.tracks[1].drive = 0;
    aste::impulse::Processor e;
    e.prepare(fs, p);
    Audio signal(static_cast<std::size_t>(fs * 2), fs);
    const std::array event{aste::impulse::MidiEvent{0, 37, 1, true}};
    e.process(signal.left.data(), signal.right.data(), signal.left.size(), p,
              {}, event);
    const double fundamental = std::abs(component(signal, 7000, .1, 1));
    if (!(fundamental > 0))
      throw std::runtime_error("missing click fundamental");
    for (double hz : {1000., 6000., 13000., 20000.})
      alias << fs << ",7000," << hz << ','
            << 20 * std::log10(std::max(
                        1e-15,
                        std::abs(component(signal, hz, .1, 1)) / fundamental))
            << '\n';
  }
}
}  // namespace power
int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--check")
      power::checks();
    else if (argc == 2 && std::string(argv[1]) == "--host-check")
      power::hostAutomationCheck();
    else if (argc == 3 && std::string(argv[1]) == "--pack")
      power::pack(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--benchmark")
      power::benchmarks(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--measure") {
      const std::filesystem::path dir = argv[2];
      std::filesystem::create_directories(dir);
      power::noteDiagnostics(dir);
      power::colourDiagnostics(dir);
      power::spaceDiagnostics(dir);
    } else {
      std::cerr << "usage: power_lab --check | --pack NEW_DIR | --measure DIR "
                   "| --benchmark CSV\n";
      return 2;
    }
  } catch (const std::exception& e) {
    auditing = false;
    std::cerr << "power_lab: " << e.what() << '\n';
    return 1;
  }
  return 0;
}
