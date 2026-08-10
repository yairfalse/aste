#include "field_plugin.hpp"

#include "decimal_parse.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace aste::field::plugin {
namespace {

constexpr auto kStateType = "field-f01";
constexpr int kStateSchema = 1;
constexpr auto kSurface = 0xff101316U;
constexpr auto kPanel = 0xff181e22U;
constexpr auto kInk = 0xffe7e9e4U;
constexpr auto kMuted = 0xff77858dU;
constexpr auto kAccent = 0xff6d91a8U;
constexpr std::array<const char*, 9> kParameterIds{
    "forever",  "mass",  "grain",  "pitch", "motion",
    "distance", "blend", "output", "bypass"};

struct FactoryPreset {
  const char* name;
  std::array<float, kParameterIds.size()> values;
};

constexpr std::array<FactoryPreset, 5> kPresets{{
    {"Open Field",
     {0.0F, 62.0F, 34.0F, 28.0F, 24.0F, 45.0F, 48.0F, -3.0F, 0.0F}},
    {"Broadcast Hall",
     {0.0F, 48.0F, 12.0F, 0.0F, 16.0F, 68.0F, 38.0F, -2.0F, 0.0F}},
    {"Silver Memory",
     {1.0F, 78.0F, 46.0F, 58.0F, 32.0F, 56.0F, 64.0F, -6.0F, 0.0F}},
    {"Granite Cloud",
     {0.0F, 88.0F, 72.0F, 36.0F, 66.0F, 74.0F, 72.0F, -7.0F, 0.0F}},
    {"White Horizon",
     {1.0F, 94.0F, 58.0F, 82.0F, 45.0F, 38.0F, 76.0F, -9.0F, 0.0F}},
}};

bool parseFiniteFloat(const juce::var& value, float& result) {
  double parsed{};
  if (!aste::parameters::parseFiniteDecimal(value.toString().toStdString(),
                                            parsed))
    return false;
  result = static_cast<float>(parsed);
  return std::isfinite(result);
}

void addPercent(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
                const char* id, const char* name, float initial) {
  const auto attributes = juce::AudioParameterFloatAttributes{}.withLabel("%");
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name,
      juce::NormalisableRange<float>{0.0F, 100.0F, 0.01F}, initial,
      attributes));
}

class FieldLookAndFeel final : public juce::LookAndFeel_V4 {
 public:
  FieldLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, juce::Colour{kInk});
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour{kSurface});
    setColour(juce::Slider::textBoxOutlineColourId,
              juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, juce::Colour{kInk});
  }

  void drawRotarySlider(juce::Graphics& graphics, int x, int y, int width,
                        int height, float position, float startAngle,
                        float endAngle, juce::Slider&) override {
    const auto bounds =
        juce::Rectangle<float>{static_cast<float>(x), static_cast<float>(y),
                               static_cast<float>(width),
                               static_cast<float>(height)}
            .reduced(8.0F);
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
    pointer.addRectangle(-1.5F, -radius + 4.0F, 3.0F, radius * 0.38F);
    graphics.setColour(juce::Colour{kInk});
    graphics.fillPath(
        pointer,
        juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
  }

  void drawToggleButton(juce::Graphics& graphics, juce::ToggleButton& button,
                        bool highlighted, bool down) override {
    auto area = button.getLocalBounds().toFloat().reduced(7.0F);
    const bool active = button.getToggleState();
    graphics.setColour(active ? juce::Colour{kAccent} : juce::Colour{kPanel});
    graphics.fillEllipse(area);
    graphics.setColour(juce::Colour{kAccent}.withAlpha(
        active ? 0.9F : (highlighted || down ? 0.62F : 0.35F)));
    graphics.drawEllipse(area, active ? 5.0F : 2.0F);
    graphics.setColour(active ? juce::Colour{kSurface} : juce::Colour{kInk});
    graphics.setFont(juce::FontOptions{22.0F, juce::Font::bold});
    graphics.drawText(button.getButtonText(), area,
                      juce::Justification::centred);
    graphics.setColour(active ? juce::Colour{kSurface}.withAlpha(0.7F)
                              : juce::Colour{kMuted});
    graphics.setFont(juce::FontOptions{10.0F, juce::Font::bold});
    graphics.drawText(active ? "ENERGY HELD" : "PRESS TO HOLD",
                      area.removeFromBottom(54.0F),
                      juce::Justification::centred);
  }
};

