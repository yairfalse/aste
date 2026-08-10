#include "harmonic_processor.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <complex>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
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

int failures = 0;

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

bool finite(const std::vector<float>& values) {
  for (float value : values) {
    if (!std::isfinite(value)) {
      return false;
    }
  }
  return true;
}

void testNeutralAndLatency() {
  aste::harmonic::Parameters parameters;
  aste::harmonic::Processor processor;
  processor.prepare(48000.0, parameters);
  std::vector<float> audio{0.0F, 0.2F, -0.4F, 0.8F, -0.7F, 0.01F, -0.03F};
  const auto original = audio;
  processor.process(audio.data(), nullptr, audio.size(), parameters);
  require(audio == original, "Neutral Harmonic preserves samples exactly");
  require(processor.latencySamples() == 0U,
          "Harmonic internal beta reports zero latency");
}

void testRatesBlocksAndStereo() {
  constexpr std::array<double, 6> rates{44100.0, 48000.0,  88200.0,
                                        96000.0, 176400.0, 192000.0};
  constexpr std::array<std::size_t, 14> blocks{
      1, 2, 7, 16, 32, 64, 127, 128, 256, 511, 512, 1024, 2048, 0};
  aste::harmonic::Parameters parameters;
  parameters.inputDb = std::numeric_limits<float>::infinity();
  parameters.foundationGainDb = 99.0F;
  parameters.foundationFrequencyHz = -1.0F;
  parameters.bodyGainDb = -99.0F;
  parameters.presenceGainDb = 12.0F;
  parameters.airGainDb = 12.0F;
  parameters.airFrequencyHz = std::numeric_limits<float>::quiet_NaN();
  parameters.harmonic = 9.0F;
  parameters.outputDb = 99.0F;
  for (double rate : rates) {
    aste::harmonic::Processor processor;
    processor.prepare(rate, parameters);
    for (std::size_t frames : blocks) {
      std::vector<float> left(frames);
      for (std::size_t sample = 0; sample < frames; ++sample) {
        left[sample] = 8.0F * std::sin(static_cast<float>(sample) * 0.13F);
      }
      if (frames > 2U) {
        left[0] = std::numeric_limits<float>::quiet_NaN();
        left[1] = std::numeric_limits<float>::infinity();
      }
      auto right = left;
      processor.process(left.data(), right.data(), frames, parameters);
      require(finite(left) && finite(right),
              "All Harmonic rate/block outputs remain finite");
      require(left == right,
              "Identical stereo input produces identical Harmonic output");
    }
  }
}

void testDeterministicBlocksAndReset() {
  constexpr std::size_t frames = 4096;
  std::vector<float> whole(frames);
  for (std::size_t sample = 0; sample < frames; ++sample) {
    whole[sample] = 0.6F * std::sin(static_cast<float>(sample) * 0.071F);
  }
  auto scheduled = whole;
  aste::harmonic::Parameters parameters;
  parameters.foundationGainDb = 5.0F;
  parameters.bodyGainDb = -3.0F;
  parameters.presenceGainDb = 4.0F;
  parameters.airGainDb = 2.0F;
  parameters.harmonic = 0.8F;
  aste::harmonic::Processor first;
  aste::harmonic::Processor second;
  first.prepare(48000.0, parameters);
  second.prepare(48000.0, parameters);
  first.process(whole.data(), nullptr, whole.size(), parameters);
  constexpr std::array<std::size_t, 7> schedule{1, 7, 16, 127, 511, 64, 2};
  std::size_t offset = 0;
  std::size_t block = 0;
  while (offset < scheduled.size()) {
    const std::size_t count = std::min(schedule[block++ % schedule.size()],
                                       scheduled.size() - offset);
    second.process(scheduled.data() + offset, nullptr, count, parameters);
    offset += count;
  }
  require(whole == scheduled,
          "Harmonic output is independent of variable block scheduling");

  auto repeated = whole;
  first.prepare(48000.0, parameters);
  std::vector<float> source(frames);
  for (std::size_t sample = 0; sample < frames; ++sample) {
    source[sample] = 0.6F * std::sin(static_cast<float>(sample) * 0.071F);
  }
  first.process(source.data(), nullptr, source.size(), parameters);
  require(source == repeated, "Prepare restores deterministic Harmonic state");
}

