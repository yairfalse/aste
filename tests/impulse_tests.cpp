#include "impulse_processor.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>

namespace {
std::atomic<std::size_t> allocations{};
int failures{};
}  // namespace
void* operator new(std::size_t size) {
  allocations.fetch_add(1);
  if (void* p = std::malloc(size)) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

std::vector<float> render(std::uint32_t seed,
                          const std::vector<std::size_t>& blocks) {
  aste::impulse::Parameters parameters;
  parameters.seed = seed;
  aste::impulse::Processor processor;
  processor.prepare(48000, parameters);
  std::vector<float> output(96000);
  std::size_t offset{}, blockIndex{};
  while (offset < output.size()) {
    const auto frames =
        std::min(blocks[blockIndex++ % blocks.size()], output.size() - offset);
    const double ppq = offset * 120.0 / (60.0 * 48000.0);
    processor.process(output.data() + offset, nullptr, frames, parameters,
                      {true, 120, ppq});
    offset += frames;
  }
  return output;
}

void testDeterminismAndBlocks() {
  const auto first = render(1701, {127});
  const auto second = render(1701, {1, 7, 127, 511});
  const auto changed = render(1702, {127});
  require(first == second,
          "Impulse output is independent of block partitioning");
  require(first != changed, "Stored seed changes deterministic variation");
  float peak{};
  for (float sample : first) {
    require(std::isfinite(sample), "Impulse output remains finite");
    peak = std::max(peak, std::abs(sample));
  }
  require(peak > 0.01F, "Impulse produces clocked physical events");
}

void testRatesMidiAndRealtime() {
  for (double rate : {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0}) {
    aste::impulse::Parameters parameters;
    parameters.energy = 1;
    parameters.mutation = 1;
    for (auto& track : parameters.tracks) {
      track.ratchet = 4;
      track.drive = 1;
      track.probability = 1;
    }
    aste::impulse::Processor processor;
    processor.prepare(rate, parameters);
    std::array<float, 127> left{}, right{};
    constexpr std::array midi{aste::impulse::MidiEvent{23, 36, 1, true},
                              aste::impulse::MidiEvent{61, 39, 0.8F, true}};
    const auto before = allocations.load();
    processor.process(left.data(), right.data(), left.size(), parameters,
                      {false, 120, 0}, midi);
    require(allocations.load() == before, "Impulse process allocates nothing");
    require(processor.meters().triggered[0] && processor.meters().triggered[3],
            "Impulse applies MIDI triggers at sample offsets");
    for (std::size_t i = 0; i < left.size(); ++i)
      require(std::isfinite(left[i]) && std::isfinite(right[i]),
              "Impulse hostile mode remains finite");
    require(processor.latencySamples() == 0, "Impulse reports zero latency");
  }
}
}  // namespace

int main() {
  testDeterminismAndBlocks();
  testRatesMidiAndRealtime();
  if (failures == 0) std::cout << "impulse_tests: ok\n";
  return failures == 0 ? 0 : 1;
}
