#include "field_plugin.hpp"

#include "decimal_parse.hpp"
#include "plugin_ui.hpp"

#include <algorithm>
#include <cmath>
#include <memory>
#include <string>

namespace aste::field::plugin {
namespace {

constexpr auto kStateType = "field-f01";
constexpr int kStateSchema = 2;
constexpr auto kSurface = 0xff101316U;
constexpr auto kPanel = 0xff181e22U;
constexpr auto kInk = 0xffe7e9e4U;
constexpr auto kMuted = 0xff77858dU;
constexpr auto kAccent = 0xff6d91a8U;
constexpr std::array<const char*, 18> kParameterIds{
    "forever",    "mass",         "grain",   "pitch",        "motion",
    "distance",   "blend",        "output",  "bypass",       "texture",
    "focus",      "loop_hold",    "loop_ms", "inner_repeat", "inner_ms",
    "loop_speed", "loop_reverse", "loop_mix"};

struct FactoryPreset {
  const char* name;
  std::array<float, kParameterIds.size()> values;
};

constexpr std::array<FactoryPreset, 15> kPresets{{
    {"Open Field", {0, 75, 65, 55, 55, 20, 65, 0, 0, 70}},
    {"Broadcast Hall", {0, 48, 12, 0, 16, 68, 38, -2, 0, 0}},
    {"Silver Memory", {1, 78, 46, 70, 48, 24, 70, -2, 0, 80}},
    {"Granite Cloud", {0, 88, 88, 46, 78, 32, 78, -3, 0, 95}},
    {"White Horizon", {1, 94, 58, 90, 60, 18, 80, -3, 0, 85}},
    {"Still Field", {0, 82, 12, 0, 8, 45, 55, -6, 0, 0}},
    {"Moving glass", {0, 70, 65, 60, 72, 18, 65, -3, 0, 85, 75}},
    {"Memory inside memory",
     {0, 65, 40, 20, 45, 20, 90, -1, 0, 65, 70, 0, 1500, 0, 90, 1, 0, 90}},
    {"Living field",
     {0, 88, 60, 40, 55, 20, 65, -3, 0, 85, 45, 0, 1500, 0, 90, 1, 0, 0}},
    {"Clean room", {0, 24, 0, 0, 4, 20, 28, 0, 0, 0, 12}},
    {"Deep hall", {0, 65, 0, 0, 18, 35, 40, 0, 0, 0, 10}},
    {"Dark dub", {0, 72, 32, 20, 38, 82, 42, 0, 0, 28, 45}},
    {"Enormous space", {0, 97, 0, 0, 28, 38, 48, -2, 0, 0, 15}},
    {"Evolving field", {0, 86, 64, 48, 70, 48, 52, -1, 0, 68, 38}},
    {"FOREVER", {1, 88, 48, 30, 65, 55, 65, -2, 0, 54, 28}},
}};

float presetValue(const FactoryPreset& preset, std::size_t i) {
  if (preset.values[i] != 0) return preset.values[i];
  if (i == 12) return 1500.F;
  if (i == 14) return 90.F;
  if (i == 15) return 1.F;
  return 0.F;
}

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
    if (button.hasKeyboardFocus(true)) {
      graphics.setColour(juce::Colour{kInk});
      graphics.drawEllipse(area.reduced(5), 2.0F);
    }
    graphics.setColour(active ? juce::Colour{kSurface} : juce::Colour{kInk});
    graphics.setFont(juce::FontOptions{18.0F, juce::Font::bold});
    graphics.drawFittedText(button.getButtonText(), area.toNearestInt(),
                            juce::Justification::centred, 2, 1.F);
    graphics.setColour(active ? juce::Colour{kSurface}.withAlpha(0.7F)
                              : juce::Colour{kMuted});
    graphics.setFont(juce::FontOptions{10.0F, juce::Font::bold});
    graphics.drawText(button.getProperties()["holdStatus"].toString(),
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
    slider_.textFromValueFunction = [this](double value) {
      return aste::ui::formattedValue(slider_, value);
    };
    slider_.setTextValueSuffix(suffix);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 22);
    slider_.setDoubleClickReturnValue(true, initial);
    slider_.setTitle(name);
    slider_.setDescription(
        aste::ui::explanation(id, state.processor.getName()));
    slider_.setTooltip(
        aste::ui::explanation(id, state.processor.getName()) +
        " Shift-drag for fine adjustment; type an exact value below.");
    if (juce::String{id} == "pitch") {
      slider_.setDescription(
          "Amount of pitch-shifted audio returning into the field, not a "
          "tuning control.");
      slider_.setTooltip(slider_.getDescription());
    }
    slider_.setWantsKeyboardFocus(true);
    slider_.setExplicitFocusOrder(focusOrder);
    label_.setText(name, juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(juce::FontOptions{11.0F, juce::Font::bold});
    addAndMakeVisible(slider_);
    addAndMakeVisible(label_);
  }

