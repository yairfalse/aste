#include "apps/field/field_processor.hpp"
#define field field_reference
#include "research/f01-foundation/reference/field_processor.hpp"
#undef field
#include "research/quality/audio.hpp"
#include <chrono>
#include <iostream>
#include <memory>
#include <new>
#include <cstdlib>
#ifdef FIELD_RT_MAC
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif
thread_local bool audit{};
thread_local std::size_t allocations{};
void* operator new(std::size_t n) {
  if (audit) ++allocations;
  if (auto* p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
using aste::field::Parameters;
using aste::field::Processor;
void check(bool b, const char* s) {
  if (!b) throw std::runtime_error(s);
}
Parameters preset(int i) {
  Parameters p;
  p.diffuseBehavior = true;
  p.outputDb = 0;
  p.blend = 1;
  constexpr std::array<std::array<float, 7>, 6> v{
      {{24, 0, 0, 4, 20, 0, 12},
       {65, 0, 0, 18, 35, 0, 10},
       {72, 32, 20, 38, 82, 28, 45},
       {97, 0, 0, 28, 38, 0, 15},
       {86, 64, 48, 70, 48, 68, 38},
       {88, 48, 30, 65, 55, 54, 28}}};
  auto a = v[i];
  p.mass = a[0] * .01F;
  p.grain = a[1] * .01F;
  p.pitch = a[2] * .01F;
  p.motion = a[3] * .01F;
  p.distance = a[4] * .01F;
  p.texture = a[5] * .01F;
  p.focus = a[6] * .01F;
  return p;
}
power::Audio render(double rate, int block, bool automation) {
  auto p = std::make_unique<Processor>();
  p->prepare(rate);
  power::Audio a(std::size_t(rate * 2), rate);
  for (std::size_t i = 0; i < a.left.size(); ++i) {
    auto t = i / rate;
    if (t < .3)
      a.left[i] = a.right[i] = float(.2 * std::sin(2 * power::pi * 173 * t));
  }
  auto c = preset(4);
  for (std::size_t at = 0; at < a.left.size();) {
    auto boundary = std::size_t(rate * .5);
    auto n = std::min(
        {std::size_t(block), a.left.size() - at, boundary - at % boundary});
    if (automation) {
      c.forever = at >= boundary && at < boundary * 3;
      c.mass = at >= boundary * 3 ? 0 : 1;
      c.texture = at >= boundary ? 1 : 0;
      c.pitch = c.grain = c.motion = c.focus = 1;
    }
    auto before = allocations;
#ifdef FIELD_RT_MAC
    asteRealtimeAuditReset();
    asteRealtimeAuditSetActive(1);
#endif
    audit = true;
    p->process(a.left.data() + at, a.right.data() + at, n, c);
    audit = false;
#ifdef FIELD_RT_MAC
    asteRealtimeAuditSetActive(0);
    check(!asteRealtimeAuditLockCalls() && !asteRealtimeAuditFileOpenCalls() &&
              !asteRealtimeAuditWriteCalls(),
          "OS callback calls");
#endif
    check(before == allocations, "audio allocation");
    at += n;
  }
  power::validate(a);
  return a;
}
int main(int argc, char** argv) {
  try {
    if (argc == 3 && std::string(argv[1]) == "--artifacts") {
      std::filesystem::path path = argv[2];
      check(!std::filesystem::exists(path), "fresh output folder required");
      std::filesystem::create_directories(path);
      auto cpu = power::output(path / "cpu.csv");
      cpu << "rate,p50_us,p99_us,max_us,deadline_us,memory_bytes\n";
      for (double rate : {44100., 48000., 96000., 192000.}) {
        auto p = std::make_unique<Processor>();
        p->prepare(rate);
        auto c = preset(4);
        c.mass = c.grain = c.pitch = c.texture = c.motion = 1;
        std::array<float, 128> l{}, r{};
        std::vector<double> us;
        for (int b = 0; b < 450; ++b) {
          l.fill(.1F);
          r.fill(-.07F);
          auto start = std::chrono::steady_clock::now();
          p->process(l.data(), r.data(), 128, c);
          auto t = std::chrono::duration<double, std::micro>(
                       std::chrono::steady_clock::now() - start)
                       .count();
          if (b >= 100) us.push_back(t);
        }
        std::sort(us.begin(), us.end());
        cpu << rate << ',' << us[175] << ',' << us[346] << ',' << us.back()
            << ',' << 128 / rate * 1e6 << ',' << p->memoryBytes() << '\n';
      }
      constexpr std::array names{"clean-room",     "deep-hall",      "dark-dub",
                                 "enormous-space", "evolving-field", "forever"};
      auto metrics = power::output(path / "levels.csv");
      metrics << "preset,peak,rms,lufs\n";
      for (int i = 0; i < 6; ++i) {
        auto p = std::make_unique<Processor>();
        p->prepare(48000);
        auto a = power::fixture(i == 0 ? 0 : 4, 48000, 6);
        a.left.resize(48000 * 16);
        a.right.resize(48000 * 16);
        auto c = preset(i);
        for (std::size_t at = 0; at < a.left.size(); at += 127) {
          if (i == 5) c.forever = at > 48000 * 2 && at < 48000 * 12;
          p->process(a.left.data() + at, a.right.data() + at,
                     std::min(std::size_t(127), a.left.size() - at), c);
        }
        auto m = power::measure(a);
        metrics << names[i] << ',' << m.peak << ',' << m.rms << ','
                << m.lufs.value_or(-999) << '\n';
        power::wav(path / (std::string(names[i]) + ".wav"), a);
      }
      return 0;
    }
    // The complete old stereo path remains sample-identical with existing
    // parameter values.
    for (bool legacy : {false, true}) {
      auto old = std::make_unique<aste::field_reference::Processor>();
      auto now = std::make_unique<Processor>();
      old->prepare(48000);
      now->prepare(48000);
      aste::field_reference::Parameters o;
      o.materialBehavior = true;
      o.texture = .7F;
      o.legacyBehavior = legacy;
      Parameters n;
      n.materialBehavior = true;
      n.texture = .7F;
      n.legacyBehavior = legacy;
      std::array<float, 127> l{}, r{}, a{}, b{};
      for (int k = 0; k < 300; ++k) {
        for (int j = 0; j < 127; ++j) {
          l[j] = a[j] = float(power::noise(k * 127 + j) * .08);
          r[j] = b[j] = l[j] * .7F;
        }
        old->process(l.data(), r.data(), 127, o);
        now->process(a.data(), b.data(), 127, n);
        check(l == a && r == b, "legacy output changed");
      }
    }
    for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
      auto a = render(rate, 127, true), b = render(rate, 511, true);
      check(a.left == b.left && a.right == b.right, "buffer-dependent sound");
    }
    auto a = render(48000, 1, false), b = render(48000, 127, false);
    check(a.left == b.left && a.right == b.right, "single sample partition");
    auto p = std::make_unique<Processor>();
    p->prepare(48000);
    std::array<float, 127> l{}, r{};
    auto c = preset(4);
    c.forever = true;
    for (int k = 0; k < 200; ++k) p->process(l.data(), r.data(), 127, c);
    for (auto x : l) check(x == 0, "empty FOREVER starts sound");
    c.forever = false;
    double early = 0, late = 0;
    for (int k = 0; k < 6000; ++k) {
      l.fill(0);
      r.fill(0);
      if (k < 100)
        for (int i = 0; i < 127; ++i)
          l[i] = r[i] =
              float(.1 * std::sin(2 * power::pi * 173 * (k * 127 + i) / 48000));
      if (k == 100) c.forever = true;
      p->process(l.data(), r.data(), 127, c);
      for (auto x : l)
        check(std::isfinite(x) && std::abs(x) < 4, "FOREVER unsafe");
      if (k > 1000 && k < 1100)
        early += p->meters().fieldEnergy * p->meters().fieldEnergy;
      if (k > 5800 && k < 5900)
        late += p->meters().fieldEnergy * p->meters().fieldEnergy;
    }
    check(early > 1e-8 && late > early * .8 && late < early * 1.2,
          "FOREVER loses or gains stored energy");
    p->reset();
    l.fill(0);
    r.fill(0);
    for (int k = 0; k < 10; ++k) {
      l.fill(0);
      r.fill(0);
      p->process(l.data(), r.data(), 127, c);
    }
    check(p->meters().fieldEnergy == 0, "clear leaves energy");
    std::cout << "Production Field: legacy exact, six-rate RT/block checks, "
                 "FOREVER energy and clear passed\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
