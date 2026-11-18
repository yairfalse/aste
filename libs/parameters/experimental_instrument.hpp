#pragma once

#include "decimal_parse.hpp"
#include "material_revision.hpp"
#include "master_output.hpp"
#include "plugin_ui.hpp"
#include <vector>

namespace aste::experimental {
struct ControlSpec {
  juce::String id, name;
  float low{}, high{100}, initial{}, step{.01F}, centre{};
  juce::String choices;
  bool toggle{};
  bool appended{};
};

inline ControlSpec knob(juce::String id, juce::String name, float low,
                        float high, float initial, float centre = 0,
                        float step = .01F) {
  return {id, name, low, high, initial, step, centre, {}, false};
}
inline ControlSpec choice(juce::String id, juce::String name,
                          juce::String choices, int initial = 0) {
  return {id, name, 0, 0, static_cast<float>(initial), 1, 0, choices, false};
}
inline ControlSpec toggle(juce::String id, juce::String name,
                          bool initial = false) {
  return {id, name, 0, 1, initial ? 1.F : 0.F, 1, 0, {}, true};
}

class Instrument : public juce::AudioProcessor {
 public:
  Instrument(juce::String name, juce::String type,
             std::vector<ControlSpec> specs, bool effect = false)
      : AudioProcessor{
            effect
                ? BusesProperties{}
                      .withInput("Input", juce::AudioChannelSet::stereo(), true)
                      .withOutput("Output", juce::AudioChannelSet::stereo(),
                                  true)
                : BusesProperties{}.withOutput(
                      "Output", juce::AudioChannelSet::stereo(), true)},
        effect_{effect},
        name_{name},
        type_{type},
        specs_{std::move(specs)},
        state_{*this, nullptr, type_, layout(specs_)} {
    for (const auto& spec : specs_)
      values_.push_back(state_.getRawParameterValue(spec.id));
    master_.bind(state_);
    bypass_ = state_.getRawParameterValue("bypass");
  }
  const juce::String getName() const override { return name_; }
  bool acceptsMidi() const override { return !effect_; }
  bool producesMidi() const override { return false; }
  bool isMidiEffect() const override { return false; }
  bool hasEditor() const override { return true; }
  juce::AudioProcessorParameter* getBypassParameter() const override {
    return state_.getParameter("bypass");
  }
  double getTailLengthSeconds() const override { return 120; }
  int getNumPrograms() override { return 1; }
  int getCurrentProgram() override { return 0; }
  void setCurrentProgram(int) override {}
  const juce::String getProgramName(int) override { return "Default"; }
  void changeProgramName(int, const juce::String&) override {}
  bool isBusesLayoutSupported(const BusesLayout& buses) const override {
    return (effect_ ? buses.getMainInputChannelSet() ==
                          buses.getMainOutputChannelSet()
                    : buses.getMainInputChannelSet().isDisabled()) &&
           (buses.getMainOutputChannelSet() == juce::AudioChannelSet::mono() ||
            buses.getMainOutputChannelSet() == juce::AudioChannelSet::stereo());
  }
  void prepareToPlay(double rate, int block) override {
    ready_ = prepareEngine(rate, block);
    rate_ = rate;
    master_.prepare(rate);
    restoreRequested_.store(false);
    clearRequested_.store(false);
    // Preserve product-declared latency. Resetting it here creates a host
    // restart loop for nonzero-latency processors such as C-01.
  }
  void releaseResources() override { clearRequested_.store(true); }
  void processBlock(juce::AudioBuffer<float>& buffer,
                    juce::MidiBuffer& midi) override {
    juce::ScopedNoDenormals noDenormals;
    if (!effect_) buffer.clear();
    if (bypass_->load() >= .5F) {
      if (effect_) {
        outputPeak_.store(passDry(buffer));
        return;
      }
      clearRequested_.store(true);
      outputPeak_.store(0);
      return;
    }
    if (!ready_ || buffer.getNumChannels() == 0 || buffer.getNumSamples() == 0)
      return;
    if (restoreRequested_.exchange(false)) onRestore();
    render(buffer, midi);
    outputPeak_.store(master_.process(buffer), std::memory_order_relaxed);
  }
  void processBlockBypassed(juce::AudioBuffer<float>& b,
                            juce::MidiBuffer&) override {
    if (effect_) {
      outputPeak_.store(passDry(b));
      return;
    }
    b.clear();
    clearRequested_.store(true);
    outputPeak_.store(0);
  }
  juce::AudioProcessorValueTreeState& state() noexcept { return state_; }
  bool usesMaterialResponse() const noexcept {
    return materialRevision_.active();
  }
  void useCurrentResponse() {
    materialRevision_.adopt();
    onRestore();
    updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
  }
  float outputPeak() const noexcept { return outputPeak_.load(); }
  float value(std::size_t index) const noexcept {
    return values_[index]->load(std::memory_order_relaxed);
  }
  const std::vector<ControlSpec>& specs() const noexcept { return specs_; }
  void set(const juce::String& id, float value) {
    if (auto* p = state_.getParameter(id)) {
      p->beginChangeGesture();
      p->setValueNotifyingHost(p->convertTo0to1(value));
      p->endChangeGesture();
    }
  }
  void defaults() {
    for (auto* p : getParameters()) {
      p->beginChangeGesture();
      p->setValueNotifyingHost(p->getDefaultValue());
      p->endChangeGesture();
    }
    clearRequested_.store(true);
  }
  void getStateInformation(juce::MemoryBlock& bytes) override {
    auto tree = state_.copyState();
    tree.setProperty("schema", 1, nullptr);
    tree.setProperty("product", type_, nullptr);
    materialRevision_.write(tree);
    writeProductMetadata(tree);
    if (auto xml = tree.createXml()) copyXmlToBinary(*xml, bytes);
  }
  void setStateInformation(const void* data, int size) override {
    auto xml = getXmlFromBinary(data, size);
    if (!xml) return;
    auto tree = juce::ValueTree::fromXml(*xml);
    if (tree.getType().toString() != type_ ||
        tree.getProperty("product").toString() != type_ ||
        tree.getProperty("schema").toString() != "1")
      return;
    bool material = false;
    if (!aste::parameters::MaterialRevision::parse(tree, material) ||
        !validateProductMetadata(tree))
      return;
    auto restored = state_.copyState();
    for (auto child : restored)
      if (auto* p = state_.getParameter(child.getProperty("id").toString()))
        child.setProperty("value", p->convertFrom0to1(p->getDefaultValue()),
                          nullptr);
    juce::StringArray seen;
    for (auto child : tree) {
      auto id = child.getProperty("id").toString();
      auto* p = state_.getParameter(id);
      if (!p) continue;
      double parsed = 0;
      if (seen.contains(id) || !child.hasProperty("value") ||
          !aste::parameters::parseFiniteDecimal(
              child.getProperty("value").toString().toStdString(), parsed) ||
          !std::isfinite(parsed))
        return;
      seen.add(id);
      const auto& r = p->getNormalisableRange();
      restored.getChildWithProperty("id", id).setProperty(
          "value", juce::jlimit(r.start, r.end, static_cast<float>(parsed)),
          nullptr);
    }
    restored.setProperty("schema", 1, nullptr);
    restored.setProperty("product", type_, nullptr);
    materialRevision_.set(material);
    state_.replaceState(restored);
    restoreProductMetadata(tree);
    restoreRequested_.store(true);
  }

