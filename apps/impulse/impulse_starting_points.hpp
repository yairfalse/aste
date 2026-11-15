#pragma once

#include "impulse_processor.hpp"

namespace aste::impulse {

enum class SparseStartingPoint { clicksAndSub, cutsAndPressure };

// Complete sound/pattern snapshots, not a new engine or host parameter mode.
inline Parameters sparseStartingPoint(SparseStartingPoint choice) noexcept {
  Parameters p;
  p.energy = 0.18F;
  p.variation = 0;
  for (auto& track : p.tracks) {
    track.length = 16;
    track.pulses = 0;
    track.rotation = 0;
    track.probability = 1;
    track.pattern.fill(0);
  }
  auto& sub = p.tracks[4];
  sub.level = 0.9F;
  sub.pitchHz = 42;
  sub.decayMs = 260;
  sub.tone = 0;
  sub.drive = 0;
  sub.pulses = 2;
  sub.pattern[0] = sub.pattern[10] = 1;

  const bool cuts = choice == SparseStartingPoint::cutsAndPressure;
  auto& edge = p.tracks[cuts ? 7 : 1];
  edge.level = cuts ? 0.4F : 0.5F;
  edge.pitchHz = cuts ? 5600 : 3200;
  edge.decayMs = cuts ? 8 : 5;
  edge.tone = cuts ? 0.18F : 0.5F;
  edge.drive = 0;
  edge.pulses = 3;
  edge.pattern[3] = edge.pattern[7] = edge.pattern[14] = 1;
  if (cuts) {
    sub.pitchHz = 36;
    sub.decayMs = 380;
  }
  return p;
}
}  // namespace aste::impulse
