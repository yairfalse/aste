#include "field_processor.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

namespace {

struct Measurement {
  float peak{};
  float earlyRms{};
  float lateRms{};
  float energy{};
  bool finite{true};
};

Measurement measure(double sampleRate, bool forever) {
  const auto frames = static_cast<std::size_t>(sampleRate * 3.0);
  std::vector<float> left(frames);
  std::vector<float> right(frames);
  left[0] = right[0] = 1.0F;
  auto processor = std::make_unique<aste::field::Processor>();
  processor->prepare(sampleRate);
  aste::field::Parameters parameters;
  parameters.forever = forever;
  parameters.mass = 0.72F;
  parameters.grain = 0.5F;
  parameters.pitch = 0.42F;
  parameters.motion = 0.38F;
  parameters.distance = 0.55F;
  parameters.blend = 1.0F;
  parameters.outputDb = -3.0F;
  constexpr std::size_t block = 127U;
  for (std::size_t position = 0; position < frames; position += block) {
    const auto count = std::min(block, frames - position);
    processor->process(left.data() + position, right.data() + position, count,
                       parameters);
  }
  Measurement result;
  double earlyEnergy{};
  double lateEnergy{};
  const auto earlyEnd = static_cast<std::size_t>(sampleRate);
  const auto lateBegin = frames - earlyEnd;
  for (std::size_t sample = 0; sample < frames; ++sample) {
    const float value = 0.5F * (left[sample] + right[sample]);
    result.finite = result.finite && std::isfinite(value);
    result.peak = std::max(result.peak, std::abs(value));
    if (sample < earlyEnd) earlyEnergy += value * value;
    if (sample >= lateBegin) lateEnergy += value * value;
  }
  result.earlyRms = static_cast<float>(
      std::sqrt(earlyEnergy / static_cast<double>(earlyEnd)));
  result.lateRms =
      static_cast<float>(std::sqrt(lateEnergy / static_cast<double>(earlyEnd)));
  result.energy = processor->meters().fieldEnergy;
  return result;
}

int report(const char* path) {
  std::ofstream output{path};
  if (!output) return 2;
  output << "sample_rate,mode,peak,early_rms,late_rms,field_energy,latency,"
            "finite\n";
  for (double rate : {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0}) {
    for (bool forever : {false, true}) {
      const auto result = measure(rate, forever);
      output << rate << ',' << (forever ? "forever" : "release") << ','
             << result.peak << ',' << result.earlyRms << ',' << result.lateRms
             << ',' << result.energy << ",0," << (result.finite ? 1 : 0)
             << '\n';
      if (!result.finite || result.peak > 8.0F || result.lateRms <= 0.0F)
        return 3;
    }
  }
  return 0;
}

int benchmark() {
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t block = 127U;
  constexpr std::size_t frames = static_cast<std::size_t>(sampleRate * 10.0);
  aste::field::Parameters parameters;
  parameters.forever = true;
  parameters.mass = parameters.grain = parameters.pitch = 1.0F;
  parameters.motion = parameters.distance = parameters.blend = 1.0F;
  std::array<double, 5> results{};
  for (double& result : results) {
    std::vector<float> left(block, 0.05F);
    std::vector<float> right(block, -0.04F);
    auto processor = std::make_unique<aste::field::Processor>();
    processor->prepare(sampleRate);
    const auto begin = std::chrono::steady_clock::now();
    for (std::size_t position = 0; position < frames; position += block)
      processor->process(left.data(), right.data(),
                         std::min(block, frames - position), parameters);
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - begin)
            .count();
    result = elapsed / 10.0 * 100.0;
  }
  std::sort(results.begin(), results.end());
  std::cout << "field_lab: worst-case median " << results[2] << "% ("
            << results.front() << '-' << results.back() << "), state "
            << sizeof(aste::field::Processor) << " bytes\n";
  return results[2] < 20.0 ? 0 : 4;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string{argv[1]} == "--benchmark") return benchmark();
  if (argc != 2) {
    std::cerr << "usage: field_lab OUTPUT.csv | --benchmark\n";
    return 1;
  }
  const int result = report(argv[1]);
  if (result == 0) std::cout << "field_lab: report written\n";
  return result;
}
