#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
#include <vector>
namespace aste::field_study {
enum class Topology { Bare8, House16, Hadamard16, Schroeder, CrossTank };
struct Controls {
  double decay{2.4}, highRatio{.55}, diffusion{.7}, modulation{.15};
};
class Engine {
  struct Delay {
    std::vector<double> data;
    std::size_t at{};
    double length{};
    void prepare(double n) {
      length = n;
      data.assign(std::size_t(std::ceil(n)) + 64, 0);
      at = 0;
    }
    double read(double n) const noexcept {
      double p = double(at) - std::clamp(n, 1., double(data.size() - 2));
      if (p < 0) p += data.size();
      auto i = std::size_t(p);
      auto j = i + 1 == data.size() ? 0 : i + 1;
      return data[i] + (p - i) * (data[j] - data[i]);
    }
    void write(double x) noexcept {
      data[at] = x;
      if (++at == data.size()) at = 0;
    }
    double step(double x) noexcept {
      double y = read(length);
      write(x);
      return y;
    }
    double ap(double x, double g, double modulation = 0) noexcept {
      double z = read(length + modulation), y = z - g * x;
      write(x + g * y);
      return y;
    }
    void reset() {
      std::fill(data.begin(), data.end(), 0);
      at = 0;
    }
  };
  std::array<Delay, 16> lines_;
  std::array<Delay, 8> diffusion_;
  std::array<Delay, 4> tankAp_;
  Delay earlyL_, earlyR_;
  std::array<double, 16> low_{}, gain_{}, highGain_{};
  std::array<double, 2> cross_{};
  std::array<double, 16> phases_{};
  double rate_{48000}, phase_{}, decay_{2.4}, ratio_{.55}, diff_{.7}, mod_{.15};
  double smooth_{}, lp_{};
  bool initialized_{};
  std::size_t clock_{};
  Topology type_{};

