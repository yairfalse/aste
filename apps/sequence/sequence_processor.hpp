#pragma once

#include <array>
#include <cstddef>
#include <span>

namespace aste::sequence {

inline constexpr std::size_t kStepCount = 16;

struct Step {
  int note{};
  bool gate{true};
  bool accent{};
  bool slide{};
};

struct Parameters {
  float pressure{0.35F};
  float shape{0.25F};
  float pulseWidth{0.5F};
  float oscillatorMix{0.45F};
  float detuneSemitones{0.08F};
  float subLevel{0.25F};
  float cutoffHz{900.0F};
  float resonance{0.35F};
  float filterMorph{0.45F};
  float filterDrive{0.25F};
  float envelopeAmount{0.55F};
  float attackMs{3.0F};
  float decayMs{180.0F};
  float sustain{0.55F};
  float releaseMs{120.0F};
  float glideMs{70.0F};
  float outputDb{-6.0F};
  int rootNote{36};
  int division{16};
  bool sequenceEnabled{true};
  bool bypass{};
  std::array<Step, kStepCount> steps{{
      {0, true, true, false},
      {0, true, false, true},
      {7, true, false, false},
      {0, false, false, false},
      {12, true, true, false},
      {7, true, false, true},
      {3, true, false, false},
      {0, true, false, false},
      {0, true, true, false},
      {-5, true, false, true},
      {0, false, false, false},
      {7, true, false, false},
      {3, true, true, false},
      {0, true, false, true},
      {-2, true, false, false},
      {0, false, false, false},
  }};
};

enum class MidiEventType { noteOn, noteOff, allNotesOff };

struct MidiEvent {
  std::size_t sampleOffset{};
  MidiEventType type{MidiEventType::noteOff};
  int note{};
  float velocity{};
};

struct Transport {
  bool valid{};
  bool playing{};
  double bpm{120.0};
  double ppqPosition{};
};

struct MeterSnapshot {
  float outputPeak{};
  float envelope{};
  float currentStep{-1.0F};
};

struct PressureMapping {
  float mixerDrive{};
  float envelopeDepth{};
  float accentGain{};
  float filterLoading{};
};

class Processor {
 public:
  void prepare(double sampleRate, const Parameters& initial = {}) noexcept;
  void reset() noexcept;
  void process(float* left, float* right, std::size_t frames,
               const Parameters& parameters, const Transport& transport,
               std::span<const MidiEvent> midi = {}) noexcept;

  [[nodiscard]] constexpr std::size_t latencySamples() const noexcept {
    return 0U;
  }
  [[nodiscard]] MeterSnapshot meters() const noexcept { return meters_; }
  [[nodiscard]] static PressureMapping mapPressure(float amount) noexcept;

 private:
  struct Smoother {
    void prepare(double sampleRate, double seconds, float initial) noexcept;
    void setTarget(float target) noexcept { target_ = target; }
    [[nodiscard]] float next() noexcept;
    void snap(float value) noexcept { current_ = target_ = value; }

    float current_{};
    float target_{};
    float coefficient_{};
  };

  struct Envelope {
    enum class Stage { idle, attack, decay, sustain, release };
    void noteOn(bool retrigger) noexcept;
    void noteOff() noexcept;
    [[nodiscard]] float process(double sampleRate, float attackMs,
                                float decayMs, float sustain,
                                float releaseMs) noexcept;
    void reset() noexcept;
    [[nodiscard]] bool active() const noexcept { return stage_ != Stage::idle; }

    Stage stage_{Stage::idle};
    float value_{};
  };

  struct CharacterFilter {
    [[nodiscard]] float process(float input, float cutoff, float resonance,
                                float weight, float drive, float loading,
                                double sampleRate) noexcept;
    void reset() noexcept { state_.fill(0.0F); }
    std::array<float, 4> state_{};
  };

  void handleMidi(const MidiEvent& event, bool sequencerRunning,
                  float glideMs) noexcept;
  void startNote(int note, float velocity, bool accent, bool slide,
                 float glideMs) noexcept;
  void stopNote() noexcept;
  void updateSequencer(const Parameters& parameters, const Transport& transport,
                       std::size_t sample) noexcept;
  [[nodiscard]] float oscillator(double& phase, float frequency, float shape,
                                 float pulseWidth) noexcept;
  [[nodiscard]] int heldNote() const noexcept;

  double sampleRate_{48000.0};
  double phase1_{};
  double phase2_{0.17};
  double subPhase_{};
  Smoother pressure_{};
  Smoother shape_{};
  Smoother pulseWidth_{};
  Smoother mix_{};
  Smoother sub_{};
  Smoother cutoff_{};
  Smoother resonance_{};
  Smoother filterMorph_{};
  Smoother filterDrive_{};
  Smoother output_{};
  Smoother frequency_{};
  Envelope envelope_{};
  CharacterFilter characterFilter_{};
  std::array<bool, 128> heldNotes_{};
  float velocity_{1.0F};
  bool accented_{};
  int lastMidiNote_{-1};
  long long absoluteStep_{-1};
  int currentStep_{-1};
  bool sequenceGate_{};
  bool wasPlaying_{};
  MeterSnapshot meters_{};
};

}  // namespace aste::sequence
