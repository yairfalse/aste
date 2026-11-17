#include "apps/non-machine/non_machine_processor.hpp"
#include "apps/field/nested_memory.hpp"
#include "research/quality/audio.hpp"
#include <iostream>
using namespace power;
void require(bool condition, const char* message) {
  if (!condition) throw std::runtime_error(message);
}
Audio object(unsigned material, float structure, float roughness, float bend,
             double rate, std::size_t block) {
  Audio a(static_cast<std::size_t>(rate * 2), rate);
  auto p = aste::nonmachine::studyPattern();
  for (auto& track : p.tracks)
    for (auto& event : track.events) event.enabled = false;
  auto& track = p.tracks[0];
  track.material = material;
  track.structure = structure;
  track.roughness = roughness;
  track.bend = bend;
  track.strike = .8F;
  auto& event = track.events[0];
  event.pitch = material == 3 ? 1800.F : 180.F;
  event.decay = .22F;
  event.color = .65F;
  aste::nonmachine::Engine engine;
  require(engine.prepare(rate, p), "Object engine prepare failed");
  for (std::size_t at = 0; at < a.left.size(); at += block) {
    const auto frames = std::min(block, a.left.size() - at);
    const std::array hit{aste::nonmachine::ManualHit{0, 0, 0, 1}};
    engine.process(a.left.data() + at, a.right.data() + at, frames,
                   {false, true, false, static_cast<std::int64_t>(at),
                    at * 2. / rate, 120, true},
                   nullptr, {},
                   at == 0 ? std::span(hit)
                           : std::span<const aste::nonmachine::ManualHit>{});
  }
  validate(a);
  return a;
}
double shapeDifference(Audio a, Audio b) {
  const auto ma = measure(a), mb = measure(b);
  require(ma.rms > 1.e-8 && mb.rms > 1.e-8, "Silent material");
  for (std::size_t i = 0; i < a.left.size(); ++i) {
    a.left[i] /= ma.rms;
    a.right[i] /= ma.rms;
    b.left[i] /= mb.rms;
    b.right[i] /= mb.rms;
  }
  return difference(a, b);
}
void temporal(double rate) {
  aste::field::NestedMemory a, b;
  a.prepare(rate);
  b.prepare(rate);
  aste::field::NestedMemory::Controls c;
  c.outerMs = 200;
  c.innerMs = 37;
  for (std::size_t i = 0; i < static_cast<std::size_t>(rate * .5); ++i) {
    float x = static_cast<float>(std::sin(i * .071) + .2 * std::sin(i * .004));
    a.process(x, -x, c);
    b.process(x, -x, c);
  }
  c.hold = true;
  for (std::size_t i = 0; i < static_cast<std::size_t>(rate * .25); ++i) {
    const auto x = a.process(0, 0, c), y = b.process(0, 0, c);
    require(x == y, "Capture not deterministic");
  }
  require(a.active() && a.bytes() < 13U * 1024 * 1024,
          "Outer capture/memory bound failed");
  const auto reference = c;
  c.inner = true;
  std::vector<float> captured;
  for (std::size_t i = 0; i < static_cast<std::size_t>(rate * .125); ++i) {
    auto x = a.process(0, 0, c);
    b.process(0, 0, reference);
    captured.push_back(x[0]);
    require(std::abs(x[0] + x[1]) < 1.e-6, "Stereo relation lost");
  }
  const auto length = static_cast<std::size_t>(rate * 37 * .001);
  for (std::size_t i = static_cast<std::size_t>(rate * .003) + 1;
       i + length < captured.size(); ++i)
    require(std::abs(captured[i] - captured[i + length]) < 1.e-6,
            "Inner fragment is not exact repeating audio");
  c.inner = false;
  for (std::size_t i = 0; i < 2000; ++i) {
    auto x = a.process(0, 0, c), y = b.process(0, 0, reference);
    if (i > rate * .003)
      require(x == y, "Release did not rejoin advancing outer phrase");
  }
  c.hold = false;
  for (int i = 0; i < static_cast<int>(rate * .005); ++i) {
    auto x = a.process(.123F, -.234F, c);
    if (i > rate * .003)
      require(x[0] == .123F && x[1] == -.234F,
              "Outer release did not return live input");
  }
  c.hold = true;
  c.inner = true;
  for (int i = 0; i < 6000; ++i) {
    c.outerMs = i % 511 < 255 ? 20.F : 4000.F;
    c.innerMs = i % 127 < 64 ? 4.F : 1000.F;
    c.speed = i % 127 < 64 ? .25F : 2.F;
    c.reverse = i % 251 < 125;
    const auto x = a.process(.2F, -.2F, c);
    require(std::isfinite(x[0]) && std::isfinite(x[1]) &&
                std::abs(x[0]) < 2.F && std::abs(x[1]) < 2.F,
            "Extreme nested changes are not bounded");
  }
  a.reset();
  c.hold = true;
  a.process(0, 0, c);
  require(!a.active(), "Reset reused stale captured audio");
}
int main(int argc, char** argv) {
  try {
    for (double rate :
         {8000., 44100., 48000., 88200., 96000., 176400., 192000.})
      temporal(rate);
    for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
      for (unsigned material = 1; material <= 4; ++material) {
        auto a = object(material, .7F, .4F, 15, rate, 127),
             b = object(material, .7F, .4F, 15, rate, 511);
        require(a.left == b.left && a.right == b.right,
                "Material depends on buffer partition");
        require(measure(a).peak < 1, "Material output exceeds bound");
      }
    for (unsigned material = 1; material <= 4; ++material) {
      const auto plain = object(material, .2F, 0, 0, 48000, 127),
                 changed = object(material, .8F, .7F, 18, 48000, 127);
      const auto delta = shapeDifference(plain, changed);
      require(delta > .1, "Material macros only change gain");
      std::cout << "material " << material << " normalized difference " << delta
                << '\n';
    }
    if (argc == 2) {
      std::filesystem::path folder{argv[1]};
      require(!std::filesystem::exists(folder), "Use a fresh folder");
      std::filesystem::create_directories(folder);
      for (unsigned material = 0; material <= 4; ++material)
        for (int variant = 0; variant < 2; ++variant) {
          auto a = object(material, variant ? .8F : .2F, variant ? .7F : 0,
                          variant ? 18 : 0, 48000, 127);
          const auto m = measure(a);
          const double scale = m.peak > .0 ? .7 / m.peak : 1;
          wav(folder / ("object-" + std::to_string(material) + "-" +
                        std::to_string(variant) + ".wav"),
              a, scale);
        }
      // Identical live phrase, capture at 2 s; inner 90 ms at 3 s, reverse at
      // 4 s, resize to 37 ms at 5 s, release inner at 6 s, live return at 7 s.
      Audio audio = fixture(5);
      aste::field::NestedMemory memory;
      memory.prepare(48000);
      aste::field::NestedMemory::Controls c;
      c.outerMs = 1500;
      c.innerMs = 90;
      for (std::size_t i = 0; i < audio.left.size(); ++i) {
        const double t = i / 48000.;
        c.hold = t >= 2 && t < 7;
        c.inner = t >= 3 && t < 6;
        c.reverse = t >= 4 && t < 6;
        c.innerMs = t >= 5 ? 37 : 90;
        const auto x = memory.process(audio.left[i], audio.right[i], c);
        audio.left[i] = x[0];
        audio.right[i] = x[1];
      }
      wav(folder / "field-capture-inner-release.wav", audio);
    }
    std::cout << "object / nested memory laboratory: passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