void testCutsRemainLinear() {
  constexpr std::size_t frames = 8192;
  std::vector<float> clean(frames);
  for (std::size_t sample = 0; sample < frames; ++sample) {
    clean[sample] = 0.7F * std::sin(static_cast<float>(sample) * 0.17F);
  }
  auto harmonic = clean;
  aste::harmonic::Parameters linearParameters;
  linearParameters.foundationGainDb = -12.0F;
  linearParameters.bodyGainDb = -6.0F;
  linearParameters.harmonic = 0.0F;
  auto nonlinearParameters = linearParameters;
  nonlinearParameters.harmonic = 1.0F;
  aste::harmonic::Processor linear;
  aste::harmonic::Processor nonlinear;
  linear.prepare(48000.0, linearParameters);
  nonlinear.prepare(48000.0, nonlinearParameters);
  linear.process(clean.data(), nullptr, clean.size(), linearParameters);
  nonlinear.process(harmonic.data(), nullptr, harmonic.size(),
                    nonlinearParameters);
  require(clean == harmonic,
          "Equal cuts remain independent of the Harmonic macro");
}

double thirdHarmonic(double amount, float gainDb = 12.0F) {
  constexpr double sampleRate = 48000.0;
  constexpr double frequency = 1000.0;
  constexpr std::size_t warmup = 24000;
  constexpr std::size_t frames = 48000;
  aste::harmonic::Parameters parameters;
  parameters.presenceGainDb = gainDb;
  parameters.presenceFrequencyHz = 1000.0F;
  parameters.harmonic = static_cast<float>(amount);
  aste::harmonic::Processor processor;
  processor.prepare(sampleRate, parameters);
  std::complex<double> fundamental{};
  std::complex<double> third{};
  std::array<float, 1> sample{};
  for (std::size_t index = 0; index < warmup + frames; ++index) {
    const double phase =
        2.0 * std::numbers::pi * frequency * index / sampleRate;
    sample[0] = static_cast<float>(0.5 * std::sin(phase));
    processor.process(sample.data(), nullptr, 1U, parameters);
    if (index >= warmup) {
      const double measured = static_cast<double>(index - warmup);
      fundamental += static_cast<double>(sample[0]) *
                     std::exp(std::complex<double>{
                         0.0, -2.0 * std::numbers::pi * frequency * measured /
                                  sampleRate});
      third += static_cast<double>(sample[0]) *
               std::exp(std::complex<double>{0.0, -6.0 * std::numbers::pi *
                                                      frequency * measured /
                                                      sampleRate});
    }
  }
  return std::abs(third) / std::abs(fundamental);
}

void testHarmonicMacro() {
  double previous = -1.0;
  std::array<double, 4> ratios{};
  std::size_t index = 0U;
  for (double amount : {0.0, 0.25, 0.5, 1.0}) {
    const double measured = thirdHarmonic(amount);
    require(std::isfinite(measured) && measured >= previous,
            "Harmonic macro increases H3 monotonically");
    ratios[index++] = measured;
    previous = measured;
  }
  std::cout << "{\"harmonic_h3_ratio_0\":" << ratios[0]
            << ",\"harmonic_h3_ratio_25\":" << ratios[1]
            << ",\"harmonic_h3_ratio_50\":" << ratios[2]
            << ",\"harmonic_h3_ratio_100\":" << ratios[3] << "}\n";
  require(ratios[1] > 1.0e-4,
          "Quarter Harmonic produces measurable nonlinear participation");
  require(previous > 1.0e-2,
          "Full Harmonic reaches a clearly audible nonlinear range");
  const double modestBoost = thirdHarmonic(1.0, 3.0F);
  std::cout << "{\"harmonic_h3_ratio_plus_3_db\":" << modestBoost << "}\n";
  require(modestBoost > 3.0e-3,
          "A modest boost engages the assertive character range");
}

void testNoProcessAllocation() {
  aste::harmonic::Parameters parameters;
  parameters.foundationGainDb = 12.0F;
  parameters.harmonic = 1.0F;
  aste::harmonic::Processor processor;
  processor.prepare(48000.0, parameters);
  std::array<float, 127> left{};
  std::array<float, 127> right{};
  const auto before = allocations.load(std::memory_order_relaxed);
  processor.process(left.data(), right.data(), left.size(), parameters);
  const auto after = allocations.load(std::memory_order_relaxed);
  require(after == before, "Harmonic process performs no heap allocation");
}

}  // namespace

int main() {
  testNeutralAndLatency();
  testRatesBlocksAndStereo();
  testDeterministicBlocksAndReset();
  testCutsRemainLinear();
  testHarmonicMacro();
  testNoProcessAllocation();
  if (failures == 0) {
    std::cout << "harmonic_tests: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
