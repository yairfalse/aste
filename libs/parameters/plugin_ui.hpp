#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

#include <vector>
#include <cmath>

namespace aste::ui {

inline juce::String formattedValue(const juce::Slider& slider, double value) {
  const int decimals = slider.getNumDecimalPlacesToDisplay();
  return juce::String{
      std::abs(value) < 0.5 * std::pow(10.0, -decimals) ? 0.0 : value,
      decimals};
}

inline juce::String explanation(const juce::String& id) {
  const auto field = id.fromLastOccurrenceOf("_", false, false);
  if (id == "density")
    return "Weight and compression character. Use Blend to mix it with the "
           "original.";
  if (id == "crush")
    return "Compression and saturation strength in the processed branch.";
  if (id == "drive") return "Push the input into compression and saturation.";
  if (id == "blend" || id == "mix")
    return "Original sound at 0%; processed sound at 100%.";
  if (id == "stereo")
    return "Link left and right compression. 100% keeps their gain reduction "
           "together.";
  if (id == "detector_hpf")
    return "Ignore low frequencies in the detector, not in the audible signal.";
  if (id == "harmonic")
    return "Adds nonlinear character to boosted bands. Flat and cut bands stay "
           "clean.";
  if (id == "foundation_gain")
    return "Low shelf: boost or cut the bass below Foundation Frequency.";
  if (id == "air_gain")
    return "High shelf: boost or cut the top end above Air Frequency.";
  if (id == "foundation_frequency" || id == "air_frequency")
    return "Shelf transition frequency; half the selected gain occurs here.";
  if (id.endsWith("_gain"))
    return "Boost or cut this frequency region. Positive boosts engage "
           "Harmonic.";
  if (id.endsWith("_frequency"))
    return "Choose the frequency region shaped by this band.";
  if (id == "pressure")
    return "Increase mixer drive, filter loading, envelope depth and accent "
           "strength.";
  if (id == "shape") return "Morph from saw through pulse to sine.";
  if (id == "pulse_width")
    return "Change the width of the pulse wave; most audible near the pulse "
           "setting.";
  if (id == "filter_form")
    return "Move from a gentler two-pole filter to a steeper four-pole filter.";
  if (id == "filter_drive")
    return "Push the filter harder for nonlinear colour.";
  if (id == "cutoff") return "Open or close the low-pass filter.";
  if (id == "resonance") return "Emphasise the area around the filter cutoff.";
  if (id == "osc_mix") return "Balance the two oscillators.";
  if (id == "detune") return "Offset the second oscillator in semitones.";
  if (id == "sub") return "Add the lower octave oscillator.";
  if (id == "env_amount")
    return "How far each note's envelope opens the filter.";
  if (id == "root")
    return "Base note for the pattern. While running, MIDI C4 keeps this root "
           "unchanged.";
  if (id == "glide") return "Time taken to slide between connected notes.";
  if (id == "overdub")
    return "Balance incoming audio and previous recording while Capture is on.";
  if (id == "feedback") return "How much previous audio survives each overdub.";
  if (id == "length_beats")
    return "Loop duration in beats when Host Sync is on.";
  if (id == "free_length")
    return "Loop duration in seconds when Host Sync is off.";
  if (id == "start") return "Move the playback start within the captured loop.";
  if (id == "speed") return "Change playback speed and pitch together.";
  if (id == "pitch")
    return "Transpose playback without changing the loop duration.";
  if (id == "splice")
    return "Smooth the join between the end and start of the loop.";
  if (id == "wow") return "Slow tape-speed movement.";
  if (id == "flutter") return "Fast tape-speed movement.";
  if (id == "drift") return "Gradual tape-speed drift.";
  if (id == "degradation")
    return "Reduce playback resolution and add loss when printing a "
           "generation.";
  if (id == "amplifier") return "Drive the playback and RELOOP record stages.";
  if (id == "mass") return "How long the field decays after the input stops.";
  if (id == "grain") return "Add small moving fragments inside the field.";
  if (id == "motion") return "Movement of the delay lines and grain updates.";
  if (id == "distance") return "Shape the field's sense of distance and tone.";
  if (id == "energy")
    return "Drive the drum voices and their combined output harder.";
  if (id == "variation")
    return "Per-hit pitch and stereo variation. Zero keeps hits consistent.";
  if (id == "mutation")
    return "Chance of adding or removing programmed hits. Zero plays your "
           "pattern unchanged.";
  if (id == "seed")
    return "Choose a repeatable variation and mutation sequence.";
  if (field == "length")
    return "Number of steps before this track repeats. Match tracks for a "
           "regular groove.";
  if (field == "pulses")
    return "Number of hits used by Generate. Does not change the pattern until "
           "you press Generate.";
  if (field == "rotation")
    return "Offset used by Generate. Does not move existing hits until you "
           "press Generate.";
  if (field == "probability")
    return "Chance that a programmed hit plays. 100% plays every hit.";
  if (field == "ratchet") return "Number of repeats within each active step.";
  if (field == "condition")
    return "Play on every cycle (1), or only every second, third or fourth "
           "cycle.";
  if (field == "timing") return "Move hits earlier or later within the step.";
  if (field == "accent") return "Extra strength for steps marked Accent.";
  if (field == "pitch") return "Tune this drum voice in Hz.";
  if (field == "tone") return "Shape the drum's bright or noisy component.";
  if (field == "drive") return "Saturate this drum voice.";
  if (field == "level") return "Volume of this drum voice.";
  if (id == "attack") return "How quickly the envelope responds.";
  if (id == "release")
    return "How slowly the envelope returns after the signal or note ends.";
  if (id == "sustain") return "Level held while a note remains on.";
  if (field == "decay" || id == "decay")
    return "How long the sound takes to decay.";
  if (id == "output")
    return "Final output level. Match loudness when comparing settings.";
  if (id == "input") return "Input level before processing.";
  return "Drag to adjust, type an exact value, or double-click to reset.";
}

inline float meterPosition(float gain) {
  return juce::jlimit(
      0.0F, 1.0F,
      (juce::Decibels::gainToDecibels(gain, -60.0F) + 60.0F) / 60.0F);
}

inline juce::String levelText(float gain) {
  return gain < 0.001F
             ? juce::String{"-inf dBFS"}
             : juce::String{juce::Decibels::gainToDecibels(gain), 1} + " dBFS";
}

inline juce::String noteName(int note) {
  return juce::MidiMessage::getMidiNoteName(juce::jlimit(0, 127, note), true,
                                            true, 4);
}

inline int noteNumber(const juce::String& text, int fallback) {
  for (int note = 0; note < 128; ++note)
    if (text.trim().equalsIgnoreCase(noteName(note))) return note;
  return fallback;
}

// The selected starting point remains visible; edits are explicitly marked.
class PresetMenu final : public juce::ComboBox, private juce::Timer {
 public:
  void remember(juce::AudioProcessor& processor) {
    processor_ = &processor;
    name_ = getText();
    values_.clear();
    for (const auto* parameter : processor.getParameters())
      values_.push_back(parameter->getValue());
    startTimerHz(4);
  }

 private:
  void timerCallback() override {
    if (!processor_) return;
    const auto& parameters = processor_->getParameters();
    bool modified =
        static_cast<std::size_t>(parameters.size()) != values_.size();
    for (int i = 0; !modified && i < parameters.size(); ++i)
      modified =
          parameters[i]->getValue() != values_[static_cast<std::size_t>(i)];
    const auto label = name_ + (modified ? " *" : "");
    if (getText() != label) setText(label, juce::dontSendNotification);
  }
  juce::AudioProcessor* processor_{};
  juce::String name_;
  std::vector<float> values_;
};

}  // namespace aste::ui
