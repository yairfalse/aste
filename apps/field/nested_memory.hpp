#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

namespace aste::field {
// Two stereo banks: one immutable phrase and one recording ring. Captures swap
// ownership in O(1); inner loops are selections into the immutable phrase.
class NestedMemory {
 public:
  struct Controls {
    bool hold{}, inner{}, reverse{};
    float outerMs{1500}, innerMs{90}, speed{1};
  };
  void prepare(double rate) {
    rate_ = rate;
    capacity_ = static_cast<std::size_t>(std::ceil(rate * 4.)) + 4;
    for (auto& bank : banks_)
      for (auto& channel : bank) channel.assign(capacity_, 0.F);
    reset();
  }
  void reset() noexcept {
    recording_ = 0;
    write_ = available_ = captured_ = start_ = lastOuter_ = 0;
    phase_ = innerPhase_ = innerStart_ = 0.;
    active_ = innerActive_ = false;
    previousInner_ = previousHold_ = previousReverse_ = false;
    last_ = {};
    fadeFrom_ = {};
    fade_ = 0;
    lastSpeed_ = 1;
  }
  std::array<float, 2> process(float left, float right,
                               const Controls& c) noexcept {
    const std::size_t requested =
        std::clamp(static_cast<std::size_t>(
                       rate_ * std::clamp(c.outerMs, 20.F, 4000.F) * .001),
                   std::size_t{1}, capacity_ - 4);
    if (!c.hold && previousHold_) {
      active_ = innerActive_ = false;
      transition();
    }
    if (c.hold && !active_ && available_ >= requested) {
      captured_ = requested;
      start_ = (write_ + capacity_ - captured_) % capacity_;
      recording_ ^= 1U;
      write_ = available_ = 0;
      phase_ = innerPhase_ = innerStart_ = 0;
      active_ = true;
      innerActive_ = false;
      transition();
    }
    if (c.reverse != previousReverse_) transition();
    auto& ring = banks_[recording_];
    ring[0][write_] = left;
    ring[1][write_] = right;
    write_ = (write_ + 1) % capacity_;
    available_ = std::min(available_ + 1, capacity_ - 4);
    const std::size_t outer = active_ ? std::min(requested, captured_) : 1;
    lastOuter_ = outer;
    const std::size_t inner = std::min(
        outer,
        std::max(std::size_t{1},
                 static_cast<std::size_t>(
                     rate_ * std::clamp(c.innerMs, 4.F, 1000.F) * .001)));
    if (active_ && c.inner && !innerActive_) {
      innerStart_ = std::fmod(phase_, static_cast<double>(outer));
      innerPhase_ = 0;
      innerActive_ = true;
      transition();
    }
    if (innerActive_ && !c.inner) {
      innerActive_ = false;
      transition();
    }
    std::array<float, 2> result{left, right};
    if (active_) {
      const double length = static_cast<double>(innerActive_ ? inner : outer);
      double cursor = innerActive_ ? innerPhase_ : phase_;
      cursor = std::fmod(cursor, length);
      if (c.reverse) cursor = length - 1. - cursor;
      cursor = wrap(cursor, length);
      const double pos = wrap((innerActive_ ? innerStart_ : 0.) + cursor,
                              static_cast<double>(outer));
      for (std::size_t channel = 0; channel < 2; ++channel) {
        const auto first = static_cast<std::size_t>(pos);
        const auto second = (first + 1) % outer;
        const float fraction = static_cast<float>(pos - first);
        const auto& source = banks_[recording_ ^ 1U][channel];
        const float a = source[(start_ + first) % capacity_],
                    b = source[(start_ + second) % capacity_];
        // Very short symmetric boundary taper preserves intentional rhythmic
        // gaps.
        const double edge = std::min(length * .125, rate_ * .0005);
        const float boundary = static_cast<float>(std::min(
            1., std::min(cursor, length - 1. - cursor) / std::max(1., edge)));
        result[channel] = (a + fraction * (b - a)) * std::max(0.F, boundary);
      }
      const double target = std::clamp(c.speed, .25F, 2.F);
      lastSpeed_ +=
          (1. - std::exp(-1. / (rate_ * .01))) * (target - lastSpeed_);
      phase_ = wrap(phase_ + lastSpeed_, static_cast<double>(outer));
      innerPhase_ = wrap(innerPhase_ + lastSpeed_, static_cast<double>(inner));
    }
    if (fade_) {
      const float weight =
          static_cast<float>(fade_) / static_cast<float>(fadeLength());
      for (std::size_t i = 0; i < 2; ++i)
        result[i] = fadeFrom_[i] * weight + result[i] * (1 - weight);
      --fade_;
    }
    previousHold_ = c.hold;
    previousInner_ = c.inner;
    previousReverse_ = c.reverse;
    last_ = result;
    return result;
  }
  bool active() const noexcept { return active_; }
  bool innerActive() const noexcept { return innerActive_; }
  std::size_t available() const noexcept { return available_; }
  std::size_t bytes() const noexcept { return capacity_ * 4 * sizeof(float); }
  float position() const noexcept {
    return active_ && lastOuter_ ? static_cast<float>(phase_ / lastOuter_)
                                 : 0.F;
  }

 private:
  static double wrap(double value, double length) noexcept {
    value = std::fmod(value, length);
    return value < 0 ? value + length : value;
  }
  std::size_t fadeLength() const noexcept {
    return std::max(std::size_t{1}, static_cast<std::size_t>(rate_ * .002));
  }
  void transition() noexcept {
    fadeFrom_ = last_;
    fade_ = fadeLength();
  }
  std::array<std::array<std::vector<float>, 2>, 2> banks_;
  double rate_{48000}, phase_{}, innerPhase_{}, innerStart_{}, lastSpeed_{1};
  std::size_t capacity_{}, recording_{}, write_{}, available_{}, captured_{},
      start_{}, lastOuter_{}, fade_{};
  bool active_{}, innerActive_{}, previousHold_{}, previousInner_{},
      previousReverse_{};
  std::array<float, 2> last_{}, fadeFrom_{};
};
}  // namespace aste::field
