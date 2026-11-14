#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace aste::r01 {
constexpr std::size_t kTones = 4, kPartials = 3, kModes = kTones * kPartials;
enum class Architecture { Modal, Waveguide };
enum class Memory { Reset, Shared, Partial, Overlap };
enum class Excitation { White, Pink, Shaped, Impulse, Granular };
struct Chord {
  double root{48};
  std::array<double, kTones> intervals{0, 3, 7, 10};
  std::array<double, kTones> cents{};
  std::size_t count{4};
};
// Experimental physical values, not public parameter IDs or a state schema.
struct Controls {
  double t60{2}, damping{0.3}, couplingHz{}, stretch{}, inharmonic{};
  double dispersion{0.5}, partialRetain{0.25}, mutationCents{};
  bool persist{};
  std::uint64_t seed{101};
  std::array<double, kModes> modeDecay = [] {
    std::array<double, kModes> result{};
    result.fill(1);
    return result;
  }();
  bool operator==(const Controls&) const = default;
};
enum class EventType { Strike, Retune, NoteOn, NoteOff, Release, Panic };
struct Event {
  std::size_t offset{};
  EventType type{EventType::Strike};
  Chord chord{};
  int note{48};
};
struct Snapshot {
  std::array<double, kModes> frequencies{}, energies{};
  std::array<std::array<double, kModes>, 2> bankFrequencies{}, bankEnergies{};
  std::size_t activeBank{};
  double energy{}, peak{};
  std::size_t rejected{}, sanitized{}, safetyInterventions{}, stolen{};
};

class Exciter {
 public:
  bool prepare(double rate) noexcept;
  void trigger(Excitation type, std::uint64_t seed,
               std::size_t frames) noexcept;
  float next() noexcept;

 private:
  double rate_{48000}, filter_{};
  std::uint64_t random_{1}, clock_{};
  std::array<float, 16> rows_{};
  std::size_t age_{}, length_{};
  Excitation type_{};
};

class Engine {
 public:
  bool prepare(double rate, Architecture architecture, Memory memory,
               const Controls& controls = {});
  // Input is the exciter's energy, not an oscillator chord or a dry/wet signal.
  void process(const float* excitation, float* left, float* right,
               std::size_t frames, const Controls& controls,
               std::span<const Event> events = {}) noexcept;
  Snapshot snapshot() const noexcept;
  std::size_t heapBytes() const noexcept;

 private:
  struct Mode {
    double x{}, y{}, cosine{}, sine{}, radius{}, frequency{};
  };
  struct String {
    std::vector<float> data;
    std::size_t write{}, available{};
    double delay{2}, gain{}, lowpass{}, allpass{}, coefficient{}, energy{};
    void clear() noexcept;
    double read(double age) const noexcept;
  };
  struct Bank {
    std::array<Mode, kModes> modes{};
    std::array<String, kTones> strings{};
    Chord chord{};
    bool active{};
  };
  bool validControls(const Controls&) const noexcept;
  void clear(Bank&) noexcept;
  void tune(Bank&) noexcept;
  void changeChord(const Chord&, bool strike) noexcept;
  void eventsAt(std::span<const Event>) noexcept;
  double modal(Bank&, double input, double& right) noexcept;
  double waveguide(Bank&, double input, double& right) noexcept;
  double rate_{48000}, couplingCos_{1}, couplingSin_{};
  Architecture architecture_{};
  Memory memory_{};
  Controls controls_{};
  std::array<Bank, 2> banks_{};
  std::array<bool, 128> held_{};
  std::size_t active_{}, strikes_{};
  bool prepared_{}, released_{};
  Snapshot counters_{};
};
}  // namespace aste::r01
