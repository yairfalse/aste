#include "sequence_plugin.hpp"

#include "decimal_parse.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>

namespace aste::sequence::plugin {
namespace {

constexpr auto kStateType = "sequence-s01";
constexpr int kStateSchema = 1;
constexpr auto kSurface = 0xff0d1114U;
constexpr auto kPanel = 0xff171d21U;
constexpr auto kInk = 0xffe7ece8U;
constexpr auto kMuted = 0xff77858bU;
constexpr auto kAccent = 0xff3f8fa8U;
constexpr std::array<const char*, 19> kMainIds{
    "pressure", "shape",     "osc_mix",     "detune",     "sub",
    "cutoff",   "resonance", "filter_form", "env_amount", "attack",
    "decay",    "sustain",   "release",     "glide",      "output",
    "root",     "division",  "sequence",    "bypass"};

juce::String stepId(std::size_t step, const char* field) {
  return "step_" + juce::String{static_cast<int>(step + 1)}.paddedLeft('0', 2) +
         "_" + field;
}

juce::String stepName(std::size_t step, const char* field) {
  return "Step " + juce::String{static_cast<int>(step + 1)}.paddedLeft('0', 2) +
         " " + field;
}

bool parseFiniteFloat(const juce::var& value, float& result) {
  double parsed{};
  if (!aste::parameters::parseFiniteDecimal(value.toString().toStdString(),
                                            parsed)) {
    return false;
  }
  result = static_cast<float>(parsed);
  return std::isfinite(result);
}

juce::ValueTree migrateState(juce::ValueTree state) {
  if (!state.isValid() || state.getType().toString() != kStateType ||
      state.getProperty("product").toString() != kStateType ||
      static_cast<int>(state.getProperty("schema", -1)) != kStateSchema) {
    return {};
  }
  return state;
}

juce::NormalisableRange<float> skewed(float minimum, float maximum,
                                      float centre, float interval) {
  juce::NormalisableRange<float> range{minimum, maximum, interval};
  range.setSkewForCentre(centre);
  return range;
}

void addFloat(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
              const char* id, const char* name,
              juce::NormalisableRange<float> range, float initial,
              const char* unit) {
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, range, initial,
      juce::AudioParameterFloatAttributes{}.withLabel(unit)));
}

struct FactoryPreset {
  const char* name;
  std::array<float, kMainIds.size()> main;
  std::array<int, kStepCount> notes;
  unsigned gates;
  unsigned accents;
  unsigned slides;
};

constexpr std::array<FactoryPreset, 4> kPresets{{
    {"Default",
     {35, 25, 45, 0.08F, 25, 900, 35, 45, 55, 3, 180, 55, 120, 70, -6, 36, 1, 1,
      0},
     {0, 0, 7, 0, 12, 7, 3, 0, 0, -5, 0, 7, 3, 0, -2, 0},
     0xdb77U,
     0x1111U,
     0x2222U},
    {"Low Current",
     {48, 10, 38, -0.05F, 55, 420, 52, 72, 72, 2, 260, 48, 180, 110, -8, 31, 1,
      1, 0},
     {0, 0, 3, 0, 7, 3, -2, 0, 0, -5, -2, 3, 0, -2, -5, 0},
     0xfff7U,
     0x1041U,
     0x4a22U},
    {"Open Form",
     {24, 62, 54, 0.12F, 12, 2400, 28, 18, 38, 8, 340, 64, 260, 45, -7, 43, 1,
      1, 0},
     {0, 4, 7, 11, 12, 7, 4, 0, 7, 11, 14, 11, 7, 4, 2, 0},
     0xffffU,
     0x1111U,
     0x8888U},
    {"Pressure Study",
     {82, 42, 62, 0.18F, 38, 680, 66, 86, 82, 1, 120, 42, 90, 140, -10, 36, 2,
      1, 0},
     {0, 0, 12, 7, 3, 0, -5, 0, 12, 7, 3, 0, -2, -5, 7, 0},
     0xf7ffU,
     0x4921U,
     0x2522U},
}};

