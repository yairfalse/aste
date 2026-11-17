#include "grain_processor.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
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
