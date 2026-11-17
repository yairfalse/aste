#include "engine.hpp"
#include "reference/field_processor.hpp"
#include "research/quality/audio.hpp"
#include <chrono>
#include <cstring>
#include <iostream>
#include <memory>
using namespace aste::field_study;
constexpr std::array names{"field-basic", "field-default", "bare8",
                           "house16",     "hadamard16",    "schroeder",
                           "cross-tank"};
power::Audio speech(const std::filesystem::path& path) {
  std::ifstream in(path, std::ios::binary);
  std::vector<char> b((std::istreambuf_iterator<char>(in)), {});
  auto u16 = [&](std::size_t p) {
    std::uint16_t v;
    std::memcpy(&v, b.data() + p, 2);
    return v;
  };
  auto u32 = [&](std::size_t p) {
    std::uint32_t v;
    std::memcpy(&v, b.data() + p, 4);
    return v;
  };
  unsigned channels = 0, rate = 0, kind = 0, bits = 0;
  std::size_t start = 0, size = 0;
  for (std::size_t p = 12; p + 8 <= b.size();) {
    auto n = u32(p + 4);
    if (p + 8 + n > b.size()) throw std::runtime_error("wave extent");
    if (!std::memcmp(b.data() + p, "fmt ", 4)) {
      kind = u16(p + 8);
      channels = u16(p + 10);
      rate = u32(p + 12);
      bits = u16(p + 22);
    }
    if (!std::memcmp(b.data() + p, "data", 4)) {
      start = p + 8;
      size = n;
    }
    p += 8 + n + (n % 2);
  }
  if (kind != 3 || bits != 32 || rate != 48000 || channels < 1 ||
      channels > 2 || !start)
    throw std::runtime_error("speech must be float32 48k WAV");
  power::Audio a(48000 * 12);
  for (std::size_t i = 0; i < std::min(a.left.size(), size / (4 * channels));
       ++i) {
    float x, y;
    std::memcpy(&x, b.data() + start + i * 4 * channels, 4);
    y = x;
    if (channels == 2) std::memcpy(&y, b.data() + start + i * 8 + 4, 4);
    a.left[i] = x * .5F;
    a.right[i] = y * .5F;
  }
  return a;
}
power::Audio render(int kind, const power::Audio& input, double decay,
                    double mod = .15, int block = 127) {
  auto a = input;
  auto old = std::make_unique<aste::field::Processor>();
  Engine e;
  if (kind < 2)
    old->prepare(a.rate);
  else
    e.prepare(a.rate, static_cast<Topology>(kind - 2));
  aste::field::Parameters p;
  p.blend = 1;
  p.outputDb = 0;
  p.materialBehavior = true;
  // Approximate nominal decay from mean delay; the current gain floor limits
  // short times.
  double retention = std::pow(10., -3 * .056 / decay);
  p.mass =
      float(std::pow(std::clamp((retention - .7) / .285, 0., 1.), 1 / 1.35));
  if (kind == 0) {
    p.grain = p.pitch = p.motion = p.texture = 0;
    p.distance = .45F;
  } else
    p.texture = .7F;
  for (std::size_t at = 0; at < a.left.size(); at += block) {
    auto n = std::min(std::size_t(block), a.left.size() - at);
    if (kind < 2)
      old->process(a.left.data() + at, a.right.data() + at, n, p);
    else
      e.process(a.left.data() + at, a.right.data() + at, n,
                {decay, .55, .7, mod});
  }
  return a;
}
int main(int argc, char** argv) {
  try {
    if (argc < 3)
      throw std::runtime_error(
          "usage: f01_lab render fresh-folder speech.wav | benchmark "
          "fresh-folder");
    std::filesystem::path folder = argv[2];
    if (std::filesystem::exists(folder))
      throw std::runtime_error("fresh folder required");
    std::filesystem::create_directories(folder);
    if (std::string(argv[1]) == "benchmark") {
      auto out = power::output(folder / "cpu.csv");
      out << "engine,rate,p50_us,p99_us,max_us,deadline_us,memory_bytes\n";
      for (int kind = 0; kind < 7; ++kind)
        for (double rate : {44100., 48000., 96000., 192000.}) {
          auto old = std::make_unique<aste::field::Processor>();
          Engine e;
          if (kind < 2)
            old->prepare(rate);
          else
            e.prepare(rate, static_cast<Topology>(kind - 2));
          aste::field::Parameters p;
          p.blend = 1;
          p.outputDb = 0;
          p.materialBehavior = true;
          if (kind == 0)
            p.grain = p.pitch = p.motion = p.texture = 0;
          else
            p.texture = .7F;
          std::array<float, 128> l{}, r{};
          std::vector<double> us;
          for (int k = 0; k < 400; ++k) {
            l.fill(.01F);
            r.fill(-.005F);
            auto start = std::chrono::steady_clock::now();
            if (kind < 2)
              old->process(l.data(), r.data(), 128, p);
            else
              e.process(l.data(), r.data(), 128, {20, .15, .78, 1});
            double t = std::chrono::duration<double, std::micro>(
                           std::chrono::steady_clock::now() - start)
                           .count();
            if (k >= 100) us.push_back(t);
          }
          std::sort(us.begin(), us.end());
          out << names[kind] << ',' << rate << ',' << us[150] << ',' << us[296]
              << ',' << us.back() << ',' << 128 / rate * 1e6 << ','
              << (kind < 2 ? old->memoryBytes() : e.memoryBytes()) << '\n';
        }
      return 0;
    }
    if (argc != 4) throw std::runtime_error("speech source required");
    std::vector<power::Audio> sources;
    power::Audio ir(48000 * 12);
    ir.left[0] = ir.right[0] = .25F;
    sources.push_back(ir);
    for (int kind : {0, 4, 2}) {
      auto a = power::fixture(kind, 48000, 6);
      a.left.resize(48000 * 12, 0);
      a.right.resize(48000 * 12, 0);
      sources.push_back(std::move(a));
    }
    sources.push_back(speech(argv[3]));
    constexpr std::array material{"impulse", "percussion", "chords", "tone",
                                  "speech"};
    auto metrics = power::output(folder / "levels.csv");
    metrics << "source,engine,raw_peak,raw_lufs,matched_lufs,gain_db\n";
    for (int src = 0; src < 5; ++src) {
      power::wav(folder / (std::string(material[src]) + "-dry.wav"),
                 sources[src]);
      std::vector<power::Audio> rendered;
      double target = -28;
      for (int kind = 0; kind < 7; ++kind) {
        auto a = render(kind, sources[src], 2.4);
        auto m = power::measure(a);
        if (m.lufs && m.peak > 0)
          target = std::min(target, *m.lufs + 20 * std::log10(.8 / m.peak));
        rendered.push_back(std::move(a));
      }
      std::array<double, 7> rawPeak{}, rawLufs{}, gains{};
      for (int kind = 0; kind < 7; ++kind) {
        auto m = power::measure(rendered[kind]);
        rawPeak[kind] = m.peak;
        rawLufs[kind] = m.lufs.value_or(-999);
      }
      for (int pass = 0; pass < 16; ++pass) {
        double peak = 0;
        double error = 0;
        for (int kind = 0; kind < 7; ++kind) {
          auto m = power::measure(rendered[kind]);
          double g = target - m.lufs.value_or(target);
          gains[kind] += g;
          for (auto& x : rendered[kind].left) x *= power::gain(g);
          for (auto& x : rendered[kind].right) x *= power::gain(g);
          auto check = power::measure(rendered[kind]);
          peak = std::max(peak, check.peak);
          error =
              std::max(error, std::abs(check.lufs.value_or(target) - target));
        }
        if (peak > .8)
          target += 20 * std::log10(.8 / peak) - .01;
        else if (error < .03)
          break;
      }
      for (int kind = 0; kind < 7; ++kind) {
        auto m = power::measure(rendered[kind]);
        if (std::abs(m.lufs.value_or(target) - target) > .05 || m.peak > .801)
          throw std::runtime_error("loudness match failed");
        metrics << material[src] << ',' << names[kind] << ',' << rawPeak[kind]
                << ',' << rawLufs[kind] << ',' << m.lufs.value_or(-999) << ','
                << gains[kind] << '\n';
        power::wav(folder / (std::string(material[src]) + "-" + names[kind] +
                             "-matched.wav"),
                   rendered[kind]);
      }
    }
    for (double decay : {.6, 2.4, 8.})
      for (int kind = 0; kind < 7; ++kind) {
        power::Audio a(48000 * 20);
        a.left[0] = a.right[0] = .1F;
        auto b = render(kind, a, decay);
        power::wav(folder / ("ir-" + std::to_string(decay) + "-" + names[kind] +
                             ".wav"),
                   b);
      }
    std::cout << "Rendered 61 WAVs (5 dry, 35 matched, 21 raw IR); no "
                 "subjective evaluation claimed\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
