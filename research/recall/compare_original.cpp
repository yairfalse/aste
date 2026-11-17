#include "grain_processor.hpp"
#include "loop_processor.hpp"
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iostream>
int main() {
  for (double rate : {48000., 192000.}) {
    aste::grain::Engine grain;
    grain.prepare(rate, .1);
    auto controls = aste::grain::scene(3);
    for (auto& node : controls.node) {
      node.captureSeconds = .02F;
      node.density = 120;
      node.sizeMs = 150;
    }
    aste::loop::Processor loop;
    loop.prepare(rate, 1);
    aste::loop::Parameters p;
    p.loopLengthSeconds = .05F;
    std::uint64_t ghash = 1469598103934665603ULL, lhash = ghash;
    std::array<float, 127> input, l, r, tape;
    for (int block = 0; block < 400; ++block) {
      for (int i = 0; i < 127; ++i)
        input[i] = .2F * std::sin(static_cast<float>(block * 127 + i) * .027F);
      grain.process(input.data(), input.data(), l.data(), r.data(), 127,
                    controls);
      tape = input;
      p.capture = block < 40;
      p.reloop = block == 80 || block == 240;
      loop.process(tape.data(), nullptr, 127, p);
      for (int i = 0; i < 127; ++i) {
        ghash = (ghash ^ std::bit_cast<std::uint32_t>(l[i])) * 1099511628211ULL;
        ghash = (ghash ^ std::bit_cast<std::uint32_t>(r[i])) * 1099511628211ULL;
        lhash =
            (lhash ^ std::bit_cast<std::uint32_t>(tape[i])) * 1099511628211ULL;
      }
    }
    std::cout << rate << " grain=" << std::hex << ghash << " loop=" << lhash
              << std::dec << '\n';
  }
}
