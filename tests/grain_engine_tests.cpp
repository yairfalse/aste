#include "grain_processor.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <thread>
using namespace aste::grain;
void require(bool ok, const char* text) {
  if (!ok) throw std::runtime_error(text);
}
std::vector<float> render(double rate, std::size_t block) {
  Engine e;
  require(e.prepare(rate, .1), "prepare");
  auto c = scene(3);
  for (auto& n : c.node) {
    n.captureSeconds = .02F;
    n.density = 240;
    n.sizeMs = 2000;
    n.interpolation = Interpolation::Sinc16;
  }
  auto count = static_cast<std::size_t>(rate * .4);
  std::vector<float> input(count, .3F), left(count), right(count);
  for (std::size_t at = 0; at < count; at += block) {
    auto frames = std::min(block, count - at);
    e.process(input.data() + at, input.data() + at, left.data() + at,
              right.data() + at, frames, c);
  }
  auto state = e.snapshot();
  for (const auto& m : state.node)
    require(m.active <= (rate <= 48000   ? 48U
                         : rate <= 96000 ? 24U
                                         : 16U),
            "rate-dependent voice limit");
  for (float v : left)
    require(std::isfinite(v) && std::abs(v) <= 1, "bounded product output");
  return left;
}
int main() {
  try {
    {
      Engine engine;
      require(engine.prepare(48000, .1), "snapshot engine prepared");
      Controls c;
      c.live = {1, 1, 1};
      c.routes = {};
      for (auto& n : c.node) {
        n.captureSeconds = .02F;
        n.density = 0;
      }
      std::array<float, 127> input, left{}, right{};
      input.fill(.3F);
      for (int i = 0; i < 10; ++i)
        engine.process(input.data(), input.data(), left.data(), right.data(),
                       127, c);
      const auto original = engine.memoryState();
      Engine restored;
      require(restored.prepare(48000, .1), "restore engine prepared");
      restored.restoreMemory(original);
      require(restored.memoryState().audio == original.audio,
              "same-rate recall preserves every captured sample");
      std::atomic<bool> done{};
      std::thread audio([&] {
        for (int i = 0; i < 3000; ++i) {
          input.fill(.1F + .03F * static_cast<float>(i % 7));
          if (i % 31 == 0) engine.clear(0);
          engine.process(input.data(), input.data(), left.data(), right.data(),
                         127, c);
        }
        done.store(true);
      });
      bool coherent = true;
      do {
        const auto saved = engine.memoryState();
        for (const auto& samples : saved.audio) {
          coherent &= samples.size() <= 4800 * 2;
          for (std::size_t i = 0; i < samples.size(); i += 2)
            coherent &=
                std::isfinite(samples[i]) && samples[i] == samples[i + 1];
        }
      } while (!done.load());
      audio.join();
      require(coherent,
              "concurrent save and clear preserve coherent stereo banks");
    }
    for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
      auto reference = render(rate, 127);
      for (std::size_t block : {1U, 511U})
        require(render(rate, block) == reference, "product buffer invariance");
    }
    for (double rate : {48000., 192000.}) {
      Engine e;
      e.prepare(rate, .1);
      auto c = scene(3);
      for (auto& n : c.node) {
        n.captureSeconds = .02F;
        n.density = 240;
        n.sizeMs = 2000;
        n.interpolation = Interpolation::Sinc16;
      }
      std::vector<float> in(127, .3F), left(127), right(127);
      for (int i = 0; i < 1000; ++i)
        e.process(in.data(), in.data(), left.data(), right.data(), 127, c);
      std::vector<double> times;
      for (int i = 0; i < 256; ++i) {
        auto start = std::chrono::steady_clock::now();
        e.process(in.data(), in.data(), left.data(), right.data(), 127, c);
        times.push_back(std::chrono::duration<double, std::micro>(
                            std::chrono::steady_clock::now() - start)
                            .count());
      }
      std::sort(times.begin(), times.end());
      std::cout << "rate=" << rate << " block=127 sinc16 p50_us=" << times[128]
                << " p99_us=" << times[253] << " max_us=" << times.back()
                << " deadline_us=" << 127 / rate * 1e6 << '\n';
    }
    std::cout << "grain_engine_tests: passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
