#include "loop_processor.hpp"

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
  aste::loop::Processor processor;
  processor.prepare(kRate, 4.0);
  aste::loop::Parameters parameters;
  parameters.loopLengthSeconds = 0.5F;
  parameters.capture = true;
  parameters.wow = 0.18F;
  parameters.amplifier = 0.4F;
  std::vector<float> audio(frames);
  for (std::size_t sample = 0; sample < 24000; ++sample) {
    audio[sample] = 0.35F * std::sin(static_cast<float>(sample) * 0.071F);
  }
  std::size_t offset{};
  while (offset < frames) {
    const auto block = std::min(kBlock, frames - offset);
    parameters.capture = offset < 24000;
    processor.process(audio.data() + offset, nullptr, block, parameters);
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
            << ",\"rms\":" << rms
            << ",\"finite\":" << (std::isfinite(rms) ? "true" : "false")
            << "}\n";
  return std::isfinite(rms) && peak > 0.01F ? 0 : 1;
}

int benchmark() {
  constexpr std::size_t seconds = 30;
  constexpr std::size_t frames = static_cast<std::size_t>(kRate) * seconds;
  aste::loop::Processor processor;
  processor.prepare(kRate, 30.0);
  aste::loop::Parameters parameters;
  parameters.capture = true;
  parameters.pitchSemitones = 7;
  parameters.wow = parameters.flutter = parameters.drift = 1;
  parameters.amplifier = parameters.degradation = 1;
  std::vector<float> left(kBlock, 0.2F), right(kBlock, -0.2F);
  const auto start = std::chrono::steady_clock::now();
  std::size_t offset{};
  while (offset < frames) {
    const auto block = std::min(kBlock, frames - offset);
    processor.process(left.data(), right.data(), block, parameters);
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
  return render(argc == 2 ? argv[1] : "loop-render.csv");
}
