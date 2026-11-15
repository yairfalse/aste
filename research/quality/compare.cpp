#include "audio.hpp"
#include "meter_checks.hpp"
#include "apps/density/density_processor.hpp"
#include "apps/harmonic/harmonic_processor.hpp"
#include "apps/sequence/sequence_processor.hpp"
#include "apps/loop/loop_processor.hpp"
#include "apps/impulse/impulse_processor.hpp"
#include "apps/field/field_processor.hpp"
#include "reference/density/density_processor.hpp"
#include "reference/harmonic/harmonic_processor.hpp"
#include "reference/sequence/sequence_processor.hpp"
#include "reference/loop/loop_processor.hpp"
#include "reference/impulse/impulse_processor.hpp"
#include "reference/field/field_processor.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <type_traits>

using namespace power;
constexpr std::array products{"density", "harmonic", "sequence",
                              "loop",    "impulse",  "field"};
void require(bool pass, const char* message) {
  if (!pass) throw std::runtime_error(message);
}
template <class P>
void behavior(P& p, bool legacy) {
  if constexpr (requires { p.legacyBehavior; }) p.legacyBehavior = legacy;
}

template <class P>
P density(int profile, double t, bool legacy) {
  P p;
  if (profile) {
    p.driveDb = 6;
    p.crush = .85F;
    p.attackMs = 6;
    p.releaseMs = 80;
    p.density = .65F;
    p.blend = 1;
    p.outputDb = -9;
  }
  if (profile == 2) {
    p.driveDb = 24;
    p.density = p.crush = 1;
    p.attackMs = .02F;
  }
  if (profile == 3) {
    p.driveDb = static_cast<int>(t * 40) % 2 ? 18 : -6;
    p.density = .9F;
  }
  if (profile == 4) {
    p.driveDb = 3;
    p.crush = .7F;
    p.attackMs = 12;
    p.releaseMs = 110;
    p.density = .55F;
    p.blend = .6F;
    p.outputDb = -5;
    p.detectorHpfHz = 100;
  }
  behavior(p, legacy);
  return p;
}
template <class P>
P harmonic(int profile, double t) {
  P p;
  if (profile) {
    p.inputDb = 12;
    p.presenceGainDb = 6;
    p.harmonic = 1;
    p.outputDb = -15;
  }
  if (profile == 2)
    p.foundationGainDb = p.bodyGainDb = p.presenceGainDb = p.airGainDb = 12;
  if (profile == 3) p.presenceGainDb = static_cast<int>(t * 40) % 2 ? 12 : -12;
  if (profile >= 4) {
    p.inputDb = profile == 5 ? 9 : profile == 6 ? 3 : 6;
    p.harmonic = .6F;
    p.outputDb = profile == 5 ? -11 : profile == 6 ? -5 : -8;
  }
  return p;
}
template <class P>
P impulse(int profile, bool legacy) {
  P p;
  p.variation = 0;
  for (auto& tr : p.tracks) {
    tr.length = 16;
    tr.rotation = 0;
    tr.probability = 1;
    tr.pulses = 0;
    tr.pattern.fill(0);
  }
  p.tracks[0].pattern[0] = 2;
  p.tracks[0].pattern[8] = 1;
  for (int n : {2, 6, 10, 14}) p.tracks[1].pattern[n] = 1;
  for (int n : {4, 12}) p.tracks[5].pattern[n] = 1;
  if (profile == 1) {
    p.energy = .3F;
    p.tracks[0].decayMs = 320;
    for (auto& tr : p.tracks) tr.pattern.fill(0);
    p.tracks[0].pattern[0] = p.tracks[0].pattern[9] = p.tracks[5].pattern[6] =
        1;
  }
  if (profile >= 2) {
    p.energy = 1;
    p.variation = .6F;
    for (auto& tr : p.tracks) {
      tr.pattern.fill(2);
      tr.ratchet = 4;
      tr.tone = 1;
    }
  }
  behavior(p, legacy);
  return p;
}

