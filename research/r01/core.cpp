#include "core.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <numbers>

namespace aste::r01 {
namespace {
constexpr double pi = std::numbers::pi, log001 = -6.907755278982137;
bool range(double x, double lo, double hi) noexcept {
  return std::isfinite(x) && x >= lo && x <= hi;
}
std::uint64_t hash(std::uint64_t x) noexcept {
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebULL;
  return x ^ (x >> 31);
}
double noise(std::uint64_t x) noexcept {
  return static_cast<double>(hash(x) >> 40) / 8388608.0 - 1;
}
bool validChord(const Chord& c) noexcept {
  if (!range(c.root, 24, 100) || c.count < 1 || c.count > kTones) return false;
  for (std::size_t t = 0; t < c.count; ++t)
    if (!range(c.intervals[t], -24, 24) || !range(c.cents[t], -100, 100) ||
        !range(c.root + c.intervals[t], 0, 127))
      return false;
  return true;
}
}  // namespace

bool Exciter::prepare(double rate) noexcept {
  if (!range(rate, 44100, 192000)) return false;
  rate_ = rate;
  length_ = age_ = 0;
  return true;
}
void Exciter::trigger(Excitation type, std::uint64_t seed,
                      std::size_t frames) noexcept {
  type_ = type;
  random_ = seed;
  length_ = frames;
  age_ = clock_ = 0;
  filter_ = 0;
  for (auto& row : rows_) row = static_cast<float>(noise(++random_));
}
float Exciter::next() noexcept {
  if (age_ >= length_) return 0;
  const auto age = age_++;
  if (type_ == Excitation::Impulse) return age == 0 ? 1 : 0;
  double value = noise(++random_);
  if (type_ == Excitation::Pink) {
    const auto row = std::min<unsigned>(15, std::countr_zero(++clock_));
    rows_[row] = static_cast<float>(value);
    value = noise(++random_);
    for (auto x : rows_) value += x;
    value /= 4.2;  // Voss-style finite octave sum; not an exact 1/f filter.
  } else if (type_ == Excitation::Shaped) {
    filter_ += (1 - std::exp(-2 * pi * 1800 / rate_)) * (value - filter_);
    value = filter_;
  }
  double phase =
      static_cast<double>(age) / std::max<std::size_t>(1, length_ - 1);
  if (type_ == Excitation::Granular) {
    phase = std::fmod(phase * 8, 1.0);
    if (phase > .5)
      return 0;  // Eight short noise fragments, no imported sample.
    phase *= 2;
  }
  return static_cast<float>(value * std::sin(pi * phase) *
                            std::sin(pi * phase));
}

void Engine::String::clear() noexcept {
  write = available = 0;
  lowpass = allpass = energy = energyCorrection = 0;
}
double Engine::String::read(double age) const noexcept {
  const auto whole = static_cast<std::size_t>(age);
  auto sample = [&](std::size_t n) -> double {
    if (n > available) return 0;
    return data[(write + data.size() - n) % data.size()];
  };
  const double fraction = age - whole;
  return sample(whole) * (1 - fraction) + sample(whole + 1) * fraction;
}
bool Engine::validControls(const Controls& c) const noexcept {
  if (!range(c.t60, .03, 120) || !range(c.damping, 0, .8) ||
      !range(c.couplingHz, 0, 20) || !range(c.stretch, 0, .3) ||
      !range(c.inharmonic, 0, 1) || !range(c.dispersion, 0, .8) ||
      !range(c.partialRetain, 0, 1) || !range(c.mutationCents, 0, 50))
    return false;
  for (auto d : c.modeDecay)
    if (!range(d, .1, 4) ||
        (architecture_ == Architecture::Waveguide && d != 1))
      return false;
  if (architecture_ == Architecture::Waveguide &&
      (c.couplingHz != 0 || c.stretch != 0 || c.inharmonic != 0))
    return false;
  return true;
}
bool Engine::prepare(double rate, Architecture architecture, Memory memory,
                     const Controls& controls) {
  prepared_ = false;
  architecture_ = architecture;
  memory_ = memory;
  if (!range(rate, 44100, 192000) || architecture > Architecture::Waveguide ||
      architecture < Architecture::Modal || memory > Memory::Overlap ||
      memory < Memory::Reset ||
      (architecture == Architecture::Waveguide && memory == Memory::Partial) ||
      !validControls(controls))
    return false;
  rate_ = rate;
  controls_ = controls;
  active_ = strikes_ = 0;
  counters_ = {};
  held_.fill(false);
  released_ = false;
  for (auto& bank : banks_) {
    for (auto& string : bank.strings)
      string.data.assign(
          architecture == Architecture::Waveguide
              ? static_cast<std::size_t>(std::ceil(rate / 16)) + 8
              : 0,
          0);
    clear(bank);
    bank.chord = {};
  }
  banks_[0].active = true;
  tune(banks_[0]);
  prepared_ = true;
  return true;
}
void Engine::clear(Bank& b) noexcept {
  for (auto& m : b.modes) m.x = m.y = 0;
  for (auto& s : b.strings) s.clear();
  b.active = false;
}
void Engine::tune(Bank& b) noexcept {
  couplingCos_ = std::cos(2 * pi * controls_.couplingHz / rate_);
  couplingSin_ = std::sin(2 * pi * controls_.couplingHz / rate_);
  for (std::size_t t = 0; t < kTones; ++t) {
    const double mutation =
        noise(controls_.seed ^ hash(strikes_ + 1) ^ hash(t + 9)) *
        controls_.mutationCents;
    const double root =
        440 * std::exp2((b.chord.root + b.chord.intervals[t] - 69 +
                         (b.chord.cents[t] + mutation) / 100) /
                        12);
    for (std::size_t k = 0; k < kPartials; ++k) {
      auto& m = b.modes[t * kPartials + k];
      const double ratio = std::pow(k + 1, 1 + controls_.stretch) *
                           std::sqrt(1 + controls_.inharmonic * .12 * k * k);
      const double frequency = root * ratio;
      const bool audible = t < b.chord.count && frequency < .4 * rate_;
      m.frequency = audible ? frequency : 0;
      const double angle = 2 * pi * m.frequency / rate_;
      m.cosine = std::cos(angle);
      m.sine = std::sin(angle);
      const double seconds =
          (released_ ? std::min(.3, controls_.t60) : controls_.t60) *
          controls_.modeDecay[t * kPartials + k] /
          (1 + controls_.damping * k * k);
      m.radius = controls_.persist && !released_
                     ? 1
                     : std::exp(log001 / (rate_ * seconds));
      if (!audible) m.x = m.y = 0;
    }
    if (architecture_ == Architecture::Waveguide) {
      auto& s = b.strings[t];
      const double w = 2 * pi * root / rate_, d = controls_.damping,
                   a = controls_.dispersion;
      const double lpPhase = std::atan2(d * std::sin(w), 1 - d * std::cos(w));
      const double apPhase = std::atan2(-std::sin(w), a + std::cos(w)) -
                             std::atan2(-a * std::sin(w), 1 + a * std::cos(w));
      s.delay = std::clamp(rate_ / root - (lpPhase - apPhase) / w, 2.0,
                           static_cast<double>(s.data.size() - 2));
      s.coefficient = a;
      const double seconds =
          released_ ? std::min(.3, controls_.t60) : controls_.t60;
      s.gain = controls_.persist && !released_
                   ? .999999
                   : std::exp(log001 / (root * seconds));
      if (t >= b.chord.count) s.clear();
    }
  }
}
void Engine::changeChord(const Chord& chord, bool strike) noexcept {
  if (!validChord(chord)) {
    ++counters_.rejected;
    return;
  }
  if (architecture_ == Architecture::Waveguide) {
    for (std::size_t t = 0; t < chord.count; ++t) {
      const double frequency =
          440 * std::exp2((chord.root + chord.intervals[t] - 69 +
                           chord.cents[t] / 100) /
                          12);
      if (frequency < 17 || frequency >= .4 * rate_) {
        ++counters_.rejected;
        return;
      }
    }
  }
  if (strike) {
    released_ = false;
    ++strikes_;
    if (memory_ == Memory::Overlap) {
      active_ = 1 - active_;
      if (banks_[active_].active) ++counters_.stolen;
      clear(banks_[active_]);
    } else if (memory_ == Memory::Reset)
      clear(banks_[active_]);
    else if (memory_ == Memory::Partial) {
      const double keep = std::sqrt(controls_.partialRetain);
      for (auto& m : banks_[active_].modes) {
        m.x *= keep;
        m.y *= keep;
      }
    }
  }
  auto& b = banks_[active_];
  b.active = true;
  b.chord = chord;
  tune(b);
}
void Engine::eventsAt(std::span<const Event> events) noexcept {
  Chord chord = banks_[active_].chord;
  bool change{}, strike{}, release{}, panic{};
  for (const auto& e : events) {
    if (e.type == EventType::Panic) {
      panic = true;
      continue;
    }
    if (e.type == EventType::Release) {
      release = true;
      continue;
    }
    if (e.type == EventType::Strike || e.type == EventType::Retune) {
      if (!validChord(e.chord)) {
        ++counters_.rejected;
        continue;
      }
      chord = e.chord;
      change = true;
      strike |= e.type == EventType::Strike;
    } else if (e.type == EventType::NoteOn || e.type == EventType::NoteOff) {
      if (e.note < 24 || e.note > 100) {
        ++counters_.rejected;
        continue;
      }
      const auto note = static_cast<std::size_t>(e.note);
      if (e.type == EventType::NoteOn && !held_[note] &&
          std::count(held_.begin(), held_.end(), true) ==
              static_cast<int>(kTones)) {
        ++counters_.rejected;
        continue;
      }
      held_[note] = e.type == EventType::NoteOn;
      std::array<int, kTones> notes{};
      std::size_t count{};
      for (std::size_t n = 0; n < held_.size(); ++n)
        if (held_[n]) notes[count++] = static_cast<int>(n);
      if (count == 0) {
        release = true;
        continue;
      }
      if (notes[count - 1] - notes[0] > 24) {
        held_[note] = false;
        ++counters_.rejected;
        continue;
      }
      chord = {};
      chord.root = notes[0];
      chord.count = count;
      for (std::size_t n = 0; n < count; ++n)
        chord.intervals[n] = notes[n] - notes[0];
      change = true;
      strike |= e.type == EventType::NoteOn;
      if (e.type == EventType::NoteOn) release = false;
    }
  }
  if (panic) {
    for (auto& b : banks_) clear(b);
    held_.fill(false);
    return;
  }
  if (change) changeChord(chord, strike);
  if (release) {
    released_ = true;
    for (auto& b : banks_)
      if (b.active) tune(b);
  }
}
double Engine::modal(Bank& b, double input, double& right) noexcept {
  for (auto& m : b.modes) {
    if (m.frequency == 0) continue;
    const double x = m.x;
    m.x = m.radius * (m.cosine * x - m.sine * m.y) + input / std::sqrt(kModes);
    m.y = m.radius * (m.sine * x + m.cosine * m.y);
  }
  // Orthogonal rotations of both quadratures transfer energy between tones.
  if (controls_.couplingHz > 0)
    for (std::size_t i = 0; i + kPartials < kModes; ++i) {
      auto& a = b.modes[i];
      auto& z = b.modes[i + kPartials];
      if (a.frequency == 0 || z.frequency == 0) continue;
      const double x = a.x, y = a.y;
      a.x = couplingCos_ * x - couplingSin_ * z.x;
      z.x = couplingSin_ * x + couplingCos_ * z.x;
      a.y = couplingCos_ * y - couplingSin_ * z.y;
      z.y = couplingSin_ * y + couplingCos_ * z.y;
    }
  double energy{};
  for (const auto& m : b.modes) energy += m.x * m.x + m.y * m.y;
  const double scale = energy > 16 ? 4 / std::sqrt(energy) : 1;
  if (scale < 1) ++counters_.safetyInterventions;
  double left{};
  right = 0;
  for (std::size_t i = 0; i < kModes; ++i) {
    auto& m = b.modes[i];
    m.x *= scale;
    m.y *= scale;
    if (std::abs(m.x) < 1e-30) m.x = 0;
    if (std::abs(m.y) < 1e-30) m.y = 0;
    left += m.x / std::sqrt(kModes);
    right += m.x * (i % 2 ? .7 : 1) / std::sqrt(kModes);
  }
  return left;
}
double Engine::waveguide(Bank& b, double input, double& right) noexcept {
  double left{};
  right = 0;
  for (std::size_t t = 0; t < b.chord.count; ++t) {
    auto& s = b.strings[t];
    const double read = s.read(s.delay);
    s.lowpass = (1 - controls_.damping) * read + controls_.damping * s.lowpass;
    const double dispersed = s.coefficient * s.lowpass + s.allpass;
    s.allpass = s.lowpass - s.coefficient * dispersed;
    double value = s.gain * dispersed + input / 2;
    if (std::abs(value) > 4) {
      value = std::clamp(value, -4.0, 4.0);
      ++counters_.safetyInterventions;
    }
    if (std::abs(value) < 1e-30) value = 0;
    const auto old = s.available == s.data.size() ? s.data[s.write] : 0;
    const auto stored = static_cast<float>(value);
    // Compensate each term separately: computing new^2 - old^2 first would
    // already lose small energy contributions before compensation can help.
    for (double term : {-old * static_cast<double>(old),
                        stored * static_cast<double>(stored)}) {
      const double total = s.energy + term;
      s.energyCorrection += std::abs(s.energy) >= std::abs(term)
                                ? (s.energy - total) + term
                                : (term - total) + s.energy;
      s.energy = total;
    }
    s.data[s.write] = stored;
    s.write = (s.write + 1) % s.data.size();
    s.available = std::min(s.available + 1, s.data.size());
    left += read / 2;
    right += read * (t % 2 ? .7 : 1) / 2;
  }
  return left;
}
void Engine::process(const float* input, float* left, float* right,
                     std::size_t frames, const Controls& c,
                     std::span<const Event> events) noexcept {
  if (!left || frames == 0) return;
  if (!prepared_) {
    std::fill_n(left, frames, 0);
    if (right) std::fill_n(right, frames, 0);
    return;
  }
  if (!(c == controls_)) {
    if (!validControls(c))
      ++counters_.rejected;
    else {
      controls_ = c;
      for (auto& b : banks_)
        if (b.active) tune(b);
    }
  }
  bool valid = events.size() <= 64;
  for (std::size_t i = 0; i < events.size() && i < 64; ++i)
    valid &= events[i].offset < frames &&
             (i == 0 || events[i].offset >= events[i - 1].offset) &&
             events[i].type >= EventType::Strike &&
             events[i].type <= EventType::Panic;
  if (!valid) {
    ++counters_.rejected;
    events = {};
  }
  std::size_t event{};
  for (std::size_t i = 0; i < frames; ++i) {
    const auto begin = event;
    while (event < events.size() && events[event].offset == i) ++event;
    if (event != begin) eventsAt(events.subspan(begin, event - begin));
    double drive = input ? input[i] : 0;
    if (!std::isfinite(drive)) {
      drive = 0;
      ++counters_.sanitized;
    }
    if (std::abs(drive) > 1) {
      drive = std::clamp(drive, -1.0, 1.0);
      ++counters_.sanitized;
    }
    double l{}, r{};
    for (std::size_t b = 0; b < banks_.size(); ++b)
      if (banks_[b].active) {
        double side{};
        const double excitation = b == active_ ? drive : 0;
        l += architecture_ == Architecture::Modal
                 ? modal(banks_[b], excitation, side)
                 : waveguide(banks_[b], excitation, side);
        r += side;
      }
    left[i] = static_cast<float>(l * .1);
    if (right) right[i] = static_cast<float>(r * .1);
    counters_.peak =
        std::max({counters_.peak, std::abs(l * .1), std::abs(r * .1)});
  }
}
Snapshot Engine::snapshot() const noexcept {
  auto result = counters_;
  result.activeBank = active_;
  for (std::size_t bank = 0; bank < banks_.size(); ++bank) {
    const auto& b = banks_[bank];
    if (b.active) {
      for (std::size_t i = 0; i < kModes; ++i) {
        const auto& m = b.modes[i];
        const double energy =
            architecture_ == Architecture::Modal
                ? m.x * m.x + m.y * m.y
                : (i % kPartials == 0
                       ? std::max(0.0,
                                  b.strings[i / kPartials].energy +
                                      b.strings[i / kPartials].energyCorrection)
                       : 0);
        result.bankEnergies[bank][i] = energy;
        result.bankFrequencies[bank][i] =
            architecture_ == Architecture::Modal || i % kPartials == 0
                ? m.frequency
                : 0;
        if (bank == active_) result.energies[i] = energy;
        result.energy += energy;
      }
    }
  }
  for (std::size_t i = 0; i < kModes; ++i)
    result.frequencies[i] = result.bankFrequencies[active_][i];
  return result;
}
std::size_t Engine::heapBytes() const noexcept {
  std::size_t bytes{};
  for (const auto& b : banks_)
    for (const auto& s : b.strings) bytes += s.data.size() * sizeof(float);
  return bytes;
}
}  // namespace aste::r01
