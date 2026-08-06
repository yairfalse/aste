#include "density_processor.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <random>
#include <vector>

namespace {
std::atomic<std::size_t> allocations{0};
}

void* operator new(std::size_t size) {
  allocations.fetch_add(1, std::memory_order_relaxed);
  if (void* pointer = std::malloc(size)) {
    return pointer;
  }
  throw std::bad_alloc{};
}

void* operator new[](std::size_t size) {
  return ::operator new(size);
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, std::size_t) noexcept { std::free(pointer); }

namespace {

int failures = 0;

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

bool allFinite(const std::vector<float>& values) {
  for (float value : values) {
    if (!std::isfinite(value)) {
      return false;
    }
  }
  return true;
}

float rms(const std::vector<float>& values) {
  double sum{};
  for (float value : values) {
    sum += static_cast<double>(value) * value;
  }
  return static_cast<float>(std::sqrt(sum / static_cast<double>(values.size())));
}

void testDensityMapping() {
  auto previous = aste::density::mapDensity(0.0F);
  for (int step = 1; step <= 100; ++step) {
    const auto current = aste::density::mapDensity(static_cast<float>(step) / 100.0F);
    require(current.thresholdDb <= previous.thresholdDb, "Density threshold is monotonic");
    require(current.ratio >= previous.ratio, "Density ratio is monotonic");
    require(current.saturationDrive >= previous.saturationDrive,
            "Density saturation is monotonic");
    require(current.releaseCurve >= previous.releaseCurve, "Density release is monotonic");
    require(current.crushMakeupDb >= previous.crushMakeupDb, "Density makeup is monotonic");
    previous = current;
  }
}

void testDryAndLatency() {
  aste::density::Parameters parameters;
  parameters.blend = 0.0F;
  parameters.protection = false;
  aste::density::Processor processor;
  processor.prepare(48000.0, parameters);
  std::vector<float> left{0.0F, 0.2F, -0.4F, 0.8F, -0.7F, 0.01F, -0.03F};
  const auto original = left;
  processor.process(left.data(), nullptr, left.size(), parameters);
  require(left == original, "Dry path preserves samples exactly");
  require(processor.latencySamples() == 0, "Cycle-1 latency is zero");
}

void testCrushHasNoSampleDelay() {
  aste::density::Parameters parameters;
  parameters.blend = 1.0F;
  parameters.crush = 0.0F;
  parameters.density = 0.0F;
  parameters.protection = false;
  aste::density::Processor processor;
  processor.prepare(48000.0, parameters);
  std::array<float, 9> impulse{};
  impulse[3] = 0.1F;
  processor.process(impulse.data(), nullptr, impulse.size(), parameters);
  require(impulse[3] != 0.0F, "Crush path emits the impulse at the input sample");
  for (std::size_t i = 0; i < impulse.size(); ++i) {
    if (i != 3) {
      require(impulse[i] == 0.0F, "Crush path adds no delayed impulse energy");
    }
  }
}

void testRatesBlocksFiniteAndStereoStable() {
  constexpr std::array<double, 6> rates{44100.0, 48000.0, 88200.0,
                                         96000.0, 176400.0, 192000.0};
  constexpr std::array<std::size_t, 14> blocks{1, 2, 7, 16, 32, 64, 127,
                                                128, 256, 511, 512, 1024, 2048, 0};
  std::mt19937 generator{0xD01U};
  std::uniform_real_distribution<float> sample{-8.0F, 8.0F};
  aste::density::Parameters parameters;
  parameters.driveDb = 24.0F;
  parameters.density = 1.0F;
  parameters.crush = 1.0F;
  for (double rate : rates) {
    aste::density::Processor processor;
    processor.prepare(rate, parameters);
    for (std::size_t frames : blocks) {
      std::vector<float> left(frames);
      for (float& value : left) {
        value = sample(generator);
      }
      if (frames > 2) {
        left[0] = std::numeric_limits<float>::quiet_NaN();
        left[1] = std::numeric_limits<float>::infinity();
      }
      auto right = left;
      processor.process(left.data(), right.data(), frames, parameters);
      require(allFinite(left) && allFinite(right), "All tested rate/block outputs are finite");
      require(left == right, "Linked processing preserves identical stereo channels");
    }
  }
}

void testDeterministicReset() {
  aste::density::Parameters parameters;
  std::vector<float> first(511);
  for (std::size_t i = 0; i < first.size(); ++i) {
    first[i] = 0.6F * std::sin(static_cast<float>(i) * 0.13F);
  }
  auto second = first;
  aste::density::Processor processor;
  processor.prepare(48000.0, parameters);
  processor.process(first.data(), nullptr, first.size(), parameters);
  processor.prepare(48000.0, parameters);
  processor.process(second.data(), nullptr, second.size(), parameters);
  require(first == second, "Prepare restores deterministic processor state");
}

void testStereoLinkEndpoints() {
  constexpr std::size_t frames = 8192;
  std::array<std::vector<float>, 3> strong;
  std::array<std::vector<float>, 3> weak;
  constexpr std::array<float, 3> links{0.0F, 0.5F, 1.0F};
  for (std::size_t mode = 0; mode < links.size(); ++mode) {
    strong[mode].resize(frames);
    weak[mode].resize(frames);
    for (std::size_t i = 0; i < frames; ++i) {
      const float wave = std::sin(static_cast<float>(i) * 0.13F);
      strong[mode][i] = 0.9F * wave;
      weak[mode][i] = 0.08F * wave;
    }
    aste::density::Parameters parameters;
    parameters.crush = 1.0F;
    parameters.density = 0.8F;
    parameters.blend = 1.0F;
    parameters.stereoLink = links[mode];
    parameters.protection = false;
    aste::density::Processor processor;
    processor.prepare(48000.0, parameters);
    processor.process(strong[mode].data(), weak[mode].data(), frames, parameters);
  }

  require(strong[0] == strong[1] && strong[1] == strong[2],
          "Stereo linking does not change the dominant channel");
  require(rms(weak[0]) > rms(weak[1]) && rms(weak[1]) > rms(weak[2]),
          "Stereo linking continuously increases gain reduction on the weak channel");
}

void testNoProcessAllocation() {
  aste::density::Parameters parameters;
  aste::density::Processor processor;
  processor.prepare(48000.0, parameters);
  std::array<float, 127> left{};
  std::array<float, 127> right{};
  const auto before = allocations.load(std::memory_order_relaxed);
  processor.process(left.data(), right.data(), left.size(), parameters);
  const auto after = allocations.load(std::memory_order_relaxed);
  require(after == before, "Process performs no heap allocation");
}

}  // namespace

int main() {
  testDensityMapping();
  testDryAndLatency();
  testCrushHasNoSampleDelay();
  testRatesBlocksFiniteAndStereoStable();
  testDeterministicReset();
  testStereoLinkEndpoints();
  testNoProcessAllocation();
  if (failures == 0) {
    std::cout << "density_tests: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