class SequenceLookAndFeel final : public juce::LookAndFeel_V4 {
 public:
  SequenceLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, juce::Colour{kInk});
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour{kSurface});
    setColour(juce::Slider::textBoxOutlineColourId,
              juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, juce::Colour{kInk});
    setColour(juce::ToggleButton::textColourId, juce::Colour{kInk});
  }

  void drawRotarySlider(juce::Graphics& graphics, int x, int y, int width,
                        int height, float position, float startAngle,
                        float endAngle, juce::Slider&) override {
    const auto bounds =
        juce::Rectangle<float>{static_cast<float>(x), static_cast<float>(y),
                               static_cast<float>(width),
                               static_cast<float>(height)}
            .reduced(7.0F);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5F;
    const auto centre = bounds.getCentre();
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0F, startAngle,
                        endAngle, true);
    graphics.setColour(juce::Colour{kMuted}.withAlpha(0.28F));
    graphics.strokePath(track, juce::PathStrokeType{3.0F});
    juce::Path value;
    value.addCentredArc(centre.x, centre.y, radius, radius, 0.0F, startAngle,
                        startAngle + position * (endAngle - startAngle), true);
    graphics.setColour(juce::Colour{kAccent});
    graphics.strokePath(value, juce::PathStrokeType{4.0F});
    const float angle = startAngle + position * (endAngle - startAngle);
    juce::Path pointer;
    pointer.addRectangle(-1.4F, -radius + 4.0F, 2.8F, radius * 0.36F);
    graphics.setColour(juce::Colour{kInk});
    graphics.fillPath(
        pointer,
        juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
  }
};

class Knob final : public juce::Component {
 public:
  Knob(juce::AudioProcessorValueTreeState& state, const char* id,
       const char* name, const char* suffix, double initial, int order)
      : attachment_{state, id, slider_} {
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 80, 20);
    slider_.setTextValueSuffix(suffix);
    slider_.setDoubleClickReturnValue(true, initial);
    slider_.setTitle(name);
    slider_.setDescription(juce::String{name} + " parameter");
    slider_.setWantsKeyboardFocus(true);
    slider_.setExplicitFocusOrder(order);
    label_.setText(name, juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(juce::FontOptions{10.0F, juce::Font::bold});
    addAndMakeVisible(slider_);
    addAndMakeVisible(label_);
  }

  void resized() override {
    auto area = getLocalBounds();
    label_.setBounds(area.removeFromTop(18));
    slider_.setBounds(area);
  }

 private:
  juce::Slider slider_;
  juce::Label label_;
  juce::AudioProcessorValueTreeState::SliderAttachment attachment_;
};

