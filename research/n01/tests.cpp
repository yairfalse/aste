#include "core.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
#include <new>
#include <cstdio>
#ifdef N01_MAC_AUDIT
#include <pthread.h>
#include <unistd.h>
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif

namespace {
thread_local bool auditAllocations{};
thread_local std::size_t allocated{}, released{};
}  // namespace
void* operator new(std::size_t n) {
  if (auditAllocations) ++allocated;
  if (auto* p = std::malloc(std::max<std::size_t>(n, 1))) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  if (auditAllocations) ++released;
  std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

using namespace aste::n01;
namespace {
int failures{};
void check(bool condition, const char* name) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL " << name << '\n';
  }
}
struct Render {
  std::vector<float> audio;
  std::vector<Hit> hits;
};
Render render(Pattern p, std::span<const std::size_t> blocks,
              double rate = 48000) {
  Engine e;
  check(e.prepare(rate, p), "valid setup accepted");
  Render result{std::vector<float>(static_cast<std::size_t>(rate * 8)), {}};
  std::array<Hit, 512> hits{};
  std::size_t at{}, b{};
  while (at < result.audio.size()) {
    const auto count =
        std::min(blocks[b++ % blocks.size()], result.audio.size() - at);
    Trace trace{hits};
    e.process(result.audio.data() + at, nullptr, count,
              {true, true, false, static_cast<std::int64_t>(at),
               at * 2.0 / rate, 120},
              &trace);
    check(trace.dropped == 0, "trace complete");
    result.hits.insert(result.hits.end(), hits.begin(),
                       hits.begin() + trace.size);
    at += count;
  }
  return result;
}
void scheduler() {
  Pattern p;
  p.tracks[0].length = 2;
  p.tracks[0].events[0] = {.enabled = true,
                           .beat = .5,
                           .microtiming = -.125,
                           .every = 2,
                           .repeats = 3,
                           .interval = .25,
                           .acceleration = .5};
  const std::array<std::size_t, 1> blocks{127};
  auto a = render(p, blocks);
  const std::array<std::int64_t, 3> expected{9000, 15000, 18000};
  check(a.hits.size() == 12, "condition skips alternate cycles");
  for (std::size_t i = 0; i < expected.size(); ++i)
    check(a.hits.size() > i && a.hits[i].sample == expected[i],
          "analytic sample oracle");
  check(a.hits[1].gain < a.hits[0].gain && a.hits[2].gain < a.hits[1].gain,
        "retrigger amplitude decays");
  p.tracks[0].events[0].probability = 0;
  check(render(p, blocks).hits.empty(), "zero probability means silence");
  p = {};
  p.tracks[0].length = .25;
  p.tracks[0].events[0].enabled = true;
  const auto all = render(p, blocks).hits;
  p.tracks[0].events[0].probability = .5F;
  const auto accepted = render(p, blocks).hits;
  check(!accepted.empty() && accepted.size() < all.size(),
        "fractional probability both accepts and rejects known-seed events");
}
void determinism() {
  const std::array<std::size_t, 1> fixed{127};
  const std::array<std::size_t, 5> irregular{1, 7, 128, 511, 2048};
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    auto a = render(studyPattern(), fixed, rate);
    auto b = render(studyPattern(), irregular, rate);
    check(a.hits == b.hits, "hit trace independent of partition");
    check(a.audio == b.audio, "audio independent of partition");
    check(std::all_of(a.audio.begin(), a.audio.end(),
                      [](float v) { return std::isfinite(v); }),
          "finite audio across six rates");
  }
  auto p = studyPattern();
  auto a = render(p, fixed);
  ++p.seed;
  check(a.audio != render(p, fixed).audio, "seed changes sound");
}
void boundaries() {
  Engine e;
  auto p = studyPattern();
  p.tracks[0].events[0].pitch = std::numeric_limits<float>::quiet_NaN();
  check(!e.prepare(48000, p), "reject NaN lock");
  check(!e.prepare(0, studyPattern()), "reject invalid sample rate");
  check(e.prepare(48000, studyPattern()), "prepare");
  std::array<float, 100> out{};
  std::array<Hit, 32> hits{};
  Trace trace{hits};
  e.process(out.data(), nullptr, 0, {}, &trace);
  check(e.resets() == 0 && trace.size == 0, "zero frame is inert");
  e.process(out.data(), nullptr, out.size(), {}, &trace);
  check(trace.size == 1 && hits[0].sample == 0, "event at block origin once");
  e.process(out.data(), nullptr, out.size(), {true, true, true, 0, 0, 120});
  Engine fresh;
  check(fresh.prepare(48000, studyPattern()), "fresh prepare");
  std::array<float, 100> reference{};
  fresh.process(reference.data(), nullptr, reference.size(), {});
  check(out == reference, "seek resets sound deterministically");
  e.process(out.data(), nullptr, out.size(), {true, false, false, 100, 0, 120});
  check(std::all_of(out.begin(), out.end(), [](float v) { return v == 0; }),
        "missing timing does not fabricate clock");
}
void fragments() {
  Fragment f;
  f.prepare(8);
  check(!f.capture(1, 0, 0, true), "empty capture rejected");
  for (int i = 0; i < 12; ++i) f.record(static_cast<float>(i));
  check(!f.capture(9, 0, 0, true), "oversized capture rejected");
  check(!f.capture(1, std::numeric_limits<std::size_t>::max(), 0, true),
        "lookback cannot overflow");
  check(f.capture(4, 2, 0, true), "wrapped historical selection accepted");
  for (float expected : {6.F, 7.F, 8.F, 9.F, 6.F}) {
    f.record(-1);
    check(f.process(99) == expected,
          "frozen bank is not overwritten by live recording");
  }
  f.reverse();
  check(f.process(99) == 8, "reverse reads selected fragment backwards");
  check(f.shorten(2), "shorten accepted");
  check(f.process(99) == 7, "shorten bounds read position");
  check(!f.shorten(3), "shorten cannot expand selection");
  f.release(true);
  check(f.process(99) == 99, "hard release returns live immediately");
  f.clear();
  check(f.available() == 0 && !f.active(),
        "clear invalidates stale audio without reading it");
  for (int i = 0; i < 8; ++i) f.record(1);
  check(f.capture(8, 0, 3, true), "finite repeat accepted");
  check(f.process(0) == 1 && f.process(0) == 1 && f.process(0) == 1 &&
            f.process(0) == 0,
        "duration ends on exact sample");
  f.prepare(256);
  for (int i = 0; i < 256; ++i) f.record(1);
  check(f.capture(128, 0, 0, false), "smooth capture accepted");
  float previous{}, maximum{};
  for (int i = 0; i < 256; ++i) {
    const auto x = f.process(0);
    maximum = std::max(maximum, std::abs(x - previous));
    previous = x;
  }
  check(maximum <= 1.0F / 32 + 1e-6F,
        "edge taper bounds constant-fragment wrap jump");
}

