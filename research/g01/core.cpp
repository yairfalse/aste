#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <numbers>
namespace aste::g01 {
namespace {
constexpr double pi = std::numbers::pi;
float bound(float v, float lo, float hi, float fallback,
            std::uint64_t& count) noexcept {
  if (!std::isfinite(v)) {
    ++count;
    return fallback;
  }
  auto b = std::clamp(v, lo, hi);
  if (b != v) ++count;
  return b;
}
double wrap(double v, double length) noexcept {
  return v - std::floor(v / length) * length;
}
float safe(float v, std::uint64_t& count) noexcept {
  if (!std::isfinite(v)) {
    ++count;
    return 0;
  }
  return std::clamp(v, -8.F, 8.F);
}
}  // namespace
Controls Engine::sanitize(Controls c, std::uint64_t& count) noexcept {
  for (auto& n : c.node) {
#define B(F, L, H, D) n.F = bound(n.F, L, H, D, count)
    B(sizeMs, 1, 2000, 120);
    B(density, 0, 240, 16);
    B(speed, 0, 8, 1);
    B(pitch, -24, 24, 0);
    B(position, 0, 1, 0);
    B(variation, 0, 1, 0);
    B(temporalSpread, 0, 1, 0);
    B(stereoSpread, 0, 1, 0);
    B(width, 0, 1, 1);
    B(pan, -1, 1, 0);
    B(panDrift, 0, 1, 0);
    B(erosion, 0, 1, 0);
    B(inheritance, 0, 1, 1);
    B(level, 0, 1, 0.65F);
    B(captureSeconds, .02F, 4, .5F);
    B(accumulate, 0, .995F, 0);
    B(division, 1.F / 64, 4, .25F);
    B(cluster, 0, 1, 0);
#undef B
    if (n.mode > TimeMode::Varispeed || n.mode < TimeMode::Independent) {
      n.mode = TimeMode::Independent;
      ++count;
    }
    if (n.window > Window::Fragment || n.window < Window::Hann) {
      n.window = Window::Hann;
      ++count;
    }
    if (n.interpolation > Interpolation::Sinc16 ||
        n.interpolation < Interpolation::Linear) {
      n.interpolation = Interpolation::Linear;
      ++count;
    }
  }
  for (auto& row : c.routes)
    for (auto& x : row) x = bound(x, 0, 1.5F, 0, count);
  for (auto& x : c.live) x = bound(x, 0, 1.5F, 0, count);
  c.time = bound(c.time, -1, 1, 0, count);
  c.spread = bound(c.spread, 0, 1, 0, count);
  c.erosion = bound(c.erosion, 0, 1, 0, count);
  c.inheritance = bound(c.inheritance, 0, 1, 1, count);
  c.master = bound(c.master, 0, 1, 0.8F, count);
  c.mix = bound(c.mix, 0, 1, 1, count);
  return c;
}
bool Engine::prepare(double rate, double seconds) {
  prepared_ = false;
  if (!std::isfinite(rate) || rate < 44100 || rate > 192000 ||
      !std::isfinite(seconds) || seconds < .02 || seconds > 4)
    return false;
  rate_ = rate;
  capacity_ = static_cast<std::size_t>(std::ceil(rate * seconds));
  for (auto& n : node_)
    for (auto& b : n.bank)
      for (auto& channel : b.data) channel.assign(capacity_, 0);
  for (std::size_t i = 0; i < hann_.size(); ++i)
    hann_[i] = static_cast<float>(.5 - .5 * std::cos(2 * pi * i / 2048));
  for (unsigned f = 0; f < 6; ++f)
    for (unsigned phase = 0; phase < 64; ++phase) {
      double sum = 0, cutoff = std::exp2(-double(f));
      for (int tap = 0; tap < 16; ++tap) {
        double x = tap - 7 - phase / 64.;
        double z = pi * x * cutoff;
        double w = .5 + .5 * std::cos(pi * x / 8);
        double h = (std::abs(z) < 1e-12 ? 1 : std::sin(z) / z) * cutoff * w;
        sinc_[f][phase][tap] = static_cast<float>(h);
        sum += h;
      }
      for (auto& h : sinc_[f][phase]) h = static_cast<float>(h / sum);
    }
  routeSmooth_ = 1 - std::exp(-1 / (rate * .015));
  levelSmooth_ = 1 - std::exp(-1 / (rate * .01));
  prepared_ = true;
  reset();
  return true;
}
void Engine::clear(std::size_t index) noexcept {
  if (index >= nodes) return;
  auto& n = node_[index];
  for (auto& g : n.grains) g = {};
  for (auto& b : n.bank) {
    b.length = b.refs = 0;
    b.power = 0;
    b.generation = 0;
  }
  n.published = -1;
  n.recording = 0;
  n.written = 0;
  n.writePower = 0;
  n.cursor = 0;
  n.phase = 1;
  n.fade = 0;
  n.previousFreeze = false;
  n.previousCapture = true;
  n.previousSync = false;
  n.previousDivision = .25F;
  n.meter = {};
  previous_[index] = {};
}
void Engine::reset() noexcept {
  meter_ = {};
  for (std::size_t i = 0; i < nodes; ++i) {
    clear(i);
    node_[i].last = {};
    node_[i].random = 1 + i * 7919;
    node_[i].previousPosition = 0;
    node_[i].drift = 0;
  }
  routes_ = {};
  live_ = {};
  levels_ = {};
  previous_ = {};
  clock_ = 0;
  initialized_ = false;
  transportKnown_ = wasPlaying_ = false;
  expectedPpq_ = 0;
}
std::size_t Engine::heapBytes() const noexcept {
  std::size_t total = 0;
  for (const auto& n : node_)
    for (const auto& b : n.bank)
      for (const auto& c : b.data) total += c.capacity() * sizeof(float);
  return total;
}
float Engine::random(Node& n) noexcept {
  auto x = n.random;
  x ^= x >> 12;
  x ^= x << 25;
  x ^= x >> 27;
  n.random = x;
  return static_cast<float>((x * 2685821657736338717ULL) >> 40) / 8388608.F - 1;
}
bool Engine::commit(std::size_t index) noexcept {
  auto& n = node_[index];
  if (n.written == 0) return false;
  int free = -1;
  for (unsigned i = 0; i < 3; ++i)
    if (i != n.recording && n.bank[i].refs == 0) {
      free = static_cast<int>(i);
      if (static_cast<int>(i) != n.published) break;
    }
  if (free < 0) {
    ++n.meter.captureStalls;
    return false;
  }
  auto& b = n.bank[n.recording];
  b.length = n.written;
  b.power = n.writePower / (n.written * 2);
  b.generation = ++n.meter.commits;
  n.published = static_cast<int>(n.recording);
  n.recording = static_cast<unsigned>(free);
  n.written = 0;
  n.writePower = 0;
  n.bank[n.recording].length = 0;
  n.meter.generation = b.generation;
  return true;
}
float Engine::read(const Bank& b, unsigned channel, double pos,
                   Interpolation mode, unsigned filter) const noexcept {
  pos = wrap(pos, static_cast<double>(b.length));
  const auto i = static_cast<std::size_t>(pos);
  const float fraction = static_cast<float>(pos - i);
  if (mode == Interpolation::Linear)
    return b.data[channel][i] +
           fraction *
               (b.data[channel][(i + 1) % b.length] - b.data[channel][i]);
  const auto phase = std::min(63U, static_cast<unsigned>(fraction * 64));
  double value = 0;
  for (int t = 0; t < 16; ++t) {
    auto index = static_cast<std::int64_t>(i) + t - 7;
    index %= static_cast<std::int64_t>(b.length);
    if (index < 0) index += b.length;
    value += b.data[channel][static_cast<std::size_t>(index)] *
             sinc_[filter][phase][t];
  }
  return static_cast<float>(value);
}
void Engine::launch(std::size_t index, const NodeControls& c) noexcept {
  auto& n = node_[index];
  if (n.published < 0) return;
  auto slot = std::find_if(n.grains.begin(), n.grains.end(),
                           [](const Grain& g) { return !g.active; });
  if (slot == n.grains.end()) {
    ++n.meter.dropped;
    return;
  }
  auto& b = n.bank[static_cast<unsigned>(n.published)];
  if (!b.length) return;
  Grain g;
  g.active = true;
  g.bank = static_cast<unsigned>(n.published);
  g.length = std::max(std::size_t{2},
                      static_cast<std::size_t>(c.sizeMs * .001 * rate_));
  double pitch = c.pitch + random(n) * 12 * (1 - c.inheritance);
  g.step = std::exp2(pitch / 12) *
           (c.mode == TimeMode::Varispeed ? c.speed : 1) * (c.reverse ? -1 : 1);
  g.filter = static_cast<unsigned>(
      std::clamp(std::ceil(std::log2(std::max(1., std::abs(g.step)))), 0., 5.));
  g.cursor =
      wrap(n.cursor +
               b.length *
                   (c.position +
                    random(n) * (c.variation + .2F * (1 - c.inheritance)) * .5),
           static_cast<double>(b.length));
  g.pan = std::clamp(c.pan + random(n) * c.stereoSpread +
                         c.panDrift * static_cast<float>(std::sin(n.drift)),
                     -1.F, 1.F);
  g.width = c.width;
  g.erosion = c.erosion;
  g.inheritance = c.inheritance;
  g.window = c.window;
  g.interpolation = c.interpolation;
  *slot = g;
  ++b.refs;
  ++n.meter.launched;
}
Stereo Engine::render(std::size_t index, const NodeControls&) noexcept {
  auto& n = node_[index];
  Stereo out{};
  double weights = 0;
  std::size_t active = 0;
  for (auto& g : n.grains) {
    if (!g.active) continue;
    ++active;
    const double phase = double(g.age) / double(g.length - 1);
    float window = 1;
    if (g.window == Window::Hann) {
      double p = phase * 2048;
      auto i = std::min(std::size_t{2047}, static_cast<std::size_t>(p));
      window = hann_[i] + static_cast<float>(p - i) * (hann_[i + 1] - hann_[i]);
    } else if (g.window == Window::Triangle)
      window = static_cast<float>(1 - std::abs(2 * phase - 1));
    else {
      const double edge =
          std::max(1., std::min(rate_ * .00015, g.length * .05));
      window = static_cast<float>(
          std::min({1., g.age / edge, (g.length - 1 - g.age) / edge}));
    }
    const auto& b = n.bank[g.bank];
    Stereo source{read(b, 0, g.cursor, g.interpolation, g.filter),
                  read(b, 1, g.cursor, g.interpolation, g.filter)};
    for (unsigned ch = 0; ch < 2; ++ch) {
      g.low[ch] += (.02F + .78F * (1 - g.erosion)) * (source[ch] - g.low[ch]);
      source[ch] += g.erosion * (g.low[ch] - source[ch]);
      if (std::abs(g.low[ch]) < 1.e-25F) g.low[ch] = 0;
      if (g.erosion > 0)
        source[ch] += g.erosion * (std::tanh(source[ch] * (1 + 3 * g.erosion)) /
                                       (1 + 3 * g.erosion) -
                                   source[ch]);
    }
    float mid = (source[0] + source[1]) * .5F,
          side = (source[0] - source[1]) * .5F * g.width;
    // Stereo balance preserves a centred stereo source; mono sum cannot cancel
    // solely because a grain was positioned away from centre.
    float l = mid + side, r = mid - side;
    l *= 1 - std::max(0.F, g.pan);
    r *= 1 + std::min(0.F, g.pan);
    out[0] += l * window;
    out[1] += r * window;
    weights += window;
    g.cursor = wrap(g.cursor + g.step, static_cast<double>(b.length));
    if (++g.age >= g.length) {
      g.active = false;
      --n.bank[g.bank].refs;
    }
  }
  const float normalization = static_cast<float>(1 / std::max(1., weights));
  for (auto& x : out) x = std::clamp(x * normalization, -2.F, 2.F);
  n.meter.active = active;
  n.fade = std::min(1.F, n.fade + static_cast<float>(1 / (rate_ * .003)));
  for (unsigned ch = 0; ch < 2; ++ch) {
    out[ch] = n.last[ch] + n.fade * (out[ch] - n.last[ch]);
    if (n.fade == 1) n.last[ch] = out[ch];
  }
  return out;
}
void Engine::process(const float* left, const float* right, float* outL,
                     float* outR, std::size_t frames, const Controls& controls,
                     Transport transport, std::span<const Event> events,
                     std::span<const Change> changes) noexcept {
  if (!prepared_ || !outL || !outR) return;
  auto valid = [frames](auto batch) {
    std::size_t last = 0;
    for (const auto& e : batch) {
      if (e.offset >= frames || e.offset < last) return false;
      last = e.offset;
    }
    return true;
  };
  bool validEvents = true;
  for (const auto& e : events)
    validEvents &= e.node < nodes && e.action >= Action::Clear &&
                   e.action <= Action::Mutate;
  if (events.size() > 128 || changes.size() > 128 || !valid(events) ||
      !valid(changes) || !validEvents) {
    ++meter_.rejected;
    events = {};
    changes = {};
  }
  target_ = sanitize(controls, meter_.sanitized);
  if (!initialized_) {
    master_ = target_.master;
    mix_ = target_.mix;
    routes_ = target_.routes;
    live_ = target_.live;
    for (std::size_t i = 0; i < nodes; ++i) {
      levels_[i] = target_.node[i].level;
      node_[i].random = target_.seed ^ (0x9e3779b97f4a7c15ULL + i * 7919);
      if (!node_[i].random) node_[i].random = 1;
    }
    initialized_ = true;
  }
  if (transport.valid && !std::isfinite(transport.ppq)) {
    transport.valid = false;
    ++meter_.sanitized;
  }
  const double bpm =
      std::isfinite(transport.bpm) ? std::clamp(transport.bpm, 20., 400.) : 120;
  if (transport.valid) {
    transport.discontinuity |= !transportKnown_ ||
                               wasPlaying_ != transport.playing ||
                               std::abs(transport.ppq - expectedPpq_) > 1.e-6;
    expectedPpq_ =
        transport.ppq + (transport.playing ? frames * bpm / (60 * rate_) : 0);
    wasPlaying_ = transport.playing;
    transportKnown_ = true;
  } else
    transportKnown_ = false;
  std::array<double, nodes> power{}, capturePower{};
  std::size_t event = 0, change = 0;
  meter_.peak = 0;
  for (std::size_t sample = 0; sample < frames; ++sample, ++clock_) {
    while (change < changes.size() && changes[change].offset == sample)
      target_ = sanitize(changes[change++].controls, meter_.sanitized);
    while (event < events.size() && events[event].offset == sample) {
      auto e = events[event++];
      if (e.node >= nodes) {
        ++meter_.rejected;
        continue;
      }
      if (e.action == Action::Clear)
        clear(e.node);
      else if (e.action == Action::CaptureNow)
        commit(e.node);
      else if (e.action == Action::Mutate) {
        auto& n = node_[e.node];
        n.cursor += random(n) * capacity_ * .25;
      }
    }
    Stereo input{left ? safe(left[sample], meter_.sanitized) : 0,
                 right ? safe(right[sample], meter_.sanitized)
                       : (left ? safe(left[sample], meter_.sanitized) : 0)};
    std::array<Stereo, nodes> current{};
    for (std::size_t i = 0; i < nodes; ++i) {
      auto& n = node_[i];
      auto c = target_.node[i];
      c.speed *= std::exp2(target_.time * (i == 1 ? -3.F : 3.F));
      c.speed = std::clamp(c.speed, 0.F, 8.F);
      c.erosion = 1 - (1 - c.erosion) * (1 - target_.erosion);
      c.inheritance *= target_.inheritance;
      c.variation = std::min(1.F, c.variation + target_.spread * .35F);
      c.temporalSpread = std::max(c.temporalSpread, target_.spread);
      c.stereoSpread = std::min(1.F, c.stereoSpread + target_.spread * .4F);
      live_[i] +=
          static_cast<float>(routeSmooth_) * (target_.live[i] - live_[i]);
      levels_[i] += static_cast<float>(levelSmooth_) * (c.level - levels_[i]);
      float sum = 0;
      for (std::size_t j = 0; j < nodes; ++j) {
        routes_[i][j] += static_cast<float>(routeSmooth_) *
                         (target_.routes[i][j] - routes_[i][j]);
        sum += routes_[i][j];
      }
      float scale = sum > .995F ? .995F / sum : 1;
      meter_.routeScale[i] = scale;
      Stereo incoming{};
      for (unsigned ch = 0; ch < 2; ++ch) {
        incoming[ch] = input[ch] * live_[i];
        for (std::size_t j = 0; j < nodes; ++j)
          incoming[ch] += previous_[j][ch] * routes_[i][j] * scale;
        incoming[ch] = 2 * std::tanh(incoming[ch] * .5F);
      }
      bool frozen = target_.freeze || c.freeze;
      if ((frozen && !n.previousFreeze) || (!c.capture && n.previousCapture))
        commit(i);
      n.previousFreeze = frozen;
      n.previousCapture = c.capture;
      if (c.capture && !frozen) {
        const auto requested = std::min(
            capacity_, std::max(std::size_t{1}, static_cast<std::size_t>(
                                                    c.captureSeconds * rate_)));
        if (n.written >= requested) commit(i);
        if (n.written < requested) {
          auto& b = n.bank[n.recording];
          for (unsigned ch = 0; ch < 2; ++ch) {
            float x = incoming[ch];
            if (c.accumulate > 0 && n.published >= 0) {
              const auto& old = n.bank[static_cast<unsigned>(n.published)];
              if (old.length)
                x += c.accumulate * old.data[ch][n.written % old.length];
            }
            x = 2 * std::tanh(x * .5F);
            if (std::abs(x) < 1.e-25F) x = 0;
            b.data[ch][n.written] = x;
            n.writePower += x * x;
            capturePower[i] += incoming[ch] * incoming[ch];
          }
          ++n.written;
          if (n.written >= requested) commit(i);
        }
      }
      if (c.position != n.previousPosition) {
        n.cursor = 0;
        n.previousPosition = c.position;
      }
      double density =
          c.sync && transport.valid ? bpm / (60 * c.division) : c.density;
      n.meter.effectiveDensity = static_cast<float>(density);
      bool allowed = !c.sync || !transport.valid || transport.playing;
      if (c.sync && transport.valid &&
          ((transport.discontinuity && sample == 0) || !n.previousSync ||
           c.division != n.previousDivision)) {
        double cycle = (transport.ppq +
                        (transport.playing ? sample * bpm / (60 * rate_) : 0)) /
                       c.division;
        n.phase = wrap(cycle, 1);
        if (n.phase < 1.e-9) n.phase = 1;
      }
      n.previousSync = c.sync && transport.valid;
      n.previousDivision = c.division;
      if (allowed) {
        n.phase += density / rate_;
        if (n.phase >= 1) {
          n.phase -= 1;
          launch(i, c);
          if (c.cluster > .5F) {
            launch(i, c);
            if (c.cluster > .9F) launch(i, c);
          }
          n.phase -= c.temporalSpread * .8 * random(n);
        }
      }
      if (n.published >= 0)
        n.cursor = wrap(n.cursor + c.speed * (c.reverse ? -1 : 1),
                        static_cast<double>(
                            n.bank[static_cast<unsigned>(n.published)].length));
      n.drift += .000011;
      if (n.drift > 2 * pi) n.drift -= 2 * pi;
      current[i] = render(i, c);
      for (auto x : current[i]) power[i] += x * x;
      n.meter.frozen = frozen;
      n.meter.capturing = c.capture && !frozen;
      n.meter.storedFrames =
          n.published < 0 ? 0
                          : n.bank[static_cast<unsigned>(n.published)].length;
      n.meter.storedRms =
          n.published < 0
              ? 0
              : static_cast<float>(std::sqrt(
                    n.bank[static_cast<unsigned>(n.published)].power));
    }
    previous_ = current;
    master_ += static_cast<float>(levelSmooth_) * (target_.master - master_);
    mix_ += static_cast<float>(levelSmooth_) * (target_.mix - mix_);
    Stereo output{};
    for (unsigned ch = 0; ch < 2; ++ch) {
      for (std::size_t i = 0; i < nodes; ++i)
        output[ch] += current[i][ch] * levels_[i];
      output[ch] = std::tanh(output[ch]);
      output[ch] = (input[ch] * (1 - mix_) + mix_ * output[ch]) * master_;
      output[ch] = std::clamp(output[ch], -8.F, 8.F);
      meter_.peak = std::max(meter_.peak, std::abs(output[ch]));
    }
    outL[sample] = output[0];
    outR[sample] = output[1];
  }
  for (std::size_t i = 0; i < nodes; ++i) {
    node_[i].meter.outputRms =
        frames ? static_cast<float>(std::sqrt(power[i] / (frames * 2))) : 0;
    node_[i].meter.captureRms =
        frames ? static_cast<float>(std::sqrt(capturePower[i] / (frames * 2)))
               : 0;
    meter_.node[i] = node_[i].meter;
  }
}
Controls scene(unsigned index) noexcept {
  Controls c;
  if (index == 0) {
    c.routes = {};
    c.live = {1, 0, 0};
    c.node[1].level = c.node[2].level = 0;
  }
  if (index == 2) {
    c.routes = {};
    c.live = {1, 1, 1};
  }
  if (index >= 3) {
    c.routes = {{{.35F, 0, .8F}, {.9F, .2F, 0}, {.2F, .85F, .3F}}};
    c.node[0].pitch = -5;
    c.node[1].pitch = 7;
    c.node[2].pitch = -12;
    c.node[0].speed = .25F;
    c.node[1].speed = 1.7F;
    c.node[2].speed = .04F;
    c.node[1].reverse = true;
    c.node[1].stereoSpread = .65F;
    c.node[2].sizeMs = 450;
    c.node[2].density = 14;
  }
  if (index == 4) {
    c.node[0].sizeMs = 4;
    c.node[0].density = 80;
    c.node[1].sizeMs = 19;
    c.node[1].density = 23;
    c.node[2].sizeMs = 160;
    c.node[2].density = 6;
    c.node[0].window = Window::Fragment;
    c.node[1].cluster = 1;
    c.erosion = .5F;
  }
  return c;
}
Controls morph(const Controls& a, const Controls& b, float amount) noexcept {
  float t = std::isfinite(amount) ? std::clamp(amount, 0.F, 1.F) : 0;
  Controls c = a;  // Routing-only morph: never switch sound at the midpoint.
  for (std::size_t i = 0; i < nodes; ++i) {
    c.live[i] = a.live[i] + t * (b.live[i] - a.live[i]);
    for (std::size_t j = 0; j < nodes; ++j)
      c.routes[i][j] = a.routes[i][j] + t * (b.routes[i][j] - a.routes[i][j]);
  }
  return c;
}
}  // namespace aste::g01
