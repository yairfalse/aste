#include "circuit.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <vector>
#ifdef C01_RED_MAC_AUDIT
#include <pthread.h>
#include <unistd.h>
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif
namespace {
thread_local bool audit{};
thread_local std::size_t allocations{};
}  // namespace
void* operator new(std::size_t n) {
  if (audit) ++allocations;
  if (auto* p = std::malloc(std::max(n, std::size_t{1}))) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  if (audit) ++allocations;
  std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
using namespace aste::contamination;
void require(bool b, const char* s) {
  if (!b) throw std::runtime_error(s);
}
std::vector<float> render(double rate, std::size_t block,
                          revised::Architecture architecture,
                          bool extreme = false, bool hearSpace = true,
                          bool memoryOff = false) {
  revised::Engine e;
  require(e.prepare(rate, Engine::recommendedFactor(rate), architecture),
          "prepare");
  Controls c;
  c.sustain = .6;
  c.release = .1;
  if (extreme) {
    c.pressure = c.contamination = c.instability = c.memory = c.motion = 1;
    c.colour = 8000;
  }
  if (memoryOff) c.memory = 0;
  auto frames = static_cast<std::size_t>(rate * .4);
  std::vector<float> l(frames), r(frames);
  std::array<Event, 8> events{
      {{17, Kind::On, 48, .8},
       {17, Kind::On, 51, .8},
       {17, Kind::On, 55, .8},
       {17, Kind::On, 58, .8},
       {static_cast<std::size_t>(rate * .13), Kind::Off, 48, 0},
       {static_cast<std::size_t>(rate * .13), Kind::Off, 51, 0},
       {static_cast<std::size_t>(rate * .13), Kind::Off, 55, 0},
       {static_cast<std::size_t>(rate * .13), Kind::Off, 58, 0}}};
  for (std::size_t at = 0; at < frames; at += block) {
    auto n = std::min(block, frames - at);
    std::array<Event, 8> local{};
    std::size_t count = 0;
    for (auto event : events)
      if (event.offset >= at && event.offset < at + n) {
        event.offset -= at;
        local[count++] = event;
      }
    allocations = 0;
#ifdef C01_RED_MAC_AUDIT
    asteRealtimeAuditReset();
    asteRealtimeAuditSetActive(1);
#endif
    audit = true;
    e.process(l.data() + at, r.data() + at, n, c, std::span(local).first(count),
              hearSpace);
    audit = false;
#ifdef C01_RED_MAC_AUDIT
    asteRealtimeAuditSetActive(0);
    require(asteRealtimeAuditLockCalls() == 0 &&
                asteRealtimeAuditFileOpenCalls() == 0 &&
                asteRealtimeAuditWriteCalls() == 0,
            "OS audio audit");
#endif
    require(allocations == 0, "callback allocated/freed");
  }
  for (float v : l)
    require(std::isfinite(v) && std::abs(v) <= 2.01, "finite and bounded");
  for (int i = 0; i < 17; ++i)
    require(l[static_cast<std::size_t>(i)] == 0, "MIDI offset");
  return l;
}
int main() {
  try {
#ifdef C01_RED_MAC_AUDIT
    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, nullptr);
    asteRealtimeAuditReset();
    asteRealtimeAuditSetActive(1);
    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);
    auto* file = std::fopen("/dev/null", "r");
    if (file) std::fclose(file);
    (void)::write(-1, "x", 1);
    asteRealtimeAuditSetActive(0);
    pthread_mutex_destroy(&mutex);
    require(asteRealtimeAuditLockCalls() > 0 &&
                asteRealtimeAuditFileOpenCalls() > 0 &&
                asteRealtimeAuditWriteCalls() > 0,
            "audit calibration");
#endif
    for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
      for (auto architecture :
           {revised::Architecture::Dense, revised::Architecture::Resonant,
            revised::Architecture::Feedback, revised::Architecture::Full}) {
        auto a = render(rate, 127, architecture, true);
        require(a == render(rate, 511, architecture, true),
                "block partition dependence");
      }
    require(
        render(48000, 127, revised::Architecture::Dense, true, false) ==
            render(48000, 127, revised::Architecture::Feedback, true, false),
        "one-way feedback cannot alter dry source");
    require(render(48000, 127, revised::Architecture::Full, false, false,
                   true) == render(48000, 127, revised::Architecture::Resonant,
                                   false, false, true),
            "Feedback zero removes network influence from the dry source");
    auto a = render(48000, 1, revised::Architecture::Full);
    require(a == render(48000, 127, revised::Architecture::Full),
            "single-sample partition");
    revised::Engine e;
    require(!e.prepare(0) && !e.prepare(48000, 3), "invalid prepare");
    require(e.prepare(48000), "valid prepare");
    Controls c;
    std::array<float, 256> l{}, r{};
    e.process(l.data(), r.data(), l.size(), c);
    require(std::all_of(l.begin(), l.end(), [](float x) { return x == 0; }),
            "unexcited space silent");
    c.colour = std::numeric_limits<double>::quiet_NaN();
    e.event({0, Kind::On, 60, .8});
    e.process(l.data(), r.data(), l.size(), c);
    e.event({0, Kind::Panic});
    allocations = 0;
    audit = true;
    e.process(l.data(), r.data(), l.size(), c);
    audit = false;
    require(allocations == 0, "panic has no allocation");
    require(std::all_of(l.begin(), l.end(), [](float x) { return x == 0; }),
            "panic clears source and feedback");
    revised::Engine connected, oneway;
    connected.prepare(48000);
    oneway.prepare(48000);
    connected.setInteraction(false);
    oneway.setInteraction(false);
    Controls memory;
    memory.memory = .9;
    memory.pressure = .7;
    memory.colour = 750;
    memory.sustain = 0;
    memory.release = .08;
    std::vector<float> oldL(48000), oldR(48000);
    for (auto* engine : {&connected, &oneway}) {
      for (int note : {48, 51, 55, 58}) engine->event({0, Kind::On, note, .75});
      engine->process(oldL.data(), oldR.data(), 6720, memory);
      for (int note : {48, 51, 55, 58}) engine->event({0, Kind::Off, note, 0});
      engine->process(oldL.data(), oldR.data(), 48000 - 6720, memory);
    }
    connected.setInteraction(true);
    std::array<float, 2048> coupledL{}, coupledR{}, plainL{}, plainR{};
    for (auto* engine : {&connected, &oneway})
      for (int note : {48, 51, 55, 58}) engine->event({0, Kind::On, note, .75});
    connected.process(coupledL.data(), coupledR.data(), 2048, memory, {},
                      false);
    oneway.process(plainL.data(), plainR.data(), 2048, memory, {}, false);
    double delta = 0, reference = 0;
    for (std::size_t i = 0; i < 2048; ++i) {
      delta += std::pow(coupledL[i] - plainL[i], 2);
      reference += plainL[i] * plainL[i];
    }
    double ratio = std::sqrt(delta / std::max(reference, 1.e-30));
    std::cout << "Prior-space influence on dry next chord: " << ratio << '\n';
    require(ratio > .001,
            "stored space must influence new excitation, not just add an echo");
    std::cout << "Revised C01 contracts passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