struct Scenario {
  std::vector<float> audio;
  double phrase{};
  std::size_t rejected{};
};
Scenario scenario(std::size_t block, Mode mode, bool transformed,
                  bool rejoin = false) {
  auto p = studyPattern();
  p.voidStart =
      p.voidEnd;  // Isolate replacement semantics from structural silence.
  Engine e;
  check(e.prepare(48000, p), "scenario prepared");
  Scenario result{std::vector<float>(96000)};
  std::array commands{
      Command{24000, 1, Action::Capture, 1200, 0, 22000, Mode::Continue, true},
      Command{24000, 3, Action::Capture, 2400, 0, 0, mode, true},
      Command{30000, 1, Action::Reverse},
      Command{32000, 3, Action::Shorten, 1200},
      Command{48000, 3, Action::Release, 0, 0, 0, mode, true},
      Command{72000, 3, rejoin ? Action::Rejoin : Action::Release}};
  for (std::size_t at = 0; at < result.audio.size();) {
    const auto count = std::min(block, result.audio.size() - at);
    std::array<Command, 8> batch{};
    std::size_t size{};
    if (transformed)
      for (auto command : commands)
        if (command.offset >= at && command.offset < at + count) {
          command.offset -= at;
          batch[size++] = command;
        }
    e.process(
        result.audio.data() + at, nullptr, count,
        {true, true, false, static_cast<std::int64_t>(at), at / 24000.0, 120},
        nullptr, std::span(batch).first(size));
    at += count;
  }
  result.phrase = e.phraseBeat();
  result.rejected = e.rejected();
  return result;
}
void nested() {
  const auto original = scenario(127, Mode::Continue, false);
  const auto continued = scenario(127, Mode::Continue, true);
  const auto suspended = scenario(127, Mode::Suspend, true);
  check(continued.rejected == 0 && suspended.rejected == 0,
        "nested commands accepted");
  check(continued.audio != original.audio, "capture makes audible change");
  check(std::equal(continued.audio.begin() + 48000, continued.audio.end(),
                   original.audio.begin() + 48000),
        "continue returns bit exactly to underlying phrase after release");
  check(std::abs(continued.phrase - 4) < 1e-9 &&
            std::abs(suspended.phrase - 3) < 1e-9,
        "suspend removes exactly one beat, continue removes none");
  check(continued.audio == scenario(511, Mode::Continue, true).audio,
        "nested continue independent of callback partition");
  check(suspended.audio == scenario(1, Mode::Suspend, true).audio,
        "nested suspend independent of callback partition");
  check(std::abs(scenario(127, Mode::Suspend, true, true).phrase - 4) < 1e-9,
        "rejoin restores host phase");
  // Structural silence overrides even an active master hold.
  auto p = studyPattern();
  p.voidStart = 1;
  p.voidEnd = 2;
  Engine e;
  check(e.prepare(48000, p), "void prepared");
  std::vector<float> audio(48000);
  const std::array commands{
      Command{12000, 3, Action::Capture, 12000, 0, 0, Mode::Continue, true}};
  e.process(audio.data(), nullptr, audio.size(), {}, nullptr, commands);
  check(std::all_of(audio.begin() + 24000, audio.end(),
                    [](float x) { return x == 0; }),
        "master repetition cannot fill a programmed void");
}

