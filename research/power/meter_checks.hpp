#pragma once
#include "audio.hpp"

namespace power {
namespace meter_check_detail {
// Independent realization: direct form I in long double, without the meter's
// Biquad state, prefix sums, dB-domain relative threshold, or block iterator.
struct DirectFilter {
  std::array<long double, 3> b;
  std::array<long double, 2> a;
  long double x1{}, x2{}, y1{}, y2{};
  long double tick(long double x) {
    const long double y =
        b[0] * x + b[1] * x1 + b[2] * x2 - a[0] * y1 - a[1] * y2;
    x2 = x1;
    x1 = x;
    y2 = y1;
    y1 = y;
    return y;
  }
};
struct Reference {
  double gated{}, absoluteOnly{};
  std::size_t eligible{}, retained{};
};
inline Audio twoLevels(double quietPeak) {
  Audio signal(12 * 48000);
  for (std::size_t i = 0; i < signal.left.size(); ++i) {
    const double peak = i < 4 * 48000 ? .1 : quietPeak;
    signal.left[i] = signal.right[i] =
        static_cast<float>(peak * std::sin(2 * pi * 1000 * i / 48000));
  }
  return signal;
}
inline Reference reference(const Audio& signal) {
  DirectFilter shelf{{1.53512485958697L, -2.69169618940638L, 1.19839281085285L},
                     {-1.69065929318241L, .73248077421585L}};
  DirectFilter high{{1.L, -2.L, 1.L}, {-1.99004745483398L, .99007225036621L}};
  std::vector<long double> energy(signal.left.size());
  for (std::size_t i = 0; i < signal.left.size(); ++i) {
    if (signal.left[i] != signal.right[i])
      throw std::runtime_error("meter oracle requires dual mono");
    const long double sample = high.tick(shelf.tick(signal.left[i]));
    energy[i] = 2 * sample * sample;
  }
  const long double absolute = std::pow(10.L, (-70.L + .691L) / 10);
  std::vector<long double> eligible;
  long double firstSum{};
  for (std::size_t start = 0; start + 19200 <= energy.size(); start += 4800) {
    long double sum{};
    for (std::size_t j = start; j < start + 19200; ++j) sum += energy[j];
    const long double mean = sum / 19200;
    if (mean >= absolute) {
      eligible.push_back(mean);
      firstSum += mean;
    }
  }
  if (eligible.empty()) throw std::runtime_error("meter oracle empty fixture");
  const long double firstMean = firstSum / eligible.size();
  // -10 LU corresponds to one tenth the mean energy, not amplitude.
  const long double threshold = std::max(absolute, firstMean / 10);
  long double finalSum{};
  std::size_t retained{};
  for (long double mean : eligible)
    if (mean >= threshold) {
      finalSum += mean;
      ++retained;
    }
  if (!retained) throw std::runtime_error("meter oracle empty relative gate");
  return {static_cast<double>(-.691L + 10 * std::log10(finalSum / retained)),
          static_cast<double>(-.691L + 10 * std::log10(firstMean)),
          eligible.size(), retained};
}
}  // namespace meter_check_detail

// Tests the relative gate on two deliberately separated operating points at
// 48 kHz. This is not independent coefficient validation, external certified
// meter conformance, a true-peak test, or proof for every signal/gate boundary.
inline void meterChecks() {
  for (double quietPeak : {.001, .04}) {
    const auto signal = meter_check_detail::twoLevels(quietPeak);
    const auto oracle = meter_check_detail::reference(signal);
    const auto actual = loudness(signal);
    if (oracle.eligible != 117 ||
        oracle.retained != (quietPeak == .001 ? 40U : 117U))
      throw std::runtime_error("relative-gate fixture classification");
    if (!actual || std::abs(*actual - oracle.gated) > 1e-5)
      throw std::runtime_error("relative-gate independent oracle mismatch");
    if (quietPeak == .001 && (!(oracle.gated - oracle.absoluteOnly > 4.5) ||
                              !(oracle.gated > -20.3 && oracle.gated < -19.9)))
      throw std::runtime_error("relative-gate exclusion lacks analytic margin");
    if (quietPeak == .04 &&
        std::abs(oracle.gated - oracle.absoluteOnly) > 1e-12)
      throw std::runtime_error("relative gate rejected eligible quiet section");
  }
}
}  // namespace power
