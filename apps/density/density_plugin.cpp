#include "density_plugin.hpp"

#include "decimal_parse.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace aste::density::plugin {
namespace {

constexpr auto kStateType = "density-d01";
constexpr int kStateSchema = 1;
constexpr auto kSurface = 0xff111113U;
constexpr auto kPanel = 0xff19191cU;
constexpr auto kInk = 0xffe8e5dcU;
constexpr auto kMuted = 0xff85827bU;
constexpr auto kAccent = 0xff812d3dU;
constexpr std::array<const char*, 11> kParameterIds{
    "drive", "crush", "attack", "release", "density", "blend", "output",
    "stereo", "detector_hpf", "protection", "bypass"};

bool parseFiniteFloat(const juce::var& value, float& result) {
  const std::string text = value.toString().toStdString();
  double parsed{};
  if (!aste::density::parseFiniteDecimal(text, parsed)) {
    return false;
  }
  result = static_cast<float>(parsed);
  return std::isfinite(result);
}

juce::ValueTree migrateState(juce::ValueTree state) {
  if (!state.isValid() || state.getType().toString() != kStateType ||
      state.getProperty("product").toString() != kStateType) {
    return {};
  }

  switch (static_cast<int>(state.getProperty("schema", -1))) {
    case 1:
      return state;
    default:
      return {};
  }
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
  const auto attributes = juce::AudioParameterFloatAttributes{}.withLabel(unit);
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, range, initial, attributes));
}

class DensityLookAndFeel final : public juce::LookAndFeel_V4 {
 public:
  DensityLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, juce::Colour{kInk});
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour{kSurface});
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, juce::Colour{kInk});
  }

  void drawRotarySlider(juce::Graphics& graphics, int x, int y, int width,
                        int height, float position, float startAngle,
                        float endAngle, juce::Slider&) override {
    const auto bounds = juce::Rectangle<float>{static_cast<float>(x),
                                                static_cast<float>(y),
                                                static_cast<float>(width),
                                                static_cast<float>(height)}
                            .reduced(8.0F);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5F;
    const auto centre = bounds.getCentre();
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0.0F, startAngle,
                        endAngle, true);
    graphics.setColour(juce::Colour{kMuted}.withAlpha(0.25F));
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
    graphics.fillPath(pointer, juce::AffineTransform::rotation(angle).translated(
                                   centre.x, centre.y));
  }
};

class Knob final : public juce::Component {
 public:
  Knob(juce::AudioProcessorValueTreeState& state, const char* id,
       const char* name, const char* suffix, double initial, int focusOrder)
      : attachment_{state, id, slider_} {
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.textFromValueFunction = [this](double value) {
      const int decimals = slider_.getNumDecimalPlacesToDisplay();
      const double displayZero = 0.5 * std::pow(10.0, -decimals);
      return juce::String{std::abs(value) < displayZero ? 0.0 : value, decimals};
    };
    slider_.setTextValueSuffix(suffix);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 82, 22);
    slider_.setDoubleClickReturnValue(true, initial);
    slider_.setTitle(name);
    slider_.setDescription(juce::String{name} + " parameter");
    slider_.setWantsKeyboardFocus(true);
    slider_.setExplicitFocusOrder(focusOrder);
    label_.setText(name, juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(juce::FontOptions{12.0F, juce::Font::bold});
    addAndMakeVisible(slider_);
    addAndMakeVisible(label_);
  }

  void resized() override {
    auto area = getLocalBounds();
    label_.setBounds(area.removeFromTop(22));
    slider_.setBounds(area);
  }

 private:
  juce::Slider slider_;
  juce::Label label_;
  juce::AudioProcessorValueTreeState::SliderAttachment attachment_;
};

class Switch final : public juce::Component {
 public:
  Switch(juce::AudioProcessorValueTreeState& state, const char* id,
         const char* name, int focusOrder)
      : attachment_{state, id, button_} {
    button_.setButtonText(name);
    button_.setTitle(name);
    button_.setDescription(juce::String{name} + " switch");
    button_.setWantsKeyboardFocus(true);
    button_.setExplicitFocusOrder(focusOrder);
    button_.setColour(juce::ToggleButton::textColourId, juce::Colour{kInk});
    button_.setColour(juce::ToggleButton::tickColourId, juce::Colour{kAccent});
    button_.setColour(juce::ToggleButton::tickDisabledColourId,
                      juce::Colour{kMuted});
    addAndMakeVisible(button_);
  }

