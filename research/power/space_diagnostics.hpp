#pragma once
#include "audio.hpp"
#include "apps/field/field_processor.hpp"
#include "apps/loop/loop_processor.hpp"
#include "candidate/loop/loop_processor.hpp"
#include <memory>

namespace power {
namespace space_detail {
inline constexpr std::array rates{44100., 48000.,  88200.,
                                  96000., 176400., 192000.};
struct Original {
  using Processor = aste::loop::Processor;
  using Parameters = aste::loop::Parameters;
  using Tape = aste::loop::TapeState;
};
struct Candidate {
  using Processor = aste::loop_candidate::Processor;
  using Parameters = aste::loop_candidate::Parameters;
  using Tape = aste::loop_candidate::TapeState;
};
template <class Types>
std::unique_ptr<typename Types::Processor> tape(double rate,
                                                bool complex = false) {
  auto e = std::make_unique<typename Types::Processor>();
  e->prepare(rate, 1, false);
  typename Types::Tape state;
  state.sampleRate = rate;
  state.generations[0] = 1;
  state.audio[0].resize(static_cast<std::size_t>(rate) * 2);
  for (std::size_t n = 0; n < state.audio[0].size() / 2; ++n) {
    const double t = n / rate;
    const float x = static_cast<float>(
        complex ? .1 * std::sin(2 * pi * 220 * t) +
                      .06 * std::sin(2 * pi * 733 * t) +
                      .02 * noise(n + 97) * std::exp(-std::fmod(t, .25) * 20)
                : .001 * std::sin(2 * pi * 440 * t));
    state.audio[0][n * 2] = state.audio[0][n * 2 + 1] = x;
  }
  e->restoreTape(state);
  return e;
}
template <class Types>
typename Types::Parameters neutral() {
  typename Types::Parameters p;
  p.loopLengthSeconds = 1;
  p.wow = p.flutter = p.drift = p.degradation = p.amplifier = 0;
  p.outputDb = 0;
  p.splice = 0;
  return p;
}
template <class Types>
Audio playback(typename Types::Processor& e, typename Types::Parameters p,
               double rate, double seconds = 1) {
  Audio a(static_cast<std::size_t>(rate * seconds), rate);
  for (std::size_t at = 0; at < a.left.size();) {
    const auto count = std::min<std::size_t>(127, a.left.size() - at);
    e.process(a.left.data() + at, a.right.data() + at, count, p, false);
    p.reloop = false;
    at += count;
  }
  validate(a);
  return a;
}
inline double stereoRms(const Audio& a) {
  double sum{};
  for (std::size_t n = 0; n < a.left.size(); ++n)
    sum += a.left[n] * static_cast<double>(a.left[n]) +
           a.right[n] * static_cast<double>(a.right[n]);
  return std::sqrt(sum / (2 * a.left.size()));
}
inline std::optional<double> correlation(const Audio& a, const Audio& b) {
  if (a.left.size() != b.left.size())
    throw std::runtime_error("space correlation extent");
  double aa{}, bb{}, ab{};
  for (std::size_t n = 0; n < a.left.size(); ++n) {
    aa += a.left[n] * static_cast<double>(a.left[n]);
    bb += b.left[n] * static_cast<double>(b.left[n]);
    ab += a.left[n] * static_cast<double>(b.left[n]);
  }
  if (!(aa > 0 && bb > 0)) return std::nullopt;
  return ab / std::sqrt(aa * bb);
}
inline void optional(std::ostream& o, std::optional<double> value) {
  if (value)
    o << *value;
  else
    o << "unknown";
}
inline double spectralPeak(const Audio& a) {
  const auto power = spectrum(a, static_cast<std::size_t>(a.rate * .25), 65536);
  const double step = a.rate / 65536;
  const auto low = static_cast<std::size_t>(20 / step);
  const auto high =
      std::min(power.size() - 2, static_cast<std::size_t>(4000 / step));
  const auto k = static_cast<std::size_t>(
      std::max_element(power.begin() + low, power.begin() + high) -
      power.begin());
  if (!(power[k] > 0)) throw std::runtime_error("empty pitch probe");
  const double x = std::log(std::max(power[k - 1], 1e-300));
  const double y = std::log(power[k]);
  const double z = std::log(std::max(power[k + 1], 1e-300));
  const double denominator = x - 2 * y + z;
  return step * (k + (denominator == 0 ? 0 : .5 * (x - z) / denominator));
}
inline double intendedPosition(std::size_t frames, double rate) {
  // Sum sin(k*w), k=1..N, in closed form. No DSP phase accumulator is reused.
  const double w = 2 * pi * .47 / rate;
  const double sum = std::sin(frames * w / 2) * std::sin((frames + 1) * w / 2) /
                     std::sin(w / 2);
  return std::fmod(frames + .012 * sum, rate) / rate;
}
inline double positionError(double actual, double expected, double rate) {
  if (!std::isfinite(actual) || !std::isfinite(expected) || actual < 0 ||
      actual > 1 || expected < 0 || expected > 1)
    throw std::runtime_error("invalid playback-position observation");
  const double distance = std::abs(actual - expected);
  return std::min(distance, 1 - distance) * rate;
}
template <class Types>
double motion(double rate, std::ostream* out, const char* name) {
  auto e = tape<Types>(rate);
  auto p = neutral<Types>();
  p.wow = 1;
  double maximum{};
  for (int step = 1; step <= 8; ++step) {
    (void)playback<Types>(*e, p, rate, .5);
    const auto frames = static_cast<std::size_t>(rate * .5) * step;
    const double actual = e->meters().position;
    const double expected = intendedPosition(frames, rate);
    const double error = positionError(actual, expected, rate);
    maximum = std::max(maximum, error);
    if (out)
      *out << name << ',' << rate << ',' << frames << ',' << actual << ','
           << expected << ',' << error << '\n';
  }
  return maximum;
}
inline void captures(const std::filesystem::path& dir) {
  auto out = output(dir / "loop_capture.csv");
  out << "overdub,rate,input_rms,stored_rms,stored_gain_db,playback_rms\n";
  for (float overdub : {0.F, .5F, 1.F}) {
    aste::loop::Processor e;
    constexpr double rate = 48000;
    e.prepare(rate, 1, true);
    aste::loop::Parameters p;
    p.capture = true;
    p.overdub = overdub;
    p.loopLengthSeconds = 1;
    Audio a(48000);
    for (std::size_t n = 0; n < a.left.size(); ++n)
      a.left[n] = a.right[n] =
          static_cast<float>(.1 * std::sin(2 * pi * 440 * n / rate));
    const double input = stereoRms(a);
    e.process(a.left.data(), a.right.data(), a.left.size(), p);
    const auto state = e.tapeState();
    if (state.audio[0].size() != a.left.size() * 2)
      throw std::runtime_error("capture snapshot missing");
    double energy{};
    for (float x : state.audio[0]) energy += x * static_cast<double>(x);
    const double stored = std::sqrt(energy / state.audio[0].size());
    p.capture = false;
    const auto played = playback<Original>(e, p, rate);
    out << overdub << ',' << rate << ',' << input << ',' << stored << ',';
    if (stored > 0)
      out << 20 * std::log10(stored / input);
    else
      out << "-inf";  // Measured exact silence, not missing data.
    out << ',' << stereoRms(played) << '\n';
  }
}
inline void generations(const std::filesystem::path& dir) {
  auto out = output(dir / "loop_generations.csv");
  out << "setting,generation,retained,rms,centroid_hz,above_8khz_fraction,"
         "correlation_to_first\n";
  for (int setting = 0; setting < 3; ++setting) {
    auto e = tape<Original>(48000, true);
    auto p = neutral<Original>();
    p.splice = .03F;
    p.amplifier = setting == 0 ? .25F : setting == 1 ? 1.F : 0.F;
    p.degradation = setting == 0 ? .08F : 1.F;
    Audio first;
    for (int generation = 1; generation <= 4; ++generation) {
      const auto a = playback<Original>(*e, p, 48000);
      if (generation == 1) first = a;
      const auto power = spectrum(a, 0, 32768);
      double sum{}, moment{}, high{};
      for (std::size_t k = 0; k < power.size(); ++k) {
        const double hz = k * 48000. / 32768;
        sum += power[k];
        moment += hz * power[k];
        if (hz > 8000) high += power[k];
      }
      out << (setting == 0   ? "default_record_loss"
              : setting == 1 ? "maximum_record_loss"
                             : "loss_only")
          << ',' << e->meters().generation << ','
          << e->meters().retainedGenerations << ',' << stereoRms(a) << ',';
      optional(out, sum > 0 ? std::optional{moment / sum} : std::nullopt);
      out << ',';
      optional(out, sum > 0 ? std::optional{high / sum} : std::nullopt);
      out << ',';
      optional(out, correlation(a, first));
      out << '\n';
      if (generation < 4) {
        p.reloop = true;
        (void)playback<Original>(*e, p, 48000);
        p.reloop = false;
      }
    }
  }
}
inline void pitches(const std::filesystem::path& dir) {
  auto out = output(dir / "loop_pitch_interactions.csv");
  out << "speed,pitch_semitones,reverse,conventional_combined_target_hz,"
         "strongest_peak_hz,rms\n";
  for (float speed : {.5F, 1.F, 2.F})
    for (float pitch : {-12.F, 0.F, 12.F})
      for (bool reverse : {false, true}) {
        auto e = tape<Original>(48000);
        auto p = neutral<Original>();
        p.speed = speed;
        p.pitchSemitones = pitch;
        p.reverse = reverse;
        const auto a = playback<Original>(*e, p, 48000, 2);
        out << speed << ',' << pitch << ',' << reverse << ','
            << 440 * speed * std::exp2(pitch / 12.) << ',' << spectralPeak(a)
            << ',' << stereoRms(a) << '\n';
      }
}
struct FieldResult {
  double stereo{}, mid{}, meter{}, retention{}, peak{};
};
inline FieldResult fieldSecond(aste::field::Processor& e,
                               const aste::field::Parameters& p, double rate,
                               bool drive, bool side, bool dc = false) {
  Audio a(static_cast<std::size_t>(rate), rate);
  if (drive)
    for (std::size_t n = 0; n < a.left.size(); ++n) {
      const double t = n / rate;
      const float x = static_cast<float>(
          dc ? .1
             : .08 * std::sin(2 * pi * 110 * t) +
                   .05 * std::sin(2 * pi * 330 * t) + .03 * noise(n + 117));
      a.left[n] = x;
      a.right[n] = side ? -x : x;
    }
  for (std::size_t at = 0; at < a.left.size();) {
    const auto count = std::min<std::size_t>(127, a.left.size() - at);
    e.process(a.left.data() + at, a.right.data() + at, count, p);
    at += count;
  }
  validate(a);
  FieldResult r;
  double mid{};
  for (std::size_t n = 0; n < a.left.size(); ++n) {
    const double value = .5 * (a.left[n] + a.right[n]);
    mid += value * value;
    r.peak = std::max({r.peak, std::abs(static_cast<double>(a.left[n])),
                       std::abs(static_cast<double>(a.right[n]))});
  }
  r.stereo = stereoRms(a);
  r.mid = std::sqrt(mid / a.left.size());
  r.meter = e.meters().fieldEnergy;
  r.retention = e.meters().retention;
  return r;
}
inline void fields(const std::filesystem::path& dir) {
  auto out = output(dir / "field_decay.csv");
  out << "rate,forever,controls,input_polarity,second,input_active,stereo_rms,"
         "mid_rms,end_energy_meter,end_retention_coefficient,peak\n";
  for (double rate : rates)
    for (bool forever : {false, true})
      for (int setting = 0; setting < 3; ++setting)
        for (bool side : {false, true}) {
          auto e = std::make_unique<aste::field::Processor>();
          e->prepare(rate);
          aste::field::Parameters p;
          p.blend = 1;
          p.outputDb = 0;
          p.forever = forever;
          if (setting != 1) {
            const float x = setting == 0 ? 0.F : 1.F;
            p.mass = p.grain = p.pitch = p.motion = p.distance = x;
          }
          for (int second = 1; second <= 10; ++second) {
            const auto r = fieldSecond(*e, p, rate, second == 1, side);
            out << rate << ',' << forever << ','
                << (setting == 0   ? "zero"
                    : setting == 1 ? "default"
                                   : "maximum")
                << ',' << (side ? "opposed" : "same") << ',' << second << ','
                << (second == 1) << ',' << r.stereo << ',' << r.mid << ','
                << r.meter << ',' << r.retention << ',' << r.peak << '\n';
          }
        }
  auto witness = output(dir / "field_meter_witness.csv");
  witness << "rate,input_polarity,second,input_active,stereo_rms,mid_rms,end_"
             "energy_meter,end_retention_coefficient\n";
  for (double rate : rates)
    for (bool side : {false, true}) {
      auto e = std::make_unique<aste::field::Processor>();
      e->prepare(rate);
      aste::field::Parameters p;
      p.mass = p.grain = p.pitch = p.motion = p.distance = 0;
      p.blend = 1;
      p.outputDb = 0;
      for (int second = 1; second <= 6; ++second) {
        const auto r = fieldSecond(*e, p, rate, second <= 5, side, true);
        witness << rate << ',' << (side ? "opposed" : "same") << ',' << second
                << ',' << (second <= 5) << ',' << r.stereo << ',' << r.mid
                << ',' << r.meter << ',' << r.retention << '\n';
      }
    }
}
}  // namespace space_detail

inline void spaceDiagnostics(const std::filesystem::path& dir) {
  std::filesystem::create_directories(dir);
  space_detail::captures(dir);
  space_detail::generations(dir);
  space_detail::pitches(dir);
  auto motion = output(dir / "loop_motion_oracle.csv");
  motion << "engine,rate,frames,observed_position,intended_position,circular_"
            "error_frames\n";
  for (double rate : space_detail::rates) {
    (void)space_detail::motion<space_detail::Original>(rate, &motion,
                                                       "production");
    (void)space_detail::motion<space_detail::Candidate>(rate, &motion,
                                                        "candidate");
  }
  space_detail::fields(dir);
}

inline void spaceChecks() {
  for (double rate : space_detail::rates) {
    if (space_detail::motion<space_detail::Candidate>(rate, nullptr,
                                                      "candidate") > .03)
      throw std::runtime_error(
          "Loop candidate fails independent integrated motion oracle");
    auto original = space_detail::tape<space_detail::Original>(rate);
    auto candidate = space_detail::tape<space_detail::Candidate>(rate);
    const auto a = space_detail::playback<space_detail::Original>(
        *original, space_detail::neutral<space_detail::Original>(), rate, 2);
    const auto b = space_detail::playback<space_detail::Candidate>(
        *candidate, space_detail::neutral<space_detail::Candidate>(), rate, 2);
    if (a.left != b.left || a.right != b.right)
      throw std::runtime_error("Loop zero-motion control changed");
    auto extreme = space_detail::neutral<space_detail::Candidate>();
    extreme.wow = extreme.flutter = extreme.drift = 1;
    extreme.amplifier = extreme.degradation = 1;
    extreme.speed = 4;
    extreme.reverse = true;
    extreme.pitchSemitones = 12;
    (void)space_detail::playback<space_detail::Candidate>(*candidate, extreme,
                                                          rate, 2);
    auto field = std::make_unique<aste::field::Processor>();
    field->prepare(rate);
    aste::field::Parameters p;
    p.forever = true;
    p.mass = p.grain = p.pitch = p.motion = p.distance = 1;
    p.blend = 1;
    p.outputDb = 12;
    (void)space_detail::fieldSecond(*field, p, rate, true, true);
    (void)space_detail::fieldSecond(*field, p, rate, false, true);
  }
}
}  // namespace power
