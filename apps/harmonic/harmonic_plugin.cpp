#include "harmonic_plugin.hpp"

#include "decimal_parse.hpp"
#include "plugin_ui.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace aste::harmonic::plugin {
namespace {

constexpr auto kStateType = "harmonic-h01";
constexpr int kStateSchema = 1;
constexpr auto kSurface = 0xff111113U;
constexpr auto kPanel = 0xff1b1916U;
constexpr auto kInk = 0xffe9e5d8U;
constexpr auto kMuted = 0xff888277U;
constexpr auto kAccent = 0xffa56328U;
constexpr std::array<const char*, 12> kParameterIds{"input",
                                                    "foundation_gain",
                                                    "foundation_frequency",
                                                    "body_gain",
                                                    "body_frequency",
                                                    "presence_gain",
                                                    "presence_frequency",
                                                    "air_gain",
                                                    "air_frequency",
                                                    "harmonic",
                                                    "output",
                                                    "bypass"};

struct FactoryPreset {
  const char* name;
  std::array<float, kParameterIds.size()> values;
};

constexpr std::array<FactoryPreset, 8> kFactoryPresets{{
    {"Default",
     {0.0F, 0.0F, 80.0F, 0.0F, 400.0F, 0.0F, 2500.0F, 0.0F, 12000.0F, 35.0F,
      0.0F, 0.0F}},
    {"Foundation",
     {0.0F, 6.0F, 65.0F, 2.0F, 320.0F, 0.0F, 2500.0F, 0.0F, 12000.0F, 40.0F,
      -3.0F, 0.0F}},
    {"Continuum",
     {0.0F, 4.0F, 80.0F, 1.5F, 450.0F, 3.0F, 2200.0F, 3.0F, 12000.0F, 55.0F,
      -3.0F, 0.0F}},
    {"Presence",
     {0.0F, 0.0F, 80.0F, -3.0F, 350.0F, 6.0F, 3200.0F, 4.0F, 14000.0F, 45.0F,
      -3.0F, 0.0F}},
    {"Hard Air",
     {0.0F, 2.0F, 90.0F, -2.0F, 450.0F, 6.0F, 4200.0F, 8.0F, 15000.0F, 80.0F,
      -4.0F, 0.0F}},
    {"Negative Space",
     {0.0F, -6.0F, 70.0F, -4.0F, 300.0F, 4.0F, 1800.0F, 2.0F, 10000.0F, 30.0F,
      0.0F, 0.0F}},
    {"Charged Presence",
     {12.0F, 0.0F, 80.0F, 0.0F, 400.0F, 6.0F, 2500.0F, 0.0F, 12000.0F, 100.0F,
      -15.0F, 0.0F}},
    {"Color / open",
     {6.0F, 0.0F, 80.0F, 0.0F, 400.0F, 6.0F, 2500.0F, 0.0F, 12000.0F, 60.0F,
      -8.0F, 0.0F}},
}};

bool parseFiniteFloat(const juce::var& value, float& result) {
  const std::string text = value.toString().toStdString();
  double parsed{};
  if (!aste::parameters::parseFiniteDecimal(text, parsed)) {
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
  return static_cast<int>(state.getProperty("schema", -1)) == kStateSchema
             ? state
             : juce::ValueTree{};
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

class HarmonicLookAndFeel final : public juce::LookAndFeel_V4 {
 public:
  HarmonicLookAndFeel() {
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
    graphics.fillPath(
        pointer,
        juce::AffineTransform::rotation(angle).translated(centre.x, centre.y));
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
      return juce::String{std::abs(value) < displayZero ? 0.0 : value,
                          decimals};
    };
    slider_.setTextValueSuffix(suffix);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 86, 22);
    slider_.setDoubleClickReturnValue(true, initial);
    slider_.setTitle(name);
    slider_.setDescription(
        aste::ui::explanation(id, state.processor.getName()));
    slider_.setTooltip(
        aste::ui::explanation(id, state.processor.getName()) +
        " Shift-drag for fine adjustment; type an exact value below.");
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
  aste::ui::PrecisionSlider slider_;
  juce::Label label_;
  juce::AudioProcessorValueTreeState::SliderAttachment attachment_;
};

class MeterPanel final : public juce::Component, private juce::Timer {
 public:
  explicit MeterPanel(HarmonicAudioProcessor& processor)
      : processor_{processor} {
    startTimerHz(30);
  }

  void paint(juce::Graphics& graphics) override {
    graphics.fillAll(juce::Colour{kPanel});
    auto area = getLocalBounds().toFloat().reduced(14.0F);
    graphics.setColour(juce::Colour{kMuted});
    graphics.setFont(juce::FontOptions{11.0F, juce::Font::bold});
    graphics.drawText("BOUNDARIES", area.removeFromTop(20.0F),
                      juce::Justification::centredLeft);
    auto levels = area.removeFromTop(std::max(40.0F, area.getHeight() - 90.0F));
    drawLevel(graphics, levels.removeFromLeft(levels.getWidth() * 0.5F), input_,
              "IN");
    drawLevel(graphics, levels, output_, "OUT");
    area.removeFromTop(18.0F);
    graphics.drawText("HARMONIC ACTIVITY", area.removeFromTop(20.0F),
                      juce::Justification::centredLeft);
    auto activityArea = area.removeFromTop(16.0F);
    graphics.setColour(juce::Colour{kSurface});
    graphics.fillRect(activityArea);
    graphics.setColour(juce::Colour{kAccent});
    graphics.fillRect(activityArea.withWidth(
        activityArea.getWidth() * juce::jlimit(0.0F, 1.0F, activity_ * 20.0F)));
  }

 private:
  static void drawLevel(juce::Graphics& graphics, juce::Rectangle<float> area,
                        float level, const char* name) {
    const auto reading = area.removeFromBottom(30.0F);
    const auto label = area.removeFromBottom(18.0F);
    graphics.setColour(juce::Colour{kMuted});
    graphics.drawText(name, label, juce::Justification::centred);
    graphics.setFont(juce::FontOptions{9.0F});
    graphics.drawFittedText(aste::ui::levelText(level), reading.toNearestInt(),
                            juce::Justification::centred, 2);
    area = area.reduced(11.0F, 0.0F);
    graphics.setColour(juce::Colour{kSurface});
    graphics.fillRect(area);
    auto fill = area;
    fill.removeFromTop(area.getHeight() *
                       (1.0F - aste::ui::meterPosition(level)));
    graphics.setColour(level > 0.95F ? juce::Colour{kAccent}
                                     : juce::Colour{kInk});
    graphics.fillRect(fill);
  }

  void timerCallback() override {
    input_ = std::max(processor_.inputPeak(), input_ * 0.84F);
    output_ = std::max(processor_.outputPeak(), output_ * 0.84F);
    activity_ = std::max(processor_.harmonicActivity(), activity_ * 0.86F);
    repaint();
  }

  HarmonicAudioProcessor& processor_;
  float input_{};
  float output_{};
  float activity_{};
};

class HarmonicEditor final : public juce::AudioProcessorEditor,
                             private juce::Timer {
 public:
  explicit HarmonicEditor(HarmonicAudioProcessor& processor)
      : AudioProcessorEditor{processor},
        processor_{processor},
        master_{processor.state()},
        meter_{processor},
        harmonic_{processor.state(), "harmonic", "HARMONIC", " %", 35.0, 1},
        input_{processor.state(), "input", "INPUT", " dB", 0.0, 2},
        output_{processor.state(), "output", "OUTPUT", " dB", 0.0, 3},
        foundationGain_{
            processor.state(), "foundation_gain", "FOUNDATION", " dB", 0.0, 4},
        foundationFrequency_{processor.state(),
                             "foundation_frequency",
                             "FOUNDATION FREQUENCY",
                             " Hz",
                             80.0,
                             5},
        bodyGain_{processor.state(), "body_gain", "BODY", " dB", 0.0, 6},
        bodyFrequency_{processor.state(),
                       "body_frequency",
                       "BODY FREQUENCY",
                       " Hz",
                       400.0,
                       7},
        presenceGain_{
            processor.state(), "presence_gain", "PRESENCE", " dB", 0.0, 8},
        presenceFrequency_{processor.state(),
                           "presence_frequency",
                           "PRESENCE FREQUENCY",
                           " Hz",
                           2500.0,
                           9},
        airGain_{processor.state(), "air_gain", "AIR", " dB", 0.0, 10},
        airFrequency_{processor.state(), "air_frequency",
                      "AIR FREQUENCY",   " Hz",
                      12000.0,           11} {
    setLookAndFeel(&lookAndFeel_);
    addAndMakeVisible(master_);
    response_.bind([this] { return processor_.usesMaterialResponse(); },
                   [this] { processor_.useCurrentResponse(); },
                   "Adopt the new material behavior. This changes sound and "
                   "interaction while preserving saved parameter values.");
    addAndMakeVisible(response_);
    workflow_.bind(*this, "INPUT > FREQUENCY REGION > MATERIAL > GAIN > MASTER",
                   "Feed audio. Tune a frequency region, turn Harmonic to "
                   "transform its substance, then shape its gain. PUSH/PULL "
                   "changes how hard the material is driven.");
    addAndMakeVisible(workflow_);
    preset_.setTextWhenNothingSelected("PRESETS");
    preset_.setTitle("PRESETS");
    preset_.setDescription("Load a Harmonic factory starting point");
    preset_.setWantsKeyboardFocus(true);
    preset_.setExplicitFocusOrder(12);
    preset_.setColour(juce::ComboBox::backgroundColourId, juce::Colour{kPanel});
    preset_.setColour(juce::ComboBox::textColourId, juce::Colour{kInk});
    preset_.setColour(juce::ComboBox::outlineColourId, juce::Colour{kMuted});
    for (int index = 0; index < processor_.factoryPresetCount(); ++index) {
      preset_.addItem(processor_.factoryPresetName(index), index + 1);
    }
    for (const auto& snapshot : kFactoryPresets)
      preset_.addSnapshot(processor_, processor_.state(), [&](auto set) {
        for (std::size_t i = 0; i < snapshot.values.size(); ++i)
          set(kParameterIds[i], snapshot.values[i]);
      });
    preset_.watch(processor_);
    preset_.onChange = [this] {
      if (const int selected = preset_.getSelectedId(); selected > 0) {
        processor_.loadFactoryPreset(selected - 1);
        preset_.remember(processor_);
      }
    };
    for (auto* component : std::array<juce::Component*, 13>{
             &meter_, &harmonic_, &input_, &output_, &foundationGain_,
             &foundationFrequency_, &bodyGain_, &bodyFrequency_, &presenceGain_,
             &presenceFrequency_, &airGain_, &airFrequency_, &preset_}) {
      addAndMakeVisible(component);
    }
    engagement_.setTitle("COLOR STATUS");
    engagement_.setFont(juce::FontOptions{11.0F});
    addAndMakeVisible(engagement_);
    push_.setButtonText("PUSH +3 dB");
    pull_.setButtonText("PULL -3 dB");
    push_.setTitle("PUSH OPERATING LEVEL");
    pull_.setTitle("PULL OPERATING LEVEL");
    push_.setTooltip(
        "Increase Input and decrease Output equally. Changes nonlinear color, "
        "not a loudness match.");
    pull_.setTooltip(
        "Decrease Input and increase Output equally, within both controls' "
        "ranges.");
    push_.onClick = [this] { shiftOperatingLevel(3); };
    pull_.onClick = [this] { shiftOperatingLevel(-3); };
    addAndMakeVisible(push_);
    addAndMakeVisible(pull_);
    setResizable(true, true);
    setResizeLimits(860, 480, 1720, 960);
    setSize(1120, 620);
    timerCallback();
    startTimerHz(20);
  }

  ~HarmonicEditor() override { setLookAndFeel(nullptr); }

  void paint(juce::Graphics& graphics) override {
    graphics.fillAll(juce::Colour{kSurface});
    auto header = getLocalBounds().reduced(24).removeFromTop(52);
    graphics.setColour(juce::Colour{kInk});
    graphics.setFont(juce::FontOptions{22.0F, juce::Font::bold});
    graphics.drawText("HARMONIC", header.removeFromLeft(190),
                      juce::Justification::centredLeft);
    graphics.setColour(juce::Colour{kAccent});
    graphics.setFont(juce::FontOptions{12.0F, juce::Font::bold});
    header = juce::Rectangle<int>{24, 53, 310, 16};
    graphics.drawText("H-01 / SPECTRAL NONLINEARITY", header,
                      juce::Justification::centredLeft);
    graphics.setColour(juce::Colour{kMuted}.withAlpha(0.35F));
    graphics.fillRect(24, 74, getWidth() - 48, 1);
    graphics.setFont(juce::FontOptions{11.0F});
    graphics.setColour(juce::Colour{kInk});
  }

  void resized() override {
    response_.setBounds(getWidth() - 224, getHeight() - 28, 200, 24);
    workflow_.setBounds(24, getHeight() - 80, getWidth() - 48, 48);
    preset_.setBounds(getWidth() - 414, 28, 180, 28);
    master_.setBounds(getWidth() - 214, 18, 190, 46);
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(68);
    area.removeFromBottom(60);
    meter_.setBounds(area.removeFromLeft(136));
    area.removeFromLeft(16);
    auto hero = area.removeFromRight(214);
    engagement_.setBounds(hero.removeFromTop(22));
    auto gestures = hero.removeFromBottom(26);
    push_.setBounds(gestures.removeFromLeft(107).reduced(1));
    pull_.setBounds(gestures.reduced(1));
    input_.setBounds(hero.removeFromTop(hero.getHeight() / 3));
    output_.setBounds(hero.removeFromBottom(hero.getHeight() / 2));
    harmonic_.setBounds(hero);
    area.removeFromRight(16);
    const int columnWidth = std::max(1, area.getWidth() / 4);
    const int rowHeight = std::max(1, area.getHeight() / 2);
    const std::array<Knob*, 8> controls{&foundationGain_,      &bodyGain_,
                                        &presenceGain_,        &airGain_,
                                        &foundationFrequency_, &bodyFrequency_,
                                        &presenceFrequency_,   &airFrequency_};
    for (std::size_t index = 0; index < controls.size(); ++index) {
      const int column = static_cast<int>(index % 4U);
      const int row = static_cast<int>(index / 4U);
      controls[index]->setBounds(area.getX() + column * columnWidth,
                                 area.getY() + row * rowHeight, columnWidth,
                                 rowHeight);
    }
  }

 private:
  void shiftOperatingLevel(float requested) {
    auto* in = processor_.state().getParameter("input");
    auto* out = processor_.state().getParameter("output");
    const float input = in->convertFrom0to1(in->getValue());
    const float output = out->convertFrom0to1(out->getValue());
    const float low = std::max(in->getNormalisableRange().start - input,
                               output - out->getNormalisableRange().end);
    const float high = std::min(in->getNormalisableRange().end - input,
                                output - out->getNormalisableRange().start);
    const float delta = juce::jlimit(low, high, requested);
    in->beginChangeGesture();
    out->beginChangeGesture();
    in->setValueNotifyingHost(in->convertTo0to1(input + delta));
    out->setValueNotifyingHost(out->convertTo0to1(output - delta));
    out->endChangeGesture();
    in->endChangeGesture();
    timerCallback();
  }
  void timerCallback() override {
    const float input =
        processor_.state().getRawParameterValue("input")->load();
    const float output =
        processor_.state().getRawParameterValue("output")->load();
    const float up = std::min({3.F, 18.F - input, output + 18.F});
    const float down = std::min({3.F, input + 18.F, 18.F - output});
    push_.setEnabled(up > 0);
    pull_.setEnabled(down > 0);
    push_.setButtonText(up > 0 ? "PUSH +" + juce::String{up, 1} + " dB"
                               : "PUSH LIMIT");
    pull_.setButtonText(down > 0 ? "PULL -" + juce::String{down, 1} + " dB"
                                 : "PULL LIMIT");
    bool boosted{};
    for (const char* id :
         {"foundation_gain", "body_gain", "presence_gain", "air_gain"})
      boosted |= processor_.state().getRawParameterValue(id)->load() > 0;
    const bool material = processor_.usesMaterialResponse();
    const bool active =
        (boosted || material) &&
        processor_.state().getRawParameterValue("harmonic")->load() > 0;
    harmonic_.setDescription(
        !boosted && !material
            ? "Inactive color: boost at least one band to hear Harmonic. "
              "Flat and cut bands remain clean."
        : !active ? "Raise Harmonic to transform the tuned frequency regions."
                  : "Harmonic is engaged; tune the regions, then use Input or "
                    "PUSH/PULL to change the material response.");
    engagement_.setText(
        active ? (material ? "MATERIAL / level dependent"
                           : "COLOR READY / level dependent")
               : ((boosted || material) ? "CLEAN / Harmonic is zero"
                                        : "CLEAN / boost a band to color"),
        juce::dontSendNotification);
  }
  juce::Label engagement_;
  juce::TextButton push_, pull_;
  HarmonicLookAndFeel lookAndFeel_;
  HarmonicAudioProcessor& processor_;
  aste::ui::MasterOutput master_;
  aste::ui::WorkflowGuide workflow_;
  aste::ui::ResponseButton response_;
  MeterPanel meter_;
  Knob harmonic_;
  Knob input_;
  Knob output_;
  Knob foundationGain_;
  Knob foundationFrequency_;
  Knob bodyGain_;
  Knob bodyFrequency_;
  Knob presenceGain_;
  Knob presenceFrequency_;
  Knob airGain_;
  Knob airFrequency_;
  aste::ui::PresetMenu preset_;
  juce::TooltipWindow tooltips_{this, 400};
};

}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout
HarmonicAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  addFloat(layout, "input", "Input", {-18.0F, 18.0F, 0.01F}, 0.0F, "dB");
  addFloat(layout, "foundation_gain", "Foundation", {-12.0F, 12.0F, 0.01F},
           0.0F, "dB");
  addFloat(layout, "foundation_frequency", "Foundation Frequency",
           skewed(35.0F, 160.0F, 80.0F, 0.1F), 80.0F, "Hz");
  addFloat(layout, "body_gain", "Body", {-12.0F, 12.0F, 0.01F}, 0.0F, "dB");
  addFloat(layout, "body_frequency", "Body Frequency",
           skewed(160.0F, 1000.0F, 400.0F, 0.1F), 400.0F, "Hz");
  addFloat(layout, "presence_gain", "Presence", {-12.0F, 12.0F, 0.01F}, 0.0F,
           "dB");
  addFloat(layout, "presence_frequency", "Presence Frequency",
           skewed(800.0F, 7000.0F, 2500.0F, 0.1F), 2500.0F, "Hz");
  addFloat(layout, "air_gain", "Air", {-12.0F, 12.0F, 0.01F}, 0.0F, "dB");
  addFloat(layout, "air_frequency", "Air Frequency",
           skewed(6000.0F, 20000.0F, 12000.0F, 0.1F), 12000.0F, "Hz");
  addFloat(layout, "harmonic", "Harmonic", {0.0F, 100.0F, 0.01F}, 35.0F, "%");
  addFloat(layout, "output", "Output", {-18.0F, 18.0F, 0.01F}, 0.0F, "dB");
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"bypass", 1}, "Bypass", false));
  aste::ui::MasterGain::addParameter(layout, 0.F);
  return layout;
}