  void resized() override { button_.setBounds(getLocalBounds().reduced(14)); }

 private:
  juce::ToggleButton button_;
  juce::AudioProcessorValueTreeState::ButtonAttachment attachment_;
};

class MeterPanel final : public juce::Component, private juce::Timer {
 public:
  explicit MeterPanel(DensityAudioProcessor& processor) : processor_{processor} {
    startTimerHz(30);
  }

  void paint(juce::Graphics& graphics) override {
    graphics.fillAll(juce::Colour{kPanel});
    auto area = getLocalBounds().toFloat().reduced(16.0F);
    graphics.setColour(juce::Colour{kMuted});
    graphics.setFont(juce::FontOptions{11.0F, juce::Font::bold});
    graphics.drawText("BOUNDARIES", area.removeFromTop(20.0F),
                      juce::Justification::centredLeft);

    auto levels = area.removeFromTop(180.0F);
    drawLevel(graphics, levels.removeFromLeft(levels.getWidth() * 0.45F),
              input_, "IN");
    drawLevel(graphics, levels, output_, "OUT");
    area.removeFromTop(18.0F);
    graphics.setColour(juce::Colour{kMuted});
    graphics.drawText("GAIN REDUCTION", area.removeFromTop(18.0F),
                      juce::Justification::centredLeft);
    auto reductionArea = area.removeFromTop(18.0F);
    graphics.setColour(juce::Colour{kSurface});
    graphics.fillRect(reductionArea);
    const float proportion = juce::jlimit(0.0F, 1.0F, reduction_ / 30.0F);
    graphics.setColour(juce::Colour{kAccent});
    graphics.fillRect(reductionArea.withWidth(reductionArea.getWidth() * proportion));
    graphics.setColour(juce::Colour{kInk});
    graphics.drawText(juce::String{reduction_, 1} + " dB", area.removeFromTop(24.0F),
                      juce::Justification::centredRight);
  }

 private:
  static void drawLevel(juce::Graphics& graphics, juce::Rectangle<float> area,
                        float level, const char* name) {
    const auto label = area.removeFromBottom(20.0F);
    graphics.setColour(juce::Colour{kMuted});
    graphics.drawText(name, label, juce::Justification::centred);
    area = area.reduced(12.0F, 0.0F);
    graphics.setColour(juce::Colour{kSurface});
    graphics.fillRect(area);
    const float proportion = juce::jlimit(0.0F, 1.0F, level);
    auto fill = area;
    fill.removeFromTop(area.getHeight() * (1.0F - proportion));
    graphics.setColour(proportion > 0.95F ? juce::Colour{kAccent}
                                          : juce::Colour{kInk});
    graphics.fillRect(fill);
  }

  void timerCallback() override {
    input_ = std::max(processor_.inputPeak(), input_ * 0.84F);
    output_ = std::max(processor_.outputPeak(), output_ * 0.84F);
    reduction_ = std::max(processor_.gainReductionDb(), reduction_ * 0.88F);
    repaint();
  }

  DensityAudioProcessor& processor_;
  float input_{};
  float output_{};
  float reduction_{};
};

class DensityEditor final : public juce::AudioProcessorEditor {
 public:
  explicit DensityEditor(DensityAudioProcessor& processor)
      : AudioProcessorEditor{processor},
        meter_{processor},
        density_{processor.state(), "density", "DENSITY", " %", 50.0, 1},
        drive_{processor.state(), "drive", "DRIVE", " dB", 0.0, 2},
        crush_{processor.state(), "crush", "CRUSH", " %", 65.0, 3},
        attack_{processor.state(), "attack", "ATTACK", " ms", 1.0, 4},
        release_{processor.state(), "release", "RELEASE", " ms", 180.0, 5},
        blend_{processor.state(), "blend", "BLEND", " %", 50.0, 6},
        stereo_{processor.state(), "stereo", "STEREO", " %", 100.0, 7},
        hpf_{processor.state(), "detector_hpf", "DETECTOR HPF", " Hz", 90.0,
             8},
        output_{processor.state(), "output", "OUTPUT", " dB", 0.0, 9},
        protection_{processor.state(), "protection", "PROTECTION", 10} {
    setLookAndFeel(&lookAndFeel_);
    for (auto* component : std::array<juce::Component*, 11>{
             &meter_, &density_, &drive_, &crush_, &attack_, &release_, &blend_,
             &stereo_, &hpf_, &output_, &protection_}) {
      addAndMakeVisible(component);
    }
    setResizable(true, true);
    setResizeLimits(760, 420, 1520, 840);
    setSize(980, 540);
  }

