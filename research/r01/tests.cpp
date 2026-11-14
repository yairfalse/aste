#include "core.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <cstdio>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
#include <vector>
#ifdef R01_MAC_AUDIT
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
thread_local std::size_t allocations{}, frees{};
}  // namespace
void* operator new(std::size_t n) {
  if (audit) ++allocations;
  if (auto* p = std::malloc(std::max<std::size_t>(1, n))) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  if (audit) ++frees;
  std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }
using namespace aste::r01;
namespace {
int failures{};
void check(bool value, const char* message) {
  if (!value) {
    ++failures;
    std::cerr << "FAIL " << message << '\n';
  }
}
std::vector<float> render(Architecture architecture, std::size_t block,
                          double rate = 48000) {
  Controls c;
  Engine e;
  check(e.prepare(rate, architecture, Memory::Shared, c), "render prepare");
  Exciter exciter;
  check(exciter.prepare(rate), "exciter prepare");
  exciter.trigger(Excitation::White, 501, 1000);
  std::vector<float> input(24000), output(input.size());
  for (auto& x : input) x = exciter.next() * .03F;
  Chord second;
  second.root = 53;
  second.intervals = {0, 5, 7, 14};
  const std::array events{Event{3, EventType::Strike},
                          Event{1023, EventType::Retune, second},
                          Event{8191, EventType::Release}};
  for (std::size_t at = 0; at < input.size();) {
    const auto frames = std::min(block, input.size() - at);
    std::array<Event, 3> batch{};
    std::size_t n{};
    for (auto event : events)
      if (event.offset >= at && event.offset < at + frames) {
        event.offset -= at;
        batch[n++] = event;
      }
    e.process(input.data() + at, output.data() + at, nullptr, frames, c,
              std::span(batch).first(n));
    at += frames;
  }
  check(e.snapshot().rejected == 0 && e.snapshot().sanitized == 0,
        "render has complete valid inputs");
  return output;
}
void analytic() {
  Controls c;
  c.t60 = 1;
  c.damping = 0;
  Engine e;
  check(e.prepare(48000, Architecture::Modal, Memory::Shared, c),
        "analytic setup");
  std::array<float, 256> input{}, output{};
  input[0] = 1;
  e.process(input.data(), output.data(), nullptr, output.size(), c);
  const auto frequencies = e.snapshot().frequencies;
  double error{};
  for (std::size_t n = 0; n < output.size(); ++n) {
    double expected{};
    for (double f : frequencies)
      expected += .1 / kModes * std::exp(-6.907755278982137 * n / 48000) *
                  std::cos(2 * std::numbers::pi * f * n / 48000);
    error = std::max(error, std::abs(expected - output[n]));
  }
  check(error < 1e-8,
        "modal impulse matches independent closed-form oscillator sum");
  check(e.prepare(48000, Architecture::Modal, Memory::Shared, c),
        "T60 prepare");
  e.process(input.data(), output.data(), nullptr, 1, c);
  std::vector<float> tail(48000);
  e.process(nullptr, tail.data(), nullptr, tail.size(), c);
  check(std::abs(e.snapshot().energy - 1e-6) < 1e-12,
        "T60 gives exact -60dB amplitude after one second");
}
void memory() {
  Controls c;
  c.persist = true;
  c.damping = 0;
  double shared{}, partial{}, overlap{};
  for (auto policy :
       {Memory::Reset, Memory::Shared, Memory::Partial, Memory::Overlap}) {
    Engine e;
    check(e.prepare(48000, Architecture::Modal, policy, c), "memory setup");
    float input = .2F, output{};
    e.process(&input, &output, nullptr, 1, c);
    const auto before = e.snapshot().energy;
    Chord chord;
    chord.root = 55;
    const std::array events{Event{0, EventType::Strike, chord}};
    e.process(nullptr, &output, nullptr, 1, c, events);
    const auto ratio = e.snapshot().energy / before;
    if (policy == Memory::Reset)
      check(ratio == 0, "full reset removes prior state");
    if (policy == Memory::Shared) shared = ratio;
    if (policy == Memory::Partial) partial = ratio;
    if (policy == Memory::Overlap) overlap = ratio;
  }
  check(std::abs(shared - 1) < 1e-12,
        "retuning shared coordinates preserves unforced energy");
  check(std::abs(partial - .25) < 1e-12,
        "partial damping keeps requested energy fraction");
  check(std::abs(overlap - 1) < 1e-12,
        "independent old bank retains energy on new chord");
  Engine both, first, second;
  check(both.prepare(48000, Architecture::Modal, Memory::Shared, c) &&
            first.prepare(48000, Architecture::Modal, Memory::Shared, c) &&
            second.prepare(48000, Architecture::Modal, Memory::Shared, c),
        "superposition setup");
  double maximum{};
  for (int n = 0; n < 10000; ++n) {
    float a = n == 0 ? .1F : 0, b = n == 1000 ? -.1F : 0, input = a + b;
    float together{}, x{}, y{};
    both.process(&input, &together, nullptr, 1, c);
    first.process(&a, &x, nullptr, 1, c);
    second.process(&b, &y, nullptr, 1, c);
    maximum =
        std::max(maximum, std::abs(static_cast<double>(together) - x - y));
  }
  check(maximum < 1e-8,
        "fixed shared linear memory equals superposition, not invented "
        "interaction");
}
void determinism() {
  for (auto arch : {Architecture::Modal, Architecture::Waveguide})
    for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
      const auto reference = render(arch, 1, rate);
      for (auto block : {7U, 127U, 511U, 2048U})
        check(reference == render(arch, block, rate),
              "audio independent of callback partition at six rates");
      check(std::all_of(reference.begin(), reference.end(),
                        [](float x) { return std::isfinite(x); }),
            "finite six-rate render");
    }
}
void stability() {
  Controls c;
  c.persist = true;
  c.couplingHz = 20;
  Engine e;
  check(e.prepare(48000, Architecture::Modal, Memory::Shared, c),
        "coupling setup");
  std::array<float, 128> input{}, output{};
  input[0] = .5F;
  e.process(input.data(), output.data(), nullptr, 1, c);
  const auto energy = e.snapshot().energy;
  for (int i = 0; i < 1000; ++i) {
    Chord chord;
    chord.root = 36 + i % 48;
    c.couplingHz = i % 21;
    const std::array events{Event{0, EventType::Retune, chord}};
    e.process(nullptr, output.data(), nullptr, output.size(), c, events);
  }
  check(std::abs(e.snapshot().energy - energy) < 1e-9,
        "time-varying rotations and coupling preserve autonomous norm");
  for (auto arch : {Architecture::Modal, Architecture::Waveguide}) {
    c.couplingHz = 0;
    check(e.prepare(48000, arch, Memory::Shared, c), "driven guard setup");
    bool finite = true;
    for (std::size_t at = 0; at < 480000; at += input.size()) {
      for (std::size_t n = 0; n < input.size(); ++n)
        input[n] =
            std::sin(2 * std::numbers::pi * 130.81278265 * (at + n) / 48000);
      c.damping = (at / 128) % 2 ? 0 : .8;
      c.dispersion = (at / 128) % 2 ? .8 : 0;
      e.process(input.data(), output.data(), nullptr, input.size(), c);
      for (float x : output)
        finite &= std::isfinite(x) && std::abs(x) <= 1.60001F;
    }
    check(finite,
          "strong driven persistence and abrupt damping remain bounded");
    if (arch == Architecture::Modal)
      check(e.snapshot().safetyInterventions > 0 &&
                e.snapshot().energy <= 16.000001,
            "energy guard actually engages under strong drive");
    const std::array panic{Event{0, EventType::Panic}};
    e.process(nullptr, output.data(), nullptr, output.size(), c, panic);
    check(std::all_of(output.begin(), output.end(),
                      [](float x) { return x == 0; }),
          "panic removes stale modal and delay history");
  }
}
void midiAndRealtime() {
  Engine e;
  Controls c;
  check(e.prepare(48000, Architecture::Modal, Memory::Overlap, c),
        "MIDI prepare");
  std::array<float, 128> input{}, output{};
  input[5] = .1F;
  const std::array notes{
      Event{5, EventType::NoteOn, {}, 48}, Event{5, EventType::NoteOn, {}, 51},
      Event{5, EventType::NoteOn, {}, 55}, Event{5, EventType::NoteOn, {}, 62}};
  e.process(input.data(), output.data(), nullptr, output.size(), c, notes);
  check(std::all_of(output.begin(), output.begin() + 5,
                    [](float x) { return x == 0; }) &&
            output[5] != 0,
        "MIDI chord excitation applied at sample offset");
  check(e.snapshot().stolen == 0 && e.snapshot().rejected == 0,
        "same-sample notes form one chord transition");
  const auto frequencies = e.snapshot().frequencies;
  check(std::abs(12 * std::log2(frequencies[3] / frequencies[0]) - 3) < 1e-10,
        "MIDI minor third tunes resonator, not exciter");
  const std::array fifth{Event{0, EventType::NoteOn, {}, 60}};
  e.process(nullptr, output.data(), nullptr, 1, c, fifth);
  check(e.snapshot().rejected == 1,
        "fifth held MIDI note is explicitly rejected");
  input[0] = std::numeric_limits<float>::quiet_NaN();
  e.process(input.data(), output.data(), nullptr, 1, c);
  check(e.snapshot().sanitized == 1 && std::isfinite(output[0]),
        "nonfinite excitation is counted and sanitized");
  const auto before = e.snapshot().energy;
  e.process(nullptr, output.data(), nullptr, 0, c, notes);
  check(before == e.snapshot().energy, "zero frame call is inert");
  audit = true;
  auto* probe = ::operator new(8);
  ::operator delete(probe);
  audit = false;
  check(allocations > 0 && frees > 0,
        "allocation guard detects positive control");
  allocations = frees = 0;
#ifdef R01_MAC_AUDIT
  pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
  asteRealtimeAuditReset();
  asteRealtimeAuditSetActive(1);
  pthread_mutex_lock(&mutex);
  pthread_mutex_unlock(&mutex);
  auto* file = std::fopen("/dev/null", "w");
  if (file) std::fclose(file);
  (void)::write(-1, "x", 1);
  asteRealtimeAuditSetActive(0);
  check(asteRealtimeAuditLockCalls() && asteRealtimeAuditFileOpenCalls() &&
            asteRealtimeAuditWriteCalls(),
        "OS audit detects lock/open/write controls");
  pthread_mutex_destroy(&mutex);
#endif
  for (auto arch : {Architecture::Modal, Architecture::Waveguide}) {
    check(e.prepare(48000, arch, Memory::Overlap, c), "audit prepare");
#ifdef R01_MAC_AUDIT
    asteRealtimeAuditReset();
    asteRealtimeAuditSetActive(1);
#endif
    audit = true;
    for (int i = 0; i < 100; ++i)
      e.process(input.data(), output.data(), nullptr, input.size(), c, notes);
    audit = false;
#ifdef R01_MAC_AUDIT
    asteRealtimeAuditSetActive(0);
    check(asteRealtimeAuditLockCalls() == 0 &&
              asteRealtimeAuditFileOpenCalls() == 0 &&
              asteRealtimeAuditWriteCalls() == 0,
          "callback uses no audited OS calls");
#endif
  }
  check(allocations == 0 && frees == 0, "callback allocates and frees nothing");
}
void midiReplacement() {
  Engine e;
  Controls c;
  c.persist = true;
  check(e.prepare(48000, Architecture::Modal, Memory::Shared, c),
        "MIDI replacement setup");
  float input = .2F, output{};
  const std::array first{Event{0, EventType::NoteOn, {}, 48}};
  e.process(&input, &output, nullptr, 1, c, first);
  const auto energy = e.snapshot().energy;
  const std::array replacement{Event{0, EventType::NoteOff, {}, 48},
                               Event{0, EventType::NoteOn, {}, 53}};
  e.process(nullptr, &output, nullptr, 1, c, replacement);
  check(std::abs(e.snapshot().energy - energy) < 1e-12,
        "same-sample note replacement must not release the newly held chord");
}
void validationAndWaveguide() {
  Engine e;
  Controls c;
  check(!e.prepare(8000, Architecture::Modal, Memory::Shared, c),
        "unsupported sample rate rejected");
  check(!e.prepare(48000, Architecture::Waveguide, Memory::Partial, c),
        "unsupported waveguide partial policy rejected");
  c.couplingHz = 1;
  check(!e.prepare(48000, Architecture::Waveguide, Memory::Shared, c),
        "unsupported waveguide coupling rejected");
  c = {};
  check(e.prepare(48000, Architecture::Modal, Memory::Shared, c),
        "validation setup");
  std::array<float, 128> output{};
  auto invalid = c;
  invalid.t60 = std::numeric_limits<double>::quiet_NaN();
  e.process(nullptr, output.data(), nullptr, output.size(), invalid);
  const std::array unsorted{Event{7}, Event{3}};
  e.process(nullptr, output.data(), nullptr, output.size(), c, unsorted);
  const std::array outside{Event{128}};
  e.process(nullptr, output.data(), nullptr, output.size(), c, outside);
  Chord bad;
  bad.count = 5;
  const std::array chord{Event{0, EventType::Strike, bad}};
  e.process(nullptr, output.data(), nullptr, output.size(), c, chord);
  check(
      e.snapshot().rejected == 4 && e.snapshot().energy == 0,
      "invalid controls, batches and chords rejected without injecting energy");
  c.t60 = .1;
  check(e.prepare(48000, Architecture::Waveguide, Memory::Reset, c),
        "waveguide decay setup");
  float impulse = .2F, sample{};
  e.process(&impulse, &sample, nullptr, 1, c);
  std::vector<float> tail(48000);
  e.process(nullptr, tail.data(), nullptr, tail.size(), c);
  check(e.snapshot().energy < 1e-15 && e.snapshot().safetyInterventions == 0,
        "unforced damped waveguide decays without safety intervention");
  c.persist = true;
  e.process(&impulse, &sample, nullptr, 1, c);
  e.process(nullptr, tail.data(), nullptr, 4000, c);
  check(e.snapshot().energy > 0, "waveguide history populated before clear");
  const std::array reset{Event{0, EventType::Strike}};
  e.process(nullptr, tail.data(), nullptr, tail.size(), c, reset);
  check(std::all_of(tail.begin(), tail.end(), [](float x) { return x == 0; }),
        "reset and restrike never expose stale delay history");
}
}  // namespace
int main() {
  analytic();
  memory();
  determinism();
  stability();
  midiAndRealtime();
  midiReplacement();
  validationAndWaveguide();
  std::cout << "r01 contracts: " << failures << " failures\n";
  return failures ? 1 : 0;
}
