#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>
namespace aste::field {
// Production promotion of the evaluated diffused Hadamard FDN. No JUCE
// dependency.
class DiffuseSpace {
  struct Delay {
    std::vector<double> data;
    std::size_t at{}, valid{};
    double length{};
    void prepare(double n) {
      length = std::round(n);
      data.assign(std::size_t(length) + 128, 0);
      clear();
    }
    void clear() noexcept { at = valid = 0; }
    double read(double n) const noexcept {
      n = std::clamp(n, 1., double(data.size() - 2));
      if (n > double(valid)) return 0;
      double p = double(at) - n;
      if (p < 0) p += data.size();
      if (p >= double(data.size()))
        p = 0;  // Tiny negative positions can round to capacity when wrapped.
      auto i = std::size_t(p);
      auto j = i + 1 == data.size() ? 0 : i + 1;
      return data[i] + (p - i) * (data[j] - data[i]);
    }
    void write(double x) noexcept {
      data[at] = x;
      if (++at == data.size()) at = 0;
      valid = std::min(valid + 1, data.size());
    }
    double ap(double x, double g) noexcept {
      double z = read(length), y = z - g * x;
      write(x + g * y);
      return y;
    }
  };
  std::array<Delay, 16> lines_;
  std::array<Delay, 8> diffusers_;
  std::array<Delay, 2> early_;
  std::array<double, 16> low_{}, gain_{}, highGain_{}, phases_{};
  double rate_{48000}, smoothing_{}, lp_{}, rt_{2.4}, hf_{.55}, motion_{},
      transform_{}, grain_{}, hold_{}, focus_{}, energy_{};
  std::size_t clock_{};
  bool initialized_{};

