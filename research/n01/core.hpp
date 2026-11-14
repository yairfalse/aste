#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

// Lab-local types, not product parameters or a portable session format.
namespace aste::n01 {
constexpr std::size_t kVoices = 3;
constexpr std::size_t kEvents = 8;
constexpr unsigned kRepeats = 8;

struct Event {
  bool enabled{};
  double beat{};
  double microtiming{};
  unsigned every{1};
  float probability{1};
  unsigned repeats{1};
  double interval{0.125};
  double acceleration{1};
  float repeatGain{0.8F};
  float pitchStep{};
  float mutation{};
  float gain{0.5F};
  float pitch{100};
  float decay{0.1F};
  float color{0.5F};
};
struct Track {
  double length{4};
  double phase{};
  std::array<Event, kEvents> events{};
};
struct Pattern {
  std::uint64_t seed{1701};
  std::array<Track, kVoices> tracks{};
};
[[nodiscard]] Pattern studyPattern() noexcept;
[[nodiscard]] bool valid(const Pattern&) noexcept;

struct Transport {
  bool playing{true};
  bool valid{true};
  bool discontinuity{};
  std::int64_t sample{};
  double ppq{};
  double bpm{120};
};
struct Hit {
  std::int64_t sample{};
  std::size_t voice{}, event{};
  std::int64_t cycle{};
  unsigned repeat{};
  float gain{}, pitch{}, color{};
  bool operator==(const Hit&) const = default;
};
struct Trace {
  std::span<Hit> hits;
  std::size_t size{}, dropped{};
  void add(const Hit& hit) noexcept;
};

class Engine {
 public:
  // Setup only. Invalid setup is rejected, never silently normalized.
  bool prepare(double sampleRate, const Pattern& pattern) noexcept;
  void process(float* left, float* right, std::size_t frames, const Transport&,
               Trace* trace = nullptr) noexcept;
  [[nodiscard]] double phraseBeat() const noexcept { return phrase_; }
  [[nodiscard]] std::size_t resets() const noexcept { return resets_; }

 private:
  struct Next {
    double beat{};
    Hit hit{};
  };
  struct Voice {
    double phase{}, second{};
    float amplitude{}, gain{}, pitch{}, color{}, decay{}, noise{};
    std::uint64_t random{};
    std::size_t age{};
  };
  void reset(double ppq) noexcept;
  [[nodiscard]] Next next(std::size_t voice, double beat) const noexcept;
  void trigger(const Hit&) noexcept;
  [[nodiscard]] float sound(std::size_t voice) noexcept;
  Pattern pattern_{};
  double rate_{48000}, phrase_{}, anchorPpq_{}, bpm_{120}, expectedPpq_{};
  std::int64_t anchorSample_{}, expectedSample_{};
  bool prepared_{}, running_{};
  std::size_t resets_{};
  std::array<Next, kVoices> next_{};
  std::array<Voice, kVoices> voices_{};
};
}  // namespace aste::n01