void hostAndRealtime() {
  Engine e;
  auto articulationPattern = studyPattern();
  for (auto& track : articulationPattern.tracks)
    for (auto& event : track.events)
      event.articulation = Articulation::Crossfade;
  check(e.prepare(48000, articulationPattern), "host test prepared");
  std::array<float, 512> out{};
  std::array<Hit, 512> hits{};
  Trace trace{hits};
  e.process(out.data(), nullptr, 1, {}, &trace);
  const auto resets = e.resets();
  e.process(out.data(), nullptr, 127, {true, true, false, 1, 1.0 / 24000, 60});
  check(e.resets() == resets, "tempo change does not reset continuous state");
  const double expected = 1.0 / 24000 + 127.0 / 48000;
  check(std::abs(e.phraseBeat() - expected) < 1e-12,
        "tempo segment has analytic beat length");
  e.process(out.data(), nullptr, 127,
            {true, true, false, 128, expected + 0.001, 60});
  check(e.resets() == resets + 1,
        "small PPQ discontinuity resets independently of block size");
  e.process(out.data(), nullptr, 127, {false, true, false, 255, 1, 60});
  check(std::all_of(out.begin(), out.begin() + 127,
                    [](float x) { return x == 0; }),
        "stop is silent");
  e.process(out.data(), nullptr, 127, {true, true, false, -24000, -1, 120});
  check(std::all_of(out.begin(), out.begin() + 127,
                    [](float x) { return x == 0; }),
        "negative preroll has no invented events");
  std::array commands{Command{0, 3, Action::Capture, 100}};
  e.process(out.data(), nullptr, 127, {true, true, true, 0, 0, 120}, nullptr,
            commands);
  check(e.rejected() == 1, "seek invalidates capture history");
  commands[0].offset = 512;
  e.process(out.data(), nullptr, 127,
            {true, true, false, 127, 127.0 / 24000, 120}, nullptr, commands);
  check(e.rejected() == 2, "out-of-block command rejected");
  auditAllocations = true;
  auto* probe = ::operator new(16);
  ::operator delete(probe);
  auditAllocations = false;
  check(allocated > 0 && released > 0,
        "allocation guard observes deliberate fault");
  allocated = released = 0;
#ifdef N01_MAC_AUDIT
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  asteRealtimeAuditReset();
  asteRealtimeAuditSetActive(1);
  pthread_mutex_lock(&mutex);
  pthread_mutex_unlock(&mutex);
  auto* file = std::fopen("/dev/null", "w");
  if (file) std::fclose(file);
  (void)::write(-1, "x", 1);
  asteRealtimeAuditSetActive(0);
  check(asteRealtimeAuditLockCalls() > 0 &&
            asteRealtimeAuditFileOpenCalls() > 0 &&
            asteRealtimeAuditWriteCalls() > 0,
        "OS audit observes deliberate lock/open/write");
  pthread_mutex_destroy(&mutex);
  asteRealtimeAuditReset();
  asteRealtimeAuditSetActive(1);
#endif
  auditAllocations = true;
  // Real processing including capture, reverse, shorten, release and seek.
  for (std::int64_t at = 0; at < 48000; at += 128) {
    const std::array actions{Command{0, 3, Action::Capture, 64, 0, 64},
                             Command{16, 3, Action::Reverse},
                             Command{32, 3, Action::Shorten, 16}};
    e.process(out.data(), out.data() + 256, 128,
              {true, true, at == 0, at, at / 24000.0, 120}, nullptr, actions);
  }
  auditAllocations = false;
#ifdef N01_MAC_AUDIT
  asteRealtimeAuditSetActive(0);
  check(asteRealtimeAuditLockCalls() == 0 &&
            asteRealtimeAuditFileOpenCalls() == 0 &&
            asteRealtimeAuditWriteCalls() == 0,
        "processing performs zero audited OS calls");
#endif
  check(allocated == 0 && released == 0,
        "processing performs zero new/delete calls");
  check(e.audioBytes() == 672000,
        "48k capture memory is bounded at 672000 bytes");
}