 public:
  void prepare(double rate, Topology t) {
    rate_ = std::isfinite(rate) ? std::clamp(rate, 8000., 192000.) : 48000.;
    type_ = t;
    constexpr std::array<double, 16> ms{29.3, 31.7, 37.1, 41.1, 43.7, 47.9,
                                        53.3, 59.3, 61.7, 67.1, 71.9, 73.9,
                                        79.7, 83.9, 89.3, 97.1};
    for (int i = 0; i < 16; ++i)
      lines_[i].prepare(std::round(ms[i] * .001 * rate_));
    constexpr std::array<double, 8> apms{3.1, 4.7, 7.3, 11.1,
                                         3.7, 5.9, 8.3, 12.7};
    for (int i = 0; i < 8; ++i)
      diffusion_[i].prepare(std::round(apms[i] * .001 * rate_));
    constexpr std::array<double, 4> tm{19.7, 27.1, 61.3, 73.7};
    for (int i = 0; i < 4; ++i)
      tankAp_[i].prepare(std::round(tm[i] * .001 * rate_));
    if (t == Topology::CrossTank) {
      lines_[0].prepare(std::round(.1273 * rate_));
      lines_[1].prepare(std::round(.1139 * rate_));
      lines_[2].prepare(std::round(.1013 * rate_));
      lines_[3].prepare(std::round(.0899 * rate_));
    }
    earlyL_.prepare(.09 * rate_);
    earlyR_.prepare(.09 * rate_);
    smooth_ = 1 - std::exp(-1 / (.03 * rate_));
    lp_ = 1 - std::exp(-2 * std::numbers::pi * 3500 / rate_);
    reset();
  }
  void reset() {
    for (auto& d : lines_) d.reset();
    for (auto& d : diffusion_) d.reset();
    for (auto& d : tankAp_) d.reset();
    earlyL_.reset();
    earlyR_.reset();
    low_ = {};
    cross_ = {};
    phases_ = {};
    phase_ = 0;
    clock_ = 0;
    initialized_ = false;
    decay_ = 2.4;
    ratio_ = .55;
    diff_ = .7;
    mod_ = .15;
  }
  std::size_t memoryBytes() const {
    std::size_t b = sizeof(*this);
    for (auto& d : lines_) b += d.data.size() * sizeof(double);
    for (auto& d : diffusion_) b += d.data.size() * sizeof(double);
    for (auto& d : tankAp_) b += d.data.size() * sizeof(double);
    return b + (earlyL_.data.size() + earlyR_.data.size()) * sizeof(double);
  }
  void process(float* l, float* r, std::size_t n, const Controls& c) noexcept {
    auto finite = [](double x, double d) { return std::isfinite(x) ? x : d; };
    const double td = std::clamp(finite(c.decay, 2.4), .15, 20.),
                 tr = std::clamp(finite(c.highRatio, .55), .15, 1.);
    const double tf = std::clamp(finite(c.diffusion, .7), 0., .78),
                 tm = std::clamp(finite(c.modulation, .15), 0., 1.);
    if (!initialized_) {
      decay_ = td;
      ratio_ = tr;
      diff_ = tf;
      mod_ = tm;
      initialized_ = true;
    }
    for (std::size_t k = 0; k < n; ++k) {
      decay_ += smooth_ * (td - decay_);
      ratio_ += smooth_ * (tr - ratio_);
      diff_ += smooth_ * (tf - diff_);
      mod_ += smooth_ * (tm - mod_);
      if ((clock_++ % 32) == 0) {
        for (int i = 0; i < 16; ++i) {
          double seconds = lines_[i].length / rate_;
          if (type_ == Topology::CrossTank)
            seconds = (lines_[i % 2].length + lines_[2 + i % 2].length +
                       tankAp_[i % 2].length + tankAp_[2 + i % 2].length) /
                      rate_;
          gain_[i] = std::pow(10., -3 * seconds / decay_);
          highGain_[i] = std::pow(10., -3 * seconds / (decay_ * ratio_));
        }
      }
      double x = std::clamp(finite(l[k], 0), -8., 8.),
             y = std::clamp(finite(r[k], 0), -8., 8.);
      earlyL_.write(x);
      earlyR_.write(y);
      double erL = .12 * earlyL_.read(.0071 * rate_) +
                   .09 * earlyR_.read(.0137 * rate_) -
                   .07 * earlyL_.read(.0239 * rate_) +
                   .05 * earlyR_.read(.0371 * rate_);
      double erR = .12 * earlyR_.read(.0089 * rate_) +
                   .09 * earlyL_.read(.0173 * rate_) -
                   .07 * earlyR_.read(.0287 * rate_) +
                   .05 * earlyL_.read(.0431 * rate_);
      if (type_ != Topology::Bare8) {
        for (int i = 0; i < 4; ++i) {
          x = diffusion_[i].ap(x, diff_);
          y = diffusion_[i + 4].ap(y, diff_);
        }
      }
      double outL = 0, outR = 0;
      if (type_ == Topology::CrossTank) {
        std::array<double, 2> next{};
        for (int j = 0; j < 2; ++j) {
          double a = (x + y) * .22 + cross_[1 - j];
          a = tankAp_[j].ap(a, -.65,
                            std::sin(phase_ + j) * rate_ * .0003 * mod_);
          a = lines_[j].step(a);
          low_[j] += lp_ * (a - low_[j]);
          a = highGain_[j] * a + (gain_[j] - highGain_[j]) * low_[j];
          a = tankAp_[j + 2].ap(a, .5);
          next[j] = lines_[j + 2].step(a);
        }
        cross_ = next;
        outL = .45 *
               (lines_[0].read(.0311 * rate_) + lines_[2].read(.0437 * rate_) -
                lines_[1].read(.0193 * rate_));
        outR = .45 *
               (lines_[1].read(.0373 * rate_) + lines_[3].read(.0521 * rate_) -
                lines_[0].read(.0239 * rate_));
      } else {
        int count =
            (type_ == Topology::Bare8 || type_ == Topology::Schroeder) ? 8 : 16;
        std::array<double, 16> a{}, mixed{};
        for (int i = 0; i < count; ++i) {
          double depth =
              type_ == Topology::Schroeder ? 0 : rate_ * .00015 * mod_;
          a[i] = lines_[i].read(lines_[i].length +
                                depth * std::sin(phases_[i] + i * .731));
          phases_[i] += 2 * std::numbers::pi * (.19 + i * .00589) / rate_;
          if (phases_[i] > 2 * std::numbers::pi)
            phases_[i] -= 2 * std::numbers::pi;
          low_[i] += lp_ * (a[i] - low_[i]);
          mixed[i] = highGain_[i] * a[i] + (gain_[i] - highGain_[i]) * low_[i];
          outL += a[i] * (i % 2 ? -.25 : .25);
          outR += a[i] * ((i / 2) % 2 ? -.25 : .25);
        }
        if (type_ == Topology::Hadamard16) {
          for (int step = 1; step < 16; step *= 2)
            for (int i = 0; i < 16; i += step * 2)
              for (int j = 0; j < step; ++j) {
                double u = mixed[i + j], v = mixed[i + j + step];
                mixed[i + j] = u + v;
                mixed[i + j + step] = u - v;
              }
          for (auto& v : mixed) v *= .25;
        } else if (type_ != Topology::Schroeder) {
          double sum = 0;
          for (int i = 0; i < count; ++i) sum += mixed[i];
          for (int i = 0; i < count; ++i) mixed[i] = 2 * sum / count - mixed[i];
        }
        for (int i = 0; i < count; ++i) {
          double input = (i % 2 ? y : x) * ((i / 2) % 2 ? -1 : 1) * .18;
          lines_[i].write(input + mixed[i]);
        }
      }
      phase_ += 2 * std::numbers::pi * .19 / rate_;
      if (phase_ > 2 * std::numbers::pi) phase_ -= 2 * std::numbers::pi;
      l[k] = float(outL + (type_ == Topology::Bare8 ? 0 : erL));
      r[k] = float(outR + (type_ == Topology::Bare8 ? 0 : erR));
    }
  }
};
}  // namespace aste::field_study