template <bool Ref>
Audio render(int product, int profile, bool legacy, const Audio& input,
             std::size_t block = 127, double* callbackUs = nullptr) {
  Audio a = input;
  using DP = std::conditional_t<Ref, aste::density_reference::Parameters,
                                aste::density::Parameters>;
  using HP = std::conditional_t<Ref, aste::harmonic_reference::Parameters,
                                aste::harmonic::Parameters>;
  using SP = std::conditional_t<Ref, aste::sequence_reference::Parameters,
                                aste::sequence::Parameters>;
  using ST = std::conditional_t<Ref, aste::sequence_reference::Transport,
                                aste::sequence::Transport>;
  using SM = std::conditional_t<Ref, aste::sequence_reference::MidiEvent,
                                aste::sequence::MidiEvent>;
  using SE = std::conditional_t<Ref, aste::sequence_reference::MidiEventType,
                                aste::sequence::MidiEventType>;
  using LP = std::conditional_t<Ref, aste::loop_reference::Parameters,
                                aste::loop::Parameters>;
  using IP = std::conditional_t<Ref, aste::impulse_reference::Parameters,
                                aste::impulse::Parameters>;
  using IM = std::conditional_t<Ref, aste::impulse_reference::MidiEvent,
                                aste::impulse::MidiEvent>;
  using FP = std::conditional_t<Ref, aste::field_reference::Parameters,
                                aste::field::Parameters>;
  std::conditional_t<Ref, aste::density_reference::Processor,
                     aste::density::Processor>
      d;
  std::conditional_t<Ref, aste::harmonic_reference::Processor,
                     aste::harmonic::Processor>
      h;
  std::conditional_t<Ref, aste::sequence_reference::Processor,
                     aste::sequence::Processor>
      s;
  std::conditional_t<Ref, aste::loop_reference::Processor,
                     aste::loop::Processor>
      l;
  std::conditional_t<Ref, aste::impulse_reference::Processor,
                     aste::impulse::Processor>
      i;
  auto f =
      std::make_unique<std::conditional_t<Ref, aste::field_reference::Processor,
                                          aste::field::Processor>>();
  SP sp;
  sp.sequenceEnabled = profile == 2;
  behavior(sp, legacy);
  if (product == 0) d.prepare(a.rate, density<DP>(profile, 0, legacy));
  if (product == 1) h.prepare(a.rate, harmonic<HP>(profile, 0));
  if (product == 2) s.prepare(a.rate, sp);
  if (product == 3) l.prepare(a.rate, 2, true);
  if (product == 4) i.prepare(a.rate, impulse<IP>(profile, legacy));
  if (product == 5) f->prepare(a.rate);
  for (std::size_t at = 0; at < a.left.size();) {
    const auto quarter = static_cast<std::size_t>(a.rate / 4);
    const auto n = std::min(
        {block, a.left.size() - at, quarter - at % quarter, 512 - at % 512});
    const double t = at / a.rate;
    std::array<SM, 16> notes{};
    std::size_t noteCount{};
    std::array<IM, 8> hits{};
    std::size_t hitCount{};
    if (product == 2)
      for (int note = 0; note < 24; ++note) {
        const auto on = static_cast<std::size_t>((.1037 + note * .24) * a.rate);
        const auto off =
            on + static_cast<std::size_t>((profile == 3 ? .28 : .1) * a.rate);
        const int pitch = 36 + std::array{0, 3, 7, 12}[note % 4];
        if (on >= at && on < at + n)
          notes[noteCount++] = {on - at, SE::noteOn, pitch, .8F};
        if (off >= at && off < at + n)
          notes[noteCount++] = {off - at, SE::noteOff, pitch, 0};
      }
    std::sort(notes.begin(), notes.begin() + noteCount,
              [](const auto& x, const auto& y) {
                return x.sampleOffset < y.sampleOffset;
              });
    if (product == 4 && profile == 3)
      for (int note = 0; note < 20; ++note) {
        const auto on =
            static_cast<std::size_t>((.1037 + note * .3517) * a.rate);
        if (on >= at && on < at + n)
          hits[hitCount++] = {on - at, 36 + note % 8, .8F, true};
      }
    LP lp;
    lp.capture = t < 2;
    lp.loopLengthSeconds = 2;
    if (profile > 0 && profile < 4) {
      lp.wow = 1;
      lp.flutter = lp.drift = 0;
    }
    if (profile == 2) {
      lp.reloop = t >= 4;
      lp.amplifier = lp.degradation = 1;
      lp.reverse = true;
    }
    if (profile == 3) {
      lp.pitchSemitones = static_cast<int>(t * 40) % 2 ? 12 : -12;
    }
    if (profile == 4) lp.overdub = 0;
    if (profile == 5) lp.overdub = 1;
    behavior(lp, legacy);
    FP fp;
    fp.forever = t >= 2;
    if (profile) {
      fp.mass = .82F;
      fp.grain = .12F;
      fp.pitch = 0;
      fp.motion = .08F;
      fp.blend = .55F;
      fp.outputDb = -6;
    }
    if (profile == 2) fp.mass = fp.grain = fp.pitch = fp.motion = 1;
    if (profile == 3) fp.forever = static_cast<int>(t * 40) % 2;
    behavior(fp, legacy);
    auto ip = impulse<IP>(profile, legacy);
    if (profile == 3) ip.sequenceEnabled = false;
    const auto dp = density<DP>(profile, t, legacy);
    const auto hp = harmonic<HP>(profile, t);
    const ST st{true, t < 6, 120, t * 2};
    const auto start = std::chrono::steady_clock::now();
    auto* left = a.left.data() + at;
    auto* right = a.right.data() + at;
    switch (product) {
      case 0:
        d.process(left, right, n, dp);
        break;
      case 1:
        h.process(left, right, n, hp);
        break;
      case 2:
        s.process(left, right, n, sp, st, {notes.data(), noteCount});
        break;
      case 3:
        l.process(left, right, n, lp);
        break;
      case 4:
        i.process(left, right, n, ip, {profile != 3, 120, t * 2},
                  {hits.data(), hitCount});
        break;
      case 5:
        f->process(left, right, n, fp);
        break;
      default:
        throw std::runtime_error("unknown graph");
    }
    if (callbackUs)
      *callbackUs += std::chrono::duration<double, std::micro>(
                         std::chrono::steady_clock::now() - start)
                         .count();
    at += n;
  }
  validate(a);
  return a;
}

