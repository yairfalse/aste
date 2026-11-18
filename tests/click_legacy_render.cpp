// Compile against both the prior and current Engine to verify unchanged
// sessions.
#include "click_processor.hpp"
#include <array>
#include <bit>
#include <cstdint>
#include <iostream>
#include <memory>
int main() {
  for (double rate : {48000., 192000.}) {
    auto engine = std::make_unique<aste::click::Engine>();
    engine->prepare(rate);
    std::uint64_t hash = 1469598103934665603ULL;
    std::array<float, 127> l{}, r{};
    for (int preset = 0; preset < 6; ++preset) {
      engine->reset();
      const auto controls = aste::click::factory(preset);
      for (int block = 0; block < 800; ++block) {
        engine->process(l.data(), r.data(), l.size(), controls,
                        {true, true, block * 127 * 120 / (60 * rate), 120});
        for (std::size_t n = 0; n < l.size(); ++n)
          for (float x : {l[n], r[n]}) {
            hash ^= std::bit_cast<std::uint32_t>(x);
            hash *= 1099511628211ULL;
          }
      }
    }
    std::cout << rate << ' ' << std::hex << hash << std::dec << '\n';
  }
}