  ~DensityEditor() override { setLookAndFeel(nullptr); }

  void paint(juce::Graphics& graphics) override {
    graphics.fillAll(juce::Colour{kSurface});
    auto header = getLocalBounds().reduced(24).removeFromTop(52);
    graphics.setColour(juce::Colour{kInk});
    graphics.setFont(juce::FontOptions{22.0F, juce::Font::bold});
    graphics.drawText("DENSITY", header.removeFromLeft(180),
                      juce::Justification::centredLeft);
    graphics.setColour(juce::Colour{kAccent});
    graphics.setFont(juce::FontOptions{12.0F, juce::Font::bold});
    graphics.drawText("D-01 / PARALLEL DYNAMICS", header,
                      juce::Justification::centredRight);
    graphics.setColour(juce::Colour{kMuted}.withAlpha(0.35F));
    graphics.fillRect(24, 74, getWidth() - 48, 1);
  }

  void resized() override {
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(68);
    auto meterColumn = area.removeFromLeft(138);
    protection_.setBounds(meterColumn.removeFromBottom(50));
    meter_.setBounds(meterColumn);
    area.removeFromLeft(18);
    density_.setBounds(area.removeFromLeft(270));
    area.removeFromLeft(18);

    const int columnWidth = std::max(1, area.getWidth() / 4);
    const int rowHeight = std::max(1, area.getHeight() / 2);
    const std::array<Knob*, 8> controls{&drive_, &crush_, &attack_, &release_,
                                         &blend_, &stereo_, &hpf_, &output_};
    for (std::size_t i = 0; i < controls.size(); ++i) {
      const int column = static_cast<int>(i % 4);
      const int row = static_cast<int>(i / 4);
      controls[i]->setBounds(area.getX() + column * columnWidth,
                             area.getY() + row * rowHeight, columnWidth, rowHeight);
    }
  }

 private:
  DensityLookAndFeel lookAndFeel_;
  MeterPanel meter_;
  Knob density_;
  Knob drive_;
  Knob crush_;
  Knob attack_;
  Knob release_;
  Knob blend_;
  Knob stereo_;
  Knob hpf_;
  Knob output_;
  Switch protection_;
};

}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout
DensityAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  addFloat(layout, "drive", "Drive", {-12.0F, 24.0F, 0.01F}, 0.0F, "dB");
  addFloat(layout, "crush", "Crush", {0.0F, 100.0F, 0.01F}, 65.0F, "%");
  addFloat(layout, "attack", "Attack", skewed(0.02F, 30.0F, 1.0F, 0.001F),
           1.0F, "ms");
  addFloat(layout, "release", "Release", skewed(20.0F, 1200.0F, 180.0F, 0.1F),
           180.0F, "ms");
  addFloat(layout, "density", "Density", {0.0F, 100.0F, 0.01F}, 50.0F, "%");
  addFloat(layout, "blend", "Blend", {0.0F, 100.0F, 0.01F}, 50.0F, "%");
  addFloat(layout, "stereo", "Stereo", {0.0F, 100.0F, 0.01F}, 100.0F, "%");
  addFloat(layout, "output", "Output", {-24.0F, 12.0F, 0.01F}, 0.0F, "dB");
  addFloat(layout, "detector_hpf", "Detector HPF",
           skewed(20.0F, 300.0F, 90.0F, 0.1F), 90.0F, "Hz");
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"protection", 1}, "Protection", true));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"bypass", 1}, "Bypass", false));
  return layout;
}

DensityAudioProcessor::DensityAudioProcessor()
    : AudioProcessor{BusesProperties{}
                         .withInput("Input", juce::AudioChannelSet::stereo(), true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(), true)},
      state_{*this, nullptr, kStateType, createParameterLayout()} {
  for (std::size_t i = 0; i < kParameterIds.size(); ++i) {
    parameterValues_[i] = state_.getRawParameterValue(kParameterIds[i]);
    jassert(parameterValues_[i] != nullptr);
  }
}