void check(bool injectFault = false) {
  meterChecks();
  std::size_t comparisons{};
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    const auto source = fixture(5, rate, 8);
    for (int product = 0; product < 6; ++product)
      for (int profile = 0; profile < 4; ++profile) {
        const auto old = render<true>(product, profile, true, source);
        auto compatible = render<false>(product, profile, true, source);
        if (injectFault) compatible.left[0] += .01F;
        require(old.left == compatible.left && old.right == compatible.right,
                "Legacy audio differs from pinned production");
        ++comparisons;
      }
  }
  std::cout << comparisons
            << " legacy stereo renders bit-identical to 8a603dc; meter oracle "
               "passed\n";
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
  for (int headroom = 0; headroom < 8; ++headroom) {
    std::vector<Match> matches;
    double nextTarget = target;
    for (const auto& r : records) {
      double scale = r.metrics.lufs       ? gain(target - *r.metrics.lufs)
                     : r.metrics.peak > 0 ? .1 / r.metrics.peak
                                          : 1;
      Match m{r.audio, {}, scale};
      for (int iteration = 0; iteration < 8; ++iteration) {
        m.audio = r.audio;
        for (std::size_t n = 0; n < m.audio.left.size(); ++n) {
          m.audio.left[n] *= static_cast<float>(scale);
          m.audio.right[n] *= static_cast<float>(scale);
        }
        m.metrics = measure(m.audio);
        m.scale = scale;
        if (!m.metrics.lufs || std::abs(*m.metrics.lufs - target) < .0001)
          break;
        scale *= gain(target - *m.metrics.lufs);
      }
      if (m.metrics.lufs)
        require(std::abs(*m.metrics.lufs - target) < .05,
                "Loudness matching failed");
      if (m.metrics.peak > .8)
        nextTarget = std::min(
            nextTarget, target - 20 * std::log10(m.metrics.peak / .8) - .00001);
      matches.push_back(std::move(m));
    }
    if (nextTarget == target) return matches;
    target = nextTarget;
  }
  throw std::runtime_error("Common headroom matching failed");
}
void number(std::ostream& out, std::optional<double> x) {
  if (x)
    out << *x;
  else
    out << "unknown";
}
void pack(const std::filesystem::path& root, bool playability = false) {
  require(!std::filesystem::exists(root),
          "Use a new export directory; do not overwrite listening responses");
  std::filesystem::create_directories(root);
  auto csv = output(root / "measurements.csv");
  csv << "product,fixture,variant,raw_peak,raw_rms,raw_lufs,matched_lufs,tail_"
         "rms,raw_hash\n";
  std::size_t files{};
  for (int product = 0; product < 6; ++product) {
    if (playability && product != 0 && product != 1 && product != 3) continue;
    const auto folder = root / products[product];
    std::filesystem::create_directories(folder / "audio");
    std::filesystem::create_directory(folder / "raw");
    const int count = product == 2 || product == 4 ? 1 : 7;
    for (int fixtureIndex = 0; fixtureIndex < count; ++fixtureIndex) {
      const auto source = fixture(fixtureIndex);
      const auto fixtureName =
          count == 1 ? "phrase" : fixtureNames[fixtureIndex];
      std::vector<Record> records;
      if (count > 1) records.push_back({"dry", source, {}});
      if (playability) {
        records.push_back(
            {"previous_start",
             render<false>(product, product == 3 ? 0 : 1, false, source),
             {}});
        records.push_back(
            {"open_start", render<false>(product, 4, false, source), {}});
        if (product == 1) {
          records.push_back(
              {"open_push_3dB", render<false>(product, 5, false, source), {}});
          records.push_back(
              {"open_pull_3dB", render<false>(product, 6, false, source), {}});
        }
        if (product == 3)
          records.push_back({"overdub_100_no_input",
                             render<false>(product, 5, false, source),
                             {}});
      } else {
        records.push_back(
            {"before_start", render<true>(product, 0, true, source), {}});
        records.push_back(
            {"after_start", render<false>(product, 1, false, source), {}});
        records.push_back({"same_controls_legacy",
                           render<false>(product, 1, true, source),
                           {}});
        records.push_back(
            {"current_extreme", render<false>(product, 2, false, source), {}});
        records.push_back({"current_automation_or_MIDI",
                           render<false>(product, 3, false, source),
                           {}});
      }
      for (auto& record : records) record.metrics = measure(record.audio);
      const auto matched = matchGroup(records, -23);
      for (std::size_t r = 0; r < records.size(); ++r) {
        const auto name =
            std::string(fixtureName) + "_" + records[r].name + ".wav";
        wav(folder / "audio" / name, matched[r].audio);
        wav(folder / "raw" / name, records[r].audio);
        const auto& m = records[r].metrics;
        csv << products[product] << ',' << fixtureName << ',' << records[r].name
            << ',' << m.peak << ',' << m.rms << ',';
        number(csv, m.lufs);
        csv << ',';
        number(csv, matched[r].metrics.lufs);
        csv << ',' << m.tail << ',' << m.hash << '\n';
        files += 2;
      }
    }
  }
  auto manifest = output(root / "README.txt");
  if (playability) {
    manifest
        << "Aste playability comparisons, source " << QUALITY_COMMIT << ".\n"
        << files
        << " WAVs: stereo float32, 48 kHz, eight seconds; seven synthetic "
           "sources per effect.\n"
        << "audio/ is group loudness matched at <=-23 LUFS with shared "
           "sample-peak headroom <=0.8; raw/ is unattenuated. Not true-peak "
           "protected.\n"
        << "Density: Grip versus Pulse / parallel. Harmonic: Charged Presence "
           "versus Color / open, plus paired +/-3 dB gestures. Loop: previous "
           "raw defaults versus First take (Overdub 0), plus 100% no-input "
           "endpoint.\n"
        << "Loop captures the first two seconds. Its previous-start comparison "
           "uses default motion, not the historical lab's exaggerated Wow.\n"
        << "Silent Loop 100% output has unknown loudness; never a zero-LUFS "
           "measurement. No human listening or Cubase acceptance claimed.\n";
    std::cout << files << " playability WAVs rendered\n";
    return;
  }
  manifest << "Aste control-quality comparison build " << QUALITY_COMMIT
           << " against pinned 8a603dc.\n"
           << files
           << " WAVs: 48k stereo float32, 8 seconds. Generated diagnostic "
              "sources, not supplied recordings.\n"
           << "Use audio/ matched files. raw/ can exceed unity. Common <=-23 "
              "LUFS group target, <=0.8 sample peak; not true peak/certified.\n"
           << "before_start = prior starting controls and DSP; after_start = "
              "new starting controls and DSP.\n"
           << "same_controls_legacy isolates the DSP change at after_start "
              "controls. Loop after_start intentionally uses Wow=100% to "
              "reveal its clock fix, not a new default.\n"
           << "Loop captures first 2sec. Field enters hold at 2sec. Sequence "
              "is gapped MIDI (extreme:host; automation:overlap). Impulse uses "
              "visible patterns (automation:MIDI all eight objects).\n"
           << "No human audition, musical approval or Cubase validation "
              "claimed. Comparison names are visible, not a blinded study.\n";
  std::cout << files << " WAVs rendered\n";
}
void spectral(const std::filesystem::path& path, int profile = 1) {
  auto csv = output(path);
  csv << "rate,input_peak_dbfs,frequency,legacy,fundamental,H3_dBc,fifth_"
         "frequency,fifth_product_dBc,peak\n";
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
    for (double db : {-36., -18., -6.})
      for (double frequency : {996., 7004.})
        for (bool legacy : {true, false}) {
          Audio source(static_cast<std::size_t>(rate * .75), rate);
          for (std::size_t n = 0; n < source.left.size(); ++n)
            source.left[n] = source.right[n] = static_cast<float>(
                gain(db) * std::sin(2 * pi * frequency * n / rate));
          const auto a = render<false>(0, profile, legacy, source);
          const double fundamental = std::abs(component(a, frequency, .5, .25));
          const auto relative = [&](double hz) {
            return 20 * std::log10(std::max(
                            1e-15,
                            std::abs(component(a, hz, .5, .25)) / fundamental));
          };
          const double fifth = std::abs(std::remainder(5 * frequency, rate));
          csv << rate << ',' << db << ',' << frequency << ',' << legacy << ','
              << fundamental << ',' << relative(3 * frequency) << ',' << fifth
              << ',' << relative(fifth) << ',' << measure(a).peak << '\n';
        }
}

