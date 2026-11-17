#include "non_machine_processor.hpp"
#include "resonator_processor.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <vector>

int main() {
  int failures = 0;
  const auto require = [&](bool ok, const char* text) {
    if (!ok) {
      ++failures;
      std::cerr << text << '\n';
    }
  };
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    aste::nonmachine::Engine n;
    auto pattern = aste::nonmachine::studyPattern();
    require(n.prepare(rate, pattern), "Non-Machine prepares");
    auto bad = pattern;
    bad.tracks[0].length = 0;
    require(!n.updatePattern(bad), "Invalid edits retain last valid pattern");
    std::array<float, 511> left{}, right{};
    const std::array hits{aste::nonmachine::ManualHit{23, 0, 0, 1}};
    n.process(left.data(), right.data(), left.size(),
              {false, true, false, 0, 0, 120, true}, nullptr, {}, hits);
    for (std::size_t i = 0; i < 23; ++i)
      require(left[i] == 0, "Manual hit respects sample offset");
    bool audible = false;
    for (float x : left) {
      require(std::isfinite(x), "Finite percussion");
      audible |= std::abs(x) > .00001F;
    }
    require(audible, "Manual percussion works without host playback");
    pattern.tracks[0].events[0].pitch = 100;
    require(n.updatePattern(pattern),
            "Valid edit applies without preparing memory");
    aste::resonator::Engine r;
    aste::resonator::Exciter exciter;
    aste::resonator::Controls c;
    c.persist = true;
    c.couplingHz = 20;
    require(r.prepare(rate, aste::resonator::Architecture::Modal,
                      aste::resonator::Memory::Overlap, c),
            "Resonator prepares");
    exciter.prepare(rate);
    exciter.trigger(aste::resonator::Excitation::White, 101,
                    static_cast<std::size_t>(rate * .04));
    std::array<float, 511> excitation{};
    for (int block = 0; block < 1000; ++block) {
      for (auto& x : excitation) x = exciter.next() * .4F;
      const std::array event{aste::resonator::Event{
          0, aste::resonator::EventType::Strike, {48. + block % 12}}};
      r.process(excitation.data(), left.data(), right.data(), left.size(), c,
                block % 32 == 0 ? std::span(event)
                                : std::span<const aste::resonator::Event>{});
      for (float x : left)
        require(std::isfinite(x) && std::abs(x) <= .81F,
                "Strong persistent modal output remains bounded");
    }
    r.reset();
    r.process(nullptr, left.data(), right.data(), left.size(), c);
    for (float x : left)
      require(x == 0, "Reset clears modal energy without reallocating state");
  }
  std::cout << "playable engine checks: " << failures << " failures\n";
  return failures ? 1 : 0;
}