  void setHelp(const juce::String& text) {
    slider_.setDescription(text);
    slider_.setTooltip(text);
    setDescription(text);
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

class FieldMeter final : public juce::Component, private juce::Timer {
 public:
  explicit FieldMeter(FieldAudioProcessor& processor) : processor_{processor} {
    setTitle("FIELD OBSERVATION");
    setDescription(
        "Updated Field activity measures RMS across the late-network taps. "
        "Retention target is a coefficient before damping and pitch losses, "
        "not a measured decay time.");
    startTimerHz(30);
  }

  void paint(juce::Graphics& graphics) override {
    graphics.fillAll(juce::Colour{kPanel});
    auto area = getLocalBounds().toFloat().reduced(14.0F);
    graphics.setColour(juce::Colour{kMuted});
    graphics.setFont(juce::FontOptions{11.0F, juce::Font::bold});
    graphics.drawText("BOUNDARIES", area.removeFromTop(20.0F),
                      juce::Justification::centredLeft);
    auto levels =
        area.removeFromTop(std::max(40.0F, area.getHeight() - 135.0F));
    drawLevel(graphics, levels.removeFromLeft(levels.getWidth() * 0.5F), input_,
              "IN");
    drawLevel(graphics, levels, output_, "OUT");
    area.removeFromTop(18.0F);
    graphics.drawText("FIELD ACTIVITY", area.removeFromTop(20.0F),
                      juce::Justification::centredLeft);
    drawBar(graphics, area.removeFromTop(15.0F), energy_ * 12.0F);
    area.removeFromTop(16.0F);
    graphics.drawText("RETENTION TARGET", area.removeFromTop(20.0F),
                      juce::Justification::centredLeft);
    drawBar(graphics, area.removeFromTop(15.0F), retention_);
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
    area = area.reduced(10.0F, 0.0F);
    graphics.setColour(juce::Colour{kSurface});
    graphics.fillRect(area);
    auto fill = area;
    fill.removeFromTop(area.getHeight() *
                       (1.0F - aste::ui::meterPosition(level)));
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

class MemoryPanel final : public juce::Component, private juce::Timer {
 public:
  explicit MemoryPanel(FieldAudioProcessor& p)
      : p_(p),
        outer_{p.state(), "loop_ms", "OUTER PHRASE", " ms", 1500., 14},
        inner_{p.state(), "inner_ms", "INNER FRAGMENT", " ms", 90., 16},
        speed_{p.state(), "loop_speed", "SPEED", " x", 1., 17},
        mix_{p.state(), "loop_mix", "MEMORY / FIELD", " %", 90., 19} {
    const std::array<const char*, 3> ids{"loop_hold", "inner_repeat",
                                         "loop_reverse"};
    const std::array<const char*, 3> titles{"CAPTURE PHRASE", "REPEAT INSIDE",
                                            "REVERSE"};
    for (std::size_t i = 0; i < 3; ++i) {
      buttons_[i].setButtonText(titles[i]);
      buttons_[i].setTitle(titles[i]);
      buttons_[i].setClickingTogglesState(true);
      buttons_[i].setWantsKeyboardFocus(true);
      addAndMakeVisible(buttons_[i]);
      attachments_[i] = std::make_unique<
          juce::AudioProcessorValueTreeState::ButtonAttachment>(
          p.state(), ids[i], buttons_[i]);
    }
    buttons_[0].setTooltip(
        "Capture the preceding outer phrase. Release returns to live audio; "
        "capture again records a new phrase.");
    buttons_[1].setTooltip(
        "Repeat a short piece of the captured phrase. Release rejoins the "
        "advancing outer phrase.");
    for (auto* knob : {&outer_, &inner_, &speed_, &mix_})
      addAndMakeVisible(knob);
    startTimerHz(25);
  }
  void resized() override {
    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(22);
    area.removeFromBottom(14);
    auto actions = area.removeFromLeft(190);
    for (auto& button : buttons_)
      button.setBounds(actions.removeFromTop(30).reduced(0, 2));
    const int width = area.getWidth() / 4;
    for (auto* knob : {&outer_, &inner_, &speed_, &mix_})
      knob->setBounds(area.removeFromLeft(width));
  }
  void paint(juce::Graphics& g) override {
    g.setColour(juce::Colour{kPanel});
    g.fillRect(getLocalBounds());
    const unsigned state = p_.loopStatus();
    g.setColour(juce::Colour{kInk});
    g.setFont(juce::FontOptions{11.F});
    g.drawText("LOOP WITHIN LOOP / " +
                   juce::String{state & 4   ? "WAITING FOR INPUT HISTORY"
                                : state & 2 ? "INNER REPEATING / OUTER ADVANCES"
                                : state & 1 ? "CAPTURED PHRASE"
                                            : "LIVE / CAPTURE TO REPEAT"},
               8, 2, getWidth() - 16, 20, juce::Justification::centredLeft);
    const int width = getWidth() - 16;
    g.setColour(juce::Colour{kMuted});
    g.fillRect(8, getHeight() - 12, width, 3);
    if (state & 1) {
      g.setColour(juce::Colour{kAccent});
      g.fillRect(8, getHeight() - 12,
                 static_cast<int>(width * p_.loopPosition()), 3);
    }
  }

 private:
  void timerCallback() override {
    buttons_[0].setButtonText(buttons_[0].getToggleState() ? "RELEASE PHRASE"
                                                           : "CAPTURE PHRASE");
    buttons_[1].setButtonText(buttons_[1].getToggleState() ? "RELEASE INNER"
                                                           : "REPEAT INSIDE");
    repaint();
  }
  FieldAudioProcessor& p_;
  Knob outer_, inner_, speed_, mix_;
  std::array<juce::TextButton, 3> buttons_;
  std::array<
      std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, 3>
      attachments_;
};

class FieldEditor final : public juce::AudioProcessorEditor,
                          private juce::Timer {
 public:
  explicit FieldEditor(FieldAudioProcessor& processor)
      : AudioProcessorEditor{processor},
        processor_{processor},
        master_{processor.state()},
        meter_{processor},
        foreverAttachment_{processor.state(), "forever", forever_},
        memory_{processor},
        mass_{processor.state(), "mass", "MASS", " %", 62.0, 2},
        grain_{processor.state(), "grain", "GRAIN", " %", 34.0, 3},
        pitch_{processor.state(), "pitch", "PITCH", " %", 28.0, 4},
        motion_{processor.state(), "motion", "MOTION", " %", 24.0, 5},
        distance_{processor.state(), "distance", "DISTANCE", " %", 45.0, 6},
        blend_{processor.state(), "blend", "BLEND", " %", 48.0, 7},
        output_{processor.state(), "output", "OUTPUT", " dB", -3.0, 8},
        texture_{processor.state(), "texture", "TRANSFORM", " %", 70.0, 11},
        focus_{processor.state(), "focus", "FOCUS", " %", 75.0, 12} {
    setLookAndFeel(&lookAndFeel_);
    mass_.setHelp(
        "Decay and spatial mass: 0.25 to 16 seconds in the updated engine.");
    texture_.setHelp(
        "TRANSFORM: blend evolving fragment and pitch readouts; zero gives the "
        "clean reverb foundation.");
    grain_.setHelp(
        "Fragment movement amount, enabled by Transform. Does not shorten the "
        "feedback decay.");
    pitch_.setHelp(
        "Pitch-shifted wet readout amount, enabled by Transform. Focus moves "
        "the interval from fifth to octave.");
    focus_.setHelp(
        "Clarify low-mid energy and move the transformed pitch interval.");
    addAndMakeVisible(master_);
    workflow_.bind(
        *this, "INPUT > DIFFUSED SPACE > TRANSFORM > FOREVER > MASTER",
        "MASS sets decay; DISTANCE darkens the space. TRANSFORM enables "
        "Grain and Pitch without shortening the decay. Feed audio "
        "before FOREVER holds its stored energy.");
    addAndMakeVisible(workflow_);
    addAndMakeVisible(memory_);
    addAndMakeVisible(focus_);
    forever_.setButtonText("HOLD SPATIAL TAIL");
    forever_.setTitle("FOREVER");
    forever_.setDescription("Hold or release the stored spatial field");
    forever_.setClickingTogglesState(true);
    forever_.setWantsKeyboardFocus(true);
    forever_.setExplicitFocusOrder(1);
    forever_.setTooltip(
        "Capture the existing spatial energy and close its input. Motion and "
        "Transform "
        "keep their control values. Release to admit new audio and resume "
        "decay.");
    clear_.setButtonText("CLEAR FIELD");
    clear_.setTitle("CLEAR FIELD");
    clear_.setExplicitFocusOrder(10);
    clear_.setTooltip(
        "Empty the stored audio. Hold stays armed if FOREVER is on.");
    clear_.onClick = [this] { processor_.clearField(); };
    addAndMakeVisible(clear_);
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
    for (const auto& snapshot : kPresets)
      preset_.addSnapshot(processor_, processor_.state(), [&](auto set) {
        for (std::size_t i = 0; i < snapshot.values.size(); ++i)
          set(kParameterIds[i], presetValue(snapshot, i));
      });
    preset_.setMatchFilter([this](std::size_t index) {
      return (index >= 9) == processor_.usesDiffuseSpace();
    });
    preset_.watch(processor_);
    response_.bind(
        [this] {
          return processor_.usesDiffuseSpace() &&
                 !processor_.usesLegacyBehavior() &&
                 processor_.usesMaterialResponse();
        },
        [this] { processor_.useCurrentResponse(); },
        "Use the dense diffused reverb. Existing projects keep their original "
        "engine "
        "until you explicitly adopt the updated sound.");
    addAndMakeVisible(response_);
    preset_.onChange = [this] {
      if (const int selected = preset_.getSelectedId(); selected > 0) {
        processor_.loadFactoryPreset(selected - 1);
        preset_.remember(processor_);
      }
    };
    for (auto* component : std::array<juce::Component*, 11>{
             &meter_, &forever_, &mass_, &grain_, &pitch_, &motion_, &distance_,
             &blend_, &output_, &preset_, &texture_})
      addAndMakeVisible(component);
    setResizable(true, true);
    setResizeLimits(900, 680, 1800, 1200);
    setSize(1080, 800);
    timerCallback();
    startTimerHz(15);
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
    header = juce::Rectangle<int>{24, 53, 310, 16};
    graphics.drawText("F-01 / STORED SPATIAL ENERGY", header,
                      juce::Justification::centredLeft);
    graphics.setColour(juce::Colour{kMuted}.withAlpha(0.35F));
    graphics.fillRect(24, 74, getWidth() - 48, 1);
    graphics.setColour(juce::Colour{kInk});
    graphics.setFont(juce::FontOptions{11.0F});
  }

  void resized() override {
    workflow_.setBounds(24, getHeight() - 80, getWidth() - 48, 48);
    preset_.setBounds(getWidth() - 414, 28, 180, 28);
    master_.setBounds(getWidth() - 214, 18, 190, 46);
    response_.setBounds(getWidth() - 224, getHeight() - 30, 200, 24);
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(68);
    area.removeFromBottom(60);
    memory_.setBounds(area.removeFromBottom(160));
    area.removeFromBottom(12);
    meter_.setBounds(area.removeFromLeft(142));
    area.removeFromLeft(20);
    auto hero = area.removeFromLeft(std::min(220, area.getWidth() / 3));
    clear_.setBounds(hero.removeFromBottom(32).reduced(18, 0));
    focus_.setBounds(hero.removeFromBottom(110).reduced(18, 0));
    forever_.setBounds(hero.reduced(18));
    area.removeFromLeft(16);
    const int columnWidth = std::max(1, area.getWidth() / 4);
    const int rowHeight = std::max(1, area.getHeight() / 2);
    const std::array<Knob*, 8> controls{&mass_,   &grain_,    &pitch_,
                                        &motion_, &distance_, &blend_,
                                        &output_, &texture_};
    for (std::size_t index = 0; index < controls.size(); ++index) {
      const int column = static_cast<int>(index % 4U);
      const int row = static_cast<int>(index / 4U);
      controls[index]->setBounds(area.getX() + column * columnWidth,
                                 area.getY() + row * rowHeight, columnWidth,
                                 rowHeight);
    }
  }

 private:
  void timerCallback() override {
    const juce::String status = forever_.getToggleState()
                                    ? (processor_.usesLegacyBehavior()
                                           ? "LEGACY HOLD / COUPLED COLOR"
                                           : "HOLD ACTIVE / COLOR INDEPENDENT")
                                    : "PRESS TO HOLD";
    if (forever_.getProperties()["holdStatus"].toString() != status) {
      forever_.getProperties().set("holdStatus", status);
      forever_.setDescription(
          status + (processor_.usesLegacyBehavior()
                        ? ". Legacy hold also adds grain, pitch and motion."
                        : ". Hold changes persistence; grain, pitch and motion "
                          "stay at their controls. Not a lossless freeze."));
      forever_.repaint();
    }
  }
  FieldLookAndFeel lookAndFeel_;
  FieldAudioProcessor& processor_;
  aste::ui::MasterOutput master_;
  aste::ui::WorkflowGuide workflow_;
  FieldMeter meter_;
  juce::ToggleButton forever_;
  juce::TextButton clear_;
  juce::AudioProcessorValueTreeState::ButtonAttachment foreverAttachment_;
  MemoryPanel memory_;
  Knob mass_;
  Knob grain_;
  Knob pitch_;
  Knob motion_;
  Knob distance_;
  Knob blend_;
  Knob output_;
  Knob texture_;
  Knob focus_;
  aste::ui::PresetMenu preset_;
  aste::ui::ResponseButton response_;
  juce::TooltipWindow tooltips_{this, 400};
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
  aste::ui::MasterGain::addParameter(layout, 0.F);
  addPercent(layout, "texture", "Texture", 70.F);
  addPercent(layout, "focus", "Focus", 0.F);
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"loop_hold", 1}, "Capture phrase", false));
  const auto addTime = [&](const char* id, const char* name, float low,
                           float high, float initial) {
    layout.add(std::make_unique<juce::AudioParameterFloat>(
        juce::ParameterID{id, 1}, name,
        juce::NormalisableRange<float>{low, high, .01F}, initial));
  };
  addTime("loop_ms", "Outer phrase ms", 20, 4000, 1500);
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"inner_repeat", 1}, "Repeat inner fragment", false));
  addTime("inner_ms", "Inner fragment ms", 4, 1000, 90);
  addTime("loop_speed", "Loop speed", .25F, 2, 1);
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"loop_reverse", 1}, "Reverse fragment", false));
  addPercent(layout, "loop_mix", "Memory / field", 0.F);
  return layout;
}

