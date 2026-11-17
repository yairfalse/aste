#include "engine.hpp"
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
#include <cstdint>
#ifdef F01_MAC_AUDIT
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif
thread_local bool audit = false;
thread_local std::size_t allocations = 0;
void* operator new(std::size_t n) {
  if (audit) ++allocations;
  if (auto* p = std::malloc(n ? n : 1)) return p;
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
using namespace aste::field_study;
void require(bool b, const char* s) {
  if (!b) throw std::runtime_error(s);
}
std::vector<float> render(double rate, Topology t, int block, bool extreme) {
  Engine e;
  e.prepare(rate, t);
  std::size_t total = std::size_t(rate * 1.5);
  std::vector<float> l(total), r(total);
  l[0] = r[0] = .5F;
  for (std::size_t at = 0; at < total;) {
    std::size_t n =
        std::min({std::size_t(block), total - at,
                  std::size_t(rate * .75) - at % std::size_t(rate * .75)});
    Controls c;
    if (extreme) {
      c = at < rate * .75 ? Controls{20, .15, .78, 1} : Controls{.15, 1, 0, 0};
    }
    auto before = allocations;
    audit = true;
#ifdef F01_MAC_AUDIT
    asteRealtimeAuditReset();
    asteRealtimeAuditSetActive(1);
#endif
    e.process(l.data() + at, r.data() + at, n, c);
#ifdef F01_MAC_AUDIT
    asteRealtimeAuditSetActive(0);
#endif
    audit = false;
    require(before == allocations, "process allocated");
#ifdef F01_MAC_AUDIT
    require(!asteRealtimeAuditLockCalls() &&
                !asteRealtimeAuditFileOpenCalls() &&
                !asteRealtimeAuditWriteCalls(),
            "process OS calls");
#endif
    at += n;
  }
  for (std::size_t i = 0; i < total; ++i)
    require(std::isfinite(l[i]) && std::isfinite(r[i]) && std::abs(l[i]) < 8 &&
                std::abs(r[i]) < 8,
            "unsafe output");
  l.insert(l.end(), r.begin(), r.end());
  return l;
}
int main() {
  try {
#ifdef F01_MAC_AUDIT
    pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
    asteRealtimeAuditReset();
    asteRealtimeAuditSetActive(1);
    pthread_mutex_lock(&mutex);
    pthread_mutex_unlock(&mutex);
    int fd = open("/dev/null", O_WRONLY);
    if (fd >= 0) {
      const char c = 'x';
      (void)write(fd, &c, 1);
      close(fd);
    }
    asteRealtimeAuditSetActive(0);
    require(asteRealtimeAuditLockCalls() && asteRealtimeAuditFileOpenCalls() &&
                asteRealtimeAuditWriteCalls(),
            "audit calibration");
#endif
    for (auto t : {Topology::Bare8, Topology::House16, Topology::Hadamard16,
                   Topology::Schroeder, Topology::CrossTank}) {
      for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
        require(render(rate, t, 127, true) == render(rate, t, 511, true),
                "block-dependent output");
      require(render(48000, t, 1, false) == render(48000, t, 127, false),
              "single-frame partition");
      Engine e;
      e.prepare(48000, t);
      std::array<float, 2048> l{}, r{};
      e.process(l.data(), r.data(), l.size(), {});
      for (auto x : l) require(x == 0, "nonzero silence");
      l[0] = std::numeric_limits<float>::quiet_NaN();
      r[1] = std::numeric_limits<float>::infinity();
      e.process(l.data(), r.data(), l.size(), {NAN, NAN, NAN, NAN});
      for (auto x : l) require(std::isfinite(x), "nonfinite recovery");
      e.reset();
      l.fill(0);
      r.fill(0);
      e.process(l.data(), r.data(), l.size(), {});
      for (auto x : l) require(x == 0, "reset tail");
    }
    for (auto t : {Topology::Bare8, Topology::House16, Topology::Hadamard16,
                   Topology::Schroeder, Topology::CrossTank}) {
      Engine e;
      e.prepare(48000, t);
      std::array<float, 256> l{}, r{};
      double peak = 0;
      for (int block = 0; block < 5625; ++block) {
        for (int i = 0; i < 256; ++i) {
          double time = (block * 256 + i) / 48000.;
          l[i] = r[i] =
              time < 8
                  ? float(2 * std::sin(2 * std::numbers::pi * 137 * time) + .5)
                  : 0;
        }
        e.process(l.data(), r.data(), l.size(), {20, .15, .78, 1});
        for (auto x : l) {
          require(std::isfinite(x) && std::abs(x) < 32, "long stress bounded");
          peak = std::max(peak, double(std::abs(x)));
        }
      }
      std::cout << "30-second maximum-decay sustained-input stress engine "
                << int(t) << " peak " << peak << '\n';
    }
    std::cout << "F01 contracts passed: five engines, six rates, partitions, "
                 "abrupt controls, finite input, silence/reset, RT audit\n";
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