 protected:
  virtual void writeProductMetadata(juce::ValueTree&) const {}
  virtual bool validateProductMetadata(const juce::ValueTree&) const {
    return true;
  }
  virtual void restoreProductMetadata(const juce::ValueTree&) {}
  virtual bool prepareEngine(double, int) = 0;
  virtual void render(juce::AudioBuffer<float>&,
                      const juce::MidiBuffer&) noexcept = 0;
  virtual void onRestore() noexcept { clearRequested_.store(true); }
  std::atomic<bool> clearRequested_{};
  double rate_{48000};

 private:
  static float passDry(juce::AudioBuffer<float>& buffer) noexcept {
    float peak = 0;
    for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
      for (int n = 0; n < buffer.getNumSamples(); ++n) {
        float x = buffer.getSample(ch, n);
        if (!std::isfinite(x)) {
          x = 0;
          buffer.setSample(ch, n, 0);
        }
        peak = std::max(peak, std::abs(x));
      }
    return peak;
  }
  static juce::AudioProcessorValueTreeState::ParameterLayout layout(
      const std::vector<ControlSpec>& specs) {
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    const auto add = [&](const ControlSpec& s) {
      if (s.toggle)
        result.add(std::make_unique<juce::AudioParameterBool>(
            juce::ParameterID{s.id, 1}, s.name, s.initial >= .5F));
      else if (s.choices.isNotEmpty())
        result.add(std::make_unique<juce::AudioParameterChoice>(
            juce::ParameterID{s.id, 1}, s.name,
            juce::StringArray::fromTokens(s.choices, "|", ""),
            static_cast<int>(s.initial)));
      else {
        juce::NormalisableRange<float> range{s.low, s.high, s.step};
        if (s.centre > s.low && s.centre < s.high)
          range.setSkewForCentre(s.centre);
        result.add(std::make_unique<juce::AudioParameterFloat>(
            juce::ParameterID{s.id, 1}, s.name, range, s.initial));
      }
    };
    for (const auto& s : specs)
      if (!s.appended) add(s);
    aste::ui::MasterGain::addParameter(result, 0);
    result.add(std::make_unique<juce::AudioParameterBool>(
        juce::ParameterID{"bypass", 1}, "Bypass", false));
    for (const auto& s : specs)
      if (s.appended) add(s);
    return result;
  }
  bool effect_{};
  aste::parameters::MaterialRevision materialRevision_;
  juce::String name_, type_;
  std::vector<ControlSpec> specs_;
  juce::AudioProcessorValueTreeState state_;
  std::vector<std::atomic<float>*> values_;
  aste::ui::MasterGain master_;
  std::atomic<float>* bypass_{};
  bool ready_{};
  std::atomic<bool> restoreRequested_{};
  std::atomic<float> outputPeak_{};
};

class Control final : public juce::Component {
 public:
  Control(Instrument& processor, const ControlSpec& spec,
          bool compact = false) {
    setTitle(spec.name);
    setDescription(aste::ui::explanation(spec.id, processor.getName()));
    label_.setText(spec.name, juce::dontSendNotification);
    label_.setFont(juce::FontOptions{10.F, juce::Font::bold});
    label_.setJustificationType(juce::Justification::centred);
    addAndMakeVisible(label_);
    if (spec.toggle) {
      button_.setButtonText(spec.name);
      button_.setTitle(spec.name);
      addAndMakeVisible(button_);
      buttonAttachment_ = std::make_unique<
          juce::AudioProcessorValueTreeState::ButtonAttachment>(
          processor.state(), spec.id, button_);
      kind_ = 1;
      label_.setVisible(false);
    } else if (spec.choices.isNotEmpty()) {
      menu_.addItemList(juce::StringArray::fromTokens(spec.choices, "|", ""),
                        1);
      menu_.setTitle(spec.name);
      addAndMakeVisible(menu_);
      menuAttachment_ = std::make_unique<
          juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          processor.state(), spec.id, menu_);
      kind_ = 2;
    } else {
      slider_.setSliderStyle(compact
                                 ? juce::Slider::LinearHorizontal
                                 : juce::Slider::RotaryHorizontalVerticalDrag);
      slider_.setTextBoxStyle(
          compact ? juce::Slider::TextBoxRight : juce::Slider::TextBoxBelow,
          false, 76, 22);
      slider_.setTitle(spec.name);
      slider_.setTooltip(
          "Shift-drag for fine adjustment. Type an exact value. Double-click "
          "to reset.");
      slider_.setDoubleClickReturnValue(true, spec.initial);
      slider_.textFromValueFunction = [this](double v) {
        return aste::ui::formattedValue(slider_, v);
      };
      addAndMakeVisible(slider_);
      sliderAttachment_ = std::make_unique<
          juce::AudioProcessorValueTreeState::SliderAttachment>(
          processor.state(), spec.id, slider_);
      if (spec.id == "root") {
        slider_.textFromValueFunction = [](double v) {
          return aste::ui::noteName(static_cast<int>(std::lround(v)));
        };
        slider_.valueFromTextFunction = [this](const juce::String& text) {
          const int named = aste::ui::noteNumber(text, -1);
          if (named >= 0) return static_cast<double>(named);
          double numeric{};
          if (aste::parameters::parseFiniteDecimal(text.trim().toStdString(),
                                                   numeric))
            return numeric;
          return slider_.getValue();
        };
      }
      if (spec.id == "root") slider_.updateText();
    }
  }
  void setCaption(const juce::String& caption) {
    label_.setText(caption, juce::dontSendNotification);
    setTitle(caption);
    slider_.setTitle(caption);
  }
  void setHelp(const juce::String& text) {
    setDescription(text);
    for (auto* child : getChildren()) {
      child->setDescription(text);
      if (auto* tooltip = dynamic_cast<juce::SettableTooltipClient*>(child))
        tooltip->setTooltip(text +
                            " Shift-drag for fine adjustment; type an exact "
                            "value; double-click to reset.");
    }
  }
  void resized() override {
    auto area = getLocalBounds().reduced(4, 1);
    if (kind_ != 1) label_.setBounds(area.removeFromTop(16));
    if (kind_ == 1)
      button_.setBounds(area);
    else if (kind_ == 2)
      menu_.setBounds(area.withSizeKeepingCentre(area.getWidth(), 28));
    else
      slider_.setBounds(area);
  }

 private:
  juce::Label label_;
  aste::ui::PrecisionSlider slider_;
  juce::ToggleButton button_;
  juce::ComboBox menu_;
  int kind_{};
  std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
      sliderAttachment_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      menuAttachment_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      buttonAttachment_;
};

class Look final : public juce::LookAndFeel_V4 {
 public:
  Look() {
    setColour(juce::ResizableWindow::backgroundColourId,
              juce::Colour{0xff101417});
    setColour(juce::Slider::rotarySliderFillColourId, juce::Colour{0xff83a99a});
    setColour(juce::Slider::trackColourId, juce::Colour{0xff83a99a});
    setColour(juce::Slider::textBoxTextColourId, juce::Colour{0xffe7ece8});
    setColour(juce::ComboBox::backgroundColourId, juce::Colour{0xff20292c});
    setColour(juce::TextButton::buttonColourId, juce::Colour{0xff253338});
    setColour(juce::TextButton::buttonOnColourId, juce::Colour{0xff557e6e});
    setColour(juce::Label::textColourId, juce::Colour{0xffe7ece8});
  }
};
}  // namespace aste::experimental