class StepCell final : public juce::Component {
 public:
  StepCell(juce::AudioProcessorValueTreeState& state, std::size_t index)
      : index_{index},
        noteAttachment_{state, stepId(index, "note"), note_},
        gateAttachment_{state, stepId(index, "gate"), gate_},
        accentAttachment_{state, stepId(index, "accent"), accent_},
        slideAttachment_{state, stepId(index, "slide"), slide_} {
    setTitle(stepName(index, "Program"));
    note_.setSliderStyle(juce::Slider::LinearVertical);
    note_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 48, 19);
    note_.setTitle(stepName(index, "Note"));
    note_.setDoubleClickReturnValue(true, 0.0);
    note_.setWantsKeyboardFocus(true);
    note_.setExplicitFocusOrder(30 + static_cast<int>(index));
    gate_.setButtonText({});
    gate_.setTitle(stepName(index, "Gate"));
    accent_.setButtonText({});
    accent_.setTitle(stepName(index, "Accent"));
    slide_.setButtonText({});
    slide_.setTitle(stepName(index, "Slide"));
    for (auto* component :
         std::array<juce::Component*, 4>{&note_, &gate_, &accent_, &slide_}) {
      addAndMakeVisible(component);
    }
  }

  void setActive(bool active) {
    if (active_ != active) {
      active_ = active;
      repaint();
    }
  }

  void paint(juce::Graphics& graphics) override {
    graphics.fillAll(active_ ? juce::Colour{kAccent}.withAlpha(0.22F)
                             : juce::Colour{kPanel});
    graphics.setColour(active_ ? juce::Colour{kAccent} : juce::Colour{kMuted});
    graphics.fillRect(0, 0, getWidth(), active_ ? 3 : 1);
    graphics.setColour(juce::Colour{kMuted});
    graphics.setFont(juce::FontOptions{10.0F, juce::Font::bold});
    graphics.drawText(
        juce::String{static_cast<int>(index_ + 1)}.paddedLeft('0', 2), 0, 5,
        getWidth(), 14, juce::Justification::centred);
    graphics.setFont(juce::FontOptions{8.0F, juce::Font::bold});
    graphics.drawText("G     A     S", 0, getHeight() - 34, getWidth(), 10,
                      juce::Justification::centred);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(3);
    area.removeFromTop(18);
    auto buttons = area.removeFromBottom(23);
    area.removeFromBottom(11);
    const int width = buttons.getWidth() / 3;
    gate_.setBounds(buttons.removeFromLeft(width));
    accent_.setBounds(buttons.removeFromLeft(width));
    slide_.setBounds(buttons);
    note_.setBounds(area);
  }

 private:
  std::size_t index_{};
  bool active_{};
  juce::Slider note_;
  juce::ToggleButton gate_;
  juce::ToggleButton accent_;
  juce::ToggleButton slide_;
  juce::AudioProcessorValueTreeState::SliderAttachment noteAttachment_;
  juce::AudioProcessorValueTreeState::ButtonAttachment gateAttachment_;
  juce::AudioProcessorValueTreeState::ButtonAttachment accentAttachment_;
  juce::AudioProcessorValueTreeState::ButtonAttachment slideAttachment_;
};

class SequenceEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer {
 public:
  explicit SequenceEditor(SequenceAudioProcessor& processor)
      : AudioProcessorEditor{processor},
        processor_{processor},
        pressure_{processor.state(), "pressure", "PRESSURE", " %", 35, 1},
        cutoff_{processor.state(), "cutoff", "CUTOFF", " Hz", 900, 2},
        resonance_{processor.state(), "resonance", "RESONANCE", " %", 35, 3},
        filterForm_{
            processor.state(), "filter_form", "FILTER FORM", " %", 45, 4},
        shape_{processor.state(), "shape", "SHAPE", " %", 25, 5},
        oscillatorMix_{processor.state(), "osc_mix", "OSC MIX", " %", 45, 6},
        detune_{processor.state(), "detune", "DETUNE", " st", 0.08, 7},
        sub_{processor.state(), "sub", "SUB", " %", 25, 8},
        envelopeAmount_{
            processor.state(), "env_amount", "ENV AMOUNT", " %", 55, 9},
        attack_{processor.state(), "attack", "ATTACK", " ms", 3, 10},
        decay_{processor.state(), "decay", "DECAY", " ms", 180, 11},
        sustain_{processor.state(), "sustain", "SUSTAIN", " %", 55, 12},
        release_{processor.state(), "release", "RELEASE", " ms", 120, 13},
        glide_{processor.state(), "glide", "GLIDE", " ms", 70, 14},
        output_{processor.state(), "output", "OUTPUT", " dB", -6, 15},
        root_{processor.state(), "root", "ROOT", " MIDI", 36, 16},
        divisionAttachment_{processor.state(), "division", division_},
        sequenceAttachment_{processor.state(), "sequence", sequence_} {
    setLookAndFeel(&lookAndFeel_);
    division_.addItemList({"1/8", "1/16", "1/32"}, 1);
    division_.setSelectedId(2, juce::dontSendNotification);
    division_.setTitle("DIVISION");
    division_.setWantsKeyboardFocus(true);
    division_.setExplicitFocusOrder(17);
    sequence_.setButtonText("SEQUENCE");
    sequence_.setTitle("SEQUENCE");
    sequence_.setWantsKeyboardFocus(true);
    sequence_.setExplicitFocusOrder(18);
    preset_.setTextWhenNothingSelected("PRESETS");
    preset_.setTitle("PRESETS");
    preset_.setWantsKeyboardFocus(true);
    preset_.setExplicitFocusOrder(19);
    for (int index = 0; index < processor_.factoryPresetCount(); ++index) {
      preset_.addItem(processor_.factoryPresetName(index), index + 1);
    }
    preset_.onChange = [this] {
      if (const int selected = preset_.getSelectedId(); selected > 0) {
        processor_.loadFactoryPreset(selected - 1);
        preset_.setSelectedId(0, juce::dontSendNotification);
      }
    };
    for (std::size_t index = 0; index < steps_.size(); ++index) {
      steps_[index] = std::make_unique<StepCell>(processor_.state(), index);
      addAndMakeVisible(*steps_[index]);
    }
    for (auto* component : std::array<juce::Component*, 19>{
             &pressure_, &cutoff_, &resonance_, &filterForm_, &shape_,
             &oscillatorMix_, &detune_, &sub_, &envelopeAmount_, &attack_,
             &decay_, &sustain_, &release_, &glide_, &output_, &root_,
             &division_, &sequence_, &preset_}) {
      addAndMakeVisible(component);
    }
    setResizable(true, true);
    setResizeLimits(960, 600, 1600, 1000);
    setSize(1280, 760);
    startTimerHz(30);
  }