 public:
  struct Controls {
    double rt{2.4}, hf{.55}, motion{}, transform{}, grain{}, hold{}, focus{};
  };
  void prepare(double rate) {
    rate_ = std::clamp(std::isfinite(rate) ? rate : 48000., 8000., 384000.);
    constexpr std::array<double, 16> ms{29.3, 31.7, 37.1, 41.1, 43.7, 47.9,
                                        53.3, 59.3, 61.7, 67.1, 71.9, 73.9,
                                        79.7, 83.9, 89.3, 97.1};
    for (int i = 0; i < 16; ++i) lines_[i].prepare(ms[i] * .001 * rate_ + 16);
    constexpr std::array<double, 8> ap{3.1, 4.7, 7.3, 11.1,
                                       3.7, 5.9, 8.3, 12.7};
    for (int i = 0; i < 8; ++i) diffusers_[i].prepare(ap[i] * .001 * rate_);
    for (auto& d : early_) d.prepare(.09 * rate_);
    smoothing_ = 1 - std::exp(-1 / (.035 * rate_));
    lp_ = 1 - std::exp(-2 * std::numbers::pi * 3500 / rate_);
    clear();
  }
  void clear() noexcept {
    for (auto& d : lines_) d.clear();
    for (auto& d : diffusers_) d.clear();
    for (auto& d : early_) d.clear();
    low_ = {};
    phases_ = {};
    clock_ = 0;
    initialized_ = false;
    energy_ = hold_ = 0;
  }
  std::size_t memoryBytes() const noexcept {
    std::size_t n = sizeof(*this);
    for (auto& d : lines_) n += d.data.size() * sizeof(double);
    for (auto& d : diffusers_) n += d.data.size() * sizeof(double);
    for (auto& d : early_) n += d.data.size() * sizeof(double);
    return n;
  }
  double energy() const noexcept { return std::sqrt(std::max(0., energy_)); }
  double retention() const noexcept { return gain_[0]; }
  std::array<float, 2> tick(float left, float right,
                            const Controls& c) noexcept {
    if (!initialized_) {
      rt_ = c.rt;
      hf_ = c.hf;
      motion_ = c.motion;
      transform_ = c.transform;
      grain_ = c.grain;
      focus_ = c.focus;
      hold_ = 0;
      initialized_ = true;
    }
    auto smooth = [&](double& x, double target) {
      x += smoothing_ * (target - x);
    };
    smooth(rt_, c.rt);
    smooth(hf_, c.hf);
    smooth(motion_, c.motion);
    smooth(transform_, c.transform);
    smooth(grain_, c.grain);
    smooth(focus_, c.focus);
    smooth(hold_, c.hold);
    if (clock_++ % 32 == 0)
      for (int i = 0; i < 16; ++i) {
        double seconds = lines_[i].length / rate_;
        gain_[i] = hold_ + (1 - hold_) * std::pow(10., -3 * seconds / rt_);
        highGain_[i] =
            hold_ + (1 - hold_) * std::pow(10., -3 * seconds / (rt_ * hf_));
      }
    double x = std::clamp(double(left), -8., 8.) * (1 - hold_),
           y = std::clamp(double(right), -8., 8.) * (1 - hold_);
    early_[0].write(x);
    early_[1].write(y);
    double erL = .12 * early_[0].read(.0071 * rate_) +
                 .09 * early_[1].read(.0137 * rate_) -
                 .07 * early_[0].read(.0239 * rate_) +
                 .05 * early_[1].read(.0371 * rate_);
    double erR = .12 * early_[1].read(.0089 * rate_) +
                 .09 * early_[0].read(.0173 * rate_) -
                 .07 * early_[1].read(.0287 * rate_) +
                 .05 * early_[0].read(.0431 * rate_);
    for (int i = 0; i < 4; ++i) {
      x = diffusers_[i].ap(x, .7);
      y = diffusers_[i + 4].ap(y, .7);
    }
    std::array<double, 16> mixed{};
    double outL = 0, outR = 0, power = 0;
    for (int i = 0; i < 16; ++i) {
      double depth = rate_ * .00015 * motion_ * (1 - hold_);
      double a = lines_[i].read(lines_[i].length +
                                depth * std::sin(phases_[i] + i * .731));
      low_[i] += lp_ * (a - low_[i]);
      mixed[i] = highGain_[i] * a + (gain_[i] - highGain_[i]) * low_[i];
      power += a * a;
      // Moving fragments affect the readout, preserving the conventional decay
      // path.
      double fragment = lines_[i].read(
          lines_[i].length +
          rate_ * .005 * grain_ * (.5 + .5 * std::sin(phases_[i] * 2 + i)));
      double heard = a + transform_ * grain_ * .65 * (fragment - a);
      outL += heard * (i % 2 ? -.25 : .25);
      outR += heard * ((i / 2) % 2 ? -.25 : .25);
      phases_[i] +=
          2 * std::numbers::pi * (.07 + motion_ * .55 + i * .00589) / rate_;
      if (phases_[i] > 2 * std::numbers::pi) phases_[i] -= 2 * std::numbers::pi;
    }
    for (int step = 1; step < 16; step *= 2)
      for (int i = 0; i < 16; i += step * 2)
        for (int j = 0; j < step; ++j) {
          double a = mixed[i + j], b = mixed[i + j + step];
          mixed[i + j] = a + b;
          mixed[i + j + step] = a - b;
        }
    for (auto& v : mixed) v *= .25;
    // Orthogonal rotations move resonant structure without a lossy pitch
    // return.
    for (int i = 0; i < 16; i += 2) {
      double angle = transform_ * .16 * std::sin(phases_[i]);
      double ca = std::cos(angle), sa = std::sin(angle);
      double a = mixed[i], b = mixed[i + 1];
      mixed[i] = ca * a - sa * b;
      mixed[i + 1] = sa * a + ca * b;
    }
    for (int i = 0; i < 16; ++i) {
      double input = (i % 2 ? y : x) * ((i / 2) % 2 ? -1 : 1) * .18;
      double v = input + mixed[i];
      if (std::abs(v) < 1e-28) v = 0;
      lines_[i].write(v);
    }
    energy_ += .001 * (power / 16 - energy_);
    return {float(outL + erL), float(outR + erR)};
  }
};
}  // namespace aste::field
