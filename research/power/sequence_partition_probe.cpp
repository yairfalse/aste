#include "research/power/render.hpp"
#include <iostream>
#include <iomanip>
using namespace power;
Audio run(std::size_t block, int setting, double bias) {
  Audio a;
  aste::sequence::Processor engine;
  engine.prepare(a.rate,
                 sequenceParameters<aste::sequence::Parameters>(setting, 0, 0));
  for (std::size_t at = 0; at < a.left.size();) {
    const auto n = chunk(at, a.left.size(), block, a.rate);
    const aste::sequence::Transport t{true, at < 288000, 120,
                                      at / a.rate * 2 + bias};
    engine.process(
        a.left.data() + at, a.right.data() + at, n,
        sequenceParameters<aste::sequence::Parameters>(setting, at / a.rate, 0),
        t);
    at += n;
  }
  return a;
}
std::vector<std::size_t> starts(std::size_t block) {
  std::vector<std::size_t> s(384000);
  for (std::size_t at = 0; at < s.size();) {
    const auto n = chunk(at, s.size(), block, 48000);
    std::fill_n(s.begin() + at, n, at);
    at += n;
  }
  return s;
}
int main() {
  std::cout << std::setprecision(17);
  auto aa = starts(127), bb = starts(511);
  int seen = 0;
  for (std::size_t n = 0; n < 288000; ++n) {
    const auto phase = [n](std::size_t at) {
      return (at / 48000.0 * 2 + (n - at) * 120.0 / (60 * 48000.0)) / .25;
    };
    const auto a = phase(aa[n]), b = phase(bb[n]);
    if ((std::floor(a) != std::floor(b)) ||
        ((a - std::floor(a) >= .78) != (b - std::floor(b) >= .78))) {
      if (seen++ < 16)
        std::cout << "decision sample=" << n << " starts=" << aa[n] << ","
                  << bb[n] << " stepPosition=" << a << "," << b
                  << " phase=" << a - std::floor(a) << "," << b - std::floor(b)
                  << "\n";
    }
  }
  std::cout << "decision_mismatches=" << seen << "\n";
  for (int setting : {0, 4})
    for (double bias : {0., 1e-12}) {
      const auto a = run(127, setting, bias), b = run(511, setting, bias);
      std::size_t first = 0;
      while (first < a.left.size() && a.left[first] == b.left[first]) ++first;
      std::cout << "setting=" << setting << " ppq_bias=" << bias
                << " rms_difference=" << difference(a, b) << " first=" << first;
      if (first < a.left.size())
        std::cout << " values=" << a.left[first] << "," << b.left[first];
      std::cout << "\n";
    }
}