class Knob final : public juce::Component {
 public:
  Knob(juce::AudioProcessorValueTreeState& state, const char* id,
       const char* name, const char* suffix, double initial, int focusOrder)
      : attachment_{state, id, slider_} {
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextValueSuffix(suffix);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 22);
    slider_.setDoubleClickReturnValue(true, initial);
    slider_.setTitle(name);
    slider_.setDescription(juce::String{name} + " parameter");
    slider_.setWantsKeyboardFocus(true);
    slider_.setExplicitFocusOrder(focusOrder);
    label_.setText(name, juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(juce::FontOptions{11.0F, juce::Font::bold});
    addAndMakeVisible(slider_);
    addAndMakeVisible(label_);
  }

  void resized() override {
    auto area = getLocalBounds();
    label_.setBounds(area.removeFromTop(21));
    slider_.setBounds(area);
  }

 private:
  juce::Slider slider_;
  juce::Label label_;
  juce::AudioProcessorValueTreeState::SliderAttachment attachment_;
};

class FieldMeter final : public juce::Component, private juce::Timer {
 public:
  explicit FieldMeter(FieldAudioProcessor& processor) : processor_{processor} {
    startTimerHz(30);
  }

  void paint(juce::Graphics& graphics) override {
    graphics.fillAll(juce::Colour{kPanel});
    auto area = getLocalBounds().toFloat().reduced(14.0F);
    graphics.setColour(juce::Colour{kMuted});
    graphics.setFont(juce::FontOptions{11.0F, juce::Font::bold});
    graphics.drawText("BOUNDARIES", area.removeFromTop(20.0F),
                      juce::Justification::centredLeft);
    auto levels = area.removeFromTop(240.0F);
    drawLevel(graphics, levels.removeFromLeft(levels.getWidth() * 0.5F), input_,
              "IN");
    drawLevel(graphics, levels, output_, "OUT");
    area.removeFromTop(18.0F);
    graphics.drawText("STORED ENERGY", area.removeFromTop(20.0F),
                      juce::Justification::centredLeft);
    drawBar(graphics, area.removeFromTop(15.0F), energy_ * 12.0F);
    area.removeFromTop(16.0F);
    graphics.drawText("RETENTION", area.removeFromTop(20.0F),
                      juce::Justification::centredLeft);
    drawBar(graphics, area.removeFromTop(15.0F), retention_);
  }

 private:
  static void drawLevel(juce::Graphics& graphics, juce::Rectangle<float> area,
                        float level, const char* name) {
    const auto label = area.removeFromBottom(20.0F);
    graphics.setColour(juce::Colour{kMuted});
    graphics.drawText(name, label, juce::Justification::centred);
    area = area.reduced(10.0F, 0.0F);
    graphics.setColour(juce::Colour{kSurface});
    graphics.fillRect(area);
    auto fill = area;
    fill.removeFromTop(area.getHeight() *
                       (1.0F - juce::jlimit(0.0F, 1.0F, level)));
    graphics.setColour(juce::Colour{kInk});
    graphics.fillRect(fill);
  }

  static void drawBar(juce::Graphics& graphics, juce::Rectangle<float> area,
                      float value) {
    graphics.setColour(juce::Colour{kSurface});
    graphics.fillRect(area);
    graphics.setColour(juce::Colour{kAccent});
    graphics.fillRect(
        area.withWidth(area.getWidth() * juce::jlimit(0.0F, 1.0F, value)));
  }

  void timerCallback() override {
    input_ = std::max(processor_.inputPeak(), input_ * 0.84F);
    output_ = std::max(processor_.outputPeak(), output_ * 0.84F);
    energy_ = std::max(processor_.fieldEnergy(), energy_ * 0.9F);
    retention_ = processor_.retention();
    repaint();
  }

  FieldAudioProcessor& processor_;
  float input_{};
  float output_{};
  float energy_{};
  float retention_{};
};

