#include "click_processor.hpp"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <limits>
#include <new>
namespace {
thread_local bool audit{};
thread_local int allocations{};
}  // namespace
void* operator new(std::size_t n) {
  if (audit) ++allocations;
  if (auto* p = std::malloc(std::max<std::size_t>(n, 1))) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  if (audit) ++allocations;
  std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
using namespace aste::click;
void require(bool b, const char* message) {
  if (!b) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
int main() {
  auto engine = std::make_unique<Engine>();
  require(engine->prepare(48000), "prepare");
  std::array<float, 128> l{}, r{};
  auto controls = factory(0);
  controls.mode = 0;
  engine->process(l.data(), r.data(), 0, controls);
  engine->midi({0, 36, 1, true, -1});
  audit = true;
  engine->process(l.data(), r.data(), l.size(), controls);
  audit = false;
  require(allocations == 0, "no callback allocation or free");
  require(engine->meter().hits == 1, "MIDI hit");
  require(engine->meter().peak > 0, "click audio");
  auto first = l;
  engine->reset();
  engine->process(l.data(), r.data(), 0, controls);
  engine->midi({0, 36, 1, true, -1});
  engine->process(l.data(), r.data(), l.size(), controls);
  require(l == first, "deterministic panic/restart");
  std::vector<float> partitionA(48000), partitionB(48000), right(48000);
  controls = factory(3);
  const auto render = [&](std::vector<float>& result, std::size_t block) {
    engine->reset();
    for (std::size_t n = 0; n < result.size(); n += block) {
      auto count = std::min(block, result.size() - n);
      engine->process(result.data() + n, right.data() + n, count, controls,
                      {true, true, n / 24000., 120});
    }
  };
  render(partitionA, 128);
  auto hits = engine->meter().hits;
  render(partitionB, 37);
  require(partitionA == partitionB, "sample scheduling block invariance");
  require(hits == engine->meter().hits && hits > 4, "curved rolls schedule");
  engine->reset();
  controls.mode = 1;
  engine->process(l.data(), r.data(), 0, controls);
  engine->midi({0, 36, 1, true, -1});
  engine->process(l.data(), r.data(), 128, controls);
  require(engine->meter().hits == 0, "host mode excludes MIDI");
  controls = factory(0);
  controls.masterLength = 4;
  controls.regionEnd = 4;
  for (int block = 0; block < 400; ++block)
    engine->process(l.data(), r.data(), 128, controls,
                    {true, true, block * 128 / 24000., 120});
  controls.repeat = true;
  engine->process(l.data(), r.data(), 128, controls,
                  {true, true, 51200 / 24000., 120});
  require(engine->meter().repeating, "master capture");
  controls.nested = true;
  engine->process(l.data(), r.data(), 128, controls,
                  {true, true, 51328 / 24000., 120});
  require(engine->meter().nested, "nested audio capture");
  controls.continueTime = false;
  engine->process(l.data(), r.data(), 128, controls,
                  {true, true, 51456 / 24000., 120});
  auto position = engine->meter().masterPosition;
  engine->process(l.data(), r.data(), 128, controls,
                  {true, true, 51584 / 24000., 120});
  require(engine->meter().masterPosition == position,
          "HOLD suspends musical clock");
  controls.continueTime = true;
  engine->process(l.data(), r.data(), 128, controls,
                  {true, true, 51712 / 24000., 120});
  require(engine->meter().masterPosition != position,
          "CONTINUE advances underneath repeat");
  engine->process(l.data(), r.data(), 128, controls,
                  {true, false, 51840 / 24000., 120});
  require(!engine->meter().repeating && !engine->meter().nested,
          "stop clears fragments");
  engine->reset();
  controls = factory(0);
  for (auto& object : controls.object)
    for (auto& step : object.step) step.on = false;
  controls.object[0].step[1].on = true;
  controls.object[0].step[1].timing = -.25F;
  std::vector<float> early(4500), earlyRight(4500);
  engine->process(early.data(), earlyRight.data(), 4500, controls,
                  {true, true, 0, 120});
  require(engine->meter().hits == 0,
          "negative microtiming waits until exact sample");
  engine->process(l.data(), r.data(), 1, controls,
                  {true, true, 4500 / 24000., 120});
  require(engine->meter().hits == 1, "negative microtiming exact onset");
  const int bank = engine->reserveSample(0);
  require(bank >= 0, "sample reserve");
  for (int ch = 0; ch < 2; ++ch)
    for (int n = 0; n < 2048; ++n)
      engine->sampleData(0, bank, ch)[n] =
          static_cast<float>(std::sin(n * .05));
  engine->publishSample(0, bank, 2048, 48000);
  controls = factory(0);
  controls.mode = 0;
  controls.object[0].source = 1;
  controls.object[0].loop = true;
  controls.object[0].loopStart = .1F;
  controls.object[0].loopEnd = .3F;
  engine->reset();
  engine->process(l.data(), r.data(), 0, controls);
  engine->audition(0);
  engine->process(l.data(), r.data(), 128, controls);
  require(engine->meter().peak > 0, "sample looping audio");
  for (int n = 0; n < 60; ++n) engine->audition(0);
  engine->process(l.data(), r.data(), 128, controls);
  require(engine->meter().stolen > 0, "bounded polyphony steals");
  for (double rate : {22050., 44100., 48000., 96000., 192000.}) {
    require(engine->prepare(rate), "sample rates");
    controls.mode = 0;
    controls.pressure = controls.dirt = 1;
    controls.recovery = 5;
    for (int n = 0; n < 4; ++n) {
      auto& s = controls.object[n];
      s.resonance = .95F;
      s.cutoff = 20000;
      s.character = 1;
      s.decay = 3000;
      s.loop = true;
      s.reverse = n % 2 != 0;
      s.tune = n % 2 == 0 ? 48.F : -48.F;
    }
    engine->process(l.data(), r.data(), 0, controls);
    for (int n = 0; n < 4; ++n) engine->audition(n);
    for (int block = 0; block < 80; ++block) {
      engine->process(l.data(), r.data(), 128, controls);
      for (float v : l)
        require(std::isfinite(v) && std::abs(v) <= .981F,
                "finite bounded extremes");
    }
  }
  require(!engine->prepare(std::numeric_limits<double>::quiet_NaN()),
          "invalid rate");
  std::cout << "rate,held_voices,heap_bytes,mean_cpu_fraction,p99_cpu_fraction,"
               "max_cpu_fraction\n";
  for (double rate : {48000., 96000., 192000.}) {
    engine->prepare(rate);
    controls = factory(0);
    controls.mode = 0;
    for (auto& object : controls.object) {
      object.decay = 8000;
      object.source = 0;
      object.level = .2F;
    }
    engine->process(l.data(), r.data(), 0, controls);
    for (int n = 0; n < 4; ++n)
      for (int hit = 0; hit < 12; ++hit) engine->audition(n);
    std::vector<double> timings;
    timings.reserve(500);
    for (int block = 0; block < 550; ++block) {
      auto start = std::chrono::steady_clock::now();
      engine->process(l.data(), r.data(), 128, controls);
      double elapsed = std::chrono::duration<double>(
                           std::chrono::steady_clock::now() - start)
                           .count() /
                       (128 / rate);
      if (block >= 50) timings.push_back(elapsed);
    }
    double sum = 0;
    for (double x : timings) sum += x;
    std::sort(timings.begin(), timings.end());
    std::cout << rate << ',' << engine->polyphony() << ','
              << engine->heapBytes() << ',' << sum / timings.size() << ','
              << timings[495] << ',' << timings.back() << '\n';
  }
  std::cout << "K01 engine contracts passed\n";
}
