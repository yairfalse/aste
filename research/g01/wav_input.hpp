#pragma once
// Offline importer only. The callback never opens files or owns these vectors.
#include "research/quality/audio.hpp"
#include <bit>
#include <cstring>
namespace aste::g01 {
inline power::Audio loadWav(const std::filesystem::path& path) {
  std::ifstream s(path, std::ios::binary);
  if (!s) throw std::runtime_error("Cannot open input WAV");
  auto u16 = [](const unsigned char* p) {
    return unsigned(p[0]) | (unsigned(p[1]) << 8);
  };
  auto u32 = [](const unsigned char* p) {
    return std::uint32_t(p[0]) | (std::uint32_t(p[1]) << 8) |
           (std::uint32_t(p[2]) << 16) | (std::uint32_t(p[3]) << 24);
  };
  std::array<unsigned char, 12> header{};
  s.read(reinterpret_cast<char*>(header.data()), 12);
  if (std::memcmp(header.data(), "RIFF", 4) ||
      std::memcmp(header.data() + 8, "WAVE", 4))
    throw std::runtime_error("Input must be little-endian WAV");
  unsigned format = 0, channels = 0, bits = 0, rate = 0;
  std::vector<unsigned char> data;
  while (s) {
    std::array<unsigned char, 8> h{};
    s.read(reinterpret_cast<char*>(h.data()), 8);
    if (!s) break;
    auto size = u32(h.data() + 4);
    if (size > 192000U * 30U * 8U)
      throw std::runtime_error(
          "WAV chunk exceeds 30 second stereo float budget");
    if (!std::memcmp(h.data(), "fmt ", 4)) {
      if (size < 16 || size > 4096)
        throw std::runtime_error("Invalid format chunk");
      std::vector<unsigned char> fmt(size);
      s.read(reinterpret_cast<char*>(fmt.data()), size);
      format = u16(fmt.data());
      channels = u16(fmt.data() + 2);
      rate = u32(fmt.data() + 4);
      bits = u16(fmt.data() + 14);
    } else if (!std::memcmp(h.data(), "data", 4)) {
      if (!data.empty())
        throw std::runtime_error("Multiple data chunks unsupported");
      data.resize(size);
      s.read(reinterpret_cast<char*>(data.data()), size);
    } else
      s.seekg(size, std::ios::cur);
    if (!s) throw std::runtime_error("Truncated WAV chunk");
    if (size & 1) s.seekg(1, std::ios::cur);
  }
  if ((channels != 1 && channels != 2) || rate < 44100 || rate > 192000 ||
      !((format == 3 && bits == 32) ||
        (format == 1 && (bits == 16 || bits == 24 || bits == 32))) ||
      data.empty())
    throw std::runtime_error(
        "Use PCM16/24/32 or float32 mono/stereo WAV at 44.1–192 kHz");
  const auto bytes = bits / 8;
  const auto frames = data.size() / (channels * bytes);
  if (data.size() % (channels * bytes) || frames > rate * 30U)
    throw std::runtime_error("Input extent exceeds 30 second budget");
  power::Audio out(frames + rate * 20, rate);
  for (std::size_t i = 0; i < frames; ++i)
    for (unsigned ch = 0; ch < 2; ++ch) {
      auto* p =
          data.data() + (i * channels + std::min(ch, channels - 1)) * bytes;
      float v = 0;
      if (format == 3)
        v = std::bit_cast<float>(u32(p));
      else if (bits == 16)
        v = static_cast<std::int16_t>(u16(p)) / 32768.F;
      else if (bits == 24) {
        std::int32_t x = std::int32_t(p[0]) | (std::int32_t(p[1]) << 8) |
                         (std::int32_t(p[2]) << 16);
        if (x & 0x800000) x -= 0x1000000;
        v = x / 8388608.F;
      } else
        v = static_cast<float>(static_cast<std::int32_t>(u32(p)) / 2147483648.);
      if (!std::isfinite(v))
        throw std::runtime_error("Nonfinite external audio");
      (ch ? out.right : out.left)[i] = v;
    }
  return out;
}
}  // namespace aste::g01
