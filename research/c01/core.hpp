#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace aste::contamination {
enum class Topology { External, Coupled, FilterReturn };
enum class Persistence { Reset, Local, Shared, Hybrid };
enum class Injection { Phase, Frequency, Bias, Feedback, Output };
enum class Noise { White, Correlated, Highpass };
struct Controls {
  double pressure{.35}, contamination{.15}, instability{.1}, memory{.5}, motion{.25};
  double colour{1100}, attack{.003}, decay{.25}, sustain{.55}, release{.2};
  double detune{.003}; // Lab ablation; fixed in the playable instrument.
  double pairDepth{1}; // Lab single-oscillator ablation; fixed at one in the plugin.
  bool operator==(const Controls&) const = default;
};
enum class Kind { On, Off, Pedal, Wheel, Touch, Bend, Panic, Clear };
struct Event { std::size_t offset{}; Kind kind{}; int note{}; double value{}; };
struct Observation { double energy{}, memory{}, bias{}, interaction{}, peak{}; std::size_t stolen{}, rejected{}; };
class Engine {
 public:
  bool prepare(double rate, int factor = 4, Topology = Topology::Coupled,
               Persistence = Persistence::Hybrid, Injection = Injection::Feedback,
               Noise = Noise::Correlated) noexcept;
  void reset() noexcept;
  void event(const Event&) noexcept;
  void process(float*, float*, std::size_t, const Controls&,
               std::span<const Event> = {}) noexcept;
  Observation observation() const noexcept { return observation_; }
  static constexpr std::size_t voices = 12;
  static constexpr int latency = 16;
  static int recommendedFactor(double rate) noexcept {
    int factor=1;
    while(rate>0 && rate*factor<176400 && factor<8)factor*=2;
    return factor;
  }
 private:
  struct Voice {
    int note{-1}, stage{}; bool down{}, pedal{};
    std::uint64_t age{}, rng{1};
    double a{}, b{}, previous{}, lp{}, bp{}, env{}, velocity{}, drift{}, noise{},
           memory{}, bias{}, last{}, tail{}; int fade{};
  };
  double tick() noexcept;
  void start(int, double) noexcept;
  std::array<Voice, voices> voice_{};
  Controls smooth_{};
  std::array<double, 257> fir_{}, historyL_{}, historyR_{};
  std::size_t cursor_{}, taps_{}, clock_{};
  double rate_{48000}, internal_{192000}, smoothing_{}, shared_{}, bias_{},
         busLow_{}, busFast_{}, wheel_{}, touch_{}, bend_{}, dcL_{}, dcR_{}, outR_{};
  int factor_{4}; bool pedal_{}, ready_{}, initialized_{};
  Topology topology_{}; Persistence persistence_{}; Injection injection_{}; Noise noise_{};
  Observation observation_{};
};
} // namespace aste::contamination
