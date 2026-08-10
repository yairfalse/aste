#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>

namespace aste::impulse {

constexpr std::size_t kTrackCount = 8;
constexpr std::size_t kPatternSteps = 32;

struct TrackParameters {
  float level{0.75F};
  float pitchHz{60.0F};
  float decayMs{300.0F};
  float tone{0.5F};
  float drive{0.25F};
  int length{16};
  int pulses{4};
  int rotation{};
  float probability{1.0F};
  int ratchet{1};
  float timing{};
  int condition{1};
  float accent{0.35F};
  std::array<std::uint8_t, kPatternSteps> pattern{};
};

struct Parameters {
  float energy{0.45F};
  int division{1};
  float variation{0.12F};
  float mutation{};
  std::uint32_t seed{1701U};
  float outputDb{-6.0F};
  bool sequenceEnabled{true};
  bool bypass{};
  std::array<TrackParameters, kTrackCount> tracks{{
      {.level = 0.9F,
       .pitchHz = 48.0F,
       .decayMs = 520.0F,
       .tone = 0.35F,
       .drive = 0.35F,
       .length = 15,
       .pulses = 4,
       .accent = 0.5F,
       .pattern = {2, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0, 0, 1, 0, 0}},
      {.level = 0.55F,
       .pitchHz = 2600.0F,
       .decayMs = 24.0F,
       .tone = 0.72F,
       .drive = 0.2F,
       .length = 23,
       .pulses = 7,
       .rotation = 2,
       .probability = 0.85F,
       .accent = 0.3F,
       .pattern = {2, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0,
                   1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0}},
      {.level = 0.48F,
       .pitchHz = 780.0F,
       .decayMs = 95.0F,
       .tone = 0.58F,
       .drive = 0.4F,
       .length = 11,
       .pulses = 4,
       .rotation = 1,
       .probability = 0.75F,
       .accent = 0.25F,
       .pattern = {0, 0, 2, 0, 0, 1, 0, 0, 1, 0, 0}},
      {.level = 0.62F,
       .pitchHz = 125.0F,
       .decayMs = 280.0F,
       .tone = 0.42F,
       .drive = 0.3F,
       .length = 16,
       .pulses = 5,
       .rotation = 3,
       .probability = 0.9F,
       .accent = 0.4F,
       .pattern = {2, 0, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 0}},
      {.level = 0.72F,
       .pitchHz = 72.0F,
       .decayMs = 190.0F,
       .tone = 0.28F,
       .drive = 0.42F,
       .length = 16,
       .pulses = 3,
       .rotation = 5,
       .probability = 0.9F,
       .accent = 0.45F,
       .pattern = {0, 0, 0, 0, 2, 0, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1}},
      {.level = 0.5F,
       .pitchHz = 1450.0F,
       .decayMs = 52.0F,
       .tone = 0.68F,
       .drive = 0.55F,
       .length = 13,
       .pulses = 5,
       .rotation = 2,
       .probability = 0.78F,
       .accent = 0.3F,
       .pattern = {0, 1, 0, 0, 2, 0, 1, 0, 0, 1, 0, 0, 1}},
      {.level = 0.44F,
       .pitchHz = 620.0F,
       .decayMs = 145.0F,
       .tone = 0.76F,
       .drive = 0.48F,
       .length = 17,
       .pulses = 6,
       .rotation = 1,
       .probability = 0.72F,
       .accent = 0.34F,
       .pattern = {0, 0, 1, 0, 0, 2, 0, 0, 1, 0, 0, 1, 0, 0, 1, 0, 1}},
      {.level = 0.5F,
       .pitchHz = 4200.0F,
       .decayMs = 18.0F,
       .tone = 0.82F,
       .drive = 0.6F,
       .length = 9,
       .pulses = 3,
       .rotation = 2,
       .probability = 0.82F,
       .accent = 0.28F,
       .pattern = {0, 0, 2, 0, 0, 1, 0, 0, 1}},
  }};
};

struct Transport {
  bool playing{};
  double bpm{120.0};
  double ppq{};
};

struct MidiEvent {
  std::size_t offset{};
  int note{};
  float velocity{};
  bool noteOn{};
};

struct MeterSnapshot {
  float outputPeak{};
  std::array<int, kTrackCount> currentStep = [] {
    std::array<int, kTrackCount> result{};
    result.fill(-1);
    return result;
  }();
  std::array<bool, kTrackCount> triggered{};
};

class Processor {
 public:
  void prepare(double sampleRate, const Parameters& parameters) noexcept;
  void reset() noexcept;
  void process(float* left, float* right, std::size_t frames,
               const Parameters& parameters, const Transport& transport,
               std::span<const MidiEvent> midi = {}) noexcept;

  [[nodiscard]] constexpr std::size_t latencySamples() const noexcept {
    return 0U;
  }
  [[nodiscard]] MeterSnapshot meters() const noexcept { return meters_; }

 private:
  struct Voice {
    double phase{};
    double secondPhase{};
    float amplitude{};
    float pitchEnvelope{};
    float noiseState{};
    float filterState{};
    float level{};
    float pitch{};
    float decay{};
    float tone{};
    float drive{};
    float pan{};
  };

  [[nodiscard]] static std::uint32_t hash(std::uint32_t value) noexcept;
  [[nodiscard]] static float random01(std::uint32_t value) noexcept;
  void trigger(std::size_t track, float velocity,
               const TrackParameters& parameters, float energy, float variation,
               std::uint32_t random) noexcept;
  [[nodiscard]] float renderVoice(std::size_t track) noexcept;

  double sampleRate_{48000.0};
  std::array<Voice, kTrackCount> voices_{};
  std::array<std::int64_t, kTrackCount> lastTick_{};
  double expectedPpq_{};
  bool transportKnown_{};
  float smoothedOutputGain_{1.0F};
  float outputSmoothing_{1.0F};
  MeterSnapshot meters_{};
};

}  // namespace aste::impulse