HarmonicAudioProcessor::HarmonicAudioProcessor()
    : AudioProcessor{BusesProperties{}
                         .withInput("Input", juce::AudioChannelSet::stereo(),
                                    true)
                         .withOutput("Output", juce::AudioChannelSet::stereo(),
                                     true)},
      state_{*this, nullptr, kStateType, createParameterLayout()} {
  masterGain_.bind(state_);
  for (std::size_t index = 0; index < kParameterIds.size(); ++index) {
    parameterValues_[index] = state_.getRawParameterValue(kParameterIds[index]);
    jassert(parameterValues_[index] != nullptr);
  }
  loadFactoryPreset(7);
}

Parameters HarmonicAudioProcessor::currentParameters() const noexcept {
  return {
      .inputDb = parameterValues_[input]->load(std::memory_order_relaxed),
      .foundationGainDb =
          parameterValues_[foundationGain]->load(std::memory_order_relaxed),
      .foundationFrequencyHz = parameterValues_[foundationFrequency]->load(
          std::memory_order_relaxed),
      .bodyGainDb = parameterValues_[bodyGain]->load(std::memory_order_relaxed),
      .bodyFrequencyHz =
          parameterValues_[bodyFrequency]->load(std::memory_order_relaxed),
      .presenceGainDb =
          parameterValues_[presenceGain]->load(std::memory_order_relaxed),
      .presenceFrequencyHz =
          parameterValues_[presenceFrequency]->load(std::memory_order_relaxed),
      .airGainDb = parameterValues_[airGain]->load(std::memory_order_relaxed),
      .airFrequencyHz =
          parameterValues_[airFrequency]->load(std::memory_order_relaxed),
      .harmonic =
          parameterValues_[harmonic]->load(std::memory_order_relaxed) * 0.01F,
      .outputDb = parameterValues_[output]->load(std::memory_order_relaxed),
      .materialBehavior = usesMaterialResponse(),
  };
}

