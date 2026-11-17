#include "research/quality/audio.hpp"
#include <cstring>
#include <iostream>
power::Audio read(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary);
  std::vector<char> b((std::istreambuf_iterator<char>(f)), {});
  std::size_t p = 12, start = 0, n = 0;
  while (p + 8 <= b.size()) {
    std::uint32_t k;
    std::memcpy(&k, b.data() + p + 4, 4);
    if (p + 8 + k > b.size()) throw std::runtime_error("extent");
    if (!std::memcmp(b.data() + p, "data", 4)) {
      start = p + 8;
      n = k;
    }
    p += 8 + k + (k % 2);
  }
  if (!start || n % 8) throw std::runtime_error("stereo float WAV required");
  power::Audio a(n / 8);
  for (std::size_t i = 0; i < n / 8; ++i) {
    std::memcpy(&a.left[i], b.data() + start + i * 8, 4);
    std::memcpy(&a.right[i], b.data() + start + i * 8 + 4, 4);
  }
  return a;
}

int main(int argc, char** argv) {
  try {
    if (argc != 5)
      throw std::runtime_error(
          "usage: reference_match internal pro-r uad fresh-output");
    std::filesystem::path dest = argv[4];
    if (std::filesystem::exists(dest))
      throw std::runtime_error("fresh output required");
    std::filesystem::create_directories(dest);
    auto levels = power::output(dest / "levels.csv");
    levels << "source,engine,input_lufs,final_lufs,peak,applied_gain_db\n";
    std::array names{"field-basic", "field-default", "bare8",
                     "house16",     "hadamard16",    "schroeder",
                     "cross-tank",  "pro-r",         "uad-224"};
    for (const char* source :
         {"impulse", "percussion", "chords", "tone", "speech"}) {
      std::vector<power::Audio> files;
      std::array<double, 9> gains{}, initial{};
      for (int i = 0; i < 9; ++i) {
        std::filesystem::path p =
            i < 7 ? std::filesystem::path(argv[1]) /
                        (std::string(source) + "-" + names[i] + "-matched.wav")
                  : std::filesystem::path(argv[i == 7 ? 2 : 3]) /
                        (std::string(source) + "-reference-raw.wav");
        auto a = read(p);
        auto m = power::measure(a);
        initial[i] = m.lufs.value_or(-999);
        if (m.peak < 1e-10) throw std::runtime_error("silent reference");
        double g = 20 * std::log10(.2 / m.peak);
        gains[i] = g;
        for (auto& x : a.left) x *= power::gain(g);
        for (auto& x : a.right) x *= power::gain(g);
        files.push_back(std::move(a));
      }
      double target = -30;
      for (int pass = 0; pass < 24; ++pass) {
        double maxPeak = 0, error = 0;
        for (int i = 0; i < 9; ++i) {
          auto m = power::measure(files[i]);
          if (!m.lufs) throw std::runtime_error("ungated reference");
          double g = target - *m.lufs;
          gains[i] += g;
          for (auto& x : files[i].left) x *= power::gain(g);
          for (auto& x : files[i].right) x *= power::gain(g);
          auto q = power::measure(files[i]);
          maxPeak = std::max(maxPeak, q.peak);
          error = std::max(error, std::abs(q.lufs.value_or(-999) - target));
        }
        if (maxPeak > .8)
          target += 20 * std::log10(.8 / maxPeak) - .01;
        else if (error < .03)
          break;
      }
      for (int i = 0; i < 9; ++i) {
        auto m = power::measure(files[i]);
        if (std::abs(m.lufs.value_or(-999) - target) > .05 || m.peak > .801)
          throw std::runtime_error("match failed");
        levels << source << ',' << names[i] << ',' << initial[i] << ','
               << *m.lufs << ',' << m.peak << ',' << gains[i] << '\n';
        power::wav(dest / (std::string(source) + "-" + names[i] + ".wav"),
                   files[i]);
      }
    }
    std::cout
        << "45 nine-way reference comparison WAVs matched within .05 LU\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
