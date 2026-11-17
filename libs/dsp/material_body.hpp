#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <numbers>
namespace aste::dsp {
// Small, bounded excited body. Its state survives strikes; only prepare/reset
// erases it. All storage is inline and coefficient updates use a sample clock.
class MaterialBody {
 public:
  void prepare(double rate, unsigned identity = 0) noexcept {
    rate_ = rate;
    identity_ = identity;
    reset();
  }
  void reset() noexcept {
    modes_ = {};
    strain_ = 0;
    tick_ = 0;
  }
  float process(float excitation, float pitch, float seconds, float tone,
                float amount) noexcept {
    pitch = std::clamp(pitch, 25.F, static_cast<float>(rate_ * .2));
    seconds = std::clamp(seconds, .015F, 6.F);
    tone = std::clamp(tone, 0.F, 1.F);
    amount = std::clamp(amount, 0.F, 1.F);
    double energy = 0;
    for (const auto& m : modes_) energy += m.x * m.x + m.y * m.y;
    strain_ += (1. - std::exp(-1. / (rate_ * .12))) *
               (energy / (energy + .3) - strain_);
    if (tick_ == 0) {
      tick_ = std::max(std::size_t{1}, static_cast<std::size_t>(rate_ * .001));
      for (std::size_t i = 0; i < 3; ++i) {
        auto& m = modes_[i];
        const double ratio = i == 0 ? 1.
                             : i == 1
                                 ? 1.41 + 1.7 * tone + .07 * (identity_ % 3)
                                 : 2.7 + 3.1 * tone + .11 * (identity_ % 4);
        const double frequency =
            std::min(rate_ * .4,
                     pitch * ratio * (1. + .018 * amount * strain_ * (i + 1)));
        const double angle = 2 * std::numbers::pi * frequency / rate_;
        m.cosine = std::cos(angle);
        m.sine = std::sin(angle);
        m.radius = std::exp(
            -6.90775527898 /
            (rate_ * seconds * (1. + .5 * amount * strain_) / (1. + .65 * i)));
      }
    }
    --tick_;
    double output = 0;
    for (std::size_t i = 0; i < 3; ++i) {
      auto& m = modes_[i];
      const double x = m.x;
      m.x = m.radius * (m.cosine * x - m.sine * m.y) +
            excitation * .13 / (1. + i * .5);
      m.y = m.radius * (m.sine * x + m.cosine * m.y);
      const double e = m.x * m.x + m.y * m.y;
      if (e > 4) {
        const double limit = 2 / std::sqrt(e);
        m.x *= limit;
        m.y *= limit;
      }
      if (std::abs(m.x) < 1.e-25) m.x = 0;
      if (std::abs(m.y) < 1.e-25) m.y = 0;
      output += m.x;
    }
    return static_cast<float>(std::tanh(output * .65));
  }

 private:
  struct Mode {
    double x{}, y{}, cosine{}, sine{}, radius{};
  };
  std::array<Mode, 3> modes_;
  double rate_{48000}, strain_{};
  unsigned identity_{};
  std::size_t tick_{};
};
}  // namespace aste::dsp