void HarmonicAudioProcessor::prepareToPlay(double sampleRate, int) {
  masterGain_.prepare(sampleRate);
  processor_.prepare(sampleRate, currentParameters());
  setLatencySamples(static_cast<int>(processor_.latencySamples()));
  publishMeters({});
}

void HarmonicAudioProcessor::releaseResources() { processor_.reset(); }

bool HarmonicAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
  const auto inputSet = layouts.getMainInputChannelSet();
  return inputSet == layouts.getMainOutputChannelSet() &&
         (inputSet == juce::AudioChannelSet::mono() ||
          inputSet == juce::AudioChannelSet::stereo());
}

void HarmonicAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
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
  outputPeak_.store(masterGain_.process(buffer), std::memory_order_relaxed);
}

void HarmonicAudioProcessor::processBlockBypassed(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
  float peak = 0.0F;
  for (int channel = 0; channel < buffer.getNumChannels(); ++channel) {
    peak =
        std::max(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
  }
  publishMeters({peak, peak, 0.0F});
}

void HarmonicAudioProcessor::publishMeters(
    const MeterSnapshot& meters) noexcept {
  inputPeak_.store(meters.inputPeak, std::memory_order_relaxed);
  outputPeak_.store(meters.outputPeak, std::memory_order_relaxed);
  harmonicActivity_.store(meters.harmonicActivity, std::memory_order_relaxed);
}

float HarmonicAudioProcessor::inputPeak() const noexcept {
  return inputPeak_.load(std::memory_order_relaxed);
}

float HarmonicAudioProcessor::outputPeak() const noexcept {
  return outputPeak_.load(std::memory_order_relaxed);
}

float HarmonicAudioProcessor::harmonicActivity() const noexcept {
  return harmonicActivity_.load(std::memory_order_relaxed);
}

int HarmonicAudioProcessor::factoryPresetCount() noexcept {
  return static_cast<int>(kFactoryPresets.size());
}

juce::String HarmonicAudioProcessor::factoryPresetName(int index) {
  return juce::isPositiveAndBelow(index, factoryPresetCount())
             ? kFactoryPresets[static_cast<std::size_t>(index)].name
             : juce::String{};
}

void HarmonicAudioProcessor::loadFactoryPreset(int index) {
  if (!juce::isPositiveAndBelow(index, factoryPresetCount())) {
    return;
  }
  const auto& preset = kFactoryPresets[static_cast<std::size_t>(index)];
  for (std::size_t parameterIndex = 0; parameterIndex < preset.values.size();
       ++parameterIndex) {
    if (auto* parameter = state_.getParameter(kParameterIds[parameterIndex])) {
      parameter->setValueNotifyingHost(
          parameter->convertTo0to1(preset.values[parameterIndex]));
    }
  }
}

juce::AudioProcessorParameter* HarmonicAudioProcessor::getBypassParameter()
    const {
  return state_.getParameter("bypass");
}

void HarmonicAudioProcessor::getStateInformation(
    juce::MemoryBlock& destination) {
  auto state = state_.copyState();
  state.setProperty("schema", kStateSchema, nullptr);
  state.setProperty("product", kStateType, nullptr);
  state.removeProperty("material", nullptr);
  materialRevision_.write(state);
  if (const auto xml = state.createXml()) {
    copyXmlToBinary(*xml, destination);
  }
}

void HarmonicAudioProcessor::setStateInformation(const void* data, int size) {
  const auto xml = getXmlFromBinary(data, size);
  if (xml == nullptr) {
    return;
  }
  const auto restored = migrateState(juce::ValueTree::fromXml(*xml));
  if (!restored.isValid()) {
    return;
  }
  bool material = false;
  if (!aste::parameters::MaterialRevision::parse(restored, material)) return;
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
  if (!seen.contains("master"))
    validated.getChildWithProperty("id", "master")
        .setProperty("value", 0.F, nullptr);
  validated.setProperty("schema", kStateSchema, nullptr);
  validated.setProperty("product", kStateType, nullptr);
  materialRevision_.set(material);
  state_.replaceState(validated);
}

juce::AudioProcessorEditor* HarmonicAudioProcessor::createEditor() {
  return new HarmonicEditor{*this};
}

}  // namespace aste::harmonic::plugin

#if !defined(ASTE_NO_PLUGIN_FACTORY)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::harmonic::plugin::HarmonicAudioProcessor{};
}
#endif
