#include "circuit.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace aste::contamination::revised {
namespace {
constexpr double tau = 2 * std::numbers::pi;
double bound(double x, double a, double b, double fallback = 0) noexcept {
  return std::isfinite(x) ? std::clamp(x, a, b) : fallback;
}
double random(std::uint64_t& r) noexcept {
  r ^= r << 13;
  r ^= r >> 7;
  r ^= r << 17;
  return static_cast<double>(r >> 11) * 0x1.0p-52 - 1;
}
double relax(double hz, double fs) noexcept {
  return -std::expm1(-tau * hz / fs);
}
Controls clean(Controls c) noexcept {
  c.pressure = bound(c.pressure, 0, 1);
  c.contamination = bound(c.contamination, 0, 1);
  c.instability = bound(c.instability, 0, 1);
  c.memory = bound(c.memory, 0, 1);
  c.motion = bound(c.motion, 0, 1);
  c.colour = bound(c.colour, 80, 8000, 1100);
  c.attack = bound(c.attack, .0005, 4, .003);
  c.decay = bound(c.decay, .01, 8, .25);
  c.sustain = bound(c.sustain, 0, 1);
  c.release = bound(c.release, .005, 12, .2);
  c.detune = bound(c.detune, 0, .01, .003);
  c.pairDepth = bound(c.pairDepth, 0, 1, 1);
  return c;
}
}  // namespace
bool Engine::prepare(double fs, int factor, Architecture architecture) {
  ready_ = false;
  if (!std::isfinite(fs) || fs < 22050 || fs > 192000 ||
      (factor != 1 && factor != 2 && factor != 4 && factor != 8) ||
      architecture < Architecture::Dense || architecture > Architecture::Full)
    return false;
  rate_ = fs;
  factor_ = factor;
  internal_ = fs * factor;
  architecture_ = architecture;
  for (auto& d : delay_)
    d.data.assign(static_cast<std::size_t>(std::ceil(fs * .6)) + 4, 0);
  noiseCoefficient_ = relax(700, internal_);
  driftCoefficient_ = relax(.4, internal_);
  gSlew_ = relax(120, internal_);
  taps_ = static_cast<std::size_t>(32 * factor + 1);
  const double cutoff = .44 / factor;
  double sum = 0;
  for (std::size_t i = 0; i < taps_; ++i) {
    const auto x = static_cast<double>(i) - (taps_ - 1) * .5;
    const double sinc =
        x == 0 ? 2 * cutoff
               : std::sin(tau * cutoff * x) / (std::numbers::pi * x);
    const double window = .42 - .5 * std::cos(tau * i / (taps_ - 1)) +
                          .08 * std::cos(2 * tau * i / (taps_ - 1));
    sum += (fir_[i] = sinc * window);
  }
  for (std::size_t i = 0; i < taps_; ++i) fir_[i] /= sum;
  smoothing_ = -std::expm1(-1 / (.015 * rate_));
  reset();
  ready_ = true;
  return true;
}
void Engine::reset() noexcept {
  voice_ = {};
  historyL_ = {};
  historyR_ = {};
  cursor_ = clock_ = 0;
  shared_ = bias_ = busLow_ = busFast_ = dcL_ = dcR_ = wheel_ = touch_ = bend_ =
      0;
  pedal_ = initialized_ = false;
  observation_ = {};
  coefficientClock_ = 0;
  clearSpace();
  lastWet_ = {};
  clearWet_ = {};
  clearFade_ = 0;
}
void Engine::start(int note, double velocity) noexcept {
  auto* selected = &voice_[0];
  for (auto& v : voice_)
    if (v.stage == 0) {
      selected = &v;
      break;
    }
  if (selected->stage != 0) {
    // Quietest release first, then oldest held voice. Fixed storage, bounded
    // fade.
    for (auto& v : voice_) {
      const bool vr = v.stage == 4, sr = selected->stage == 4;
      if ((vr && !sr) ||
          (vr == sr && (vr ? v.env < selected->env : v.age < selected->age)))
        selected = &v;
    }
    ++observation_.stolen;
  }
  auto& v = *selected;
  const double retained = v.memory, retainedBias = v.bias, last = v.last,
               drift = v.drift;
  const auto rng = v.rng;
  v = {};
  v.note = note;
  v.velocity = velocity;
  v.stage = 1;
  v.down = true;
  v.fresh = true;
  v.age = ++clock_;
  v.rng = static_cast<std::uint64_t>(note + 1) * 0x9e3779b97f4a7c15ULL;
  if (smooth_.instability > 0 && rng != 1) {
    v.rng = rng;
    v.drift = drift;
  }
  {
    v.memory = retained * smooth_.memory;
    v.bias = retainedBias * smooth_.memory;
  }
  v.tail = last;
  v.fade = static_cast<int>(.005 * internal_);
}
void Engine::event(const Event& e) noexcept {
  if (!std::isfinite(e.value)) {
    ++observation_.rejected;
    return;
  }
  switch (e.kind) {
    case Kind::On:
      if (e.note < 0 || e.note > 127) {
        ++observation_.rejected;
        break;
      }
      if (e.value <= 0) {
        event({0, Kind::Off, e.note, 0});
        break;
      }
      start(e.note, bound(e.value, 0, 1));
      break;
    case Kind::Off:
      for (auto& v : voice_)
        if (v.note == e.note && v.down) {
          v.down = false;
          v.pedal = pedal_;
          if (!pedal_) v.stage = 4;
        }
      break;
    case Kind::Pedal:
      pedal_ = e.value >= .5;
      if (!pedal_)
        for (auto& v : voice_)
          if (v.pedal) {
            v.pedal = false;
            v.stage = 4;
          }
      break;
    case Kind::Wheel:
      wheel_ = bound(e.value, 0, 1);
      break;
    case Kind::Touch:
      touch_ = bound(e.value, 0, 1);
      break;
    case Kind::Bend:
      bend_ = bound(e.value, -1, 1) * 2;
      break;
    case Kind::Clear:
      shared_ = bias_ = busLow_ = busFast_ = 0;
      clearWet_ = lastWet_;
      clearFade_ = static_cast<std::size_t>(rate_ * .005);
      clearSpace();
      for (auto& v : voice_) {
        v.memory = v.bias = v.drift = 0;
      }
      break;
    case Kind::Panic:
      reset();
      break;
  }
}
double Engine::tick() noexcept {
  const auto& c = smooth_;
  const double p = std::clamp(c.pressure + .2 * touch_, 0., 1.);
  const bool nonlinear = architecture_ == Architecture::Resonant ||
                         architecture_ == Architecture::Full;
  const double stored =
      interactionEnabled_ && architecture_ == Architecture::Full
          ? spaceLoad_ * c.memory
          : 0;
  const bool update = coefficientClock_ == 0;
  if (update) {
    coefficientClock_ =
        std::max(std::size_t{1}, static_cast<std::size_t>(internal_ * .001));
    releaseCoefficient_ = std::exp(-6.907755 / (c.release * internal_));
    decayCoefficient_ = std::exp(-6.907755 / (c.decay * internal_));
    memoryRetention_ =
        std::exp(-1 / ((.08 + 12 * c.memory * c.memory) * internal_));
  }
  --coefficientClock_;
  double left = 0, right = 0, excitation = 0, signedBus = 0, interaction = 0;
  for (auto& v : voice_) {
    v.memory *= memoryRetention_;
    v.bias *= memoryRetention_;
    if (std::abs(v.memory) < 1.e-24) v.memory = 0;
    if (std::abs(v.bias) < 1.e-24) v.bias = 0;
    if (v.stage == 0) {
      v.last = 0;
      continue;
    }
    if (v.stage == 1) {
      v.env += 1 / (c.attack * internal_);
      if (v.env >= 1) {
        v.env = 1;
        v.stage = 2;
      }
    } else if (v.stage == 2) {
      v.env = c.sustain + (v.env - c.sustain) * decayCoefficient_;
      if (std::abs(v.env - c.sustain) < 1.e-5) v.stage = 3;
    } else if (v.stage == 3)
      v.env = c.sustain;
    else {
      v.env *= releaseCoefficient_;
      if (v.env < 1.e-6) {
        v.env = 0;
        v.stage = 0;
      }
    }
    double white = random(v.rng);
    v.noise += noiseCoefficient_ * (white - v.noise);
    double n = v.noise * std::sqrt((2 - noiseCoefficient_) / noiseCoefficient_);
    v.drift += driftCoefficient_ * (white * 80 - v.drift);
    double operating =
        std::clamp((shared_ + v.memory) * c.memory + stored, 0., 2.);
    double frequency = std::min(
        .12 * internal_,
        440 * std::exp2((v.note - 69 + bend_ + .11 * c.instability * v.drift +
                         .12 * stored * std::sin(v.b)) /
                        12));
    v.a = std::fmod(v.a + tau * frequency / internal_, tau);
    v.b = std::fmod(v.b + tau * frequency *
                              (1 + c.detune * (.55 + .7 * p) +
                               .005 * c.instability * v.drift) /
                              internal_,
                    tau);
    v.sub = std::fmod(v.sub + tau * frequency * .5 / internal_, tau);
    double coupled =
        .22 * std::tanh((1 + 2 * p) * v.previous + .18 * c.contamination * n +
                        .15 * v.bias * c.memory);
    double a = v.a + coupled * (.35 + .35 * p + .12 * operating) +
               .06 * c.contamination * n;
    const double fundamental = std::sin(a);
    double dense = fundamental;
    for (int harmonic = 2; harmonic <= 5; ++harmonic) {
      const double band = std::clamp(
          (.44 * internal_ - frequency * harmonic) / (.04 * internal_), 0., 1.);
      const double amplitude = (harmonic % 2 ? 1. : -1.) * .72 /
                               (harmonic * std::sqrt(double(harmonic)));
      dense += amplitude * band * std::sin(harmonic * a);
    }
    double oscillator = .62 * dense + .26 * std::sin(v.b + .22 * fundamental) +
                        (.14 + .12 * p) * std::sin(v.sub);
    // Noise is multiplied by the harmonic body and enters the driven return,
    // rather than becoming an unrelated additive hiss after the chord.
    double rust = c.contamination * (.025 + .12 * std::abs(oscillator)) * n;
    double drive = 1 + 3.5 * p + 1.5 * c.contamination;
    double bias = .09 * c.contamination *
                  (v.memory + .4 * (interactionEnabled_ &&
                                            architecture_ == Architecture::Full
                                        ? spaceBias_ * c.memory
                                        : 0));
    double feed = (std::tanh(drive * (oscillator + rust + bias)) -
                   std::tanh(drive * bias)) /
                  std::sqrt(1 + .5 * p);
    v.previous += relax(3500, internal_) * (oscillator - v.previous);
    double sample = 0;
    if (nonlinear) {
      if (update || v.fresh) {
        double movement = std::sin(filterPhase_ + v.note * .17) +
                          .5 * c.instability * v.drift;
        double cutoff =
            std::clamp(c.colour * (.5 + 1.15 * v.env) *
                           (1 + .17 * c.motion * movement + .65 * stored),
                       40., internal_ * .08);
        v.gTarget = std::tan(std::numbers::pi * cutoff / internal_);
        if (v.fresh) {
          v.g = v.gTarget;
          v.fresh = false;
        }
      }
      v.g += gSlew_ * (v.gTarget - v.g);
      double damping =
          std::clamp(1.5 - 1.05 * p - .2 * c.motion - .35 * stored, .22, 1.6);
      double g = v.g, denom = 1 / (1 + g * (g + damping));
      double input =
          feed - .16 * p * std::tanh(v.ic2) + .02 * c.contamination * n;
      double v3 = input - v.ic2, v1 = denom * v.ic1 + g * denom * v3,
             v2 = v.ic2 + g * denom * v.ic1 + g * g * denom * v3;
      double i1 = 2 * v1 - v.ic1, i2 = 2 * v2 - v.ic2;
      if (std::abs(i1) > 6 || std::abs(i2) > 6) ++observation_.safety;
      v.ic1 = std::clamp(i1, -6., 6.);
      v.ic2 = std::clamp(i2, -6., 6.);
      sample = (.84 * v2 + .08 * p * v1);
    } else {
      double k = relax(
          std::min(.08 * internal_, c.colour * (.55 + 1.4 * v.env)), internal_);
      v.bp += k * (feed - v.bp);
      v.lp += k * (v.bp - v.lp);
      sample = v.lp;
    }
    sample *= v.env * v.velocity;
    if (v.fade > 0) {
      sample += v.tail * v.fade / (.005 * internal_);
      --v.fade;
    }
    v.last = sample;
    double pan = (v.note % 7 - 3) * .045;
    left += sample * (.707 - pan);
    right += sample * (.707 + pan);
    excitation += std::abs(sample);
    signedBus += sample;
    interaction += std::abs(coupled) * v.env;
    if (std::abs(v.ic1) < 1.e-24) v.ic1 = 0;
    if (std::abs(v.ic2) < 1.e-24) v.ic2 = 0;
    if (std::abs(v.memory) < 1.e-24) v.memory = 0;
    if (std::abs(v.bias) < 1.e-24) v.bias = 0;
    v.memory += relax(3, internal_) * std::abs(sample);
    v.bias += relax(1, internal_) * std::tanh(sample);
  }
  busLow_ += relax(120, internal_) * (signedBus / voices - busLow_);
  busFast_ += relax(4000, internal_) * (signedBus / voices - busFast_);
  shared_ =
      std::min(2., shared_ * memoryRetention_ +
                       relax(2, internal_) * (excitation / voices +
                                              std::abs(busFast_ - busLow_)));
  bias_ = std::clamp(bias_ * memoryRetention_ + relax(.7, internal_) * busLow_,
                     -1., 1.);
  left = std::tanh(left * .24);
  right = std::tanh(right * .24);
  dcL_ += relax(15, internal_) * (left - dcL_);
  dcR_ += relax(15, internal_) * (right - dcR_);
  observation_.energy = excitation / voices;
  observation_.memory = shared_;
  observation_.bias = bias_;
  observation_.interaction = interaction / voices;
  if (std::abs(dcL_) < 1.e-24) dcL_ = 0;
  if (std::abs(dcR_) < 1.e-24) dcR_ = 0;
  outR_ = right - dcR_;
  return left - dcL_;
}
float Engine::Delay::read(double age) const noexcept {
  if (valid < 2 || age >= valid) return 0;
  double pos = double(write) - age;
  if (pos < 0) pos += data.size();
  auto i = static_cast<std::size_t>(pos);
  auto next = i + 1 == data.size() ? 0 : i + 1;
  float frac = static_cast<float>(pos - i);
  return data[i] + frac * (data[next] - data[i]);
}
void Engine::Delay::push(float x) noexcept {
  data[write] = std::abs(x) < 1.e-25F ? 0 : x;
  write = (write + 1) % data.size();
  valid = std::min(valid + 1, data.size() - 1);
}
void Engine::clearSpace() noexcept {
  for (auto& d : delay_) d.clear();
  spaceLoad_ = sourceLoad_ = spaceNoise_ = spaceBias_ = spacePhase_ =
      filterPhase_ = 0;
  networkRandom_ = 101;
}
std::size_t Engine::heapBytes() const noexcept {
  std::size_t n = 0;
  for (const auto& d : delay_) n += d.data.capacity() * sizeof(float);
  return n;
}
std::array<float, 2> Engine::space(float left, float right) noexcept {
  if (architecture_ != Architecture::Feedback &&
      architecture_ != Architecture::Full)
    return {};
  const auto& c = smooth_;
  double inputPower = (left * left + right * right) * .5;
  sourceLoad_ += relax(5, rate_) * (inputPower - sourceLoad_);
  const std::array<double, 4> seconds{.0931, .1377, .2119, .3073};
  const std::array<double, 4> signL{1, 1, -1, 1}, signR{1, -1, 1, 1};
  std::array<double, 4> reads{};
  double sum = 0, power = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    double modulation =
        (.0002 + .0035 * c.instability) * std::sin(spacePhase_ + i * 1.7) +
        .001 * c.motion * std::clamp(sourceLoad_ * 30, 0., 1.) *
            std::sin(spacePhase_ * .7 + i);
    reads[i] = delay_[i].read((seconds[i] + modulation) * rate_);
    sum += reads[i];
    power += reads[i] * reads[i];
  }
  double loading = power / (power + .00005);
  spaceLoad_ += relax(.8, rate_) * (loading - spaceLoad_);
  spaceBias_ += relax(.5, rate_) * (sum * .25 - spaceBias_);
  spaceNoise_ += relax(600, rate_) * (random(networkRandom_) - spaceNoise_);
  double noise = spaceNoise_ * std::sqrt(power) * c.contamination * .035;
  double gain = .32 + .66 * std::sqrt(c.memory), wetL = 0, wetR = 0;
  for (std::size_t i = 0; i < 4; ++i) {
    auto& d = delay_[i];
    double feedback = sum * .5 - reads[i];
    double cutoff =
        std::clamp(c.colour * (.45 + .45 * (1 - c.motion)) *
                       (1 + .4 * std::clamp(sourceLoad_ * 20, 0., 1.)),
                   120., 6000.);
    d.low += relax(cutoff, rate_) * (feedback - d.low);
    d.high += relax(16 + 35 * c.motion, rate_) * (d.low - d.high);
    if (std::abs(d.low) < 1.e-24) d.low = 0;
    if (std::abs(d.high) < 1.e-24) d.high = 0;
    double returned = d.low - d.high;
    double drive = 1 + 2 * c.contamination + .6 * c.pressure;
    double bias = .08 * c.contamination * spaceBias_;
    double shaped = (std::tanh(drive * (returned + bias + noise)) -
                     std::tanh(drive * bias)) /
                    drive;
    double injection = (left * signL[i] + right * signR[i]) * .28;
    double value = shaped * gain + injection;
    d.push(static_cast<float>(2 * std::tanh(value * .5)));
    wetL += reads[i] * signL[i];
    wetR += reads[i] * signR[i];
  }
  spacePhase_ += tau * (.04 + .12 * c.instability + .08 * c.motion) / rate_;
  if (spacePhase_ >= tau) spacePhase_ -= tau;
  observation_.spaceEnergy = power * .25;
  std::array<float, 2> result{static_cast<float>(wetL * .58 * c.memory),
                              static_cast<float>(wetR * .58 * c.memory)};
  if (clearFade_ > 0) {
    float blend = static_cast<float>(clearFade_ / (rate_ * .005));
    for (std::size_t i = 0; i < 2; ++i)
      result[i] = result[i] * (1 - blend) + clearWet_[i] * blend;
    --clearFade_;
  }
  lastWet_ = result;
  return result;
}
void Engine::process(float* l, float* r, std::size_t frames,
                     const Controls& input, std::span<const Event> events,
                     bool hearSpace) noexcept {
  const auto target = clean(input);
  if (!initialized_) {
    smooth_ = target;
    initialized_ = true;
  }
  std::size_t e = 0;
  observation_.peak = 0;
  for (std::size_t i = 0; i < frames; ++i) {
    while (e < events.size() && events[e].offset <= i) event(events[e++]);
    if (!ready_) {
      l[i] = r[i] = 0;
      continue;
    }
    const auto approach = [&](double& x, double y) {
      x += smoothing_ * (y - x);
      if (std::abs(x - y) < 1e-9) x = y;
    };
    approach(smooth_.pressure, target.pressure);
    approach(smooth_.contamination, target.contamination);
    approach(smooth_.instability, target.instability);
    approach(smooth_.memory, target.memory);
    approach(smooth_.motion, target.motion);
    approach(smooth_.colour, target.colour);
    approach(smooth_.attack, target.attack);
    approach(smooth_.decay, target.decay);
    approach(smooth_.sustain, target.sustain);
    approach(smooth_.release, target.release);
    approach(smooth_.detune, target.detune);
    approach(smooth_.pairDepth, target.pairDepth);
    filterPhase_ += tau * (.025 + .13 * smooth_.motion) / rate_;
    if (filterPhase_ >= tau) filterPhase_ -= tau;
    for (int k = 0; k < factor_; ++k) {
      historyL_[cursor_] = tick();
      historyR_[cursor_] = outR_;
      cursor_ = (cursor_ + 1) % taps_;
    }
    double a = 0, b = 0;
    auto index = (cursor_ + taps_ - 1) % taps_;
    for (std::size_t j = 0; j < taps_; ++j) {
      a += fir_[j] * historyL_[index];
      b += fir_[j] * historyR_[index];
      index = index == 0 ? taps_ - 1 : index - 1;
    }
    observation_.dryPeak = std::max(std::abs(a), std::abs(b));
    const auto wet = space(static_cast<float>(a), static_cast<float>(b));
    if (hearSpace) {
      a += wet[0];
      b += wet[1];
    }
    a = std::clamp(a, -2., 2.);
    b = std::clamp(b, -2., 2.);
    l[i] = static_cast<float>(a);
    r[i] = static_cast<float>(b);
    observation_.peak = std::max({observation_.peak, std::abs(a), std::abs(b)});
  }
}
}  // namespace aste::contamination::revised
