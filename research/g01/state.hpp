#pragma once
// Experimental configuration only. Offline/UI thread; never callback I/O.
#include "core.hpp"
#include <iomanip>
#include <sstream>
#include <string>
namespace aste::g01 {
#define G01_NODE_FIELDS(X)                                                     \
  X(sizeMs)                                                                    \
  X(density)                                                                   \
  X(speed)                                                                     \
  X(pitch) X(position) X(variation) X(temporalSpread) X(stereoSpread) X(width) \
      X(pan) X(panDrift) X(erosion) X(inheritance) X(level) X(captureSeconds)  \
          X(accumulate) X(division) X(cluster)
inline std::string save(const Controls& c) {
  std::ostringstream s;
  s << std::setprecision(9) << "G01LAB 1\n"
    << c.seed << ' ' << c.time << ' ' << c.spread << ' ' << c.erosion << ' '
    << c.inheritance << ' ' << c.master << ' ' << c.mix << ' ' << c.freeze
    << '\n';
  for (auto v : c.live) s << v << ' ';
  s << '\n';
  for (auto row : c.routes) {
    for (auto v : row) s << v << ' ';
    s << '\n';
  }
  for (auto n : c.node) {
#define WRITE(F) s << n.F << ' ';
    G01_NODE_FIELDS(WRITE)
#undef WRITE
    s << n.capture << ' ' << n.freeze << ' ' << n.reverse << ' ' << n.sync
      << ' ' << static_cast<int>(n.mode) << ' ' << static_cast<int>(n.window)
      << ' ' << static_cast<int>(n.interpolation) << '\n';
  }
  return s.str();
}
inline bool restore(const std::string& text, Controls& output) {
  if (text.size() > 16384) return false;
  std::istringstream s(text);
  std::string magic;
  unsigned version = 0;
  Controls c;
  if (!(s >> magic >> version) || magic != "G01LAB" || version != 1)
    return false;
  if (!(s >> c.seed >> c.time >> c.spread >> c.erosion >> c.inheritance >>
        c.master >> c.mix >> c.freeze))
    return false;
  for (auto& v : c.live)
    if (!(s >> v)) return false;
  for (auto& row : c.routes)
    for (auto& v : row)
      if (!(s >> v)) return false;
  for (auto& n : c.node) {
#define READ(F) \
  if (!(s >> n.F)) return false;
    G01_NODE_FIELDS(READ)
#undef READ
    int mode = 0, window = 0, interpolation = 0;
    if (!(s >> n.capture >> n.freeze >> n.reverse >> n.sync >> mode >> window >>
          interpolation))
      return false;
    n.mode = static_cast<TimeMode>(mode);
    n.window = static_cast<Window>(window);
    n.interpolation = static_cast<Interpolation>(interpolation);
  }
  s >> std::ws;
  if (!s.eof() || !Engine::validControls(c)) return false;
  output = c;
  return true;
}
#undef G01_NODE_FIELDS
}  // namespace aste::g01