  ~SequenceEditor() override { setLookAndFeel(nullptr); }

  void paint(juce::Graphics& graphics) override {
    graphics.fillAll(juce::Colour{kSurface});
    auto header = getLocalBounds().reduced(24).removeFromTop(48);
    graphics.setColour(juce::Colour{kInk});
    graphics.setFont(juce::FontOptions{22.0F, juce::Font::bold});
    graphics.drawText("SEQUENCE", header.removeFromLeft(190),
                      juce::Justification::centredLeft);
    graphics.setColour(juce::Colour{kAccent});
    graphics.setFont(juce::FontOptions{12.0F, juce::Font::bold});
    header.removeFromRight(205);
    graphics.drawText("S-01 / PROGRAMMED CURRENT", header,
                      juce::Justification::centredRight);
    graphics.setColour(juce::Colour{kMuted}.withAlpha(0.35F));
    graphics.fillRect(24, 72, getWidth() - 48, 1);

    auto meter = juce::Rectangle<float>{24.0F, 88.0F, 12.0F,
                                        static_cast<float>(getHeight() - 118)};
    graphics.setColour(juce::Colour{kPanel});
    graphics.fillRect(meter);
    auto fill = meter;
    fill.removeFromTop(meter.getHeight() * (1.0F - displayedPeak_));
    graphics.setColour(juce::Colour{kAccent});
    graphics.fillRect(fill);
    graphics.setColour(juce::Colour{kMuted});
    graphics.setFont(juce::FontOptions{10.0F, juce::Font::bold});
    graphics.drawText("OUT", 15, getHeight() - 27, 30, 12,
                      juce::Justification::centred);
  }

  void resized() override {
    preset_.setBounds(getWidth() - 194, 24, 170, 28);
    auto area = getLocalBounds().reduced(48, 24);
    area.removeFromTop(60);
    auto sequenceArea = area.removeFromBottom(
        std::max(205, static_cast<int>(area.getHeight() * 0.42F)));
    area.removeFromBottom(14);
    const int columns = 8;
    const int knobWidth = std::max(1, area.getWidth() / columns);
    const int knobHeight = std::max(1, area.getHeight() / 2);
    const std::array<Knob*, 16> knobs{
        &pressure_,       &cutoff_,        &resonance_, &filterForm_,
        &shape_,          &oscillatorMix_, &detune_,    &sub_,
        &envelopeAmount_, &attack_,        &decay_,     &sustain_,
        &release_,        &glide_,         &output_,    &root_};
    for (std::size_t index = 0; index < knobs.size(); ++index) {
      const int column = static_cast<int>(index % columns);
      const int row = static_cast<int>(index / columns);
      knobs[index]->setBounds(area.getX() + column * knobWidth,
                              area.getY() + row * knobHeight, knobWidth,
                              knobHeight);
    }
    auto sequenceHeader = sequenceArea.removeFromTop(34);
    sequence_.setBounds(sequenceHeader.removeFromLeft(120));
    division_.setBounds(sequenceHeader.removeFromLeft(100).reduced(5, 2));
    const int stepWidth = std::max(1, sequenceArea.getWidth() / 16);
    for (std::size_t index = 0; index < steps_.size(); ++index) {
      steps_[index]->setBounds(
          sequenceArea.getX() + static_cast<int>(index) * stepWidth,
          sequenceArea.getY(), stepWidth - 2, sequenceArea.getHeight());
    }
  }

