#include "impulse_processor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {
constexpr double kRate = 48000.0;
constexpr std::size_t kBlock = 127;
int render(const char* path) {
  constexpr std::size_t frames = 96000;
  aste::impulse::Parameters parameters;
  aste::impulse::Processor processor;
  processor.prepare(kRate, parameters);
  std::vector<float> audio(frames);
  std::size_t offset{};
  while (offset < frames) {
    const auto block = std::min(kBlock, frames - offset);
    processor.process(audio.data() + offset, nullptr, block, parameters,
                      {true, 120, offset * 120.0 / (60.0 * kRate)});
    offset += block;
  }
  std::ofstream output{path};
  if (!output) return 1;
  output << "sample,audio\n";
  double sum{};
  float peak{};
  for (std::size_t sample = 0; sample < audio.size(); ++sample) {
    output << sample << ',' << audio[sample] << '\n';
    sum += static_cast<double>(audio[sample]) * audio[sample];
    peak = std::max(peak, std::abs(audio[sample]));
  }
  const double rms = std::sqrt(sum / audio.size());
  std::cout << "{\"frames\":" << frames << ",\"peak\":" << peak
            << ",\"rms\":" << rms << "}\n";
  return std::isfinite(rms) && peak > 0.01F ? 0 : 1;
}
int benchmark() {
  constexpr std::size_t seconds = 30;
  constexpr std::size_t frames = static_cast<std::size_t>(kRate) * seconds;
  aste::impulse::Parameters parameters;
  parameters.energy = parameters.variation = parameters.mutation = 1;
  for (auto& track : parameters.tracks) {
    track.pulses = track.length = 32;
    track.pattern.fill(1U);
    track.ratchet = 4;
    track.drive = 1;
  }
  aste::impulse::Processor processor;
  processor.prepare(kRate, parameters);
  std::vector<float> left(kBlock), right(kBlock);
  const auto start = std::chrono::steady_clock::now();
  std::size_t offset{};
  while (offset < frames) {
    const auto block = std::min(kBlock, frames - offset);
    processor.process(left.data(), right.data(), block, parameters,
                      {true, 180, offset * 180.0 / (60.0 * kRate)});
    offset += block;
  }
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  const double cpu = 100.0 * elapsed / seconds;
  std::cout << "{\"seconds\":" << seconds << ",\"block_size\":" << kBlock
            << ",\"cpu_percent\":" << cpu << "}\n";
  return std::isfinite(cpu) ? 0 : 1;
}
}  // namespace
int main(int argc, char** argv) {
  if (argc == 2 && std::string{argv[1]} == "--benchmark") return benchmark();
  return render(argc == 2 ? argv[1] : "impulse-render.csv");
}