class FieldEditor final : public juce::AudioProcessorEditor {
 public:
  explicit FieldEditor(FieldAudioProcessor& processor)
      : AudioProcessorEditor{processor},
        processor_{processor},
        meter_{processor},
        foreverAttachment_{processor.state(), "forever", forever_},
        mass_{processor.state(), "mass", "MASS", " %", 62.0, 2},
        grain_{processor.state(), "grain", "GRAIN", " %", 34.0, 3},
        pitch_{processor.state(), "pitch", "PITCH", " %", 28.0, 4},
        motion_{processor.state(), "motion", "MOTION", " %", 24.0, 5},
        distance_{processor.state(), "distance", "DISTANCE", " %", 45.0, 6},
        blend_{processor.state(), "blend", "BLEND", " %", 48.0, 7},
        output_{processor.state(), "output", "OUTPUT", " dB", -3.0, 8} {
    setLookAndFeel(&lookAndFeel_);
    forever_.setButtonText("FOREVER");
    forever_.setTitle("FOREVER");
    forever_.setDescription("Hold or release the stored spatial field");
    forever_.setClickingTogglesState(true);
    forever_.setWantsKeyboardFocus(true);
    forever_.setExplicitFocusOrder(1);
    preset_.setTextWhenNothingSelected("PRESETS");
    preset_.setTitle("PRESETS");
    preset_.setDescription("Load a Field factory starting point");
    preset_.setWantsKeyboardFocus(true);
    preset_.setExplicitFocusOrder(9);
    preset_.setColour(juce::ComboBox::backgroundColourId, juce::Colour{kPanel});
    preset_.setColour(juce::ComboBox::textColourId, juce::Colour{kInk});
    preset_.setColour(juce::ComboBox::outlineColourId, juce::Colour{kMuted});
    for (int index = 0; index < processor_.factoryPresetCount(); ++index)
      preset_.addItem(processor_.factoryPresetName(index), index + 1);
    preset_.onChange = [this] {
      if (const int selected = preset_.getSelectedId(); selected > 0) {
        processor_.loadFactoryPreset(selected - 1);
        preset_.setSelectedId(0, juce::dontSendNotification);
      }
    };
    for (auto* component : std::array<juce::Component*, 10>{
             &meter_, &forever_, &mass_, &grain_, &pitch_, &motion_, &distance_,
             &blend_, &output_, &preset_})
      addAndMakeVisible(component);
    setResizable(true, true);
    setResizeLimits(900, 520, 1800, 1040);
    setSize(1080, 620);
  }

  ~FieldEditor() override { setLookAndFeel(nullptr); }

  void paint(juce::Graphics& graphics) override {
    graphics.fillAll(juce::Colour{kSurface});
    auto header = getLocalBounds().reduced(24).removeFromTop(52);
    graphics.setColour(juce::Colour{kInk});
    graphics.setFont(juce::FontOptions{22.0F, juce::Font::bold});
    graphics.drawText("FIELD", header.removeFromLeft(160),
                      juce::Justification::centredLeft);
    graphics.setColour(juce::Colour{kAccent});
    graphics.setFont(juce::FontOptions{12.0F, juce::Font::bold});
    header.removeFromRight(210);
    graphics.drawText("F-01 / STORED SPATIAL ENERGY", header,
                      juce::Justification::centredRight);
    graphics.setColour(juce::Colour{kMuted}.withAlpha(0.35F));
    graphics.fillRect(24, 74, getWidth() - 48, 1);
  }

  void resized() override {
    preset_.setBounds(getWidth() - 204, 28, 180, 28);
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(68);
    meter_.setBounds(area.removeFromLeft(142));
    area.removeFromLeft(20);
    auto hero = area.removeFromLeft(std::min(310, area.getWidth() / 2));
    forever_.setBounds(hero.reduced(18));
    area.removeFromLeft(16);
    const int columnWidth = std::max(1, area.getWidth() / 4);
    const int rowHeight = std::max(1, area.getHeight() / 2);
    const std::array<Knob*, 7> controls{&mass_,     &grain_, &pitch_, &motion_,
                                        &distance_, &blend_, &output_};
    for (std::size_t index = 0; index < controls.size(); ++index) {
      const int column = static_cast<int>(index % 4U);
      const int row = static_cast<int>(index / 4U);
      controls[index]->setBounds(area.getX() + column * columnWidth,
                                 area.getY() + row * rowHeight, columnWidth,
                                 rowHeight);
    }
  }

