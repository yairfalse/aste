#include "density_processor.hpp"
#include "decimal_parse.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <random>
#include <string_view>
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
  return static_cast<float>(
      std::sqrt(sum / static_cast<double>(values.size())));
}

void testStrictDecimalParsing() {
  double value{};
  require(aste::density::parseFiniteDecimal("-123.5e-2", value) &&
              std::abs(value + 1.235) < 1.0e-12,
          "Decimal parsing accepts finite classic-locale values");
  for (const auto invalid : std::array<std::string_view, 8>{
           "", " 1", "1 ", "1x", "1,5", "nan", "inf", "1e9999"}) {
    require(!aste::density::parseFiniteDecimal(invalid, value),
            "Decimal parsing rejects malformed or non-finite values");
  }
}

void testDensityMapping() {
  auto previous = aste::density::mapDensity(0.0F);
  for (int step = 1; step <= 100; ++step) {
    const auto current =
        aste::density::mapDensity(static_cast<float>(step) / 100.0F);
    require(current.thresholdDb <= previous.thresholdDb,
            "Density threshold is monotonic");
    require(current.ratio >= previous.ratio, "Density ratio is monotonic");
    require(current.saturationDrive >= previous.saturationDrive,
            "Density saturation is monotonic");
    require(current.releaseCurve >= previous.releaseCurve,
            "Density release is monotonic");
    require(current.crushMakeupDb >= previous.crushMakeupDb,
            "Density makeup is monotonic");
    previous = current;
  }
}

void testNonlinearNumericalSafety() {
  const float nan = std::numeric_limits<float>::quiet_NaN();
  const float infinity = std::numeric_limits<float>::infinity();
  require(aste::density::saturateSample(nan, 0.0F) == 0.0F,
          "Saturation handles invalid sample and drive");
  require(std::isfinite(aste::density::saturateSample(infinity, infinity)),
          "Saturation output remains finite for infinity");
  require(aste::density::controlledClipSample(nan) == 0.0F,
          "Controlled clip handles NaN");
}

void testOversamplerRealtimeBoundary() {
  aste::density::CrushOversampler4x oversampler;
  const auto before = allocations.load(std::memory_order_relaxed);
  for (const std::size_t tapsPerPhase :
       std::array<std::size_t, 4>{16U, 32U, 48U, 64U}) {
    oversampler.prepare(tapsPerPhase);
    std::array<float, 127> samples{};
    samples[0] = 0.1F;
    oversampler.process(samples.data(), samples.size(), 3.88F);
    require(oversampler.latencySamples() == tapsPerPhase,
            "4x oversampler latency follows prepare-time filter length");
    require(std::all_of(samples.begin(), samples.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "4x oversampler output remains finite");
  }
  aste::density::CrushOversampler4xHalfBand halfBand;
  for (const auto configuration : std::array<std::array<std::size_t, 2>, 4>{
           {{{33U, 33U}}, {{65U, 33U}}, {{97U, 33U}}, {{129U, 33U}}}}) {
    halfBand.prepare(configuration[0], configuration[1]);
    std::array<float, 127> samples{};
    samples[0] = 0.1F;
    halfBand.process(samples.data(), samples.size(), 3.88F);
    require(std::all_of(samples.begin(), samples.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "Half-band oversampler output remains finite");
  }
  for (const float beta :
       std::array<float, 7>{-1.0F, 3.0F, 5.0F, 7.0F, 9.0F, 11.0F,
                            std::numeric_limits<float>::quiet_NaN()}) {
    halfBand.prepare(113U, 33U, beta);
    std::array<float, 16> samples{};
    samples[0] = 0.1F;
    require(std::isfinite(halfBand.processLinearSample(0.1F)),
            "Kaiser linear half-band path remains finite");
    halfBand.reset();
    halfBand.process(samples.data(), samples.size(), 3.88F);
    require(std::all_of(samples.begin(), samples.end(),
                        [](float sample) { return std::isfinite(sample); }),
            "Kaiser half-band preparation remains finite");
  }
  const auto after = allocations.load(std::memory_order_relaxed);
  require(after == before, "4x oversampler processes perform no allocation");
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
  require(impulse[3] != 0.0F,
          "Crush path emits the impulse at the input sample");
  for (std::size_t i = 0; i < impulse.size(); ++i) {
    if (i != 3) {
      require(impulse[i] == 0.0F, "Crush path adds no delayed impulse energy");
    }
  }
}

void testOversamplingPrototypeAlignment() {
  aste::density::Parameters parameters;
  parameters.blend = 0.0F;
  parameters.protection = false;
  aste::density::Processor processor;
  processor.prepareOversamplingPrototype(48000.0, parameters);
  std::array<float, 64> impulse{};
  impulse[3] = 0.1F;
  processor.process(impulse.data(), nullptr, impulse.size(), parameters);
  require(processor.latencySamples() == 44U,
          "Oversampling prototype reports 44 samples latency");
  require(impulse[47] == 0.1F,
          "Oversampling prototype delays dry path by 44 samples");
  for (std::size_t sample = 0; sample < impulse.size(); ++sample) {
    if (sample != 47U) {
      require(impulse[sample] == 0.0F,
              "Oversampling prototype dry delay adds no extra energy");
    }
  }
}

void testRatesBlocksFiniteAndStereoStable() {
  constexpr std::array<double, 6> rates{44100.0, 48000.0,  88200.0,
                                        96000.0, 176400.0, 192000.0};
  constexpr std::array<std::size_t, 14> blocks{
      1, 2, 7, 16, 32, 64, 127, 128, 256, 511, 512, 1024, 2048, 0};
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
      require(allFinite(left) && allFinite(right),
              "All tested rate/block outputs are finite");
      require(left == right,
              "Linked processing preserves identical stereo channels");
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
    processor.process(strong[mode].data(), weak[mode].data(), frames,
                      parameters);
  }

  require(strong[0] == strong[1] && strong[1] == strong[2],
          "Stereo linking does not change the dominant channel");
  require(rms(weak[0]) > rms(weak[1]) && rms(weak[1]) > rms(weak[2]),
          "Stereo linking continuously increases gain reduction on the weak "
          "channel");
}

void testNoProcessAllocation() {
  aste::density::Parameters parameters;
  aste::density::Processor processor;
  aste::density::Processor prototype;
  processor.prepare(48000.0, parameters);
  prototype.prepareOversamplingPrototype(48000.0, parameters);
  std::array<float, 127> left{};
  std::array<float, 127> right{};
  const auto before = allocations.load(std::memory_order_relaxed);
  processor.process(left.data(), right.data(), left.size(), parameters);
  prototype.process(left.data(), right.data(), left.size(), parameters);
  const auto after = allocations.load(std::memory_order_relaxed);
  require(after == before, "Process performs no heap allocation");
}

}  // namespace

int main() {
  testStrictDecimalParsing();
  testDensityMapping();
  testNonlinearNumericalSafety();
  testOversamplerRealtimeBoundary();
  testDryAndLatency();
  testCrushHasNoSampleDelay();
  testOversamplingPrototypeAlignment();
  testRatesBlocksFiniteAndStereoStable();
  testDeterministicReset();
  testStereoLinkEndpoints();
  testNoProcessAllocation();
  if (failures == 0) {
    std::cout << "density_tests: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
