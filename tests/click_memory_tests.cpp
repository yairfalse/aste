#include "click_memory.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <new>
#include <thread>

namespace {
thread_local bool audit{};
thread_local int allocations{};
}  // namespace
void* operator new(std::size_t n) {
  if (audit) ++allocations;
  if (auto* p = std::malloc(std::max<std::size_t>(1, n))) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  if (audit) ++allocations;
  std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void require(bool condition, const char* message) {
  if (!condition) {
    std::cerr << "FAIL: " << message << '\n';
    std::exit(1);
  }
}
using namespace aste::click;
int main() {
  MasterMemory memory;
  require(memory.prepare(48000), "prepare memory");
  MemoryControls c;
  c.captureBeats = .125;
  std::vector<float> l(3000), r(3000);
  for (std::size_t n = 0; n < l.size(); ++n)
    l[n] = r[n] = .4F * std::sin(n * .05);
  const auto source = l;
  memory.process(l.data(), r.data(), l.size(), c, 120);
  require(l == source, "memory bypass preserves original audio exactly");
  audit = true;
  memory.process(l.data(), r.data(), 1, c, 120, MasterMemory::Capture);
  audit = false;
  require(allocations == 0, "capture has no allocation or free");
  require(memory.count() == 1, "instant rendered audio capture");
  const auto original = memory.state().audio[0];
  require(original.size() == 6000 && original[200] == source[100],
          "capture chronology");
  c.play = true;
  c.generation[0].pitch = -12;
  c.generation[0].cutoff = 1800;
  c.generation[0].resonance = .7F;
  c.generation[0].morph = .4F;
  c.generation[0].playback = 1;
  memory.process(l.data(), r.data(), 1, c, 120, MasterMemory::Resample);
  std::atomic<bool> done{};
  std::thread saving([&] {
    while (!done.load()) {
      const auto state = memory.state();
      require(state.audio[0] == original,
              "concurrent save retains original PCM");
    }
  });
  audit = true;
  for (int n = 0; n < 4; ++n)
    memory.process(l.data(), r.data(), l.size(), c, 120);
  audit = false;
  done.store(true);
  saving.join();
  require(allocations == 0, "resample has no allocation or free");
  const auto printed = memory.state();
  require(memory.count() == 2 && memory.selected() == 1,
          "resample publishes a new generation");
  require(printed.audio[1].size() == 12000,
          "octave-down print contains twice as many actual frames");
  require(
      printed.audio[0] == original && printed.audio[1][200] != original[200],
      "print is transformed PCM, not a recipe");
  require(std::any_of(printed.audio[1].begin(), printed.audio[1].end(),
                      [](float x) { return std::abs(x) > .001F; }),
          "printed PCM contains audible signal");
  require(c.generation[1].pitch == 0 && c.generation[1].cutoff == 20000,
          "generation starts neutral");
  memory.process(l.data(), r.data(), 1, c, 120, MasterMemory::Original);
  require(memory.selected() == 0, "original recall");
  memory.process(l.data(), r.data(), 1, c, 120, MasterMemory::Next);
  require(memory.selected() == 1, "next recall");
  MasterMemory restored;
  restored.prepare(48000);
  restored.restore(printed);
  require(restored.state().audio == printed.audio,
          "bit exact state round trip");
  MemoryControls repeat;
  repeat.selected = 1;
  repeat.repeat = true;
  repeat.division = 4;
  restored.process(l.data(), r.data(), 1500, repeat, 120, MasterMemory::None,
                   true);
  require(restored.position() == 0,
          "1/64 repeat period is exactly 1500 samples at 120 BPM");
  restored.process(l.data(), r.data(), 750, repeat, 240, MasterMemory::None,
                   true);
  require(restored.position() == 0, "tempo change keeps exact repeat division");
  MasterMemory shortRegion;
  shortRegion.prepare(48000);
  shortRegion.restore(printed);
  repeat.division = 0;
  for (int n = 0; n < 4; ++n) {
    std::fill(l.begin(), l.end(), 0);
    std::fill(r.begin(), r.end(), 0);
    shortRegion.process(l.data(), r.data(), l.size(), repeat, 120);
  }
  require(std::all_of(l.begin(), l.end(),
                      [](float x) { return std::abs(x) < 1e-8F; }),
          "short fragment leaves silence until the selected rhythmic division");
  for (int n = 0; n < 4; ++n)
    shortRegion.process(l.data(), r.data(), l.size(), repeat, 120);
  require(shortRegion.position() == 0,
          "short fragment retains exact quarter-note cycle");
  repeat.generation[1].pitch = -48;
  repeat.generation[1].speed = .125F;
  repeat.repeat = false;
  restored.process(l.data(), r.data(), 1, repeat, 120, MasterMemory::Resample);
  require(restored.status() == 4 && restored.count() == 2,
          "oversized print rejected rather than silently shortened");
  MasterMemory changedRate;
  changedRate.prepare(96000);
  changedRate.restore(printed);
  require(changedRate.state().audio[0].size() == original.size() * 2,
          "sample rate restore preserves duration");
  for (int n = 2; n < memoryGenerations; ++n)
    memory.process(l.data(), r.data(), l.size(), c, 120, MasterMemory::Capture);
  memory.process(l.data(), r.data(), 1, c, 120, MasterMemory::Capture);
  require(memory.count() == 8 && memory.status() == 2,
          "bounded history refuses overwrite");
  require(memory.state().audio[0] == original, "full history retains original");
  c.repeat = true;
  for (int n = 0; n < 200; ++n) {
    c.division = n % 6;
    c.selected = n % 8;
    for (auto& p : c.generation) {
      p.pitch = n % 2 ? 48 : -48;
      p.speed = n % 2 ? 4 : .125F;
      p.resonance = .95F;
      p.cutoff = n % 2 ? 30 : 20000;
      p.morph = (n % 4) / 3.F;
      p.reverse = n % 2;
    }
    memory.process(l.data(), r.data(), l.size(), c, n % 2 ? 20 : 400,
                   MasterMemory::None, n % 3 == 0);
    for (float x : l)
      require(std::isfinite(x) && std::abs(x) <= .98F,
              "bounded abrupt automation / transport extremes");
  }
  std::cout << "capture / PCM resampling / history / concurrent save / rate "
               "restoration / RT allocation / extreme automation: PASS\n"
            << "48k memory bytes: " << memory.heapBytes() << '\n';
}