std::vector<float> transportStress(std::size_t block) {
  Engine e;
  check(e.prepare(48000, studyPattern()), "stress prepared");
  std::vector<float> audio(32000);
  double ppq{};
  for (std::size_t segment = 0; segment < 32; ++segment) {
    const double bpm = 60 + (segment * 37) % 280;
    const bool seek = segment == 7 || segment == 19;
    if (seek) ppq = segment == 7 ? -.02 : 2.75;
    for (std::size_t within = 0; within < 1000;) {
      const auto count = std::min(block, 1000 - within);
      const auto at = segment * 1000 + within;
      e.process(audio.data() + at, nullptr, count,
                {segment != 13, true, seek && within == 0,
                 static_cast<std::int64_t>(at),
                 ppq + within * bpm / (60 * 48000), bpm});
      within += count;
    }
    ppq += 1000 * bpm / (60 * 48000);
  }
  return audio;
}
void transportSegments() {
  const auto reference = transportStress(1);
  for (const auto block : {7U, 127U, 128U, 511U, 2048U})
    check(reference == transportStress(block),
          "tempo/stop/restart/seek segments independent of partition");
  Engine e;
  check(e.prepare(192000, studyPattern()), "highest rate capture setup");
  check(e.audioBytes() == 2688000,
        "capture memory bounded at maximum sample rate");
  std::array<float, 128> out{};
  std::array<Hit, 1> hits{};
  Trace trace{hits};
  Pattern p;
  for (auto& t : p.tracks) t.events[0].enabled = true;
  check(e.prepare(48000, p), "trace overflow setup");
  e.process(out.data(), nullptr, out.size(), {}, &trace);
  check(trace.size == 1 && trace.dropped == 2,
        "trace overflow reported, not silently complete");
  p.tracks[0].events[1] = {.enabled = true, .gain = .25F, .pitch = 400};
  check(e.prepare(48000, p), "collision setup");
  trace.size = trace.dropped = 0;
  e.process(out.data(), nullptr, 1, {}, &trace);
  check(hits[0].event == 1 && hits[0].pitch == 400,
        "exact event ties use later slot");
  const std::array bad{Command{20}, Command{10}};
  e.process(out.data(), nullptr, out.size(), {}, nullptr, bad);
  check(e.rejected() == 1, "unsorted action batch rejected atomically");
  e.process(
      out.data(), nullptr, out.size(),
      {true, true, false, 0, std::numeric_limits<double>::infinity(), 120});
  check(std::all_of(out.begin(), out.end(), [](float x) { return x == 0; }),
        "nonfinite host position is silent");
}
}  // namespace
void articulations() {
  Pattern p;
  p.tracks[1].events[0] = {.enabled = true,
                           .repeats = 2,
                           .interval = .04,
                           .repeatGain = 1,
                           .gain = .7F,
                           .pitch = 173.F,
                           .decay = .4F};
  const std::array<std::size_t, 1> fixed{127};
  const std::array<std::size_t, 4> irregular{1, 7, 511, 2048};
  const auto restart = render(p, fixed);
  p.tracks[1].events[0].articulation = Articulation::Carry;
  const auto carry = render(p, fixed);
  p.tracks[1].events[0].articulation = Articulation::Crossfade;
  const auto crossfade = render(p, fixed);
  check(restart.hits == carry.hits && carry.hits == crossfade.hits,
        "articulation leaves scheduling and lock decisions unchanged");
  check(restart.audio != carry.audio && restart.audio != crossfade.audio,
        "articulations produce distinct source audio");
  for (auto policy : {Articulation::Carry, Articulation::Crossfade}) {
    p.tracks[1].events[0].articulation = policy;
    for (double rate : {44100., 48000., 96000., 192000.})
      check(render(p, fixed, rate).audio == render(p, irregular, rate).audio,
            "new articulation is partition-independent at low/high rates");
  }
  p.tracks[1].events[0].repeats = 1;
  const auto continuous = render(p, fixed);
  check(crossfade.audio[960] == continuous.audio[960],
        "crossfade begins on the uninterrupted outgoing source sample");
  check(
      std::equal(crossfade.audio.begin() + 1056, crossfade.audio.begin() + 5000,
                 restart.audio.begin() + 1056),
      "two-ms crossfade reaches the exact restarted source without an extra "
      "tail");
  p.tracks[1].events[0].articulation = static_cast<Articulation>(99);
  Engine engine;
  check(!engine.prepare(48000, p),
        "invalid articulation is rejected during setup");
}

int main() {
  articulations();
  scheduler();
  determinism();
  boundaries();
  fragments();
  nested();
  hostAndRealtime();
  transportSegments();
  std::cout << "n01 contracts: " << failures << " failures\n";
  return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
