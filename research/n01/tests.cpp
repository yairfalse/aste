#include "core.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>

using namespace aste::n01;
namespace {
int failures{};
void check(bool condition, const char* name) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL " << name << '\n';
  }
}
struct Render {
  std::vector<float> audio;
  std::vector<Hit> hits;
};
Render render(Pattern p, std::span<const std::size_t> blocks,
              double rate = 48000) {
  Engine e;
  check(e.prepare(rate, p), "valid setup accepted");
  Render result{std::vector<float>(static_cast<std::size_t>(rate * 8)), {}};
  std::array<Hit, 512> hits{};
  std::size_t at{}, b{};
  while (at < result.audio.size()) {
    const auto count =
        std::min(blocks[b++ % blocks.size()], result.audio.size() - at);
    Trace trace{hits};
    e.process(result.audio.data() + at, nullptr, count,
              {true, true, false, static_cast<std::int64_t>(at),
               at * 2.0 / rate, 120},
              &trace);
    check(trace.dropped == 0, "trace complete");
    result.hits.insert(result.hits.end(), hits.begin(),
                       hits.begin() + trace.size);
    at += count;
  }
  return result;
}
void scheduler() {
  Pattern p;
  p.tracks[0].length = 2;
  p.tracks[0].events[0] = {.enabled = true,
                           .beat = .5,
                           .microtiming = -.125,
                           .every = 2,
                           .repeats = 3,
                           .interval = .25,
                           .acceleration = .5};
  const std::array<std::size_t, 1> blocks{127};
  auto a = render(p, blocks);
  const std::array<std::int64_t, 3> expected{9000, 15000, 18000};
  check(a.hits.size() == 12, "condition skips alternate cycles");
  for (std::size_t i = 0; i < expected.size(); ++i)
    check(a.hits.size() > i && a.hits[i].sample == expected[i],
          "analytic sample oracle");
  check(a.hits[1].gain < a.hits[0].gain && a.hits[2].gain < a.hits[1].gain,
        "retrigger amplitude decays");
  p.tracks[0].events[0].probability = 0;
  check(render(p, blocks).hits.empty(), "zero probability means silence");
}
void determinism() {
  const std::array<std::size_t, 1> fixed{127};
  const std::array<std::size_t, 5> irregular{1, 7, 128, 511, 2048};
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    auto a = render(studyPattern(), fixed, rate);
    auto b = render(studyPattern(), irregular, rate);
    check(a.hits == b.hits, "hit trace independent of partition");
    check(a.audio == b.audio, "audio independent of partition");
    check(std::all_of(a.audio.begin(), a.audio.end(),
                      [](float v) { return std::isfinite(v); }),
          "finite audio across six rates");
  }
  auto p = studyPattern();
  auto a = render(p, fixed);
  ++p.seed;
  check(a.audio != render(p, fixed).audio, "seed changes sound");
}
void boundaries() {
  Engine e;
  auto p = studyPattern();
  p.tracks[0].events[0].pitch = std::numeric_limits<float>::quiet_NaN();
  check(!e.prepare(48000, p), "reject NaN lock");
  check(!e.prepare(0, studyPattern()), "reject invalid sample rate");
  check(e.prepare(48000, studyPattern()), "prepare");
  std::array<float, 100> out{};
  std::array<Hit, 32> hits{};
  Trace trace{hits};
  e.process(out.data(), nullptr, 0, {}, &trace);
  check(e.resets() == 0 && trace.size == 0, "zero frame is inert");
  e.process(out.data(), nullptr, out.size(), {}, &trace);
  check(trace.size == 1 && hits[0].sample == 0, "event at block origin once");
  e.process(out.data(), nullptr, out.size(), {true, true, true, 0, 0, 120});
  Engine fresh;
  check(fresh.prepare(48000, studyPattern()), "fresh prepare");
  std::array<float, 100> reference{};
  fresh.process(reference.data(), nullptr, reference.size(), {});
  check(out == reference, "seek resets sound deterministically");
  e.process(out.data(), nullptr, out.size(), {true, false, false, 100, 0, 120});
  check(std::all_of(out.begin(), out.end(), [](float v) { return v == 0; }),
        "missing timing does not fabricate clock");
}
}  // namespace
int main() {
  scheduler();
  determinism();
  boundaries();
  std::cout << "n01 contracts: " << failures << " failures\n";
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