 private:
  void timerCallback() override {
    displayedPeak_ = std::max(processor_.outputPeak(), displayedPeak_ * 0.84F);
    const int active = processor_.currentStep();
    for (std::size_t index = 0; index < steps_.size(); ++index) {
      steps_[index]->setActive(static_cast<int>(index) == active);
    }
    repaint();
  }

  SequenceLookAndFeel lookAndFeel_;
  SequenceAudioProcessor& processor_;
  Knob pressure_;
  Knob cutoff_;
  Knob resonance_;
  Knob filterForm_;
  Knob shape_;
  Knob oscillatorMix_;
  Knob detune_;
  Knob sub_;
  Knob envelopeAmount_;
  Knob attack_;
  Knob decay_;
  Knob sustain_;
  Knob release_;
  Knob glide_;
  Knob output_;
  Knob root_;
  juce::ComboBox division_;
  juce::ToggleButton sequence_;
  juce::ComboBox preset_;
  juce::AudioProcessorValueTreeState::ComboBoxAttachment divisionAttachment_;
  juce::AudioProcessorValueTreeState::ButtonAttachment sequenceAttachment_;
  std::array<std::unique_ptr<StepCell>, kStepCount> steps_{};
  float displayedPeak_{};
};

}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout
SequenceAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  addFloat(layout, "pressure", "Pressure", {0.0F, 100.0F, 0.01F}, 35.0F, "%");
  addFloat(layout, "shape", "Shape", {0.0F, 100.0F, 0.01F}, 25.0F, "%");
  addFloat(layout, "osc_mix", "Oscillator Mix", {0.0F, 100.0F, 0.01F}, 45.0F,
           "%");
  addFloat(layout, "detune", "Detune", {-12.0F, 12.0F, 0.01F}, 0.08F, "st");
  addFloat(layout, "sub", "Sub", {0.0F, 100.0F, 0.01F}, 25.0F, "%");
  addFloat(layout, "cutoff", "Cutoff", skewed(30.0F, 18000.0F, 900.0F, 0.1F),
           900.0F, "Hz");
  addFloat(layout, "resonance", "Resonance", {0.0F, 100.0F, 0.01F}, 35.0F, "%");
  addFloat(layout, "filter_form", "Filter Form", {0.0F, 100.0F, 0.01F}, 45.0F,
           "%");
  addFloat(layout, "env_amount", "Envelope Amount", {0.0F, 100.0F, 0.01F},
           55.0F, "%");
  addFloat(layout, "attack", "Attack", skewed(0.2F, 2000.0F, 20.0F, 0.1F), 3.0F,
           "ms");
  addFloat(layout, "decay", "Decay", skewed(5.0F, 4000.0F, 250.0F, 0.1F),
           180.0F, "ms");
  addFloat(layout, "sustain", "Sustain", {0.0F, 100.0F, 0.01F}, 55.0F, "%");
  addFloat(layout, "release", "Release", skewed(5.0F, 5000.0F, 300.0F, 0.1F),
           120.0F, "ms");
  addFloat(layout, "glide", "Glide", skewed(0.0F, 1000.0F, 80.0F, 0.1F), 70.0F,
           "ms");
  addFloat(layout, "output", "Output", {-24.0F, 6.0F, 0.01F}, -6.0F, "dB");
  layout.add(std::make_unique<juce::AudioParameterInt>(
      juce::ParameterID{"root", 1}, "Root", 24, 60, 36));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"division", 1}, "Division",
      juce::StringArray{"1/8", "1/16", "1/32"}, 1));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"sequence", 1}, "Sequence", true));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"bypass", 1}, "Bypass", false));
  const Parameters defaults;
  for (std::size_t step = 0; step < kStepCount; ++step) {
    layout.add(std::make_unique<juce::AudioParameterInt>(
        juce::ParameterID{stepId(step, "note"), 1}, stepName(step, "Note"), -12,
        12, defaults.steps[step].note));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{stepId(step, "gate"), 1}, stepName(step, "Gate"),
        defaults.steps[step].gate));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{stepId(step, "accent"), 1}, stepName(step, "Accent"),
        defaults.steps[step].accent));
    layout.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{stepId(step, "slide"), 1}, stepName(step, "Slide"),
        defaults.steps[step].slide));
  }
  return layout;
}

