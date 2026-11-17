#pragma once
#include "experimental_instrument.hpp"
#include "resonator_processor.hpp"

namespace aste::resonator::plugin {
class ResonatorAudioProcessor final : public aste::experimental::Instrument {
 public:
  enum Parameter {
    source,
    burst,
    excitation,
    root,
    chord,
    inputMode,
    decay,
    damping,
    coupling,
    stretch,
    inharmonic,
    memory,
    retain,
    persist,
    mutation,
    seed,
    gate,
    parameterCount
  };
  ResonatorAudioProcessor();
  juce::AudioProcessorEditor* createEditor() override;
  void excite() noexcept { exciteRequested_.store(true); }
  void retune() noexcept { retuneRequested_.store(true); }
  void clear() noexcept { clearRequested_.store(true); }
  void scene(int);
  float frequency(std::size_t bank, std::size_t mode) const noexcept {
    return frequencies_[bank * 12 + mode].load();
  }
  float energy(std::size_t bank, std::size_t mode) const noexcept {
    return energies_[bank * 12 + mode].load();
  }
  bool midiTuningActive() const noexcept { return midiTuningActive_.load(); }
  unsigned rejected() const noexcept { return rejected_.load(); }

 private:
  static std::vector<aste::experimental::ControlSpec> controls();
  bool prepareEngine(double, int) override;
  void render(juce::AudioBuffer<float>&,
              const juce::MidiBuffer&) noexcept override;
  void onRestore() noexcept override;
  Chord selectedChord() const noexcept;
  bool key(int note, bool on) noexcept;
  Chord heldChord() const noexcept;
  Engine engine_;
  Exciter exciter_;
  float lastRoot_{48}, lastStructure_{};
  std::atomic<bool> midiTuningActive_{};
  std::array<bool, 128> held_{};
  std::array<bool, 128> sustained_{};
  bool pedal_{};
  float velocity_{1};
  float excitationGain_{1};
  std::uint64_t strikes_{};
  std::atomic<bool> exciteRequested_{}, retuneRequested_{};
  std::array<std::atomic<float>, 24> frequencies_{}, energies_{};
  std::atomic<unsigned> rejected_{};
};
}  // namespace aste::resonator::plugin
