#pragma once

#include "decimal_parse.hpp"
#include <juce_audio_utils/juce_audio_utils.h>
#include <cmath>
#include <optional>
#include <span>
#include <vector>

namespace aste::parameters {
// Host-state work only: never call these allocating codecs from processing.
struct StoredAudio {
  double rate{48000};
  std::vector<float> stereo;
};
inline juce::ValueTree encodeAudio(const juce::Identifier& type, double rate,
                                   std::span<const float> stereo) {
  juce::ValueTree tree{type};
  tree.setProperty("format", 1, nullptr);
  tree.setProperty("rate", rate, nullptr);
  tree.setProperty("frames", static_cast<int>(stereo.size() / 2), nullptr);
  juce::MemoryOutputStream bytes;
  for (float x : stereo) bytes.writeFloat(x);
  tree.setProperty("audio",
                   juce::Base64::toBase64(bytes.getData(), bytes.getDataSize()),
                   nullptr);
  return tree;
}
inline std::optional<StoredAudio> decodeAudio(const juce::ValueTree& tree,
                                              std::size_t maxFrames,
                                              double minRate = 8000,
                                              double maxRate = 192000) {
  StoredAudio result;
  double frames = 0;
  if (!tree.isValid() || tree.getNumChildren() != 0 ||
      tree.getProperty("format").toString() != "1" ||
      !parseFiniteDecimal(tree.getProperty("rate").toString().toStdString(),
                          result.rate) ||
      result.rate < minRate || result.rate > maxRate ||
      !parseFiniteDecimal(tree.getProperty("frames").toString().toStdString(),
                          frames) ||
      frames < 0 || frames > maxFrames || frames != std::floor(frames))
    return std::nullopt;
  const auto count = static_cast<std::size_t>(frames) * 2;
  const auto encoded = tree.getProperty("audio").toString();
  if (static_cast<std::size_t>(encoded.length()) != ((count * 4 + 2) / 3) * 4)
    return std::nullopt;
  juce::MemoryOutputStream bytes;
  if (!juce::Base64::convertFromBase64(bytes, encoded) ||
      bytes.getDataSize() != count * 4)
    return std::nullopt;
  juce::MemoryInputStream input{bytes.getData(), bytes.getDataSize(), false};
  result.stereo.resize(count);
  for (auto& x : result.stereo) {
    x = input.readFloat();
    if (!std::isfinite(x) || std::abs(x) > 8) return std::nullopt;
  }
  return result;
}
}  // namespace aste::parameters