void benchmark(const std::filesystem::path& path) {
  auto csv = output(path);
  csv << "product,rate,block,trial,legacy,callback_realtime_percent\n";
  for (double rate : {48000., 96000.})
    for (std::size_t block : {128U, 511U})
      for (int product = 0; product < 6; ++product)
        for (int trial = 0; trial < 3; ++trial)
          for (bool legacy : {true, false}) {
            double us{};
            const auto a = render<false>(product, 2, legacy,
                                         fixture(5, rate, 5), block, &us);
            require(measure(a).rms > 0, "Silent benchmark");
            csv << products[product] << ',' << rate << ',' << block << ','
                << trial << ',' << legacy << ',' << us / 50000. << '\n';
          }
}
int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--check-fault")
      check(true);
    else if (argc == 2 && std::string(argv[1]) == "--check")
      check();
    else if (argc == 3 && std::string(argv[1]) == "--pack")
      pack(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--playability-pack")
      pack(argv[2], true);
    else if (argc == 3 && std::string(argv[1]) == "--benchmark")
      benchmark(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--spectral")
      spectral(argv[2]);
    else if (argc == 3 && std::string(argv[1]) == "--playability-spectral")
      spectral(argv[2], 4);
    else
      throw std::runtime_error(
          "Use --check, --pack NEW_DIRECTORY or --benchmark CSV");
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
