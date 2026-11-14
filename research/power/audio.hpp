#pragma once
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <complex>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numbers>
#include <optional>
#include <stdexcept>
#include <string>
#include <vector>

namespace power {
constexpr double pi = std::numbers::pi;
struct Audio {
  double rate{48000};
  std::vector<float> left, right;
  explicit Audio(std::size_t frames = 384000, double fs = 48000)
      : rate(fs), left(frames), right(frames) {}
};
inline void validate(const Audio& a) {
  if (a.left.empty() || a.left.size() != a.right.size() ||
      !std::isfinite(a.rate) || a.rate <= 0)
    throw std::runtime_error("invalid stereo audio extent/rate");
  for (std::size_t i = 0; i < a.left.size(); ++i)
    if (!std::isfinite(a.left[i]) || !std::isfinite(a.right[i]))
      throw std::runtime_error("nonfinite audio");
}
inline double gain(double db) { return std::pow(10., db / 20); }
inline double noise(std::uint64_t n) {
  n += 0x9e3779b97f4a7c15ULL;
  n = (n ^ (n >> 30)) * 0xbf58476d1ce4e5b9ULL;
  n = (n ^ (n >> 27)) * 0x94d049bb133111ebULL;
  n ^= n >> 31;
  return static_cast<double>(n >> 40) / 8388608. - 1;
}
inline Audio fixture(int kind, double fs = 48000, double seconds = 8) {
  Audio a(static_cast<std::size_t>(fs * seconds), fs);
  for (std::size_t i = 0; i < a.left.size(); ++i) {
    const double t = i / fs, beat = std::fmod(t, .5);
    const double kick =
        std::sin(2 * pi * (52 * beat + .6 * (1 - std::exp(-beat * 60)))) *
        std::exp(-beat * 18);
    const double hat = noise(i + 197) * std::exp(-std::fmod(t, .25) * 150);
    const double bass = std::sin(2 * pi * 55 * t) * std::exp(-beat * 6) +
                        .3 * std::sin(2 * pi * 110 * t) * std::exp(-beat * 12);
    double chord{};
    for (int note : {48, 51, 55, 62}) {
      const double f = 440 * std::exp2((note - 69.) / 12);
      chord += std::sin(2 * pi * f * t) * .25;
    }
    chord *=
        std::min(1., std::fmod(t, 2.) * 30) * std::exp(-std::fmod(t, 2.) * 1.5);
    double l{}, r{};
    switch (kind) {
      case 0:
        l = .22 * kick + .05 * hat;
        r = .22 * kick + .04 * hat;
        break;
      case 1:
        l = r = .19 * bass;
        break;
      case 2:
        l = r = .18 * std::sin(2 * pi * 220 * t);
        break;
      case 3:
        l = .12 * noise(i + 501);
        r = .12 * noise(i + 817);
        break;
      case 4:
        l = .22 * chord;
        r = .21 * chord + .01 * std::sin(2 * pi * 196 * t);
        break;
      case 5:
        l = .13 * kick + .06 * hat + .1 * bass + .1 * chord;
        r = .13 * kick - .035 * hat + .1 * bass + .11 * chord;
        break;
      default:
        l = r = std::fmod(t, 2.) < .18 ? .2 * kick + .08 * hat : 0;
        break;
    }
    // Every fixture leaves two seconds of genuine silence for release/decay.
    if (t >= seconds - 2) l = r = 0;
    a.left[i] = static_cast<float>(l);
    a.right[i] = static_cast<float>(r);
  }
  return a;
}
inline constexpr std::array fixtureNames{
    "percussion",      "bass",  "oscillator", "noise", "chord",
    "generated_music", "sparse"};

inline std::ofstream output(const std::filesystem::path& path,
                            bool binary = false) {
  std::ofstream o(path, binary ? std::ios::binary : std::ios::out);
  o.exceptions(std::ios::failbit | std::ios::badbit);
  o << std::setprecision(12);
  return o;
}
inline void u16(std::ostream& out, std::uint16_t x) {
  out.put(static_cast<char>(x));
  out.put(static_cast<char>(x >> 8));
}
inline void u32(std::ostream& out, std::uint32_t x) {
  for (unsigned b = 0; b < 4; ++b) out.put(static_cast<char>(x >> (8 * b)));
}
inline void wav(const std::filesystem::path& path, const Audio& a,
                double scale = 1) {
  if (a.left.size() != a.right.size() || a.left.size() > 500000000)
    throw std::runtime_error("WAV extent invalid");
  auto out = output(path, true);
  const auto bytes = static_cast<std::uint32_t>(a.left.size() * 8);
  out.write("RIFF", 4);
  u32(out, bytes + 48);
  out.write("WAVEfmt ", 8);
  u32(out, 16);
  u16(out, 3);
  u16(out, 2);
  u32(out, static_cast<std::uint32_t>(a.rate));
  u32(out, static_cast<std::uint32_t>(a.rate) * 8);
  u16(out, 8);
  u16(out, 32);
  out.write("fact", 4);
  u32(out, 4);
  u32(out, static_cast<std::uint32_t>(a.left.size()));
  out.write("data", 4);
  u32(out, bytes);
  for (std::size_t i = 0; i < a.left.size(); ++i)
    for (float x : {a.left[i], a.right[i]}) {
      const float y = static_cast<float>(x * scale);
      if (!std::isfinite(y)) throw std::runtime_error("nonfinite WAV");
      u32(out, std::bit_cast<std::uint32_t>(y));
    }
}
struct Biquad {
  std::array<double, 3> b;
  std::array<double, 2> a;
  double z1{}, z2{};
  double tick(double x) {
    const double y = b[0] * x + z1;
    z1 = b[1] * x - a[0] * y + z2;
    z2 = b[2] * x - a[1] * y;
    return y;
  }
};
// BS.1770 K weighting and EBU-style 400 ms/100 ms integrated gating, stereo.
// Deliberately limited to the pack's 48 kHz; never reuse these coefficients at
// another rate.
inline std::optional<double> loudness(const Audio& x) {
  validate(x);
  if (x.rate != 48000 || x.left.size() < 19200) return std::nullopt;
  std::array<Biquad, 2> shelf{
      Biquad{{1.53512485958697, -2.69169618940638, 1.19839281085285},
             {-1.69065929318241, .73248077421585}},
      Biquad{{1.53512485958697, -2.69169618940638, 1.19839281085285},
             {-1.69065929318241, .73248077421585}}};
  std::array<Biquad, 2> high{
      Biquad{{1, -2, 1}, {-1.99004745483398, .99007225036621}},
      Biquad{{1, -2, 1}, {-1.99004745483398, .99007225036621}}};
  std::vector<double> prefix(x.left.size() + 1);
  for (std::size_t i = 0; i < x.left.size(); ++i) {
    const double l = high[0].tick(shelf[0].tick(x.left[i]));
    const double r = high[1].tick(shelf[1].tick(x.right[i]));
    prefix[i + 1] = prefix[i] + l * l + r * r;
  }
  const auto db = [](double energy) { return -.691 + 10 * std::log10(energy); };
  std::vector<double> blocks;
  double first{};
  for (std::size_t i = 19200; i < prefix.size(); i += 4800) {
    const double e = std::max(0., (prefix[i] - prefix[i - 19200]) / 19200);
    if (e > 0 && db(e) >= -70) {
      blocks.push_back(e);
      first += e;
    }
  }
  if (blocks.empty()) return std::nullopt;
  const double threshold = std::max(-70., db(first / blocks.size()) - 10);
  double final{};
  std::size_t count{};
  for (double e : blocks)
    if (db(e) >= threshold) {
      final += e;
      ++count;
    }
  if (!count) return std::nullopt;
  return db(final / count);
}
inline std::vector<double> spectrum(const Audio& a, std::size_t offset = 48000,
                                    std::size_t n = 32768) {
  validate(a);  // This routine deliberately reports MID-channel power, not
                // stereo power.
  if (!std::has_single_bit(n) || offset + n > a.left.size())
    throw std::runtime_error("FFT extent");
  std::vector<std::complex<double>> x(n);
  for (std::size_t i = 0; i < n; ++i)
    x[i] = .5 * (a.left[offset + i] + a.right[offset + i]) *
           (.5 - .5 * std::cos(2 * pi * i / n));
  for (std::size_t i = 1, j = 0; i < n; ++i) {
    auto bit = n >> 1;
    for (; j & bit; bit >>= 1) j ^= bit;
    j ^= bit;
    if (i < j) std::swap(x[i], x[j]);
  }
  for (std::size_t length = 2; length <= n; length *= 2) {
    const auto step = std::polar(1., -2 * pi / length);
    for (std::size_t start = 0; start < n; start += length) {
      std::complex<double> phase{1};
      for (std::size_t i = 0; i < length / 2; ++i) {
        const auto p = x[start + i], q = phase * x[start + i + length / 2];
        x[start + i] = p + q;
        x[start + i + length / 2] = p - q;
        phase *= step;
      }
    }
  }
  std::vector<double> result(n / 2 + 1);
  for (std::size_t i = 0; i < result.size(); ++i) result[i] = std::norm(x[i]);
  return result;
}
inline std::complex<double> component(const Audio& a, double hz,
                                      double begin = 1, double duration = 1) {
  const auto start = static_cast<std::size_t>(a.rate * begin);
  const auto count = static_cast<std::size_t>(a.rate * duration);
  if (start + count > a.left.size() || !count)
    throw std::runtime_error("component extent");
  std::complex<double> sum{};
  for (std::size_t i = 0; i < count; ++i)
    sum += static_cast<double>(a.left[start + i]) *
           std::polar(1., -2 * pi * hz * (start + i) / a.rate);
  return sum * (2. / count);
}
struct Metrics {
  double peak{}, rms{}, tail{}, delta{}, midEnergy{}, sideEnergy{};
  std::optional<double> lufs, correlation;
  std::uint64_t hash{14695981039346656037ULL};
};
inline Metrics measure(const Audio& a) {
  validate(a);
  Metrics m;
  double sum{}, tail{}, ll{}, rr{}, lr{};
  for (std::size_t i = 0; i < a.left.size(); ++i) {
    const double l = a.left[i], r = a.right[i];
    ll += l * l;
    rr += r * r;
    lr += l * r;
    m.midEnergy += .25 * (l + r) * (l + r);
    m.sideEnergy += .25 * (l - r) * (l - r);
    for (float x : {a.left[i], a.right[i]}) {
      if (!std::isfinite(x)) throw std::runtime_error("nonfinite audio");
      m.peak = std::max(m.peak, static_cast<double>(std::abs(x)));
      sum += x * static_cast<double>(x);
      if (i + static_cast<std::size_t>(a.rate) >= a.left.size())
        tail += x * static_cast<double>(x);
      const auto bits = std::bit_cast<std::uint32_t>(x);
      for (unsigned b = 0; b < 4; ++b) {
        m.hash ^= (bits >> (b * 8)) & 255;
        m.hash *= 1099511628211ULL;
      }
    }
    if (i)
      m.delta = std::max(
          {m.delta, std::abs(l - a.left[i - 1]), std::abs(r - a.right[i - 1])});
  }
  m.rms = std::sqrt(sum / (2 * a.left.size()));
  m.tail = std::sqrt(tail / (2 * std::min<double>(a.rate, a.left.size())));
  if (ll > 0 && rr > 0) m.correlation = lr / std::sqrt(ll * rr);
  m.lufs = loudness(a);
  return m;
}
inline double rms(const Audio& a, double start, double seconds) {
  const auto begin = static_cast<std::size_t>(start * a.rate),
             count = static_cast<std::size_t>(seconds * a.rate);
  if (!count || begin + count > a.left.size())
    throw std::runtime_error("RMS extent");
  double sum{};
  for (std::size_t i = begin; i < begin + count; ++i)
    sum += a.left[i] * static_cast<double>(a.left[i]);
  return std::sqrt(sum / count);
}
inline double difference(const Audio& a, const Audio& b) {
  validate(a);
  validate(b);
  if (a.left.size() != b.left.size())
    throw std::runtime_error("comparison extent");
  double sum{};
  for (std::size_t i = 0; i < a.left.size(); ++i) {
    const double l = a.left[i] - b.left[i], r = a.right[i] - b.right[i];
    sum += l * l + r * r;
  }
  return std::sqrt(sum / (2 * a.left.size()));
}
}  // namespace power
