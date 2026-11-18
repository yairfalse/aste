#include "click_memory.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace aste::click {
namespace {
double bound(double x, double a, double b, double fallback = 0) noexcept {
  return std::isfinite(x) ? std::clamp(x, a, b) : fallback;
}
double wrap(double x, double length) noexcept {
  return std::fmod(std::fmod(x, length) + length, length);
}
}  // namespace
bool MasterMemory::prepare(double rate) {
  if (!std::isfinite(rate) || rate < 22050 || rate > 192000) return false;
  rate_ = rate;
  count_.store(0);
  selected_.store(0);
  status_.store(0);
  position_.store(0);
  progress_.store(0);
  written_ = available_ = printAt_ = printLength_ = 0;
  cursor_ = mix_ = transition_ = 0;
  requested_ = 0;
  playingGeneration_ = -1;
  printing_ = wasCapture_ = wasResample_ = wasRepeat_ = false;
  initialized_ = false;
  last_ = ic1_ = ic2_ = ic3_ = ic4_ = {};
  for (auto& b : bank_) b.start = b.length = 0;
  capacity_ = static_cast<std::size_t>(std::ceil(rate * 8));
  for (auto& b : bank_)
    for (auto& channel : b.data) channel.assign(capacity_, 0);
  for (auto& channel : live_.data) channel.assign(capacity_, 0);
  return true;
}
void MasterMemory::restore(const CapturedState& saved) {
  if (capacity_ == 0 || saved.rate < 22050 || saved.rate > 192000 ||
      !std::isfinite(saved.rate))
    return;
  int count = 0;
  for (int n = 0; n < memoryGenerations; ++n) {
    const auto& source = saved.audio[n];
    const auto frames = source.size() / 2;
    if (frames < 2) break;
    auto& b = bank_[n];
    b.length = std::min(
        capacity_,
        static_cast<std::size_t>(std::round(frames * rate_ / saved.rate)));
    for (std::size_t i = 0; i < b.length; ++i) {
      const double at =
          std::min(i * saved.rate / rate_, static_cast<double>(frames - 1));
      const auto a = static_cast<std::size_t>(at),
                 next = std::min(a + 1, frames - 1);
      for (int ch = 0; ch < 2; ++ch)
        b.data[ch][i] = static_cast<float>(
            bound(at == a ? source[a * 2 + ch]
                          : std::lerp(source[a * 2 + ch], source[next * 2 + ch],
                                      static_cast<float>(at - a)),
                  -8, 8));
    }
    ++count;
  }
  count_.store(count);
  requested_ = std::clamp(saved.selected, 0, std::max(0, count - 1));
  selected_.store(requested_);
}
CapturedState MasterMemory::state() const {
  CapturedState result;
  result.rate = rate_;
  const int count = count_.load();
  result.selected = std::clamp(selected_.load(), 0, std::max(0, count - 1));
  for (int n = 0; n < count; ++n) {
    const auto& b = bank_[n];
    auto& audio = result.audio[n];
    audio.resize(b.length * 2);
    for (std::size_t i = 0; i < b.length; ++i)
      for (int ch = 0; ch < 2; ++ch)
        audio[i * 2 + ch] = b.data[ch][(b.start + i) % capacity_];
  }
  return result;
}
std::array<float, 128> MasterMemory::waveform(int generation) const {
  std::array<float, 128> result{};
  if (generation < 0 || generation >= count_.load()) return result;
  const auto& b = bank_[generation];
  for (std::size_t i = 0; i < b.length; ++i) {
    const auto bin = std::min<std::size_t>(127, i * 128 / b.length);
    for (int ch = 0; ch < 2; ++ch)
      result[bin] = std::max(result[bin],
                             std::abs(b.data[ch][(b.start + i) % capacity_]));
  }
  return result;
}
bool MasterMemory::capture(const MemoryControls& c, double bpm) noexcept {
  const int next = count_.load();
  if (next == memoryGenerations) {
    status_.store(2);
    return false;
  }
  if (available_ < 2) {
    status_.store(1);
    return false;
  }
  auto& b = bank_[next];
  b.length = std::min(available_,
                      static_cast<std::size_t>(bound(
                          c.captureBeats * 60 / bpm * rate_, 2, capacity_, 2)));
  const auto ago =
      std::min(available_ - b.length,
               static_cast<std::size_t>(
                   bound(c.captureAgoBeats * 60 / bpm * rate_, 0, capacity_)));
  b.start = (written_ + capacity_ - b.length - ago) % capacity_;
  b.data.swap(
      live_
          .data);  // Both banks preallocated; constant-time ownership transfer.
  available_ = written_ = 0;
  publish();
  return true;
}
void MasterMemory::publish() noexcept {
  const int next = count_.load();
  selected_.store(next);
  count_.store(next + 1);  // Release immutable PCM and its metadata to readers.
  cursor_ = 0;
  transition_ = 1;
  printing_ = false;
  progress_.store(0);
  status_.store(0);
}
void MasterMemory::process(float* l, float* r, std::size_t frames,
                           const MemoryControls& c, double bpm, Command command,
                           bool discontinuity) noexcept {
  if (capacity_ == 0 || frames == 0) return;
  if (!initialized_) {
    initialized_ = true;
    if ((c.play || c.repeat) && count_.load() > 0) {
      mix_ = 1;
      transition_ =
          1;  // Recalled playback fades from silence, never from live objects.
    }
  }
  bpm = bound(bpm, 20, 400, 120);
  if (discontinuity) {
    cursor_ = 0;
    printing_ = false;
    transition_ = 1;
    progress_.store(0);
    status_.store(0);
  }
  if (c.selected != requested_ || command == Select) {
    requested_ = c.selected;
    selected_.store(std::clamp(c.selected, 0, std::max(0, count_.load() - 1)));
    cursor_ = 0;
    transition_ = 1;
  }
  if (command == Previous || command == Next || command == Original) {
    const int next = command == Original
                         ? 0
                         : selected_.load() + (command == Previous ? -1 : 1);
    selected_.store(std::clamp(next, 0, std::max(0, count_.load() - 1)));
    cursor_ = 0;
    transition_ = 1;
  }
  if (command == Capture || (c.capture && !wasCapture_) ||
      (c.repeat && !wasRepeat_ && count_.load() == 0)) {
    if (!printing_) capture(c, bpm);
  }
  wasCapture_ = c.capture;
  const bool print = command == Resample || (c.resample && !wasResample_);
  wasResample_ = c.resample;
  wasRepeat_ = c.repeat;
  if (print && !printing_) {
    if (count_.load() == 0)
      status_.store(1);
    else if (count_.load() == memoryGenerations)
      status_.store(2);
    else {
      printing_ = true;
      printAt_ = 0;
      cursor_ = 0;
      printSource_ = selected_.load();
      printSettings_ = c;
      printTempo_ = bpm;
      playingGeneration_ =
          -1;  // Deterministic print, independent of monitor filter history.
      status_.store(3);
    }
  }
  const double fade = 1 - std::exp(-1 / (.003 * rate_));
  for (std::size_t i = 0; i < frames; ++i) {
    // Capture point: engine output after compression/repeats, before this
    // sampler and final Master. Never feed the resampled output into live_.
    live_.data[0][written_] = static_cast<float>(bound(l[i], -8, 8));
    live_.data[1][written_] = static_cast<float>(bound(r[i], -8, 8));
    written_ = (written_ + 1) % capacity_;
    available_ = std::min(capacity_, available_ + 1);
    std::array<double, 2> wet{};
    const auto& playback = printing_ ? printSettings_ : c;
    const int selected = printing_ ? printSource_ : selected_.load();
    bool committed = false;
    if (selected < count_.load()) {
      const auto& b = bank_[selected];
      const auto& target = playback.generation[selected];
      const bool fresh = selected != playingGeneration_;
      if (fresh) {
        ic1_ = ic2_ = ic3_ = ic4_ = {};
        playingGeneration_ = selected;
        smooth_ = FragmentControls{};
      }
      if (smooth_.reverse != target.reverse ||
          smooth_.playback != target.playback)
        transition_ = 1;
      const auto approach = [&](float& value, float desired, double lo,
                                double hi, double fallback = 0) {
        desired = static_cast<float>(bound(desired, lo, hi, fallback));
        value = printing_ || fresh
                    ? desired
                    : value + static_cast<float>(fade) * (desired - value);
      };
      approach(smooth_.start, target.start, 0, .999999);
      approach(smooth_.end, target.end, .000001, 1, 1);
      approach(smooth_.loopStart, target.loopStart, 0, .999999);
      approach(smooth_.loopEnd, target.loopEnd, .000001, 1, 1);
      approach(smooth_.pitch, target.pitch, -48, 48);
      approach(smooth_.speed, target.speed, .125, 4, 1);
      approach(smooth_.cutoff, target.cutoff, 30, 20000, 20000);
      approach(smooth_.resonance, target.resonance, 0, .95);
      approach(smooth_.morph, target.morph, 0, 1);
      smooth_.reverse = target.reverse;
      smooth_.playback = target.playback;
      const auto& p = smooth_;
      const double start = bound(p.start, 0, .999999) * (b.length - 1);
      const double end = std::max(start + 1, bound(p.end, 0, 1, 1) * b.length);
      const double a = bound(p.loopStart * b.length, start, end - 1, start);
      double z = bound(p.loopEnd * b.length, a + 1, end, end);
      const double step =
          bound(p.speed, .125, 4, 1) * std::exp2(bound(p.pitch, -48, 48) / 12);
      constexpr std::array<double, 6> divisions{1, .5, .25, .125, .0625, 0};
      double repeatSpan = 0;
      if (playback.repeat) {
        const double beats =
            playback.division == 5
                ? bound(playback.freeBeats, .015625, 4, .125)
                : divisions[std::clamp(playback.division, 0, 4)];
        repeatSpan = std::max(
            step, beats * 60 / (printing_ ? printTempo_ : bpm) * rate_ * step);
        z = std::min(z, a + repeatSpan);
      }
      const double audioSpan = z - a;
      const double span = playback.repeat ? repeatSpan : audioSpan;
      cursor_ = wrap(cursor_, span);
      const int mode = std::clamp(p.playback, 0, 3);
      const auto rawRead = [&](double position, int ch) {
        position = bound(position, 0, b.length - 1);
        const auto k = static_cast<std::size_t>(position);
        const auto next = std::min(k + 1, b.length - 1);
        return std::lerp(
            static_cast<double>(b.data[ch][(b.start + k) % capacity_]),
            static_cast<double>(b.data[ch][(b.start + next) % capacity_]),
            position - k);
      };
      const auto read = [&](double position, int ch) {
        if (mode == 0) return rawRead(position, ch);
        constexpr std::array<double, 4> rates{0, 44100, 22050, 11025},
            levels{0, 32768, 2048, 128};
        const double grid = rate_ / rates[mode],
                     cell = std::floor(position / grid);
        const double a =
            std::round(rawRead(cell * grid, ch) * levels[mode]) / levels[mode];
        const double b =
            std::round(rawRead((cell + 1) * grid, ch) * levels[mode]) /
            levels[mode];
        return mode == 1 ? std::lerp(a, b, position / grid - cell) : a;
      };
      const double reverseStart = a + std::max(0., audioSpan - 1);
      const double position =
          p.reverse ? std::max(a, reverseStart - cursor_) : a + cursor_;
      for (int ch = 0; ch < 2; ++ch) {
        if (cursor_ >= audioSpan)
          continue;  // Space belongs to the clock, not to the sample's
                     // duration.
        wet[ch] = read(position, ch);
        const double seam = std::min(audioSpan * .125, rate_ * .001 * step);
        if (seam > 0 && cursor_ > audioSpan - seam)
          wet[ch] = std::lerp(
              wet[ch],
              span > audioSpan ? 0. : read(p.reverse ? reverseStart : a, ch),
              (cursor_ - audioSpan + seam) / seam);
      }
      const double cutoff = bound(p.cutoff, 30, 20000, 20000),
                   res = bound(p.resonance, 0, .95),
                   morph = bound(p.morph, 0, 1);
      if (cutoff < 19999 || res > 0 || morph > 0) {
        const double g = std::tan(std::numbers::pi *
                                  std::min(cutoff, rate_ * .4) / rate_),
                     k = 2 - 1.9 * res, h = 1 / (1 + g * (g + k));
        const double g2 = std::tan(
                         std::numbers::pi *
                         std::min(cutoff * (1.25 + 2.75 * morph), rate_ * .4) /
                         rate_),
                     h2 = 1 / (1 + g2 * (g2 + k));
        for (int ch = 0; ch < 2; ++ch) {
          const double band = h * (ic1_[ch] + g * (wet[ch] - ic2_[ch])),
                       low = ic2_[ch] + g * band;
          ic1_[ch] = 2 * band - ic1_[ch];
          ic2_[ch] = 2 * low - ic2_[ch];
          const double band2 = h2 * (ic3_[ch] + g2 * (wet[ch] - ic4_[ch])),
                       low2 = ic4_[ch] + g2 * band2;
          ic3_[ch] = 2 * band2 - ic3_[ch];
          ic4_[ch] = 2 * low2 - ic4_[ch];
          const double high = wet[ch] - k * band - low, m = morph * 3;
          const std::array modes{low, (band + .65 * band2) * (1 + .5 * res),
                                 high + low - .5 * band2, high};
          const int region = std::min(2, static_cast<int>(m));
          wet[ch] = bound(
              std::lerp(modes[region], modes[region + 1], m - region), -8, 8);
        }
      }
      if (printing_) {
        if (printAt_ == 0) {
          const auto wanted = static_cast<std::size_t>(std::ceil(span / step));
          if (wanted > capacity_) {
            printing_ = false;
            status_.store(4);
          }
          printLength_ = std::max(std::size_t{2}, wanted);
        }
      }
      if (printing_) {
        auto& destination = bank_[count_.load()];
        for (int ch = 0; ch < 2; ++ch)
          destination.data[ch][printAt_] =
              static_cast<float>(bound(wet[ch], -.98, .98));
        ++printAt_;
        progress_.store(static_cast<float>(printAt_) / printLength_);
        if (printAt_ == printLength_) {
          destination.length = printLength_;
          destination.start = 0;
          publish();
          committed = true;
        }
      }
      if (!committed) cursor_ += step;
      position_.store(static_cast<float>(
          bound((p.reverse ? std::max(a, reverseStart - wrap(cursor_, span))
                           : a + std::min(audioSpan, wrap(cursor_, span))) /
                    b.length,
                0, 1)));
    }
    mix_ += (((c.play || c.repeat) && count_.load() > 0 ? 1 : 0) - mix_) * fade;
    for (int ch = 0; ch < 2; ++ch) {
      const double blended = std::lerp(wet[ch], last_[ch], transition_);
      last_[ch] = blended;
      auto& out = ch == 0 ? l[i] : r[i];
      out = static_cast<float>(bound(out + mix_ * (blended - out), -.98, .98));
    }
    transition_ *= 1 - fade;
  }
}
std::size_t MasterMemory::heapBytes() const noexcept {
  std::size_t result = 0;
  for (const auto& b : bank_)
    for (const auto& channel : b.data)
      result += channel.capacity() * sizeof(float);
  for (const auto& channel : live_.data)
    result += channel.capacity() * sizeof(float);
  return result;
}
}  // namespace aste::click