FieldAudioProcessor::FieldAudioProcessor()
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
  loadFactoryPreset(9);
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
          .bypass = value(bypass) >= 0.5F,
          .legacyBehavior = usesLegacyBehavior(),
          .loopHold = value(loopHold) >= .5F,
          .innerRepeat = value(innerRepeat) >= .5F,
          .loopReverse = value(loopReverse) >= .5F,
          .loopMs = value(loopMs),
          .innerMs = value(innerMs),
          .loopSpeed = value(loopSpeed),
          .loopMix = value(loopMix) * .01F,
          .focus = value(focus) * .01F,
          .materialBehavior = usesMaterialResponse() && !usesLegacyBehavior(),
          .diffuseBehavior = usesDiffuseSpace() && !usesLegacyBehavior(),
          .texture = value(texture) * .01F};
}

void FieldAudioProcessor::prepareToPlay(double sampleRate, int) {
  masterGain_.prepare(sampleRate);
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
  if (clearRequested_.exchange(false)) processor_.reset();
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
    if (metadata.numBytes == 3 && (metadata.data[0] & 0xf0U) == 0x90U &&
        metadata.data[2] > 0)
      processor_.noteOn(metadata.data[1], metadata.data[2] / 127.F);
  }
  processRange(buffer, processed, buffer.getNumSamples() - processed,
               parameters);
  publishMeters(processor_.meters());
  outputPeak_.store(masterGain_.process(buffer), std::memory_order_relaxed);
}

void FieldAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                               juce::MidiBuffer&) {
  if (clearRequested_.exchange(false)) {
    processor_.reset();
    publishMeters({});
  }
  float peak{};
  for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    peak =
        std::max(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
  publishMeters({peak, peak, fieldEnergy(), retention()});
}

void FieldAudioProcessor::publishMeters(const MeterSnapshot& meters) noexcept {
  loopPosition_.store(meters.loopPosition, std::memory_order_relaxed);
  loopStatus_.store((meters.loopActive ? 1U : 0U) |
                        (meters.innerActive ? 2U : 0U) |
                        (meters.loopWaiting ? 4U : 0U),
                    std::memory_order_relaxed);
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
  if (index >= 9)
    useCurrentResponse();
  else {
    if (engineRevision_.exchange(1) != 1) clearRequested_.store(true);
    if (index == 8) materialRevision_.adopt();
  }
  const auto& preset = kPresets[static_cast<std::size_t>(index)];
  legacyBehavior_.store(index < 5 && preset.values[9] == 0,
                        std::memory_order_relaxed);
  for (std::size_t parameter = 0; parameter < preset.values.size(); ++parameter)
    if (auto* target = state_.getParameter(kParameterIds[parameter]))
      target->setValueNotifyingHost(
          target->convertTo0to1(presetValue(preset, parameter)));
}

juce::AudioProcessorParameter* FieldAudioProcessor::getBypassParameter() const {
  return state_.getParameter("bypass");
}

void FieldAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
  auto state = state_.copyState();
  state.setProperty("schema", kStateSchema, nullptr);
  state.setProperty("product", kStateType, nullptr);
  state.setProperty("engine", usesDiffuseSpace() ? 2 : 1, nullptr);
  state.setProperty("behavior", usesLegacyBehavior() ? 0 : 1, nullptr);
  state.removeProperty("material", nullptr);
  materialRevision_.write(state);
  if (const auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void FieldAudioProcessor::setStateInformation(const void* data, int size) {
  const auto xml = getXmlFromBinary(data, size);
  if (!xml) return;
  const auto restored = juce::ValueTree::fromXml(*xml);
  if (!restored.isValid() || restored.getType().toString() != kStateType ||
      restored.getProperty("product").toString() != kStateType ||
      (restored.getProperty("schema").toString() != "1" &&
       restored.getProperty("schema").toString() != "2"))
    return;
  const bool oldSchema = restored.getProperty("schema").toString() == "1";
  const auto engine = restored.getProperty("engine", 1).toString();
  if (engine != "1" && engine != "2") return;
  const auto behavior = restored.getProperty("behavior").toString();
  if (!oldSchema && behavior != "0" && behavior != "1") return;
  const bool legacy = oldSchema || behavior == "0";
  bool material = false;
  if (!aste::parameters::MaterialRevision::parse(restored, material)) return;
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
  if (!seen.contains("texture"))
    validated.getChildWithProperty("id", "texture")
        .setProperty("value", 0.F, nullptr);
  for (const auto* id :
       {"loop_hold", "inner_repeat", "loop_reverse", "loop_mix"})
    if (!seen.contains(id))
      validated.getChildWithProperty("id", id).setProperty("value", 0.F,
                                                           nullptr);
  if (!seen.contains("focus"))
    validated.getChildWithProperty("id", "focus")
        .setProperty("value", 0.F, nullptr);
  if (!seen.contains("master"))
    validated.getChildWithProperty("id", "master")
        .setProperty("value", 0.F, nullptr);
  validated.setProperty("schema", kStateSchema, nullptr);
  validated.setProperty("product", kStateType, nullptr);
  validated.setProperty("engine", oldSchema ? 1 : engine.getIntValue(),
                        nullptr);
  validated.setProperty("behavior", legacy ? 0 : 1, nullptr);
  engineRevision_.store(oldSchema ? 1 : engine.getIntValue());
  legacyBehavior_.store(legacy, std::memory_order_relaxed);
  materialRevision_.set(material);
  state_.replaceState(validated);
  clearRequested_.store(true, std::memory_order_release);
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
