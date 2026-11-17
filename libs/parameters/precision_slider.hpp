#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

namespace aste::ui {
class PrecisionSlider : public juce::Slider {
 public:
  PrecisionSlider() { setMouseDragSensitivity(500); }
  void mouseDrag(const juce::MouseEvent& event) override {
    setMouseDragSensitivity(event.mods.isShiftDown() ? 5000 : 500);
    juce::Slider::mouseDrag(event);
  }
  void mouseUp(const juce::MouseEvent& event) override {
    juce::Slider::mouseUp(event);
    setMouseDragSensitivity(500);
  }
};
}  // namespace aste::ui
