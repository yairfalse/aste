#pragma once
#include "reference/core.hpp"
#include <algorithm>
#include <vector>
namespace aste::c01_study {
using namespace aste::contamination;
inline std::vector<Event> phrase(double rate, int shape = 0) {
  std::vector<Event> events;
  const std::array<double, 6> times{.5, 2.75, 5.25, 8, 10.75, 13.5};
  const int hits = shape == 1 || shape == 2 ? 1 : shape == 3 ? 6 : 4;
  for (int h = 0; h < hits; ++h) {
    const std::array<int, 4> minor{48, 51, 55, 58}, suspended{43, 48, 50, 55};
    const auto& notes = h == 2 ? suspended : minor;
    const double gate = shape == 2 ? 6 : .14;
    for (int note : notes) {
      events.push_back(
          {static_cast<std::size_t>(times[static_cast<std::size_t>(h)] * rate),
           Kind::On, note, .75});
      events.push_back({static_cast<std::size_t>(
                            (times[static_cast<std::size_t>(h)] + gate) * rate),
                        Kind::Off, note, 0});
    }
  }
  std::stable_sort(
      events.begin(), events.end(),
      [](const auto& a, const auto& b) { return a.offset < b.offset; });
  return events;
}
inline Controls patch(int index) {
  Controls c;
  c.attack = .004;
  c.decay = .11;
  c.sustain = 0;
  c.release = .09;
  c.colour = 650;
  c.pressure = .6;
  c.contamination = .25;
  c.instability = .12;
  c.memory = .3;
  c.motion = .22;
  if (index == 1) {
    c.pressure = .85;
    c.contamination = .65;
    c.instability = .32;
    c.memory = .48;
    c.motion = .45;
    c.colour = 1100;
    c.attack = .002;
    c.decay = .07;
    c.sustain = .03;
    c.release = .08;
  }
  if (index == 2) {
    c.pressure = .62;
    c.contamination = .4;
    c.instability = .4;
    c.memory = .85;
    c.motion = .62;
    c.colour = 900;
    c.attack = .005;
    c.decay = .08;
    c.release = .07;
  }
  if (index == 3) {
    c.pressure = .85;
    c.contamination = .55;
    c.instability = .8;
    c.memory = .72;
    c.motion = .72;
    c.colour = 600;
    c.attack = .15;
    c.decay = .6;
    c.sustain = .8;
    c.release = 2;
  }
  if (index == 4) {
    c.pressure = .7;
    c.contamination = .5;
    c.instability = .45;
    c.memory = .9;
    c.motion = .55;
    c.colour = 750;
    c.attack = .003;
    c.decay = .11;
    c.release = .1;
  }
  return c;
}
}  // namespace aste::c01_study