Parameters DensityAudioProcessor::currentParameters() const noexcept {
  return {
      .driveDb = parameterValues_[drive]->load(std::memory_order_relaxed),
      .crush = parameterValues_[crush]->load(std::memory_order_relaxed) * 0.01F,
      .attackMs = parameterValues_[attack]->load(std::memory_order_relaxed),
      .releaseMs = parameterValues_[release]->load(std::memory_order_relaxed),
      .density = parameterValues_[density]->load(std::memory_order_relaxed) * 0.01F,
      .blend = parameterValues_[blend]->load(std::memory_order_relaxed) * 0.01F,
      .stereoLink = parameterValues_[stereo]->load(std::memory_order_relaxed) * 0.01F,
      .outputDb = parameterValues_[output]->load(std::memory_order_relaxed),
      .detectorHpfHz = parameterValues_[detectorHpf]->load(std::memory_order_relaxed),
      .protection = parameterValues_[protection]->load(std::memory_order_relaxed) >=
                    0.5F,
  };
}

void DensityAudioProcessor::prepareToPlay(double sampleRate, int) {
  processor_.prepare(sampleRate, currentParameters());
  setLatencySamples(static_cast<int>(processor_.latencySamples()));
  publishMeters({});
}

void DensityAudioProcessor::releaseResources() { processor_.reset(); }

bool DensityAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const {
  const auto input = layouts.getMainInputChannelSet();
  return input == layouts.getMainOutputChannelSet() &&
         (input == juce::AudioChannelSet::mono() ||
          input == juce::AudioChannelSet::stereo());
}

void DensityAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  if (parameterValues_[bypass]->load(std::memory_order_relaxed) >= 0.5F) {
    processBlockBypassed(buffer, midi);
    return;
  }
  const int channels = buffer.getNumChannels();
  if (channels < 1 || buffer.getNumSamples() == 0) {
    publishMeters({});
    return;
  }
  processor_.process(buffer.getWritePointer(0),
                     channels > 1 ? buffer.getWritePointer(1) : nullptr,
                     static_cast<std::size_t>(buffer.getNumSamples()),
                     currentParameters());
  publishMeters(processor_.meters());
}

void DensityAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                                 juce::MidiBuffer&) {
  float peak = 0.0F;
  for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
    peak = std::max(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
  }
  publishMeters({peak, peak, 0.0F});
}

void DensityAudioProcessor::publishMeters(const MeterSnapshot& meters) noexcept {
  inputPeak_.store(meters.inputPeak, std::memory_order_relaxed);
  outputPeak_.store(meters.outputPeak, std::memory_order_relaxed);
  gainReduction_.store(meters.gainReductionDb, std::memory_order_relaxed);
}

float DensityAudioProcessor::inputPeak() const noexcept {
  return inputPeak_.load(std::memory_order_relaxed);
}

float DensityAudioProcessor::outputPeak() const noexcept {
  return outputPeak_.load(std::memory_order_relaxed);
}

float DensityAudioProcessor::gainReductionDb() const noexcept {
  return gainReduction_.load(std::memory_order_relaxed);
}

juce::AudioProcessorParameter* DensityAudioProcessor::getBypassParameter() const {
  return state_.getParameter("bypass");
}

void DensityAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
  auto state = state_.copyState();
  state.setProperty("schema", kStateSchema, nullptr);
  state.setProperty("product", kStateType, nullptr);
  if (const auto xml = state.createXml()) {
    copyXmlToBinary(*xml, destination);
  }
}

void DensityAudioProcessor::setStateInformation(const void* data, int size) {
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
      child.setProperty("value",
                        parameter->convertFrom0to1(parameter->getDefaultValue()),
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
    destination.setProperty("value", juce::jlimit(range.start, range.end, value),
                            nullptr);
  }
  validated.setProperty("schema", kStateSchema, nullptr);
  validated.setProperty("product", kStateType, nullptr);
  state_.replaceState(validated);
}

juce::AudioProcessorEditor* DensityAudioProcessor::createEditor() {
  return new DensityEditor{*this};
}

}  // namespace aste::density::plugin

#if !defined(ASTE_NO_PLUGIN_FACTORY)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::density::plugin::DensityAudioProcessor{};
}
#endif
