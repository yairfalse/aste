#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

// Lab-local types, not product parameters or a portable session format.
namespace aste::n01 {
constexpr std::size_t kVoices = 3;
constexpr std::size_t kEvents = 8;
constexpr unsigned kRepeats = 8;
enum class Articulation { Restart, Carry, Crossfade };

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
  Articulation articulation{Articulation::Restart};
};
struct Track {
  double length{4};
  double phase{};
  std::array<Event, kEvents> events{};
};
struct Pattern {
  std::uint64_t seed{1701};
  double phraseLength{8}, voidStart{8}, voidEnd{8};
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

enum class Mode { Continue, Suspend };
enum class Action { Capture, Reverse, Shorten, Release, Rejoin };
struct Command {
  std::size_t offset{};
  std::size_t target{kVoices};  // 0..2 local mono, 3 master stereo.
  Action action{Action::Capture};
  std::size_t length{}, lookback{}, duration{};  // Samples; duration 0 = hold.
  Mode mode{Mode::Continue};
  bool hard{};
};

// Capture swaps recording and immutable playback banks in constant time.
// A new selection requires fresh history after the preceding capture.
class Fragment {
 public:
  void prepare(std::size_t capacity);
  void clear() noexcept;
  void record(float sample) noexcept;
  bool capture(std::size_t length, std::size_t lookback, std::size_t duration,
               bool hard) noexcept;
  bool shorten(std::size_t length) noexcept;
  void reverse() noexcept;
  void release(bool hard) noexcept;
  float process(float live) noexcept;
  bool active() const noexcept { return active_; }
  std::size_t available() const noexcept { return available_; }
  std::size_t bytes() const noexcept {
    return banks_[0].size() * sizeof(float) * 2;
  }

 private:
  std::array<std::vector<float>, 2> banks_;
  std::size_t writing_{}, write_{}, available_{}, start_{}, length_{}, read_{};
  std::size_t remaining_{}, transition_{};
  bool active_{}, reverse_{}, hard_{};
  float previous_{}, from_{};
};

class Engine {
 public:
  // Setup only. Invalid setup is rejected, never silently normalized.
  bool prepare(double sampleRate, const Pattern& pattern);
  void process(float* left, float* right, std::size_t frames, const Transport&,
               Trace* trace = nullptr,
               std::span<const Command> commands = {}) noexcept;
  [[nodiscard]] double phraseBeat() const noexcept { return phrase_; }
  [[nodiscard]] std::size_t resets() const noexcept { return resets_; }
  [[nodiscard]] std::size_t rejected() const noexcept { return rejected_; }
  [[nodiscard]] std::size_t audioBytes() const noexcept;

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
  void command(const Command&, double hostBeat) noexcept;
  [[nodiscard]] Next next(std::size_t voice, double beat) const noexcept;
  void trigger(const Hit&) noexcept;
  [[nodiscard]] float sound(std::size_t voice) noexcept;
  [[nodiscard]] float renderVoice(Voice&, std::size_t voice) noexcept;
  Pattern pattern_{};
  double rate_{48000}, phrase_{}, anchorPpq_{}, bpm_{120}, expectedPpq_{};
  std::int64_t anchorSample_{}, expectedSample_{};
  bool prepared_{}, running_{};
  double suspendedBeats_{};
  Mode mode_{Mode::Continue};
  std::size_t resets_{}, rejected_{};
  std::array<Next, kVoices> next_{};
  std::array<Voice, kVoices> voices_{};
  std::array<Voice, kVoices> tails_{};
  std::array<std::size_t, kVoices> fades_{};
  std::array<Fragment, kVoices> local_{};
  std::array<Fragment, 2> master_{};
};
}  // namespace aste::n01