SequenceAudioProcessor::SequenceAudioProcessor()
    : AudioProcessor{BusesProperties{}.withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)},
      state_{*this, nullptr, kStateType, createParameterLayout()} {
  for (std::size_t index = 0; index < kMainIds.size(); ++index) {
    mainValues_[index] = state_.getRawParameterValue(kMainIds[index]);
    jassert(mainValues_[index] != nullptr);
  }
  for (std::size_t step = 0; step < kStepCount; ++step) {
    noteValues_[step] = state_.getRawParameterValue(stepId(step, "note"));
    gateValues_[step] = state_.getRawParameterValue(stepId(step, "gate"));
    accentValues_[step] = state_.getRawParameterValue(stepId(step, "accent"));
    slideValues_[step] = state_.getRawParameterValue(stepId(step, "slide"));
  }
}

Parameters SequenceAudioProcessor::currentParameters() const noexcept {
  Parameters parameters;
  parameters.pressure =
      mainValues_[pressure]->load(std::memory_order_relaxed) * 0.01F;
  parameters.shape =
      mainValues_[shape]->load(std::memory_order_relaxed) * 0.01F;
  parameters.oscillatorMix =
      mainValues_[oscillatorMix]->load(std::memory_order_relaxed) * 0.01F;
  parameters.detuneSemitones =
      mainValues_[detune]->load(std::memory_order_relaxed);
  parameters.subLevel =
      mainValues_[sub]->load(std::memory_order_relaxed) * 0.01F;
  parameters.cutoffHz = mainValues_[cutoff]->load(std::memory_order_relaxed);
  parameters.resonance =
      mainValues_[resonance]->load(std::memory_order_relaxed) * 0.01F;
  parameters.filterMorph =
      mainValues_[filterMorph]->load(std::memory_order_relaxed) * 0.01F;
  parameters.envelopeAmount =
      mainValues_[envelopeAmount]->load(std::memory_order_relaxed) * 0.01F;
  parameters.attackMs = mainValues_[attack]->load(std::memory_order_relaxed);
  parameters.decayMs = mainValues_[decay]->load(std::memory_order_relaxed);
  parameters.sustain =
      mainValues_[sustain]->load(std::memory_order_relaxed) * 0.01F;
  parameters.releaseMs = mainValues_[release]->load(std::memory_order_relaxed);
  parameters.glideMs = mainValues_[glide]->load(std::memory_order_relaxed);
  parameters.outputDb = mainValues_[output]->load(std::memory_order_relaxed);
  parameters.rootNote =
      static_cast<int>(mainValues_[root]->load(std::memory_order_relaxed));
  const int divisionIndex =
      static_cast<int>(mainValues_[division]->load(std::memory_order_relaxed));
  parameters.division = divisionIndex == 0 ? 8 : (divisionIndex == 2 ? 32 : 16);
  parameters.sequenceEnabled =
      mainValues_[sequenceEnabled]->load(std::memory_order_relaxed) >= 0.5F;
  parameters.bypass =
      mainValues_[bypass]->load(std::memory_order_relaxed) >= 0.5F;
  for (std::size_t step = 0; step < kStepCount; ++step) {
    parameters.steps[step] = {
        static_cast<int>(noteValues_[step]->load(std::memory_order_relaxed)),
        gateValues_[step]->load(std::memory_order_relaxed) >= 0.5F,
        accentValues_[step]->load(std::memory_order_relaxed) >= 0.5F,
        slideValues_[step]->load(std::memory_order_relaxed) >= 0.5F};
  }
  return parameters;
}

