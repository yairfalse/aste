#include "core.hpp"
#include "state.hpp"
#include "wav_input.hpp"
#include "research/quality/audio.hpp"
#include "apps/loop/loop_processor.hpp"
#include "apps/field/field_processor.hpp"
#include <chrono>
#include <iostream>
#include <memory>
using namespace aste::g01;
using power::Audio;
namespace {
void require(bool b, const char* text) {
  if (!b) throw std::runtime_error(text);
}
Audio source(int kind, double seconds = 12, double rate = 48000) {
  Audio a(static_cast<std::size_t>(seconds * rate), rate);
  for (std::size_t i = 0; i < a.left.size(); ++i) {
    double t = i / rate, x = 0, y = 0;
    if (t < 2) {
      switch (kind) {
        case 0:
          for (int note : {48, 51, 55, 62})
            x += .06 * std::sin(2 * power::pi * 440 *
                                std::exp2((note - 69) / 12.) * t);
          x *= std::min(1., t * 30) * std::exp(-t * 2);
          y = x;
          break;
        case 1:
          x = .2 * power::noise(i + 17);
          y = .15 * power::noise(i + 117);
          break;
        case 2: {
          double b = std::fmod(t, .31);
          x = .25 * std::sin(2 * power::pi * 80 * b) * std::exp(-b * 35) +
              .08 * power::noise(i) * std::exp(-b * 170);
          y = x;
          break;
        }
        case 3:
          x = y = i == 1200 ? .8 : 0;
          break;
        case 4:
          x = y = .2 * std::sin(2 * power::pi * 440 * t);
          break;
        default:
          x = .09 * power::noise(i + 41) * (.5 + .5 * std::sin(t * 7)) +
              .05 * std::sin(2 * power::pi * (130 * t + 5 * t * t));
          y = .09 * power::noise(i + 491) * (.5 + .5 * std::cos(t * 9));
          break;
      }
    }
    a.left[i] = static_cast<float>(x);
    a.right[i] = static_cast<float>(y);
  }
  return a;
}
struct Render {
  Audio audio;
  Snapshot state;
  std::size_t bytes{};
  double seconds{};
};
Render run(const Audio& input, Controls c, bool perform = false,
           std::size_t block = 127, std::ostream* telemetry = nullptr) {
  Engine e;
  require(e.prepare(input.rate), "prepare");
  Audio out(input.left.size(), input.rate);
  auto start = std::chrono::steady_clock::now();
  for (std::size_t at = 0; at < input.left.size();) {
    auto n = std::min({block, input.left.size() - at,
                       static_cast<std::size_t>(input.rate) -
                           at % static_cast<std::size_t>(input.rate)});
    if (perform && at % static_cast<std::size_t>(input.rate) == 0) {
      float phase = static_cast<float>(at / input.rate / 12);
      const auto routing = morph(scene(1), scene(3), std::min(1.F, phase));
      c.routes = routing.routes;
      c.live = routing.live;
      c.time = std::sin(phase * 3) * .7F;
      c.erosion = std::min(.7F, phase * .3F);
      c.spread = .45F;
      c.node[0].freeze = at >= static_cast<std::size_t>(input.rate);
    }
    e.process(input.left.data() + at, input.right.data() + at,
              out.left.data() + at, out.right.data() + at, n, c);
    at += n;
    if (telemetry && at % static_cast<std::size_t>(input.rate) == 0) {
      const auto state = e.snapshot();
      *telemetry << at / input.rate;
      for (const auto& m : state.node)
        *telemetry << ',' << m.active << ',' << m.storedRms << ','
                   << m.outputRms << ',' << m.commits << ',' << m.frozen;
      *telemetry << '\n';
    }
  }
  double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  power::validate(out);
  return {std::move(out), e.snapshot(), e.heapBytes() + sizeof(Engine),
          elapsed};
}
Audio match(Audio a, double target = -25) {
  auto level = power::loudness(a);
  if (!level) return a;
  double scale = power::gain(target - *level);
  for (int i = 0; i < 8; ++i) {
    Audio b = a;
    for (auto* channel : {&b.left, &b.right})
      for (auto& v : *channel) v = static_cast<float>(v * scale);
    auto measured = power::loudness(b);
    if (!measured || std::abs(*measured - target) < .01) return b;
    scale *= power::gain(target - *measured);
  }
  for (auto* channel : {&a.left, &a.right})
    for (auto& v : *channel) v = static_cast<float>(v * scale);
  return a;
}
void matchedPair(Audio& a, Audio& b) {
  double target = -25;
  for (int k = 0; k < 4; ++k) {
    auto x = match(a, target), y = match(b, target);
    double peak = std::max(power::measure(x).peak, power::measure(y).peak);
    if (peak <= .8) {
      a = std::move(x);
      b = std::move(y);
      return;
    }
    target -= 20 * std::log10(peak / .8) + .01;
  }
  throw std::runtime_error("pair headroom");
}
void record(std::ostream& csv, const std::filesystem::path& folder,
            const std::string& name, const Render& r) {
  auto m = power::measure(r.audio);
  csv << name << ',' << r.audio.rate << ',' << r.audio.left.size() << ','
      << m.peak << ',' << m.rms << ',' << (m.lufs ? *m.lufs : -999) << ','
      << r.seconds << ',' << r.bytes;
  for (const auto& n : r.state.node)
    csv << ',' << n.launched << ',' << n.dropped << ',' << n.commits << ','
        << n.captureStalls << ',' << n.storedRms;
  csv << '\n';
  power::wav(folder / (name + ".wav"), r.audio);
}
void benchmark(const std::filesystem::path& folder) {
  auto csv = power::output(folder / "benchmark.csv");
  csv << "rate,block,interpolation,p50_us,p99_us,max_us,deadline_us,heap_bytes,"
         "dropped\n";
  for (double rate : {48000., 192000.})
    for (std::size_t block : {1U, 127U, 511U, 2048U})
      for (bool sinc : {false, true}) {
        Engine e;
        e.prepare(rate, .1);
        auto c = scene(3);
        for (auto& n : c.node) {
          n.sizeMs = 2000;
          n.density = 240;
          n.captureSeconds = .02F;
          n.interpolation =
              sinc ? Interpolation::Sinc16 : Interpolation::Linear;
        }
        std::vector<float> input(block, .2F), l(block), r(block);
        for (int i = 0; i < static_cast<int>(rate * .4 / block) + 1; ++i)
          e.process(input.data(), input.data(), l.data(), r.data(), block, c);
        std::vector<double> timings;
        const std::size_t runs =
            std::min(std::size_t{2000},
                     std::max(std::size_t{64},
                              static_cast<std::size_t>(rate * .2 / block)));
        for (std::size_t i = 0; i < runs; ++i) {
          auto a = std::chrono::steady_clock::now();
          e.process(input.data(), input.data(), l.data(), r.data(), block, c);
          timings.push_back(std::chrono::duration<double, std::micro>(
                                std::chrono::steady_clock::now() - a)
                                .count());
        }
        std::sort(timings.begin(), timings.end());
        std::uint64_t dropped = 0;
        for (auto& n : e.snapshot().node) dropped += n.dropped;
        csv << rate << ',' << block << ',' << (sinc ? "sinc16" : "linear")
            << ',' << timings[timings.size() / 2] << ','
            << timings[static_cast<std::size_t>((timings.size() - 1) * .99)]
            << ',' << timings.back() << ',' << block / rate * 1.e6 << ','
            << e.heapBytes() + sizeof(Engine) << ',' << dropped << '\n';
      }
}
}  // namespace
int main(int argc, char** argv) {
  try {
    if (argc == 2 && std::string(argv[1]) == "--check") {
      auto a = run(source(0, 4), scene(3), true, 127),
           b = run(source(0, 4), scene(3), true, 511);
      require(a.audio.left == b.audio.left && a.audio.right == b.audio.right,
              "network render partitions");
      require(power::measure(a.audio).peak > .001, "nonempty recursion");
      std::cout << "G01 render reproducibility passed\n";
      return 0;
    }
    if (argc == 3 && std::string(argv[1]) == "--benchmark") {
      std::filesystem::path folder = argv[2];
      require(!std::filesystem::exists(folder), "fresh benchmark folder");
      std::filesystem::create_directories(folder);
      benchmark(folder);
      return 0;
    }
    if (argc == 4 && std::string(argv[1]) == "--input") {
      std::filesystem::path folder = argv[3];
      require(!std::filesystem::exists(folder), "fresh external render folder");
      std::filesystem::create_directories(folder);
      auto input = loadWav(argv[2]);
      auto rendered = run(input, scene(3));
      power::wav(folder / "external-source.wav", input);
      power::wav(folder / "external-network.wav", rendered.audio);
      auto config = power::output(folder / "configuration.g01");
      config << save(scene(3));
      std::cout << "External WAV rendered: " << argv[2] << " at " << input.rate
                << " Hz; includes 20 second tail\n";
      return 0;
    }
    require(
        argc == 2,
        "usage: g01_lab --check | fresh-output-folder | --benchmark folder");
    std::filesystem::path folder = argv[1];
    require(!std::filesystem::exists(folder), "use fresh output directory");
    std::filesystem::create_directories(folder);
    auto config = power::output(folder / "configuration.g01");
    config << save(scene(3));
    auto csv = power::output(folder / "measurements.csv");
    csv << "name,rate,frames,peak,rms,lufs,wall_seconds,bytes";
    for (char n : {'A', 'B', 'C'})
      csv << ',' << n << "_launched," << n << "_dropped," << n << "_commits,"
          << n << "_stall_frames," << n << "_stored_rms";
    csv << '\n';
    for (int kind = 0; kind < 6; ++kind) {
      auto input = source(kind);
      power::wav(folder / ("source-" + std::to_string(kind) + ".wav"), input);
      auto result = run(input, scene(3));
      record(csv, folder, "G10-network-source-" + std::to_string(kind), result);
    }
    auto aliasInput = source(4, 6);
    for (std::size_t i = 0; i < aliasInput.left.size(); ++i)
      aliasInput.left[i] = aliasInput.right[i] =
          i < 96000 ? static_cast<float>(
                          .2 * std::sin(2 * power::pi * 17000 * i / 48000))
                    : 0;
    auto aa = scene(0);
    aa.node[0].mode = TimeMode::Varispeed;
    aa.node[0].speed = 2;
    aa.node[0].variation = 0;
    aa.node[0].density = 16;
    aa.node[0].sizeMs = 250;
    record(csv, folder, "G1-alias-linear", run(aliasInput, aa));
    aa.node[0].interpolation = Interpolation::Sinc16;
    record(csv, folder, "G1-alias-sinc16", run(aliasInput, aa));
    auto input = source(0);
    for (int window = 0; window < 3; ++window) {
      auto c = scene(0);
      c.node[0].window = static_cast<Window>(window);
      record(csv, folder, "G1-window-" + std::to_string(window), run(input, c));
    }
    auto cascade = scene(1), direct = cascade;
    cascade.node[0].level = cascade.node[2].level = 0;
    direct = cascade;
    direct.live = {0, 1, 0};
    direct.routes = {};
    auto a = run(input, direct), b = run(input, cascade);
    matchedPair(a.audio, b.audio);
    record(csv, folder, "G2-direct-B", a);
    record(csv, folder, "G2-cascade-A-B", b);
    std::cout << "G2 matched RMS difference "
              << power::difference(a.audio, b.audio) << " downstream captures "
              << b.state.node[1].commits << '\n';
    record(csv, folder, "G3-parallel", run(input, scene(2)));
    record(csv, folder, "G3-cross-routes", run(input, scene(3)));
    auto overload = scene(3);
    for (auto& row : overload.routes) row.fill(1.5F);
    overload.live = {1.5F, 1.5F, 1.5F};
    for (auto& n : overload.node) {
      n.density = 240;
      n.sizeMs = 2000;
    }
    record(csv, folder, "G4-full-feedback-stress", run(source(1), overload));
    auto slow = scene(0);
    slow.node[0].speed = .25F;
    slow.node[0].variation = 0;
    slow.node[0].sizeMs = 180;
    slow.node[0].density = 24;
    slow.node[0].captureSeconds = 1;
    auto independent = run(source(4), slow);
    slow.node[0].mode = TimeMode::Varispeed;
    auto varispeed = run(source(4), slow);
    matchedPair(independent.audio, varispeed.audio);
    record(csv, folder, "G5-independent", independent);
    record(csv, folder, "G5-varispeed", varispeed);
    auto narrow = scene(0), wide = narrow;
    wide.node[0].stereoSpread = 1;
    wide.node[0].panDrift = .7F;
    record(csv, folder, "G6-centred", run(source(2), narrow));
    record(csv, folder, "G6-scattered", run(source(2), wide));
    auto clean = scene(3), eroded = clean;
    eroded.erosion = .85F;
    eroded.inheritance = .2F;
    auto cleanRender = run(input, clean), erosionRender = run(input, eroded);
    matchedPair(cleanRender.audio, erosionRender.audio);
    record(csv, folder, "G7-inherited", cleanRender);
    record(csv, folder, "G7-eroded", erosionRender);
    record(csv, folder, "G8-performed-network", run(input, scene(1), true));
    auto longInput = source(0, 180);
    auto telemetry = power::output(folder / "telemetry.csv");
    telemetry << "seconds,A_active,A_stored,A_output,A_commits,A_frozen,B_"
                 "active,B_stored,B_output,B_commits,B_frozen,C_active,C_"
                 "stored,C_output,C_commits,C_frozen\n";
    auto longRun = run(longInput, scene(3), true, 127, &telemetry);
    for (int second : {30, 90, 179}) {
      double energy = 0;
      for (std::size_t i = static_cast<std::size_t>(second * 48000);
           i < static_cast<std::size_t>((second + 1) * 48000); ++i)
        energy += longRun.audio.left[i] * longRun.audio.left[i];
      std::cout << "long tail " << second << "s rms "
                << std::sqrt(energy / 48000) << '\n';
    }
    record(csv, folder, "G8-three-minutes-two-second-source", longRun);
    // Actual current Aste cores, linked only to this comparison executable.
    for (int product = 0; product < 2; ++product) {
      Audio out = input;
      if (product == 0) {
        aste::loop::Processor e;
        e.prepare(48000, 4);
        aste::loop::Parameters p;
        p.capture = true;
        p.loopLengthSeconds = 2;
        p.mix = 1;
        p.degradation = .28F;
        p.amplifier = .48F;
        p.feedback = .9F;
        for (std::size_t at = 0; at < out.left.size();) {
          auto n = std::min({std::size_t{127}, out.left.size() - at,
                             std::size_t{48000} - at % 48000});
          p.capture = at < 96000;
          p.reloop = at == 192000 || at == 384000;
          e.process(out.left.data() + at, out.right.data() + at, n, p);
          at += n;
        }
      } else {
        auto e = std::make_unique<aste::field::Processor>();
        e->prepare(48000);
        aste::field::Parameters p;
        p.materialBehavior = true;
        p.forever = true;
        p.texture = .85F;
        p.focus = .45F;
        p.mass = .88F;
        p.grain = .6F;
        p.pitch = .4F;
        p.motion = .55F;
        p.blend = 1;
        for (std::size_t at = 0; at < out.left.size(); at += 127)
          e->process(out.left.data() + at, out.right.data() + at,
                     std::min(std::size_t{127}, out.left.size() - at), p);
      }
      auto network = run(input, scene(3));
      matchedPair(out, network.audio);
      power::wav(folder / (product == 0 ? "comparison-loop.wav"
                                        : "comparison-field.wav"),
                 out);
      power::wav(folder / (product == 0 ? "comparison-grain-vs-loop.wav"
                                        : "comparison-grain-vs-field.wav"),
                 network.audio);
    }
    benchmark(folder);
    std::cout << "G01 listening pack rendered; musical listening not claimed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
