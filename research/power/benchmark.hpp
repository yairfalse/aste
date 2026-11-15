#pragma once
#include "render.hpp"
#include <chrono>
namespace power {
template <class Process>
void timed(std::ostream& out, const std::string& name, double fs,
           std::size_t block, int trial, std::size_t stateBytes,
           const char* heap, Process process) {
  auto a = fixture(5, fs, 7);
  const auto end = static_cast<std::size_t>(fs * 5);
  double elapsed{}, maximum{}, energy{};
  for (std::size_t at = 0; at < end;) {
    const auto n = std::min(block, end - at);
    const auto start = std::chrono::steady_clock::now();
    process(a.left.data() + at, a.right.data() + at, n, at);
    const double us = std::chrono::duration<double, std::micro>(
                          std::chrono::steady_clock::now() - start)
                          .count();
    elapsed += us;
    maximum = std::max(maximum, us);
    for (std::size_t i = at; i < at + n; ++i) {
      if (!std::isfinite(a.left[i]) || !std::isfinite(a.right[i]))
        throw std::runtime_error("nonfinite benchmark");
      energy += a.left[i] * static_cast<double>(a.left[i]) +
                a.right[i] * static_cast<double>(a.right[i]);
    }
    at += n;
  }
  if (!(energy > 0)) throw std::runtime_error("silent benchmark");
  out << POWER_COMMIT << ',' << POWER_COMPILER << ',' << name << ',' << fs
      << ',' << block << ',' << trial << ',' << elapsed / 5000000 * 100 << ','
      << maximum << ',' << stateBytes << ',' << heap << ',' << energy << '\n';
}
inline void benchmarks(const std::filesystem::path& path) {
  auto out = output(path);
  out << "commit,compiler,variant,rate,block,trial,callback_realtime_percent,"
         "max_callback_us,object_bytes,heap_bytes,output_energy_probe\n";
  for (double fs : {48000., 96000.})
    for (auto block : {128U, 511U})
      for (int trial = 0; trial < 3; ++trial) {
        aste::density::Processor d;
        auto dp = densityParameters(3, 0);
        d.prepare(fs, dp);
        timed(out, "density", fs, block, trial, sizeof(d), "0",
              [&](float* l, float* r, std::size_t n, std::size_t at) {
                dp.density = (at / block) % 2 ? 1 : .3F;
                d.process(l, r, n, dp);
              });
        for (bool linked : {false, true}) {
          aste::harmonic::Processor h;
          auto hp = harmonicParameters(2, 0, linked);
          h.prepare(fs, hp);
          timed(out, linked ? "harmonic_linked" : "harmonic", fs, block, trial,
                sizeof(h), "0",
                [&](float* l, float* r, std::size_t n, std::size_t at) {
                  hp.presenceGainDb = (at / block) % 2 ? 12 : 3;
                  h.process(l, r, n, hp);
                });
        }
        aste::sequence::Processor s;
        auto sp = sequenceParameters<aste::sequence::Parameters>(3, 0, 0);
        s.prepare(fs, sp);
        timed(out, "sequence", fs, block, trial, sizeof(s), "0",
              [&](float* l, float* r, std::size_t n, std::size_t at) {
                s.process(l, r, n, sp, {true, true, 120, at / fs * 2});
              });
        aste::sequence_candidate::Processor sc;
        auto scp =
            sequenceParameters<aste::sequence_candidate::Parameters>(3, 0, 0);
        sc.prepare(fs, scp);
        timed(out, "sequence_candidate", fs, block, trial, sizeof(sc), "0",
              [&](float* l, float* r, std::size_t n, std::size_t at) {
                sc.process(l, r, n, scp, {true, true, 120, at / fs * 2});
              });
        aste::impulse::Processor i;
        auto ip = impulseParameters(3, 0);
        for (auto& track : ip.tracks) {
          track.pattern.fill(2);
          track.probability = 1;
        }
        i.prepare(fs, ip);
        timed(out, "impulse_eight_tracks", fs, block, trial, sizeof(i), "0",
              [&](float* l, float* r, std::size_t n, std::size_t at) {
                i.process(l, r, n, ip, {true, 120, at / fs * 2});
              });
        auto f = std::make_unique<aste::field::Processor>();
        f->prepare(fs);
        auto fp = fieldParameters(3, 3);
        timed(out, "field_forever", fs, block, trial, sizeof(*f), "0",
              [&](float* l, float* r, std::size_t n, std::size_t) {
                f->process(l, r, n, fp);
              });
        auto loop = std::make_unique<aste::loop::Processor>();
        loop->prepare(fs, 16, true);
        timed(out, "loop_snapshots", fs, block, trial, sizeof(*loop), "unknown",
              [&](float* l, float* r, std::size_t n, std::size_t at) {
                loop->process(
                    l, r, n,
                    loopParameters<aste::loop::Parameters>(3, at / fs));
              });
        loop.reset();
        auto lc = std::make_unique<aste::loop_candidate::Processor>();
        lc->prepare(fs, 16, true);
        timed(
            out, "loop_candidate_snapshots", fs, block, trial, sizeof(*lc),
            "unknown", [&](float* l, float* r, std::size_t n, std::size_t at) {
              lc->process(
                  l, r, n,
                  loopParameters<aste::loop_candidate::Parameters>(3, at / fs));
            });
      }
}
}  // namespace power
