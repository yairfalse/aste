#pragma once

#include "precision_slider.hpp"
#include "workflow_guide.hpp"

#include <juce_audio_utils/juce_audio_utils.h>

#include <vector>
#include <functional>
#include <cmath>

namespace aste::ui {

inline juce::String formattedValue(const juce::Slider& slider, double value) {
  const int decimals = slider.getNumDecimalPlacesToDisplay();
  return juce::String{
      std::abs(value) < 0.5 * std::pow(10.0, -decimals) ? 0.0 : value,
      decimals};
}

inline juce::String explanation(const juce::String& id,
                                const juce::String& product = {}) {
  if (product == "Density D-01" && id == "character")
    return "Give quieter material a denser low body, with asymmetric color and "
           "breathing recovery. Earlier saved responses retain the previous "
           "shaper until updated.";
  if (product == "Harmonic H-01" && id == "harmonic")
    return "Excite and deform the selected frequency regions. Updated material "
           "behavior works at flat and cut gain as well as boosts; older saved "
           "responses color boosts only.";
  if (product == "Field F-01") {
    if (id == "pitch")
      return "Amount of fifth/octave pitch returns in the field. Texture makes "
             "them direct; Focus and Motion move their tuning.";
    if (id == "forever")
      return "Hold the spatial tail. Phrase capture and inner repetition are "
             "separate actions above.";
    if (id == "loop_ms")
      return "Length of the preceding audio to capture. Feed this much audio "
             "before capturing; shorten live, recapture to expand.";
    if (id == "inner_ms")
      return "Length of the repeating fragment inside the captured phrase. "
             "Release Inner to rejoin the advancing phrase.";
    if (id == "loop_speed")
      return "Playback speed of captured audio; changes timing and pitch "
             "together.";
    if (id == "loop_mix")
      return "Blend captured memory against the spatial field while a phrase "
             "is held. Main Blend still controls dry/wet.";
    if (id == "texture")
      return "Expose moving grain heads and stereo pitch layers. Grain and "
             "Pitch determine which layers you hear.";
  }
  if (product == "Resonator R-01") {
    if (id == "root")
      return "Root note of the chosen chord structure. Used in Play chord "
             "roots mode.";
    if (id == "chord")
      return "Intervals that tune the resonating structure. Used in Play chord "
             "roots mode.";
    if (id == "source")
      return "Excitation that supplies energy, not the chord itself. Press "
             "EXCITE or play MIDI to hear it.";
    if (id == "burst")
      return "Duration of the excitation burst in milliseconds.";
    if (id == "excitation")
      return "Energy injected on each excitation. Master sets the final output "
             "level.";
    if (id == "input_mode")
      return "Play chord roots builds the selected structure; individual tones "
             "accepts up to four notes across two octaves.";
    if (id == "memory")
      return "Choose whether the next excitation resets, shares, partly damps, "
             "or overlaps the previous resonant energy.";
    if (id == "retain")
      return "Energy kept before a new excitation in Partial memory mode. "
             "Inactive in other memory modes.";
    if (id == "coupling")
      return "Rate of energy transfer between resonators; affects evolving "
             "beating and harmonic motion.";
    if (id == "stretch")
      return "Stretch resonant frequency relationships away from harmonic "
             "spacing.";
    if (id == "inharmonic")
      return "Move the mode bank away from harmonic relationships.";
    if (id == "damping")
      return "Damp upper resonant energy relative to lower modes.";
    if (id == "persist")
      return "Keep supplying resonant energy; Key Release Damping can end it "
             "when keys are released.";
    if (id == "mutation")
      return "Deterministic spectral variation across repeated excitations, in "
             "cents; does not change a rhythm pattern.";
    if (id == "gate")
      return "Damp resonant energy when the played keys are released.";
  }
  if (product == "Non-Machine N-01") {
    if (id.endsWith("_repeat_gain"))
      return "Amplitude retained by each successive repeat of this selected "
             "hit.";
    if (id.endsWith("_pitch_step"))
      return "Pitch change in semitones for each successive repeat of this "
             "hit.";
    if (id == "seed")
      return "Seed makes variations reproducible. Use NEW RHYTHM to place a "
             "different pattern; Seed alone is not a pattern generator.";
    if (id.endsWith("_cycle"))
      return "Length of this object's independent cycle in quarter notes. "
             "Event positions scale with the cycle.";
    if (id.endsWith("_phase"))
      return "Offset this whole object's cycle against the master phrase.";

    const auto field = id.fromLastOccurrenceOf("_", false, false);
    if (field == "position")
      return "Move this selected hit within its object's cycle. Other hits "
             "keep their positions.";
    if (field == "pitch")
      return "Pitch lock for this hit. Use Hit Sound For All Hits to apply it "
             "to the whole object.";
    if (field == "gain")
      return "Level lock for this hit; Master controls the whole instrument.";
    if (field == "color")
      return "Timbral lock for this hit; its meaning follows the selected "
             "material.";
    if (field == "repeats")
      return "Number of repeats of this selected hit; Interval and "
             "Acceleration place them.";
    if (field == "interval")
      return "Initial distance between repeats, in quarter notes. This is "
             "timing, not sound decay.";
    if (field == "acceleration")
      return "Multiply each repeat interval: below 1 accelerates, above 1 "
             "slows down.";
    if (field == "mutation")
      return "Timbral variation within this hit's repeats; Seed makes it "
             "reproducible.";
    if (field == "every")
      return "Play this hit only every chosen number of local cycles.";
    if (field == "nudge")
      return "Fine timing offset for this hit, in quarter notes.";
    if (field == "articulation")
      return "Restart the source, carry its phase, or crossfade from the "
             "outgoing hit.";
    if (id == "hold")
      return "Capture the master audio fragment. Release to hear current "
             "underlying edits.";
    if (id == "fragment")
      return "Length of the master audio fragment in milliseconds. Feed audio "
             "before holding it.";
  }

  const auto field = id.fromLastOccurrenceOf("_", false, false);
  if (id.endsWith("_material"))
    return "Choose the sound object. Original preserves the earlier source; "
           "the other materials enable Structure, Strike, Pitch Fall and "
           "Roughness.";
  if (id.endsWith("_structure"))
    return "Change modal spacing, modulation depth or fragment density for "
           "this whole object.";
  if (id.endsWith("_strike"))
    return "Change the attack edge and transient excitation for this object.";
  if (id.endsWith("_bend"))
    return "Pitch excursion at the start of each hit, in semitones. Negative "
           "values rise toward the event pitch.";
  if (id.endsWith("_roughness"))
    return "Blend noisy, broken edges into the object's material.";
  if (id == "character")
    return "Asymmetric bite and level-dependent compressor recovery. Zero "
           "retains the previous sound.";
  if (id == "focus")
    return "Remove low-frequency buildup, open the top end and animate grain "
           "and pitch returns. Zero retains the previous field.";
  if (id == "density")
    return "Weight and compression character. Use Blend to mix it with the "
           "original.";
  if (id == "crush")
    return "Compression and saturation strength in the processed branch.";
  if (id == "drive")
    return "Push the audible path into saturation. Legacy Density also drives "
           "its compression detector.";
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
    return "Capture only: 0% writes new input; 100% keeps existing tape and "
           "writes no new input. Empty tape stays silent at 100%.";
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
    return "Processing output trim. Master Output is the final family level "
           "control; match loudness when comparing settings.";
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

// Explicit, non-automatable opt-in for recalled sound behavior; UI thread only.
class ResponseButton final : public juce::TextButton, private juce::Timer {
 public:
  void bind(std::function<bool()> current, std::function<void()> upgrade,
            juce::String explanation) {
    current_ = std::move(current);
    explanation_ = std::move(explanation);
    setTitle("RESPONSE MODE");
    setWantsKeyboardFocus(true);
    setExplicitFocusOrder(900);
    onClick = [this, upgrade = std::move(upgrade)] {
      if (!current_()) upgrade();
      refresh();
    };
    refresh();
    startTimerHz(10);
  }

 private:
  void refresh() {
    const bool current = current_();
    setEnabled(!current);
    setButtonText(current ? "UPDATED SOUND" : "USE UPDATED SOUND");
    setTooltip(explanation_);
    setDescription(
        (current
             ? "Updated sound in use. "
             : "Your saved sound is retained until you choose this update. ") +
        explanation_);
  }
  void timerCallback() override { refresh(); }
  std::function<bool()> current_;
  juce::String explanation_;
};

// Identify values without loading or auditioning a factory scene.
class PresetMenu final : public juce::ComboBox, private juce::Timer {
 public:
  void setMatchFilter(std::function<bool(std::size_t)> filter) {
    matchFilter_ = std::move(filter);
  }
  template <class Emit>
  void addSnapshot(juce::AudioProcessor& processor,
                   juce::AudioProcessorValueTreeState& state, Emit emit) {
    std::vector<float> values;
    for (const auto* parameter : processor.getParameters())
      values.push_back(parameter->getDefaultValue());
    emit([&](const juce::String& id, float plain) {
      if (auto* parameter = state.getParameter(id))
        values[static_cast<std::size_t>(parameter->getParameterIndex())] =
            parameter->convertTo0to1(
                parameter->getNormalisableRange().snapToLegalValue(plain));
    });
    snapshots_.push_back(std::move(values));
  }
  template <class Product>
  void addSceneSnapshot(Product& processor, int scene) {
    // Build reference values in an unattached temporary instance; never load a
    // scene into the live instrument just to name it in the editor.
    Product reference;
    reference.scene(scene);
    addSnapshot(processor, processor.state(), [&](auto emit) {
      for (auto* base : reference.getParameters())
        if (auto* parameter = dynamic_cast<juce::RangedAudioParameter*>(base))
          emit(parameter->paramID,
               parameter->convertFrom0to1(parameter->getValue()));
    });
  }
  void watch(juce::AudioProcessor& processor) {
    processor_ = &processor;
    refresh();
    startTimerHz(4);
  }
  void remember(juce::AudioProcessor& processor) {
    name_ = getText();
    watch(processor);
  }
  void refresh() {
    if (!processor_) return;
    const auto& parameters = processor_->getParameters();
    for (std::size_t index = 0; index < snapshots_.size(); ++index) {
      if (matchFilter_ && !matchFilter_(index)) continue;
      const auto& values = snapshots_[index];
      bool match = values.size() == static_cast<std::size_t>(parameters.size());
      for (int i = 0; match && i < parameters.size(); ++i)
        match =
            parameters[i]->getValue() == values[static_cast<std::size_t>(i)];
      if (match) {
        name_ = getItemText(static_cast<int>(index));
        setSelectedId(getItemId(static_cast<int>(index)),
                      juce::dontSendNotification);
        return;
      }
    }
    const auto label = name_.isEmpty() ? juce::String{"CUSTOM"} : name_ + " *";
    if (getText() != label) setText(label, juce::dontSendNotification);
  }

 private:
  void timerCallback() override { refresh(); }
  juce::AudioProcessor* processor_{};
  juce::String name_;
  std::function<bool(std::size_t)> matchFilter_;
  std::vector<std::vector<float>> snapshots_;
};

}  // namespace aste::ui
