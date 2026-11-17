#include "research/quality/audio.hpp"
#include "apps/density/density_processor.hpp"
#include "apps/field/field_processor.hpp"
#include <chrono>
#include <iostream>
#include <memory>
using namespace power;
void require(bool b, const char* text) {
  if (!b) throw std::runtime_error(text);
}
Audio render(Audio input, bool field, float amount, std::size_t block,
             bool extreme = false) {
  if (field) {
    auto dsp = std::make_unique<aste::field::Processor>();
    dsp->prepare(input.rate);
    aste::field::Parameters p;
    p.mass = extreme ? 1.F : .7F;
    p.grain = .65F;
    p.pitch = .6F;
    p.motion = .72F;
    p.distance = .18F;
    p.blend = 1;
    p.outputDb = -3;
    p.texture = .85F;
    p.focus = amount;
    p.forever = extreme;
    for (std::size_t at = 0; at < input.left.size(); at += block)
      dsp->process(input.left.data() + at, input.right.data() + at,
                   std::min(block, input.left.size() - at), p);
  } else {
    aste::density::Parameters p;
    p.driveDb = extreme ? 24.F : 6.F;
    p.crush = .9F;
    p.attackMs = 9;
    p.releaseMs = 75;
    p.density = .7F;
    p.blend = .85F;
    p.outputDb = -6;
    p.character = amount;
    aste::density::Processor dsp;
    dsp.prepare(input.rate, p);
    for (std::size_t at = 0; at < input.left.size(); at += block)
      dsp.process(input.left.data() + at, input.right.data() + at,
                  std::min(block, input.left.size() - at), p);
  }
  validate(input);
  return input;
}
double bin(const Audio& a, double hz) {
  double real = 0, imag = 0;
  const auto begin = a.left.size() / 2;
  for (std::size_t i = begin; i < a.left.size(); ++i) {
    const double phase = 2 * pi * hz * i / a.rate;
    real += a.left[i] * std::cos(phase);
    imag += a.left[i] * std::sin(phase);
  }
  return std::hypot(real, imag) / (a.left.size() - begin);
}
int main(int argc, char** argv) {
  try {
    for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
      for (bool field : {false, true}) {
        auto input = fixture(5, rate, 3);
        const auto a = render(input, field, 1, 127, true),
                   b = render(input, field, 1, 511, true);
        require(a.left == b.left && a.right == b.right,
                "Processing depends on buffer partition");
        require(measure(a).peak < 8, "Extreme output exceeds bound");
      }
    for (bool field : {false, true}) {
      auto input = fixture(5, 8000, 60);
      const auto a = render(input, field, 1, 511, true);
      require(measure(a).peak < 8, "Long extreme render exceeds bound");
    }
    Audio tone(48000 * 4);
    for (std::size_t i = 0; i < tone.left.size(); ++i)
      tone.left[i] = tone.right[i] =
          .2F * std::sin(2 * pi * 997 * i / tone.rate);
    const auto plain = render(tone, false, 0, 127),
               colored = render(tone, false, .8F, 127);
    const double evenBefore = bin(plain, 1994) / bin(plain, 997),
                 evenAfter = bin(colored, 1994) / bin(colored, 997);
    require(evenAfter > evenBefore + .001,
            "Character does not add asymmetric harmonics");
    for (std::size_t i = 0; i < tone.left.size(); ++i)
      tone.left[i] = tone.right[i] =
          .15F * (std::sin(2 * pi * 60 * i / tone.rate) +
                  std::sin(2 * pi * 2000 * i / tone.rate));
    const auto dark = render(tone, true, 0, 127),
               focused = render(tone, true, .75F, 127);
    const double lowBefore = bin(dark, 60) / bin(dark, 2000),
                 lowAfter = bin(focused, 60) / bin(focused, 2000);
    require(lowAfter < lowBefore,
            "Focus does not reduce bass accumulation relative to treble");
    std::cout << "H2/fundamental " << evenBefore << " -> " << evenAfter
              << "; wet 60/2000 Hz " << lowBefore << " -> " << lowAfter << '\n';
    for (std::size_t i = 0; i < tone.left.size(); ++i)
      tone.left[i] = tone.right[i] =
          .2F * std::sin(2 * pi * 7001 * i / tone.rate);
    const auto aliasBefore = render(tone, false, 0, 127),
               aliasAfter = render(tone, false, .8F, 127);
    require(bin(aliasAfter, 12995) / bin(aliasAfter, 7001) <
                bin(aliasBefore, 12995) / bin(aliasBefore, 7001),
            "Character increases the selected high-frequency fold");
    std::cout << "7001 Hz folded H5 dBc "
              << 20 * std::log10(bin(aliasBefore, 12995) /
                                 bin(aliasBefore, 7001))
              << " -> "
              << 20 * std::log10(bin(aliasAfter, 12995) / bin(aliasAfter, 7001))
              << '\n';
    if (argc == 2) {
      const std::filesystem::path folder{argv[1]};
      require(!std::filesystem::exists(folder), "Use a fresh output folder");
      std::filesystem::create_directories(folder);
      auto csv = output(folder / "measurements.csv");
      csv << "product,source,amount,raw_peak,raw_rms,matched_lufs,cpu_realtime_"
             "percent\n";
      for (bool field : {false, true})
        for (int source : {0, 2, 4, 5, 6}) {
          const std::array<float, 3> amounts{0, .45F, field ? .75F : .8F};
          std::array<Audio, 3> audio;
          std::array<double, 3> costs;
          for (std::size_t j = 0; j < 3; ++j) {
            const auto start = std::chrono::steady_clock::now();
            audio[j] = render(fixture(source), field, amounts[j], 127);
            costs[j] = std::chrono::duration<double>(
                           std::chrono::steady_clock::now() - start)
                           .count() /
                       8 * 100;
          }
          const auto original = audio;
          double target = -23;
          for (int pass = 0; pass < 8; ++pass) {
            double nextTarget = target;
            for (std::size_t j = 0; j < 3; ++j) {
              auto level = loudness(original[j]);
              require(level.has_value(), "Unmeasurable listening source");
              double scale = gain(target - *level);
              for (int iteration = 0; iteration < 8; ++iteration) {
                audio[j] = original[j];
                for (auto* channel : {&audio[j].left, &audio[j].right})
                  for (auto& x : *channel) x *= scale;
                auto m = measure(audio[j]);
                require(m.lufs.has_value(), "Match lost gated loudness");
                if (std::abs(*m.lufs - target) < .0001) break;
                scale *= gain(target - *m.lufs);
              }
              const auto m = measure(audio[j]);
              require(std::abs(*m.lufs - target) < .05, "Match failed");
              if (m.peak > .8)
                nextTarget = std::min(
                    nextTarget, target - 20 * std::log10(m.peak / .8) - .00001);
            }
            if (nextTarget == target) break;
            target = nextTarget;
          }
          for (std::size_t j = 0; j < 3; ++j) {
            const std::string product = field ? "field" : "density";
            wav(folder / (product + "-source" + std::to_string(source) + "-" +
                          std::to_string(j) + ".wav"),
                audio[j]);
            const auto raw = measure(original[j]), m = measure(audio[j]);
            require(m.peak <= .8001, "Listening peak exceeds bound");
            csv << product << ',' << source << ',' << amounts[j] << ','
                << raw.peak << ',' << raw.rms << ',' << *m.lufs << ','
                << costs[j] << '\n';
          }
        }
    }
    std::cout << "character laboratory: passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
