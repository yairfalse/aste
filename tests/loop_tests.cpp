#include "loop_processor.hpp"

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
  allocations.fetch_add(1, std::memory_order_relaxed);
  if (void* pointer = std::malloc(size)) return pointer;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
  std::free(pointer);
}

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

void testCapturePlaybackAndReset() {
  aste::loop::Processor processor;
  processor.prepare(48000.0, 2.0);
  aste::loop::Parameters parameters;
  parameters.capture = true;
  parameters.loopLengthSeconds = 0.1F;
  parameters.mix = 1.0F;
  std::vector<float> source(4800);
  for (std::size_t sample = 0; sample < source.size(); ++sample) {
    source[sample] = 0.5F * std::sin(static_cast<float>(sample) * 0.07F);
  }
  auto captured = source;
  processor.process(captured.data(), nullptr, captured.size(), parameters);
  require(processor.meters().captured > 0.99F,
          "Loop fills the requested capture region");
  parameters.capture = false;
  std::vector<float> playback(4800);
  processor.process(playback.data(), nullptr, playback.size(), parameters);
  float peak{};
  for (float sample : playback) {
    require(std::isfinite(sample), "Loop playback remains finite");
    peak = std::max(peak, std::abs(sample));
  }
  require(peak > 0.01F, "Captured audio plays without live input");
  processor.clear();
  std::fill(playback.begin(), playback.end(), 0.0F);
  processor.process(playback.data(), nullptr, playback.size(), parameters);
  require(processor.meters().outputPeak == 0.0F,
          "Clear deterministically removes captured memory");
}

void testModesAndRealtimeSafety() {
  for (double rate : {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0}) {
    aste::loop::Processor processor;
    processor.prepare(rate, 1.0);
    require(processor.capacitySamples() >= static_cast<std::size_t>(rate),
            "Loop preallocates its requested maximum duration");
    aste::loop::Parameters parameters;
    parameters.capture = true;
    parameters.loopLengthSeconds = 0.05F;
    parameters.pitchSemitones = 7.0F;
    parameters.reverse = true;
    parameters.speed = 0.5F;
    parameters.amplifier = 1.0F;
    parameters.degradation = 1.0F;
    std::array<float, 127> left{};
    std::array<float, 127> right{};
    for (std::size_t sample = 0; sample < left.size(); ++sample) {
      left[sample] = right[sample] =
          0.4F * std::sin(static_cast<float>(sample) * 0.13F);
    }
    const auto before = allocations.load(std::memory_order_relaxed);
    processor.process(left.data(), right.data(), left.size(), parameters);
    const auto after = allocations.load(std::memory_order_relaxed);
    require(after == before, "Loop process performs no allocation");
    for (std::size_t sample = 0; sample < left.size(); ++sample) {
      require(std::isfinite(left[sample]) && left[sample] == right[sample],
              "Loop modes remain finite and stereo-identical");
    }
    require(processor.latencySamples() == 0U,
            "Loop reports no algorithmic latency");
  }
}

void testVariableBlocks() {
  aste::loop::Parameters parameters;
  parameters.capture = true;
  parameters.loopLengthSeconds = 0.2F;
  std::vector<float> input(9600);
  for (std::size_t sample = 0; sample < input.size(); ++sample) {
    input[sample] = 0.3F * std::sin(static_cast<float>(sample) * 0.031F);
  }
  auto whole = input;
  auto divided = input;
  aste::loop::Processor first;
  aste::loop::Processor second;
  first.prepare(48000.0, 1.0);
  second.prepare(48000.0, 1.0);
  first.process(whole.data(), nullptr, whole.size(), parameters);
  constexpr std::array blocks{1U, 7U, 127U, 511U};
  std::size_t offset{};
  std::size_t index{};
  while (offset < divided.size()) {
    const auto block = std::min<std::size_t>(blocks[index++ % blocks.size()],
                                             divided.size() - offset);
    second.process(divided.data() + offset, nullptr, block, parameters);
    offset += block;
  }
  require(whole == divided,
          "Loop capture is independent of variable block boundaries");
}

}  // namespace

int main() {
  testCapturePlaybackAndReset();
  testModesAndRealtimeSafety();
  testVariableBlocks();
  if (failures == 0) std::cout << "loop_tests: ok\n";
  return failures == 0 ? 0 : 1;
}
