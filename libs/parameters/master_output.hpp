#pragma once

#include "precision_slider.hpp"
#include <cmath>

namespace aste::ui {

// The final level stage is independent of each instrument's character/drive.
class MasterGain {
 public:
  static void addParameter(
      juce::AudioProcessorValueTreeState::ParameterLayout& layout,
      float initial = 0.F) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{"master", 1}, "Master Output",
        juce::NormalisableRange<float>{-36.F, 18.F, .01F}, initial,
        juce::AudioParameterFloatAttributes{}.withLabel("dB")));
  }
  void bind(juce::AudioProcessorValueTreeState& state) noexcept {
    value_ = state.getRawParameterValue("master");
  }
  void prepare(double rate) noexcept {
    gain_.reset(rate, .02);
    gain_.setCurrentAndTargetValue(target());
  }
  float process(juce::AudioBuffer<float>& audio) noexcept {
    gain_.setTargetValue(target());
    float peak = 0;
    for (int n = 0; n < audio.getNumSamples(); ++n) {
      const float gain = gain_.getNextValue();
      for (int ch = 0; ch < audio.getNumChannels(); ++ch) {
        const float sample = audio.getSample(ch, n) * gain;
        const float clean = std::isfinite(sample) ? sample : 0.F;
        audio.setSample(ch, n, clean);
        peak = std::max(peak, std::abs(clean));
      }
    }
    return peak;
  }

 private:
  float target() const noexcept {
    float db = value_ ? value_->load(std::memory_order_relaxed) : 0.F;
    if (std::abs(db) < .005F) db = 0.F;
    return juce::Decibels::decibelsToGain(
        std::isfinite(db) ? std::clamp(db, -36.F, 18.F) : 0.F);
  }
  std::atomic<float>* value_{};
  juce::SmoothedValue<float> gain_{1.F};
};

class MasterOutput final : public juce::Component {
 public:
  explicit MasterOutput(juce::AudioProcessorValueTreeState& state)
      : attachment_{state, "master", level_} {
    label_.setText("MASTER OUTPUT", juce::dontSendNotification);
    label_.setFont(juce::FontOptions{11.F, juce::Font::bold});
    level_.setSliderStyle(juce::Slider::LinearHorizontal);
    level_.setTextBoxStyle(juce::Slider::TextBoxRight, false, 76, 24);
    level_.setTextValueSuffix(" dB");
    level_.textFromValueFunction = [](double value) {
      return juce::String{std::abs(value) < .005 ? 0. : value, 2};
    };
    level_.setMouseDragSensitivity(500);
    level_.setDoubleClickReturnValue(true, 0);
    level_.setTitle("MASTER OUTPUT");
    level_.setTooltip(
        "Final clean level after sound shaping. Type an exact dB value. "
        "Double-click for unity gain.");
    setTitle("MASTER OUTPUT");
    addAndMakeVisible(label_);
    addAndMakeVisible(level_);
    level_.updateText();
  }
  void resized() override {
    auto area = getLocalBounds();
    label_.setBounds(area.removeFromTop(17));
    level_.setBounds(area);
  }

 private:
  juce::Label label_;
  PrecisionSlider level_;
  juce::AudioProcessorValueTreeState::SliderAttachment attachment_;
};
}  // namespace aste::ui