Transport SequenceAudioProcessor::currentTransport() const noexcept {
  Transport result;
  if (const auto* playHead = getPlayHead()) {
    if (const auto position = playHead->getPosition()) {
      const auto bpm = position->getBpm();
      const auto ppq = position->getPpqPosition();
      if (bpm && ppq && std::isfinite(*bpm) && std::isfinite(*ppq)) {
        result.valid = true;
        result.playing = position->getIsPlaying();
        result.bpm = *bpm;
        result.ppqPosition = *ppq;
      }
    }
  }
  return result;
}

std::span<const MidiEvent> SequenceAudioProcessor::readMidi(
    const juce::MidiBuffer& midi, int frames) noexcept {
  midiCount_ = 0;
  for (const auto metadata : midi) {
    if (midiCount_ >= midiScratch_.size()) {
      break;
    }
    const auto message = metadata.getMessage();
    MidiEvent event;
    event.sampleOffset = static_cast<std::size_t>(
        juce::jlimit(0, std::max(0, frames - 1), metadata.samplePosition));
    event.note = message.getNoteNumber();
    if (message.isNoteOn()) {
      event.type = MidiEventType::noteOn;
      event.velocity = message.getFloatVelocity();
    } else if (message.isNoteOff()) {
      event.type = MidiEventType::noteOff;
    } else if (message.isAllNotesOff() || message.isAllSoundOff()) {
      event.type = MidiEventType::allNotesOff;
    } else {
      continue;
    }
    midiScratch_[midiCount_++] = event;
  }
  return {midiScratch_.data(), midiCount_};
}

void SequenceAudioProcessor::prepareToPlay(double sampleRate, int) {
  processor_.prepare(sampleRate, currentParameters());
  setLatencySamples(static_cast<int>(processor_.latencySamples()));
  publishMeters({});
}

void SequenceAudioProcessor::releaseResources() { processor_.reset(); }

bool SequenceAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
  const auto input = layouts.getMainInputChannelSet();
  const auto outputSet = layouts.getMainOutputChannelSet();
  return input.isDisabled() && (outputSet == juce::AudioChannelSet::mono() ||
                                outputSet == juce::AudioChannelSet::stereo());
}

void SequenceAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                          juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  const int channels = buffer.getNumChannels();
  const int frames = buffer.getNumSamples();
  if (channels < 1 || frames == 0) {
    publishMeters({});
    return;
  }
  processor_.process(buffer.getWritePointer(0),
                     channels > 1 ? buffer.getWritePointer(1) : nullptr,
                     static_cast<std::size_t>(frames), currentParameters(),
                     currentTransport(), readMidi(midi, frames));
  for (int channel = 2; channel < channels; ++channel) {
    buffer.clear(channel, 0, frames);
  }
  publishMeters(processor_.meters());
}

void SequenceAudioProcessor::processBlockBypassed(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
  buffer.clear();
  publishMeters({});
}

void SequenceAudioProcessor::publishMeters(
    const MeterSnapshot& meters) noexcept {
  outputPeak_.store(meters.outputPeak, std::memory_order_relaxed);
  envelopeLevel_.store(meters.envelope, std::memory_order_relaxed);
  currentStep_.store(meters.currentStep, std::memory_order_relaxed);
}

float SequenceAudioProcessor::outputPeak() const noexcept {
  return outputPeak_.load(std::memory_order_relaxed);
}

float SequenceAudioProcessor::envelopeLevel() const noexcept {
  return envelopeLevel_.load(std::memory_order_relaxed);
}

int SequenceAudioProcessor::currentStep() const noexcept {
  return static_cast<int>(currentStep_.load(std::memory_order_relaxed));
}

