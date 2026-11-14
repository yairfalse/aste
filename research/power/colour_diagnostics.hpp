#pragma once
#include "audio.hpp"
#include "../../apps/density/density_processor.hpp"
#include "../../apps/harmonic/harmonic_processor.hpp"

namespace power {
namespace colour_detail {
constexpr double fs = 48000;
constexpr double settle = .5, window = .25;
struct Stats {
  double peak{}, rms{}, maxReduction{}, activity{};
  std::size_t overUnity{}, atBound{};
};
inline Stats stats(const Audio& a, std::size_t begin = 0) {
  validate(a);
  if (begin >= a.left.size()) throw std::runtime_error("colour stats extent");
  Stats s;
  double sum{};
  for (std::size_t i = begin; i < a.left.size(); ++i) {
    for (float x : {a.left[i], a.right[i]}) {
      s.peak = std::max(s.peak, static_cast<double>(std::abs(x)));
      sum += x * static_cast<double>(x);
      s.overUnity += std::abs(x) > 1;
      s.atBound += std::abs(x) >= 16;
    }
  }
  s.rms = std::sqrt(sum / (2 * (a.left.size() - begin)));
  return s;
}
inline std::optional<double> ratioDb(double numerator, double denominator,
                                     bool power = false) {
  if (!(denominator > 0) || !std::isfinite(numerator) ||
      !std::isfinite(denominator) || numerator < 0)
    return std::nullopt;
  // A declared numerical display floor, not a missing-data substitute.
  return std::max(-300.,
                  (power ? 10. : 20.) *
                      std::log10(std::max(numerator / denominator, 1e-300)));
}
inline void optional(std::ostream& out, std::optional<double> value) {
  if (value)
    out << *value;
  else
    out << "unknown";
}
inline Audio tones(double rate, double peak, double f1, double f2 = 0,
                   double firstShare = 1) {
  Audio a(static_cast<std::size_t>(rate * (settle + window)), rate);
  for (std::size_t i = 0; i < a.left.size(); ++i) {
    const double t = i / rate;
    const double x = peak * (firstShare * std::sin(2 * pi * f1 * t) +
                             (1 - firstShare) * std::sin(2 * pi * f2 * t));
    a.left[i] = a.right[i] = static_cast<float>(x);
  }
  return a;
}
inline std::complex<double> bin(const Audio& a, double hz) {
  return component(a, hz, settle, window);
}
inline std::pair<Audio, Stats> harmonic(const Audio& input,
                                        aste::harmonic::Parameters p) {
  auto a = input;
  aste::harmonic::Processor processor;
  processor.prepare(a.rate, p);
  double activity{};
  for (std::size_t at = 0; at < a.left.size(); at += 127) {
    const auto n = std::min<std::size_t>(127, a.left.size() - at);
    processor.process(a.left.data() + at, a.right.data() + at, n, p);
    activity = std::max(
        activity, static_cast<double>(processor.meters().harmonicActivity));
  }
  auto s = stats(a);
  s.activity = activity;
  return {std::move(a), s};
}
inline std::pair<Audio, Stats> density(const Audio& input,
                                       aste::density::Parameters p) {
  auto a = input;
  aste::density::Processor processor;
  processor.prepare(a.rate, p);
  double reduction{};
  for (std::size_t at = 0; at < a.left.size(); at += 127) {
    const auto n = std::min<std::size_t>(127, a.left.size() - at);
    processor.process(a.left.data() + at, a.right.data() + at, n, p);
    reduction = std::max(
        reduction, static_cast<double>(processor.meters().gainReductionDb));
  }
  auto s = stats(a);
  s.maxReduction = reduction;
  return {std::move(a), s};
}
inline void bandGain(aste::harmonic::Parameters& p, std::size_t band,
                     float db) {
  switch (band) {
    case 0:
      p.foundationGainDb = db;
      break;
    case 1:
      p.bodyGainDb = db;
      break;
    case 2:
      p.presenceGainDb = db;
      break;
    case 3:
      p.airGainDb = db;
      break;
    default:
      throw std::runtime_error("colour band index");
  }
}
inline void allGains(aste::harmonic::Parameters& p, float db) {
  for (std::size_t band = 0; band < 4; ++band) bandGain(p, band, db);
}
inline void gesture(aste::harmonic::Parameters& p, bool active) {
  p.inputDb = active ? 12 : 0;
  p.outputDb = active ? -12 : 0;
}
inline constexpr std::array names{"foundation", "body", "presence", "air"};

inline void contours(const std::filesystem::path& dir) {
  auto out = output(dir / "harmonic-contours.csv");
  out << "rate,band,gain_db,probe,frequency_hz,input_peak_dbfs,magnitude_db,"
         "phase_deg,peak,over_unity_channel_samples\n";
  constexpr std::array<std::array<double, 3>, 4> probes{{{20, 80, 2000},
                                                         {100, 400, 1600},
                                                         {624, 2500, 10000},
                                                         {1000, 12000, 20000}}};
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
    for (std::size_t band = 0; band < names.size(); ++band)
      for (float db : {-12.F, -6.F, 6.F, 12.F})
        for (std::size_t probe = 0; probe < 3; ++probe) {
          aste::harmonic::Parameters p;
          p.harmonic = 0;
          bandGain(p, band, db);
          const auto input = tones(rate, gain(-36), probes[band][probe]);
          const auto [a, s] = harmonic(input, p);
          const auto transfer =
              bin(a, probes[band][probe]) / bin(input, probes[band][probe]);
          out << rate << ',' << names[band] << ',' << db << ','
              << (probe == 0   ? "low_side"
                  : probe == 1 ? "control_frequency"
                               : "high_side")
              << ',' << probes[band][probe] << ",-36,"
              << 20 * std::log10(std::abs(transfer)) << ','
              << std::arg(transfer) * 180 / pi << ',' << s.peak << ','
              << s.overUnity << '\n';
        }
}
struct Profile {
  std::string name;
  aste::harmonic::Parameters parameters;
  double frequency{1000};
};
inline std::vector<Profile> profiles() {
  std::vector<Profile> result;
  constexpr std::array frequencies{80., 400., 2500., 6004.};
  for (std::size_t band = 0; band < 4; ++band)
    for (bool boost : {false, true}) {
      aste::harmonic::Parameters p;
      bandGain(p, band, boost ? 6.F : -6.F);
      if (band == 3) p.airFrequencyHz = 6004;
      result.push_back(
          {std::string(names[band]) + (boost ? "_boost6" : "_cut6"), p,
           frequencies[band]});
    }
  for (float db : {-6.F, 6.F, 12.F}) {
    aste::harmonic::Parameters p;
    allGains(p, db);
    result.push_back({db < 0    ? "all_cut6"
                      : db == 6 ? "all_boost6"
                                : "all_boost12",
                      p, 1000});
  }
  return result;
}
inline void nonlinear(const std::filesystem::path& dir) {
  auto tonesOut = output(dir / "harmonic-products.csv");
  tonesOut << "profile,frequency_hz,input_peak_dbfs,harmonic,linked_input_db,"
              "fundamental_gain_db,h2_dbc,h3_dbc,h5_dbc,peak,rms,correction_"
              "peak,over_unity_channel_samples,at_16_channel_samples\n";
  auto imdOut = output(dir / "harmonic-imd.csv");
  imdOut << "profile,fixture,input_peak_bound_dbfs,harmonic,linked_input_db,"
            "carrier_hz,product1_hz,product2_hz,products_dbc,peak,over_unity_"
            "channel_samples,at_16_channel_samples\n";
  for (const auto& profile : profiles())
    for (double db : {-36., -24., -12., -6.})
      for (float h : {0.F, 1.F})
        for (bool linked : {false, true}) {
          auto p = profile.parameters;
          p.harmonic = h;
          gesture(p, linked);
          const auto input = tones(fs, gain(db), profile.frequency);
          const auto [a, s] = harmonic(input, p);
          const double fundamental = std::abs(bin(a, profile.frequency));
          tonesOut << profile.name << ',' << profile.frequency << ',' << db
                   << ',' << h << ',' << p.inputDb << ',';
          optional(tonesOut, ratioDb(fundamental,
                                     std::abs(bin(input, profile.frequency))));
          for (int order : {2, 3, 5}) {
            tonesOut << ',';
            if (order * profile.frequency < fs / 2)
              optional(tonesOut,
                       ratioDb(std::abs(bin(a, order * profile.frequency)),
                               fundamental));
            else
              tonesOut << "not_observable";
          }
          tonesOut << ',' << s.peak << ',' << s.rms << ',' << s.activity << ','
                   << s.overUnity << ',' << s.atBound << '\n';
          for (bool ccif : {false, true}) {
            const auto source = ccif ? tones(fs, gain(db), 18000, 19000, .5)
                                     : tones(fs, gain(db), 60, 7000, .8);
            const auto [result, m] = harmonic(source, p);
            const double carrier = ccif ? 18000 : 7000;
            const double first = ccif ? 17000 : 6880;
            const double second = ccif ? 20000 : 7120;
            const double referencePower =
                ccif ? .5 * (std::norm(bin(result, 18000)) +
                             std::norm(bin(result, 19000)))
                     : std::norm(bin(result, 7000));
            const double productPower =
                std::norm(bin(result, first)) + std::norm(bin(result, second));
            imdOut << profile.name << ','
                   << (ccif ? "ccif_style" : "smpte_style") << ',' << db << ','
                   << h << ',' << p.inputDb << ',' << carrier << ',' << first
                   << ',' << second << ',';
            optional(imdOut, ratioDb(productPower, referencePower, true));
            imdOut << ',' << m.peak << ',' << m.overUnity << ',' << m.atBound
                   << '\n';
          }
        }
}
inline void densityInteractions(const std::filesystem::path& dir) {
  auto out = output(dir / "density-interactions.csv");
  out << "fixture,setting,drive_db,crush,density,blend,output_db,input_rms,"
         "output_rms,rms_gain_db,output_peak,crest_db,max_gr_db,over_unity_"
         "channel_samples\n";
  std::vector<std::pair<std::string, aste::density::Parameters>> settings;
  settings.emplace_back("default", aste::density::Parameters{});
  for (float value : {0.F, .25F, .5F, .75F, 1.F}) {
    aste::density::Parameters p;
    p.density = value;
    settings.emplace_back("density_" + std::to_string(value), p);
  }
  for (float value : {0.F, .25F, .65F, 1.F}) {
    aste::density::Parameters p;
    p.crush = value;
    settings.emplace_back("crush_" + std::to_string(value), p);
  }
  for (float value : {-12.F, 0.F, 12.F, 24.F}) {
    aste::density::Parameters p;
    p.driveDb = value;
    settings.emplace_back("drive_" + std::to_string(value), p);
  }
  for (float value : {0.F, .5F, .9F, 1.F}) {
    aste::density::Parameters p;
    p.blend = value;
    settings.emplace_back("blend_" + std::to_string(value), p);
  }
  for (float value : {-12.F, -7.F, 0.F, 12.F}) {
    aste::density::Parameters p;
    p.outputDb = value;
    settings.emplace_back("output_" + std::to_string(value), p);
  }
  for (float d : {0.F, 1.F})
    for (float c : {0.F, 1.F}) {
      aste::density::Parameters p;
      p.density = d;
      p.crush = c;
      p.driveDb = 24;
      p.blend = 1;
      settings.emplace_back(
          "joint_" + std::to_string(d) + "_" + std::to_string(c), p);
    }
  std::vector<std::pair<std::string, Audio>> sources;
  for (int kind : {0, 1, 5})
    sources.emplace_back(fixtureNames[kind], fixture(kind));
  for (double db : {-36., -24., -12., -6.})
    sources.emplace_back("tone1k_" + std::to_string(db),
                         tones(fs, gain(db), 1000));
  for (const auto& [name, source] : sources) {
    const double inputRms = stats(source).rms;
    for (const auto& [setting, p] : settings) {
      const auto [a, s] = density(source, p);
      out << name << ',' << setting << ',' << p.driveDb << ',' << p.crush << ','
          << p.density << ',' << p.blend << ',' << p.outputDb << ',' << inputRms
          << ',' << s.rms << ',';
      optional(out, ratioDb(s.rms, inputRms));
      out << ',' << s.peak << ',';
      optional(out, ratioDb(s.peak, s.rms));
      out << ',' << s.maxReduction << ',' << s.overUnity << '\n';
    }
  }
}
inline void stageAlias(const std::filesystem::path& dir) {
  constexpr std::size_t n = 8192;
  auto out = output(dir / "density-stage-folds.csv");
  out << "rate,stage,density,crush,drive_db,saturation_drive,input_peak,"
         "frequency_hz,fold_bins,fundamental_peak,selected_fold_power_dbc,"
         "output_peak\n";
  constexpr std::array<std::array<float, 3>, 3> laws{
      {{0, 0, 0}, {.5F, .65F, 0}, {1, 1, 24}}};
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
    for (double target : {7000., 11000.}) {
      const auto k =
          static_cast<std::size_t>(std::round(target * n / rate)) | 1U;
      const double frequency = rate * k / n;
      std::vector<std::size_t> folds;
      for (std::size_t order = 3; order <= 63; order += 2)
        if (order * k > n / 2) {
          const auto modulo = (order * k) % n;
          const auto folded = std::min(modulo, n - modulo);
          if (folded != k &&
              std::find(folds.begin(), folds.end(), folded) == folds.end())
            folds.push_back(folded);
        }
      for (const auto& law : laws)
        for (double peak : {.01, .1, .5, 1.1})
          for (int stage = 0; stage < 3; ++stage) {
            const float drive =
                aste::density::mapCrushSaturation(law[0], law[1], law[2]);
            Audio a(n, rate);
            for (std::size_t i = 0; i < n; ++i) {
              const float input =
                  static_cast<float>(peak * std::sin(2 * pi * k * i / n));
              float result = stage == 2
                                 ? input
                                 : aste::density::saturateSample(input, drive);
              if (stage != 0)
                result = aste::density::controlledClipSample(result);
              a.left[i] = a.right[i] = result;
            }
            const auto power = spectrum(a, 0, n);
            double folded{};
            for (auto index : folds) folded += power[index];
            out << rate << ','
                << (stage == 0   ? "saturation"
                    : stage == 1 ? "saturation_crush_clip"
                                 : "output_protection_clip")
                << ',' << law[0] << ',' << law[1] << ',' << law[2] << ','
                << drive << ',' << peak << ',' << frequency << ','
                << folds.size() << ',' << 4 * std::sqrt(power[k]) / n << ',';
            optional(out, ratioDb(folded, power[k], true));
            out << ',' << stats(a).peak << '\n';
          }
    }
}
inline void headroom(const std::filesystem::path& dir) {
  auto out = output(dir / "harmonic-headroom.csv");
  out << "input_peak_dbfs,all_band_gain_db,harmonic,input_db,output_db,peak,"
         "rms,over_unity_channel_samples,at_16_channel_samples\n";
  const auto input = tones(fs, gain(-6), 1000);
  for (float inputDb : {0.F, 18.F})
    for (float outputDb : {0.F, 18.F})
      for (float h : {0.F, 1.F}) {
        aste::harmonic::Parameters p;
        allGains(p, 12);
        p.inputDb = inputDb;
        p.outputDb = outputDb;
        p.harmonic = h;
        const auto [a, s] = harmonic(input, p);
        out << "-6,12," << h << ',' << inputDb << ',' << outputDb << ','
            << s.peak << ',' << s.rms << ',' << s.overUnity << ',' << s.atBound
            << '\n';
      }
}
inline void harmonicFolds(const std::filesystem::path& dir) {
  auto out = output(dir / "harmonic-folds.csv");
  out << "rate,input_peak_dbfs,harmonic,linked_input_db,frequency_hz,natural_"
         "h3_dbc,selected_fold_power_dbc,peak,over_unity_channel_samples\n";
  constexpr double frequency = 7004;
  for (double db : {-36., -6.})
    for (float h : {0.F, 1.F})
      for (bool linked : {false, true}) {
        aste::harmonic::Parameters p;
        p.presenceFrequencyHz = p.airFrequencyHz = 7000;
        p.presenceGainDb = p.airGainDb = 6;
        p.harmonic = h;
        gesture(p, linked);
        const auto [a, s] = harmonic(tones(fs, gain(db), frequency), p);
        const double fundamental = std::norm(bin(a, frequency));
        double folded{};
        for (int order : {5, 7, 9, 11, 13, 15}) {
          const double modulo = std::fmod(order * frequency, fs);
          folded += std::norm(bin(a, std::min(modulo, fs - modulo)));
        }
        out << fs << ',' << db << ',' << h << ',' << p.inputDb << ','
            << frequency << ',';
        optional(out,
                 ratioDb(std::norm(bin(a, 3 * frequency)), fundamental, true));
        out << ',';
        optional(out, ratioDb(folded, fundamental, true));
        out << ',' << s.peak << ',' << s.overUnity << '\n';
      }
}
}  // namespace colour_detail

inline void colourChecks() {
  using namespace colour_detail;
  const auto require = [](bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
  };
  const auto source = tones(fs, .1, 1000);
  require(std::abs(std::abs(bin(source, 1000)) - .1) < 1e-8,
          "colour coherent amplitude oracle");
  require(std::abs(std::arg(bin(source, 1000)) + pi / 2) < 1e-7,
          "colour coherent phase oracle");
  require(std::abs(bin(source, 3000)) < 1e-8,
          "colour coherent orthogonal tone oracle");
  for (int mode = 0; mode < 3; ++mode) {
    aste::harmonic::Parameters p;
    p.harmonic = mode == 2 ? 0 : 1;
    if (mode == 1) allGains(p, -6);
    if (mode == 2) allGains(p, 6);
    const auto [baseline, unused] = harmonic(source, p);
    gesture(p, true);
    const auto [linked, alsoUnused] = harmonic(source, p);
    require(difference(baseline, linked) < 1e-6,
            "Harmonic linked-gain linear negative control");
    if (mode == 0)
      require(difference(source, baseline) == 0,
              "Harmonic flat default exact null");
  }
  aste::harmonic::Parameters band;
  band.bodyFrequencyHz = 1000;
  band.bodyGainDb = 6;
  band.harmonic = 0;
  const auto linear = harmonic(source, band).first;
  require(std::abs(20 * std::log10(std::abs(bin(linear, 1000)) / .1) - 6) < .02,
          "Harmonic bell gain reference");
  band.harmonic = 1;
  const auto coloured = harmonic(source, band).first;
  require(difference(linear, coloured) > 1e-5,
          "Harmonic active branch positive control");
  gesture(band, true);
  require(difference(coloured, harmonic(source, band).first) > 1e-5,
          "Harmonic selected gesture changes operating point");
  aste::density::Parameters d;
  d.blend = 0;
  d.outputDb = 0;
  require(difference(source, density(source, d).first) == 0,
          "Density dry unity exact null");
  d.blend = 1;
  d.crush = 0;
  d.density = 0;
  const auto low = density(source, d).first;
  d.density = 1;
  require(difference(low, density(source, d).first) == 0,
          "Density macro inactive when Crush zero");
  d.driveDb = 24;
  const auto clipped = density(tones(fs, .5, 1000), d);
  require(std::abs(clipped.second.peak - .97037037037037) < 1e-6,
          "Density fixed clip followed by Protection retains its second knee");
  d.protection = false;
  require(
      density(tones(fs, .5, 1000), d).second.peak == 1,
      "Density Crush zero still participates in fixed clip without Protection");
}
inline void colourDiagnostics(std::filesystem::path dir) {
  std::filesystem::create_directories(dir);
  colourChecks();
  colour_detail::contours(dir);
  colour_detail::nonlinear(dir);
  colour_detail::densityInteractions(dir);
  colour_detail::stageAlias(dir);
  colour_detail::harmonicFolds(dir);
  colour_detail::headroom(dir);
}
}  // namespace power
