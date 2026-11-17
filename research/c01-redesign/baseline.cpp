#include "score.hpp"
#include "research/quality/audio.hpp"
#include <iostream>
int main(int argc, char** argv) {
  try {
    if (argc != 2) throw std::runtime_error("fresh output folder required");
    std::filesystem::path folder = argv[1];
    if (std::filesystem::exists(folder))
      throw std::runtime_error("choose fresh folder");
    std::filesystem::create_directories(folder);
    auto metrics = power::output(folder / "baseline.csv");
    metrics << "patch,peak,rms,lufs,tail_rms\n";
    for (int p = -1; p < 5; ++p) {
      aste::contamination::Controls c =
          p < 0 ? aste::contamination::Controls{} : aste::c01_study::patch(p);
      auto midi = aste::c01_study::phrase(48000);
      power::Audio a(48000 * 18);
      aste::contamination::Engine e;
      e.prepare(48000, 4);
      for (std::size_t at = 0; at < a.left.size(); at += 127) {
        auto n = std::min(std::size_t{127}, a.left.size() - at);
        std::array<aste::contamination::Event, 16> local;
        std::size_t count = 0;
        for (auto ev : midi)
          if (ev.offset >= at && ev.offset < at + n) {
            ev.offset -= at;
            local[count++] = ev;
          }
        e.process(a.left.data() + at, a.right.data() + at, n, c,
                  std::span(local).first(count));
      }
      auto m = power::measure(a);
      double tail = 0;
      for (std::size_t i = 48000 * 12; i < 48000 * 14; ++i)
        tail += a.left[i] * a.left[i];
      metrics << p << ',' << m.peak << ',' << m.rms << ','
              << (m.lufs ? *m.lufs : -999) << ',' << std::sqrt(tail / 96000)
              << '\n';
      power::wav(folder / ("old-" + std::to_string(p) + ".wav"), a);
    }
    std::cout << "Original C01 baseline rendered\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