int SequenceAudioProcessor::factoryPresetCount() noexcept {
  return static_cast<int>(kPresets.size());
}

juce::String SequenceAudioProcessor::factoryPresetName(int index) {
  return juce::isPositiveAndBelow(index, factoryPresetCount())
             ? kPresets[static_cast<std::size_t>(index)].name
             : juce::String{};
}

void SequenceAudioProcessor::loadFactoryPreset(int index) {
  if (!juce::isPositiveAndBelow(index, factoryPresetCount())) {
    return;
  }
  const auto& preset = kPresets[static_cast<std::size_t>(index)];
  for (std::size_t parameter = 0; parameter < preset.main.size(); ++parameter) {
    if (auto* destination = state_.getParameter(kMainIds[parameter])) {
      destination->setValueNotifyingHost(
          destination->convertTo0to1(preset.main[parameter]));
    }
  }
  for (std::size_t step = 0; step < kStepCount; ++step) {
    const std::array values{static_cast<float>(preset.notes[step]),
                            (preset.gates >> step) & 1U ? 1.0F : 0.0F,
                            (preset.accents >> step) & 1U ? 1.0F : 0.0F,
                            (preset.slides >> step) & 1U ? 1.0F : 0.0F};
    const std::array<const char*, 4> fields{"note", "gate", "accent", "slide"};
    for (std::size_t field = 0; field < fields.size(); ++field) {
      if (auto* destination =
              state_.getParameter(stepId(step, fields[field]))) {
        destination->setValueNotifyingHost(
            destination->convertTo0to1(values[field]));
      }
    }
  }
}

juce::AudioProcessorParameter* SequenceAudioProcessor::getBypassParameter()
    const {
  return state_.getParameter("bypass");
}

void SequenceAudioProcessor::getStateInformation(juce::MemoryBlock& output) {
  auto state = state_.copyState();
  state.setProperty("schema", kStateSchema, nullptr);
  state.setProperty("product", kStateType, nullptr);
  if (const auto xml = state.createXml()) {
    copyXmlToBinary(*xml, output);
  }
}

void SequenceAudioProcessor::setStateInformation(const void* data, int size) {
  const auto xml = getXmlFromBinary(data, size);
  if (xml == nullptr) {
    return;
  }
  const auto restored = migrateState(juce::ValueTree::fromXml(*xml));
  if (!restored.isValid()) {
    return;
  }
  auto validated = state_.copyState();
  for (int index = 0; index < validated.getNumChildren(); ++index) {
    auto child = validated.getChild(index);
    if (auto* parameter =
            state_.getParameter(child.getProperty("id").toString())) {
      child.setProperty(
          "value", parameter->convertFrom0to1(parameter->getDefaultValue()),
          nullptr);
    }
  }
  juce::StringArray seen;
  for (int index = 0; index < restored.getNumChildren(); ++index) {
    const auto child = restored.getChild(index);
    const auto id = child.getProperty("id").toString();
    auto* parameter = state_.getParameter(id);
    if (parameter == nullptr) {
      continue;
    }
    if (seen.contains(id) || !child.hasProperty("value")) {
      return;
    }
    seen.add(id);
    float value{};
    if (!parseFiniteFloat(child.getProperty("value"), value)) {
      return;
    }
    const auto& range = parameter->getNormalisableRange();
    auto destination = validated.getChildWithProperty("id", id);
    if (!destination.isValid()) {
      return;
    }
    destination.setProperty(
        "value", juce::jlimit(range.start, range.end, value), nullptr);
  }
  validated.setProperty("schema", kStateSchema, nullptr);
  validated.setProperty("product", kStateType, nullptr);
  state_.replaceState(validated);
}

juce::AudioProcessorEditor* SequenceAudioProcessor::createEditor() {
  return new SequenceEditor{*this};
}

}  // namespace aste::sequence::plugin

#if !defined(ASTE_NO_PLUGIN_FACTORY)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::sequence::plugin::SequenceAudioProcessor{};
}
#endif
