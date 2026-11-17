#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
#include "reference/core.hpp"

namespace aste::contamination::revised {
using ::aste::contamination::Controls;
using ::aste::contamination::Event;
using ::aste::contamination::Kind;
enum class Architecture { Dense, Resonant, Feedback, Full };
struct Observation {
  double energy{}, memory{}, bias{}, interaction{}, peak{}, spaceEnergy{},
      dryPeak{};
  std::size_t stolen{}, rejected{}, safety{};
};
class Engine {
 public:
  bool prepare(double rate, int factor = 4,
               Architecture architecture = Architecture::Full);
  void reset() noexcept;
  void event(const Event&) noexcept;
  void process(float*, float*, std::size_t, const Controls&,
               std::span<const Event> = {}, bool hearSpace = true) noexcept;
  void setInteraction(bool enabled) noexcept { interactionEnabled_ = enabled; }
  std::size_t heapBytes() const noexcept;
  Observation observation() const noexcept { return observation_; }
  static constexpr std::size_t voices = 12;
  static constexpr int latency = 16;
  static int recommendedFactor(double rate) noexcept {
    int factor = 1;
    while (rate > 0 && rate * factor < 176400 && factor < 8) factor *= 2;
    return factor;
  }

 private:
  struct Voice {
    int note{-1}, stage{};
    bool down{}, pedal{}, fresh{};
    std::uint64_t age{}, rng{1};
    double a{}, b{}, sub{}, previous{}, lp{}, bp{}, env{}, velocity{}, drift{},
        noise{}, g{}, gTarget{}, ic1{}, ic2{}, memory{}, bias{}, last{}, tail{};
    int fade{};
  };
  double tick() noexcept;
  void clearSpace() noexcept;
  std::array<float, 2> space(float left, float right) noexcept;
  struct Delay {
    std::vector<float> data;
    std::size_t write{}, valid{};
    double low{}, high{}, dc{};
    float read(double age) const noexcept;
    void push(float x) noexcept;
    void clear() noexcept {
      write = valid = 0;
      low = high = dc = 0;
    }
  };
  std::array<Delay, 4> delay_;
  std::array<float, 2> lastWet_{}, clearWet_{};
  std::size_t clearFade_{};
  Architecture architecture_{Architecture::Full};
  bool interactionEnabled_{true};
  double filterPhase_{}, spacePhase_{}, spaceLoad_{}, sourceLoad_{},
      spaceNoise_{}, spaceBias_{};
  std::uint64_t networkRandom_{101};
  std::size_t coefficientClock_{};
  double releaseCoefficient_{}, decayCoefficient_{}, memoryRetention_{},
      noiseCoefficient_{}, driftCoefficient_{}, gSlew_{};

  void start(int, double) noexcept;
  std::array<Voice, voices> voice_{};
  Controls smooth_{};
  std::array<double, 257> fir_{}, historyL_{}, historyR_{};
  std::size_t cursor_{}, taps_{}, clock_{};
  double rate_{48000}, internal_{192000}, smoothing_{}, shared_{}, bias_{},
      busLow_{}, busFast_{}, wheel_{}, touch_{}, bend_{}, dcL_{}, dcR_{},
      outR_{};
  int factor_{4};
  bool pedal_{}, ready_{}, initialized_{};

  Observation observation_{};
};
}  // namespace aste::contamination::revised
