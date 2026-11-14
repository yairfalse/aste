#include "core.hpp"

#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>

namespace aste::n01 {
namespace {
std::uint64_t hash(std::uint64_t x) noexcept {
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}
float unit(std::uint64_t x) noexcept {
  return static_cast<float>(hash(x) >> 40) / 16777216.0F;
}
bool range(double x, double lo, double hi) noexcept {
  return std::isfinite(x) && x >= lo && x <= hi;
}
}  // namespace

bool valid(const Pattern& p) noexcept {
  for (const auto& t : p.tracks) {
    if (!range(t.length, 0.25, 32) || !range(t.phase, 0, 32)) return false;
    for (const auto& e : t.events) {
      if (!range(e.beat, 0, t.length) || e.beat == t.length ||
          !range(e.microtiming, -0.125, 0.125) || e.every < 1 || e.every > 8 ||
          !range(e.probability, 0, 1) || e.repeats < 1 ||
          e.repeats > kRepeats || !range(e.interval, 0.001, 1) ||
          !range(e.acceleration, 0.5, 2) || !range(e.repeatGain, 0, 1) ||
          !range(e.pitchStep, -12, 12) || !range(e.mutation, 0, 1) ||
          !range(e.gain, 0, 1) || !range(e.pitch, 20, 8000) ||
          !range(e.decay, 0.002, 2) || !range(e.color, 0, 1))
        return false;
      double duration{}, interval = e.interval;
      for (unsigned r = 1; r < e.repeats; ++r) {
        duration += interval;
        interval *= e.acceleration;
      }
      // Repeats cannot leak into the next occurrence of their own event.
      if (duration >= t.length) return false;
    }
  }
  return true;
}

Pattern studyPattern() noexcept {
  Pattern p;
  p.tracks[0].length = 4;
  p.tracks[1].length = 3;
  p.tracks[2].length = 5;
  p.tracks[0].events[0] = {.enabled = true,
                           .gain = 0.6F,
                           .pitch = 43,
                           .decay = 0.18F,
                           .color = 0.2F};
  p.tracks[0].events[1] = {
      .enabled = true, .beat = 2.75, .gain = 0.4F, .pitch = 61, .decay = 0.09F};
  p.tracks[1].events[0] = {.enabled = true,
                           .beat = 0.75,
                           .repeats = 3,
                           .acceleration = 0.65,
                           .pitchStep = 2,
                           .mutation = 0.25F,
                           .pitch = 310,
                           .decay = 0.045F};
  p.tracks[1].events[1] = {.enabled = true,
                           .beat = 2.25,
                           .microtiming = -0.02,
                           .every = 2,
                           .gain = 0.3F,
                           .pitch = 730,
                           .decay = 0.025F};
  p.tracks[2].events[0] = {.enabled = true,
                           .beat = 1.5,
                           .probability = 0.75F,
                           .repeats = 4,
                           .interval = 0.0625,
                           .acceleration = 0.75,
                           .repeatGain = 0.65F,
                           .mutation = 0.5F,
                           .gain = 0.4F,
                           .pitch = 2300,
                           .decay = 0.008F,
                           .color = 0.7F};
  return p;
}

void Trace::add(const Hit& hit) noexcept {
  if (size < hits.size())
    hits[size++] = hit;
  else
    ++dropped;
}

bool Engine::prepare(double sampleRate, const Pattern& pattern) noexcept {
  prepared_ = false;
  if (!range(sampleRate, 8000, 192000) || !valid(pattern)) return false;
  rate_ = sampleRate;
  pattern_ = pattern;
  running_ = false;
  resets_ = 0;
  reset(0);
  prepared_ = true;
  return true;
}

Engine::Next Engine::next(std::size_t v, double beat) const noexcept {
  Next best{std::numeric_limits<double>::infinity(), {}};
  const auto& track = pattern_.tracks[v];
  for (std::size_t i = 0; i < kEvents; ++i) {
    const auto& e = track.events[i];
    if (!e.enabled || e.probability == 0) continue;
    double offset = track.phase + e.beat + e.microtiming;
    double interval = e.interval;
    for (unsigned r = 0; r < e.repeats; ++r) {
      auto cycle = static_cast<std::int64_t>(
          std::max(0.0, std::ceil((beat - offset) / track.length)));
      cycle += (e.every - cycle % e.every) % e.every;
      // A probability rejection skips this cycle, not arbitrary future cycles.
      const double at = offset + cycle * track.length;
      if (at <= best.beat) {
        const auto key = hash(pattern_.seed ^ hash(v + 1) ^ hash(i + 31) ^
                              hash(static_cast<std::uint64_t>(cycle) + 101));
        const float variation = (unit(key ^ hash(r + 17)) * 2 - 1) * e.mutation;
        // Keep rejected events as silent appointments, allowing bounded lookup.
        best = {at,
                {0, v, i, cycle, r,
                 unit(key) < e.probability
                     ? e.gain * std::pow(e.repeatGain, static_cast<float>(r))
                     : 0.0F,
                 std::clamp(
                     e.pitch * std::exp2((e.pitchStep * r + variation) / 12.0F),
                     20.0F, 8000.0F),
                 std::clamp(e.color + variation * 0.25F, 0.0F, 1.0F)}};
      }
      offset += interval;
      interval *= e.acceleration;
    }
  }
  return best;
}

void Engine::reset(double ppq) noexcept {
  voices_ = {};
  phrase_ = ppq;
  for (std::size_t v = 0; v < kVoices; ++v) next_[v] = next(v, ppq);
}

void Engine::trigger(const Hit& hit) noexcept {
  auto& v = voices_[hit.voice];
  const auto& e = pattern_.tracks[hit.voice].events[hit.event];
  v = {};
  v.amplitude = 1;
  v.gain = hit.gain;
  v.pitch = std::min(hit.pitch, static_cast<float>(rate_ * 0.12));
  v.color = hit.color;
  v.decay = static_cast<float>(std::exp(-1.0 / (rate_ * e.decay)));
  v.random =
      hash(pattern_.seed ^ hash(hit.voice + 1) ^ hash(hit.event + 31) ^
           hash(static_cast<std::uint64_t>(hit.cycle) + 101) ^ hit.repeat);
}

float Engine::sound(std::size_t index) noexcept {
  auto& v = voices_[index];
  if (v.amplitude < 1e-7F) return 0;
  v.phase += v.pitch / rate_;
  v.phase -= std::floor(v.phase);
  v.second += v.pitch * 2.731 / rate_;
  v.second -= std::floor(v.second);
  const auto sine =
      static_cast<float>(std::sin(2 * std::numbers::pi * v.phase));
  const auto partial =
      static_cast<float>(std::sin(2 * std::numbers::pi * v.second));
  float signal{};
  if (index == 0)
    signal = 0.8F * sine + 0.2F * v.color * partial;
  else if (index == 1)
    signal = 0.6F * sine + 0.4F * v.color * partial;
  else {
    v.random = hash(v.random + 0x9e3779b97f4a7c15ULL);
    const float noise = unit(v.random) * 2 - 1;
    const float coefficient = static_cast<float>(
        1 - std::exp(-2 * std::numbers::pi * v.pitch / rate_));
    v.noise += coefficient * (noise - v.noise);
    signal = v.noise * (1 - v.color) + (noise - v.noise) * v.color;
  }
  // Two-ms attack avoids making every source a hard-edged drum oscillator.
  const float attack =
      static_cast<float>(std::min(1.0, v.age++ / (rate_ * .002)));
  v.amplitude *= v.decay;
  return signal * v.amplitude * v.gain * attack;
}

void Engine::process(float* left, float* right, std::size_t frames,
                     const Transport& t, Trace* trace) noexcept {
  if (left == nullptr || frames == 0) return;
  const bool usable = prepared_ && t.valid && t.playing &&
                      range(t.ppq, -1e8, 1e8) && range(t.bpm, 20, 400) &&
                      t.sample >= -1000000000000LL &&
                      t.sample <= 1000000000000LL && frames <= 1048576;
  if (!usable) {
    std::fill_n(left, frames, 0);
    if (right) std::fill_n(right, frames, 0);
    running_ = false;
    return;
  }
  const double increment = t.bpm / (60 * rate_);
  const bool jump = !running_ || t.discontinuity ||
                    t.sample != expectedSample_ ||
                    std::abs(t.ppq - expectedPpq_) > increment * 0.5;
  if (jump) {
    reset(t.ppq);
    ++resets_;
  }
  if (jump || t.bpm != bpm_) {
    anchorSample_ = t.sample;
    anchorPpq_ = t.ppq;
    bpm_ = t.bpm;
  }
  running_ = true;
  for (std::size_t i = 0; i < frames; ++i) {
    const auto sample = t.sample + static_cast<std::int64_t>(i);
    const double ppq = anchorPpq_ + (sample - anchorSample_) * increment;
    phrase_ = ppq;
    float l{}, r{};
    for (std::size_t v = 0; v < kVoices; ++v) {
      // Half-open sample interval: never schedule early by more than one
      // sample.
      if (next_[v].beat < ppq + increment - 1e-12) {
        auto hit = next_[v].hit;
        hit.sample = sample;
        if (hit.gain > 0) {
          trigger(hit);
          if (trace) trace->add(hit);
        }
        // Collisions within one sample coalesce; later event slot wins exact
        // ties.
        next_[v] = next(v, ppq + increment);
      }
      const float value = sound(v) * 0.45F;
      l += value * (v == 2 ? 0.65F : 1.0F);
      r += value * (v == 1 ? 0.65F : 1.0F);
    }
    left[i] = l;
    if (right) right[i] = r;
  }
  expectedSample_ = t.sample + static_cast<std::int64_t>(frames);
  expectedPpq_ = anchorPpq_ + (expectedSample_ - anchorSample_) * increment;
  phrase_ = expectedPpq_;
}
}  // namespace aste::n01
