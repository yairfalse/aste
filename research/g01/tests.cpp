#include "core.hpp"
#include "state.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#ifdef G01_MAC_AUDIT
#include <pthread.h>
#include <cstdio>
#include <unistd.h>
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif
namespace {
thread_local bool audit = false;
thread_local std::size_t operations = 0;
}  // namespace
void* operator new(std::size_t n) {
  if (audit) ++operations;
  if (auto* p = std::malloc(std::max(std::size_t{1}, n))) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  if (audit) ++operations;
  std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
using namespace aste::g01;
void require(bool ok, const char* text) {
  if (!ok) throw std::runtime_error(text);
}
std::vector<float> render(double rate, std::size_t block, bool sinc = false) {
  Engine e;
  require(e.prepare(rate, .12), "prepare");
  auto c = scene(3);
  for (auto& n : c.node) {
    n.captureSeconds = .04F;
    n.sizeMs = 70;
    n.density = 85;
    n.interpolation = sinc ? Interpolation::Sinc16 : Interpolation::Linear;
  }
  const auto size = static_cast<std::size_t>(rate * .65);
  std::vector<float> input(size), l(size), r(size);
  for (std::size_t i = 0; i < size; ++i)
    input[i] = i < rate * .08 ? static_cast<float>(.4 * std::sin(i * .071)) : 0;
  auto changed = c;
  changed.routes = scene(2).routes;
  changed.node[0].speed = .03F;
  changed.node[1].reverse = false;
  for (std::size_t at = 0; at < size; at += block) {
    auto n = std::min(block, size - at);
    std::array<Change, 2> changes;
    std::size_t count = 0;
    for (const auto& x :
         std::array<Change, 2>{{{static_cast<std::size_t>(rate * .17), changed},
                                {static_cast<std::size_t>(rate * .31), c}}})
      if (x.offset >= at && x.offset < at + n) {
        changes[count] = x;
        changes[count++].offset -= at;
      }
    std::array<Event, 1> events;
    std::size_t eventsCount = 0;
    auto clearAt = static_cast<std::size_t>(rate * .44);
    if (clearAt >= at && clearAt < at + n)
      events[eventsCount++] = {clearAt - at, Action::Clear, 1};
    // Each block's base is the latest automation value, not a reset to initial.
    const auto& base = at >= static_cast<std::size_t>(rate * .31)   ? c
                       : at >= static_cast<std::size_t>(rate * .17) ? changed
                                                                    : c;
    operations = 0;
#ifdef G01_MAC_AUDIT
    asteRealtimeAuditReset();
    asteRealtimeAuditSetActive(1);
#endif
    audit = true;
    e.process(input.data() + at, input.data() + at, l.data() + at,
              r.data() + at, n, base, {}, std::span(events).first(eventsCount),
              std::span(changes).first(count));
    audit = false;
#ifdef G01_MAC_AUDIT
    asteRealtimeAuditSetActive(0);
    require(asteRealtimeAuditLockCalls() == 0 &&
                asteRealtimeAuditFileOpenCalls() == 0 &&
                asteRealtimeAuditWriteCalls() == 0,
            "OS callback audit");
#endif
    require(operations == 0, "callback allocated or freed");
  }
  for (float x : l)
    require(std::isfinite(x) && std::abs(x) <= 1, "finite bounded output");
  require(e.snapshot().node[1].commits > 0, "downstream recapture");
  return l;
}
int main() {
  try {
#ifdef G01_MAC_AUDIT
    pthread_mutex_t m;
    pthread_mutex_init(&m, nullptr);
    asteRealtimeAuditReset();
    asteRealtimeAuditSetActive(1);
    pthread_mutex_lock(&m);
    pthread_mutex_unlock(&m);
    auto* f = std::fopen("/dev/null", "r");
    if (f) std::fclose(f);
    (void)::write(-1, "x", 1);
    asteRealtimeAuditSetActive(0);
    pthread_mutex_destroy(&m);
    require(asteRealtimeAuditLockCalls() > 0 &&
                asteRealtimeAuditFileOpenCalls() > 0 &&
                asteRealtimeAuditWriteCalls() > 0,
            "audit calibration");
#endif
    for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
      auto reference = render(rate, 127);
      for (std::size_t block : {1U, 7U, 511U, 2048U})
        require(render(rate, block) == reference,
                "block independent routes/automation/clear");
      require(render(rate, 127) == reference, "determinism");
    }
    require(render(48000, 127, true) == render(48000, 511, true),
            "sinc partition");
    for (float amount : {0.F, .5F, 1.F})
      require(morph(scene(1), scene(3), amount).node == scene(1).node,
              "routing morph preserves node sound at every amount");
    Controls saved = scene(3), copy;
    require(restore(save(saved), copy) && saved == copy,
            "configuration round trip");
    auto original = copy;
    require(!restore(save(saved) + "junk", copy) && copy == original,
            "malformed state rejected transactionally");
    require(!restore("G01LAB 2", copy), "unknown state version rejected");
    Engine e;
    require(!e.prepare(0) && !e.prepare(48000, 20), "invalid prepare");
    require(e.prepare(48000, .1), "prepare short");
    auto c = scene(3);
    float input[257]{}, l[257]{}, r[257]{};
    c.routes[0][0] = std::numeric_limits<float>::quiet_NaN();
    c.node[0].speed = std::numeric_limits<float>::infinity();
    input[0] = std::numeric_limits<float>::infinity();
    e.process(input, input, l, r, 257, c);
    require(e.snapshot().sanitized >= 3, "bad controls and input sanitized");
    std::array<Event, 1> bad{{{258, Action::Clear, 0}}};
    e.process(input, input, l, r, 257, c, {}, bad);
    require(e.snapshot().rejected == 1, "invalid event batch rejected");
    e.reset();
    c = scene(0);
    for (auto& n : c.node) n.captureSeconds = .08F;
    std::vector<float> in(4000, .2F), a(4000), b(4000);
    e.process(in.data(), in.data(), a.data(), b.data(), 2000, c);
    c.node[0].freeze = true;
    e.process(in.data(), in.data(), a.data(), b.data(), 2000, c);
    require(e.snapshot().node[0].storedFrames == 2000,
            "freeze seals partial capture");
    auto length = e.snapshot().node[0].storedFrames;
    e.process(nullptr, nullptr, a.data(), b.data(), 2000, c);
    require(e.snapshot().node[0].storedFrames == length,
            "frozen bank immutable");
    e.clear(0);
    c.node[0].capture = false;
    e.process(nullptr, nullptr, a.data(), b.data(), 2000, c);
    require(e.snapshot().node[0].storedFrames == 0, "clear forgets memory");
    Engine freeA, freeB;
    require(freeA.prepare(48000, .1) && freeB.prepare(48000, .1),
            "transport prepare");
    auto freeControls = scene(0);
    freeControls.node[0].captureSeconds = .04F;
    freeA.process(in.data(), in.data(), a.data(), b.data(), 4000, freeControls);
    freeB.process(in.data(), in.data(), a.data(), b.data(), 4000, freeControls);
    freeControls.freeze = true;
    std::vector<float> seekL(4000), seekR(4000);
    freeA.process(in.data(), in.data(), a.data(), b.data(), 4000, freeControls);
    freeB.process(nullptr, nullptr, seekL.data(), seekR.data(), 4000,
                  freeControls, {true, true, true, 999, 87});
    require(a == seekL && b == seekR,
            "Free frozen textures ignore DAW seek and tempo");
    auto syncControls = freeControls;
    syncControls.node[0].sync = true;
    freeA.process(nullptr, nullptr, a.data(), b.data(), 4000, syncControls,
                  {true, true, true, 0, 120});
    auto births = freeA.snapshot().node[0].launched;
    freeA.process(nullptr, nullptr, a.data(), b.data(), 4000, syncControls,
                  {true, false, false, 0, 120});
    require(freeA.snapshot().node[0].launched == births,
            "Stopped host stops synced grain births without clearing memory");
    e.reset();
    auto pressure = scene(3);
    for (auto& n : pressure.node) {
      n.sizeMs = 2000;
      n.density = 240;
      n.captureSeconds = .02F;
    }
    for (int i = 0; i < 16; ++i)
      e.process(in.data(), in.data(), a.data(), b.data(), 4000, pressure);
    for (const auto& m : e.snapshot().node) {
      require(m.active <= maxGrains && m.dropped > 0,
              "grain pool drops new births instead of growing");
      require(m.captureStalls > 0,
              "reader-pinned bank pressure stalls capture safely");
    }
    e.reset();
    auto dry = scene(0);
    dry.mix = 0;
    dry.master = .5F;
    e.process(in.data(), in.data(), a.data(), b.data(), 4000, dry);
    require(std::all_of(a.begin(), a.end(), [](float x) { return x == .1F; }),
            "master controls dry and wet mix together");
    std::cout << "G01 contracts passed: six rates, blocks, automation, "
                 "capture, immutable banks, realtime audits\n";
    return 0;
  } catch (const std::exception& ex) {
    std::cerr << ex.what() << '\n';
    return 1;
  }
}
