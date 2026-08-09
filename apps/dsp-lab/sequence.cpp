#include "sequence_processor.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

constexpr double kSampleRate = 48000.0;
constexpr std::size_t kBlockSize = 127;

int render(const char* path) {
  constexpr std::size_t frames = 96000;
  aste::sequence::Parameters parameters;
  aste::sequence::Processor processor;
  processor.prepare(kSampleRate, parameters);
  std::vector<float> audio(frames);
  std::size_t offset{};
  while (offset < frames) {
    const std::size_t block = std::min(kBlockSize, frames - offset);
    const double ppq =
        static_cast<double>(offset) * 120.0 / (60.0 * kSampleRate);
    processor.process(audio.data() + offset, nullptr, block, parameters,
                      {true, true, 120.0, ppq});
    offset += block;
  }
  std::ofstream output{path};
  if (!output) {
    std::cerr << "sequence_lab: cannot write " << path << '\n';
    return 1;
  }
  output << "sample,audio\n";
  double sumSquares{};
  float peak{};
  for (std::size_t sample = 0; sample < audio.size(); ++sample) {
    output << sample << ',' << audio[sample] << '\n';
    sumSquares += static_cast<double>(audio[sample]) * audio[sample];
    peak = std::max(peak, std::abs(audio[sample]));
  }
  const double rms = std::sqrt(sumSquares / static_cast<double>(audio.size()));
  std::cout << "{\"frames\":" << frames << ",\"peak\":" << peak
            << ",\"rms\":" << rms
            << ",\"finite\":" << (std::isfinite(rms) ? "true" : "false")
            << "}\n";
  return std::isfinite(rms) && peak > 0.0F ? 0 : 1;
}

int benchmark() {
  constexpr std::size_t seconds = 30;
  constexpr std::size_t frames =
      static_cast<std::size_t>(kSampleRate) * seconds;
  aste::sequence::Parameters parameters;
  parameters.pressure = 1.0F;
  parameters.resonance = 0.85F;
  aste::sequence::Processor processor;
  processor.prepare(kSampleRate, parameters);
  std::vector<float> left(kBlockSize);
  std::vector<float> right(kBlockSize);
  const auto start = std::chrono::steady_clock::now();
  std::size_t offset{};
  while (offset < frames) {
    const std::size_t block = std::min(kBlockSize, frames - offset);
    const double ppq =
        static_cast<double>(offset) * 120.0 / (60.0 * kSampleRate);
    processor.process(left.data(), right.data(), block, parameters,
                      {true, true, 120.0, ppq});
    offset += block;
  }
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  const double cpu = 100.0 * elapsed / static_cast<double>(seconds);
  std::cout << "{\"seconds\":" << seconds << ",\"block_size\":" << kBlockSize
            << ",\"cpu_percent\":" << cpu << "}\n";
  return std::isfinite(cpu) && cpu > 0.0 ? 0 : 1;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string{argv[1]} == "--benchmark") {
    return benchmark();
  }
  return render(argc == 2 ? argv[1] : "sequence-render.csv");
}