 private:
  FieldLookAndFeel lookAndFeel_;
  FieldAudioProcessor& processor_;
  FieldMeter meter_;
  juce::ToggleButton forever_;
  juce::AudioProcessorValueTreeState::ButtonAttachment foreverAttachment_;
  Knob mass_;
  Knob grain_;
  Knob pitch_;
  Knob motion_;
  Knob distance_;
  Knob blend_;
  Knob output_;
  juce::ComboBox preset_;
};

}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout
FieldAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"forever", 1}, "Forever", false));
  addPercent(layout, "mass", "Mass", 62.0F);
  addPercent(layout, "grain", "Grain", 34.0F);
  addPercent(layout, "pitch", "Pitch", 28.0F);
  addPercent(layout, "motion", "Motion", 24.0F);
  addPercent(layout, "distance", "Distance", 45.0F);
  addPercent(layout, "blend", "Blend", 48.0F);
  const auto attributes = juce::AudioParameterFloatAttributes{}.withLabel("dB");
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{"output", 1}, "Output",
      juce::NormalisableRange<float>{-18.0F, 12.0F, 0.01F}, -3.0F, attributes));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"bypass", 1}, "Bypass", false));
  return layout;
}

FieldAudioProcessor::FieldAudioProcessor()
    : AudioProcessor{BusesProperties{}
                         .withInput("Input", juce::AudioChannelSet::stereo(),
                                    true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(),
                                     true)},
      state_{*this, nullptr, kStateType, createParameterLayout()} {
  for (std::size_t index = 0; index < kParameterIds.size(); ++index) {
    parameterValues_[index] = state_.getRawParameterValue(kParameterIds[index]);
    jassert(parameterValues_[index] != nullptr);
  }
}

Parameters FieldAudioProcessor::currentParameters() const noexcept {
  const auto value = [this](ParameterIndex index) {
    return parameterValues_[index]->load(std::memory_order_relaxed);
  };
  return {.forever = value(forever) >= 0.5F,
          .mass = value(mass) * 0.01F,
          .grain = value(grain) * 0.01F,
          .pitch = value(pitch) * 0.01F,
          .motion = value(motion) * 0.01F,
          .distance = value(distance) * 0.01F,
          .blend = value(blend) * 0.01F,
          .outputDb = value(output),
          .bypass = value(bypass) >= 0.5F};
}

void FieldAudioProcessor::prepareToPlay(double sampleRate, int) {
  processor_.prepare(sampleRate);
  setLatencySamples(0);
  publishMeters({});
}
void FieldAudioProcessor::releaseResources() {}

bool FieldAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
  const auto input = layouts.getMainInputChannelSet();
  return input == layouts.getMainOutputChannelSet() &&
         (input == juce::AudioChannelSet::mono() ||
          input == juce::AudioChannelSet::stereo());
}

void FieldAudioProcessor::processRange(juce::AudioBuffer<float>& buffer,
                                       int start, int frames,
                                       const Parameters& parameters) noexcept {
  if (frames <= 0) return;
  processor_.process(
      buffer.getWritePointer(0, start),
      buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, start) : nullptr,
      static_cast<std::size_t>(frames), parameters);
}

void FieldAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                       juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  if (buffer.getNumChannels() < 1 || buffer.getNumSamples() == 0) {
    publishMeters({});
    return;
  }
  const Parameters parameters = currentParameters();
  int processed{};
  for (const auto metadata : midi) {
    const int position =
        juce::jlimit(0, buffer.getNumSamples(), metadata.samplePosition);
    processRange(buffer, processed, position - processed, parameters);
    processed = position;
    const auto& message = metadata.getMessage();
    if (message.isNoteOn())
      processor_.noteOn(message.getNoteNumber(), message.getFloatVelocity());
  }
  processRange(buffer, processed, buffer.getNumSamples() - processed,
               parameters);
  publishMeters(processor_.meters());
}

void FieldAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&) {
  float peak{};
  for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    peak =
        std::max(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
  publishMeters({peak, peak, fieldEnergy(), retention()});
}

void FieldAudioProcessor::publishMeters(const MeterSnapshot& meters) noexcept {
  inputPeak_.store(meters.inputPeak, std::memory_order_relaxed);
  outputPeak_.store(meters.outputPeak, std::memory_order_relaxed);
  fieldEnergy_.store(meters.fieldEnergy, std::memory_order_relaxed);
  retention_.store(meters.retention, std::memory_order_relaxed);
}
float FieldAudioProcessor::inputPeak() const noexcept {
  return inputPeak_.load(std::memory_order_relaxed);
}
float FieldAudioProcessor::outputPeak() const noexcept {
  return outputPeak_.load(std::memory_order_relaxed);
}
float FieldAudioProcessor::fieldEnergy() const noexcept {
  return fieldEnergy_.load(std::memory_order_relaxed);
}
float FieldAudioProcessor::retention() const noexcept {
  return retention_.load(std::memory_order_relaxed);
}

int FieldAudioProcessor::factoryPresetCount() noexcept {
  return static_cast<int>(kPresets.size());
}
juce::String FieldAudioProcessor::factoryPresetName(int index) {
  return juce::isPositiveAndBelow(index, factoryPresetCount())
             ? kPresets[static_cast<std::size_t>(index)].name
             : juce::String{};
}
void FieldAudioProcessor::loadFactoryPreset(int index) {
  if (!juce::isPositiveAndBelow(index, factoryPresetCount())) return;
  const auto& preset = kPresets[static_cast<std::size_t>(index)];
  for (std::size_t parameter = 0; parameter < preset.values.size(); ++parameter)
    if (auto* target = state_.getParameter(kParameterIds[parameter]))
      target->setValueNotifyingHost(
          target->convertTo0to1(preset.values[parameter]));
}

juce::AudioProcessorParameter* FieldAudioProcessor::getBypassParameter() const {
  return state_.getParameter("bypass");
}

void FieldAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
  auto state = state_.copyState();
  state.setProperty("schema", kStateSchema, nullptr);
  state.setProperty("product", kStateType, nullptr);
  if (const auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void FieldAudioProcessor::setStateInformation(const void* data, int size) {
  const auto xml = getXmlFromBinary(data, size);
  if (!xml) return;
  const auto restored = juce::ValueTree::fromXml(*xml);
  if (!restored.isValid() || restored.getType().toString() != kStateType ||
      restored.getProperty("product").toString() != kStateType ||
      static_cast<int>(restored.getProperty("schema", -1)) != kStateSchema)
    return;
  auto validated = state_.copyState();
  for (int index = 0; index < validated.getNumChildren(); ++index) {
    auto child = validated.getChild(index);
    if (auto* parameter =
            state_.getParameter(child.getProperty("id").toString()))
      child.setProperty(
          "value", parameter->convertFrom0to1(parameter->getDefaultValue()),
          nullptr);
  }
  juce::StringArray seen;
  for (int index = 0; index < restored.getNumChildren(); ++index) {
    const auto child = restored.getChild(index);
    const auto id = child.getProperty("id").toString();
    auto* parameter = state_.getParameter(id);
    if (!parameter) continue;
    if (seen.contains(id) || !child.hasProperty("value")) return;
    seen.add(id);
    float value{};
    if (!parseFiniteFloat(child.getProperty("value"), value)) return;
    auto destination = validated.getChildWithProperty("id", id);
    if (!destination.isValid()) return;
    const auto& range = parameter->getNormalisableRange();
    destination.setProperty(
        "value", juce::jlimit(range.start, range.end, value), nullptr);
  }
  validated.setProperty("schema", kStateSchema, nullptr);
  validated.setProperty("product", kStateType, nullptr);
  state_.replaceState(validated);
}

juce::AudioProcessorEditor* FieldAudioProcessor::createEditor() {
  return new FieldEditor{*this};
}

}  // namespace aste::field::plugin

#if !defined(ASTE_NO_PLUGIN_FACTORY)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::field::plugin::FieldAudioProcessor{};
}
#endif
