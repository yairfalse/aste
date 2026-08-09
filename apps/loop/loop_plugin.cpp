#include "loop_plugin.hpp"

#include "decimal_parse.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace aste::loop::plugin {
namespace {

constexpr auto kStateType = "loop-l01";
constexpr int kStateSchema = 2;
constexpr auto kSurface = 0xff0d1212U;
constexpr auto kPanel = 0xff172120U;
constexpr auto kInk = 0xffe5ece8U;
constexpr auto kMuted = 0xff718781U;
constexpr auto kAccent = 0xff2f9a86U;
constexpr std::array<const char*, 20> kParameterIds{
    "capture",     "overdub",    "feedback", "sync",    "length_beats",
    "free_length", "start",      "speed",    "reverse", "pitch",
    "splice",      "wow",        "flutter",  "drift",   "degradation",
    "amplifier",   "tape_speed", "mix",      "output",  "bypass"};

struct Preset {
  const char* name;
  std::array<float, kParameterIds.size()> values;
};

constexpr std::array<Preset, 5> kPresets{{
    {"Clean Memory",
     {0, 50, 90, 1, 4, 2, 0, 1, 0, 0, 3, 2, 1, 1, 2, 12, 2, 100, -3, 0}},
    {"Oxide Circle",
     {0, 58, 82, 1, 8, 4, 7, 0.75F, 0, -5, 7, 18, 9, 7, 14, 35, 1, 100, -5, 0}},
    {"Reverse Field",
     {0, 42, 88, 1, 6, 3, 18, 0.5F, 1, 7, 10, 10, 5, 4, 8, 28, 1, 100, -5, 0}},
    {"Short Splice",
     {0, 70, 72, 0, 4, 0.35F, 0, 1, 0, 12, 2, 5, 3, 2, 20, 52, 0, 100, -7, 0}},
    {"Half Current", {0,  60, 94, 1,  8,  4,  12, 0.5F, 0,  -12,
                      12, 30, 12, 12, 28, 62, 0,  86,   -8, 0}},
}};

bool parseFiniteFloat(const juce::var& value, float& result) {
  double parsed{};
  if (!aste::parameters::parseFiniteDecimal(value.toString().toStdString(),
                                            parsed)) {
    return false;
  }
  result = static_cast<float>(parsed);
  return std::isfinite(result);
}

juce::NormalisableRange<float> skewed(float low, float high, float centre,
                                      float interval) {
  juce::NormalisableRange<float> range{low, high, interval};
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

class LoopLookAndFeel final : public juce::LookAndFeel_V4 {
 public:
  LoopLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, juce::Colour{kInk});
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour{kSurface});
    setColour(juce::Slider::textBoxOutlineColourId,
              juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, juce::Colour{kInk});
  }

  void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                        float position, float startAngle, float endAngle,
                        juce::Slider&) override {
    const auto bounds =
        juce::Rectangle<float>{static_cast<float>(x), static_cast<float>(y),
                               static_cast<float>(width),
                               static_cast<float>(height)}
            .reduced(7.0F);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5F;
    const auto centre = bounds.getCentre();
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0, startAngle,
                        endAngle, true);
    g.setColour(juce::Colour{kMuted}.withAlpha(0.3F));
    g.strokePath(track, juce::PathStrokeType{3.0F});
    juce::Path value;
    value.addCentredArc(centre.x, centre.y, radius, radius, 0, startAngle,
                        startAngle + position * (endAngle - startAngle), true);
    g.setColour(juce::Colour{kAccent});
    g.strokePath(value, juce::PathStrokeType{4.0F});
  }
};

class Knob final : public juce::Component {
 public:
  Knob(juce::AudioProcessorValueTreeState& state, const char* id,
       const char* name, const char* suffix, double initial, int order)
      : attachment_{state, id, slider_} {
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 20);
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

class Toggle final : public juce::ToggleButton {
 public:
  Toggle(juce::AudioProcessorValueTreeState& state, const char* id,
         const char* name, int order)
      : juce::ToggleButton{name}, attachment_{state, id, *this} {
    setTitle(name);
    setWantsKeyboardFocus(true);
    setExplicitFocusOrder(order);
    setColour(juce::ToggleButton::textColourId, juce::Colour{kInk});
    setColour(juce::ToggleButton::tickColourId, juce::Colour{kAccent});
  }

 private:
  juce::AudioProcessorValueTreeState::ButtonAttachment attachment_;
};

class MemoryPanel final : public juce::Component, private juce::Timer {
 public:
  explicit MemoryPanel(LoopAudioProcessor& processor) : processor_{processor} {
    reloop_.setButtonText("RELOOP");
    reloop_.setTitle("RELOOP");
    reloop_.setColour(juce::TextButton::buttonColourId, juce::Colour{kAccent});
    reloop_.setColour(juce::TextButton::textColourOffId,
                      juce::Colour{kSurface});
    reloop_.onClick = [this] { processor_.reloop(); };
    previous_.setButtonText("PREVIOUS");
    previous_.setTitle("PREVIOUS GENERATION");
    previous_.onClick = [this] { processor_.previousGeneration(); };
    next_.setButtonText("NEXT");
    next_.setTitle("NEXT GENERATION");
    next_.onClick = [this] { processor_.nextGeneration(); };
    clear_.setButtonText("CLEAR MEMORY");
    clear_.setTitle("CLEAR MEMORY");
    clear_.setColour(juce::TextButton::buttonColourId, juce::Colour{kPanel});
    clear_.setColour(juce::TextButton::textColourOffId, juce::Colour{kInk});
    clear_.onClick = [this] { processor_.clearLoop(); };
    for (auto* button : {&previous_, &next_}) {
      button->setColour(juce::TextButton::buttonColourId,
                        juce::Colour{kSurface});
      button->setColour(juce::TextButton::textColourOffId, juce::Colour{kInk});
    }
    addAndMakeVisible(reloop_);
    addAndMakeVisible(previous_);
    addAndMakeVisible(next_);
    addAndMakeVisible(clear_);
    startTimerHz(30);
  }
  void resized() override {
    reloop_.setBounds(16, getHeight() - 138, getWidth() - 32, 48);
    previous_.setBounds(16, getHeight() - 82, (getWidth() - 38) / 2, 26);
    next_.setBounds(previous_.getRight() + 6, getHeight() - 82,
                    (getWidth() - 38) / 2, 26);
    clear_.setBounds(16, getHeight() - 42, getWidth() - 32, 26);
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{kPanel});
    auto area = getLocalBounds().toFloat().reduced(16);
    g.setColour(juce::Colour{kMuted});
    g.setFont(juce::FontOptions{11.0F, juce::Font::bold});
    g.drawText("THREE-DECK MEMORY", area.removeFromTop(20),
               juce::Justification::centredLeft);
    area.removeFromTop(10);
    for (int deck = 0; deck < 3; ++deck) {
      auto lane = area.removeFromTop(48).reduced(2, 7);
      g.setColour(deck == activeDeck_ ? juce::Colour{kAccent}
                                      : juce::Colour{kMuted}.withAlpha(0.55F));
      g.setFont(juce::FontOptions{10.0F, juce::Font::bold});
      g.drawText("TAPE " + juce::String::charToString(
                               static_cast<juce::juce_wchar>('A' + deck)),
                 lane.removeFromLeft(48), juce::Justification::centredLeft);
      const auto path = lane.reduced(2, 12);
      g.fillRect(path.withHeight(2.0F).withCentre(path.getCentre()));
      if (deck == activeDeck_) {
        const float head = path.getX() + position_ * path.getWidth();
        g.fillRect(head - 2.0F, path.getY(), 4.0F, path.getHeight());
      }
    }
    area.removeFromTop(8);
    g.setColour(juce::Colour{kInk});
    g.setFont(juce::FontOptions{20.0F, juce::Font::bold});
    g.drawText(
        generation_ > 0 ? "GENERATION " + juce::String{generation_} : "EMPTY",
        area.removeFromTop(28), juce::Justification::centred);
    g.setColour(juce::Colour{kMuted});
    g.setFont(juce::FontOptions{10.0F});
    g.drawText(juce::String{retained_} + " RETAINED / " +
                   juce::String{captured_ * 100.0F, 0} + " % CAPTURED",
               area.removeFromTop(18), juce::Justification::centred);
    if (printing_ > 0.0F) {
      auto progress = area.removeFromTop(12).reduced(4, 3);
      g.setColour(juce::Colour{kSurface});
      g.fillRect(progress);
      g.setColour(juce::Colour{kAccent});
      g.fillRect(progress.withWidth(progress.getWidth() * printing_));
    } else {
      area.removeFromTop(12);
    }
    auto levels = area.removeFromTop(54).reduced(5, 6);
    drawLevel(g, levels.removeFromTop(18), input_, "IN");
    drawLevel(g, levels.removeFromTop(18), output_, "OUT");
  }

 private:
  static void drawLevel(juce::Graphics& g, juce::Rectangle<float> area,
                        float level, const char* label) {
    const auto text = area.removeFromLeft(28);
    g.setColour(juce::Colour{kMuted});
    g.drawText(label, text, juce::Justification::centredLeft);
    g.setColour(juce::Colour{kSurface});
    g.fillRect(area);
    g.setColour(juce::Colour{kInk});
    g.fillRect(
        area.withWidth(area.getWidth() * juce::jlimit(0.0F, 1.0F, level)));
  }
  void timerCallback() override {
    input_ = std::max(processor_.inputPeak(), input_ * 0.84F);
    output_ = std::max(processor_.outputPeak(), output_ * 0.84F);
    position_ = processor_.loopPosition();
    captured_ = processor_.capturedAmount();
    printing_ = processor_.printingProgress();
    generation_ = processor_.generation();
    retained_ = processor_.retainedGenerations();
    activeDeck_ = processor_.activeDeck();
    repaint();
  }
  LoopAudioProcessor& processor_;
  juce::TextButton reloop_, previous_, next_, clear_;
  float input_{};
  float output_{};
  float position_{};
  float captured_{};
  float printing_{};
  int generation_{};
  int retained_{};
  int activeDeck_{};
};

class LoopEditor final : public juce::AudioProcessorEditor {
 public:
  explicit LoopEditor(LoopAudioProcessor& processor)
      : AudioProcessorEditor{processor},
        processor_{processor},
        memory_{processor},
        capture_{processor.state(), "capture", "CAPTURE", 1},
        sync_{processor.state(), "sync", "HOST SYNC", 2},
        reverse_{processor.state(), "reverse", "REVERSE", 3},
        overdub_{processor.state(), "overdub", "OVERDUB", " %", 50, 4},
        feedback_{processor.state(), "feedback", "FEEDBACK", " %", 85, 5},
        lengthBeats_{processor.state(), "length_beats", "BEATS", "", 4, 6},
        freeLength_{
            processor.state(), "free_length", "FREE LENGTH", " s", 2, 7},
        start_{processor.state(), "start", "START", " %", 0, 8},
        speed_{processor.state(), "speed", "SPEED", " x", 1, 9},
        pitch_{processor.state(), "pitch", "PITCH", " st", 0, 10},
        splice_{processor.state(), "splice", "SPLICE", " %", 3, 11},
        wow_{processor.state(), "wow", "WOW", " %", 8, 12},
        flutter_{processor.state(), "flutter", "FLUTTER", " %", 3, 13},
        drift_{processor.state(), "drift", "DRIFT", " %", 2, 14},
        degradation_{processor.state(), "degradation", "LOSS", " %", 8, 15},
        amplifier_{processor.state(), "amplifier", "RECORD", " %", 25, 16},
        mix_{processor.state(), "mix", "MIX", " %", 100, 17},
        output_{processor.state(), "output", "OUTPUT", " dB", -3, 18} {
    setLookAndFeel(&lookAndFeel_);
    tapeSpeed_.setTitle("TAPE SPEED");
    tapeSpeed_.setTextWhenNothingSelected("TAPE SPEED");
    tapeSpeed_.addItem("3 3/4 IPS", 1);
    tapeSpeed_.addItem("7 1/2 IPS", 2);
    tapeSpeed_.addItem("15 IPS", 3);
    tapeSpeedAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.state(), "tape_speed", tapeSpeed_);
    tapeSpeed_.setColour(juce::ComboBox::backgroundColourId,
                         juce::Colour{kPanel});
    tapeSpeed_.setColour(juce::ComboBox::textColourId, juce::Colour{kInk});
    preset_.setTitle("PRESETS");
    preset_.setTextWhenNothingSelected("PRESETS");
    for (int index = 0; index < processor_.factoryPresetCount(); ++index)
      preset_.addItem(processor_.factoryPresetName(index), index + 1);
    preset_.onChange = [this] {
      if (preset_.getSelectedId() > 0) {
        processor_.loadFactoryPreset(preset_.getSelectedId() - 1);
        preset_.setSelectedId(0, juce::dontSendNotification);
      }
    };
    for (auto* component : std::array<juce::Component*, 20>{
             &memory_,    &capture_,  &sync_,        &reverse_,
             &overdub_,   &feedback_, &lengthBeats_, &freeLength_,
             &start_,     &speed_,    &pitch_,       &splice_,
             &wow_,       &flutter_,  &drift_,       &degradation_,
             &amplifier_, &mix_,      &output_,      &tapeSpeed_})
      addAndMakeVisible(component);
    addAndMakeVisible(preset_);
    setResizable(true, true);
    setResizeLimits(920, 540, 1840, 1080);
    setSize(1160, 650);
  }
  ~LoopEditor() override { setLookAndFeel(nullptr); }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{kSurface});
    auto header = getLocalBounds().reduced(24).removeFromTop(50);
    g.setColour(juce::Colour{kInk});
    g.setFont(juce::FontOptions{22.0F, juce::Font::bold});
    g.drawText("LOOP", header.removeFromLeft(180),
               juce::Justification::centredLeft);
    g.setColour(juce::Colour{kAccent});
    g.setFont(juce::FontOptions{12.0F, juce::Font::bold});
    header.removeFromRight(210);
    g.drawText("L-01 / PLAYABLE MEMORY", header,
               juce::Justification::centredRight);
    g.setColour(juce::Colour{kMuted}.withAlpha(0.35F));
    g.fillRect(24, 72, getWidth() - 48, 1);
  }
  void resized() override {
    preset_.setBounds(getWidth() - 204, 27, 180, 28);
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(66);
    memory_.setBounds(area.removeFromLeft(310));
    area.removeFromLeft(14);
    auto switches = area.removeFromTop(36);
    capture_.setBounds(switches.removeFromLeft(120));
    sync_.setBounds(switches.removeFromLeft(130));
    reverse_.setBounds(switches.removeFromLeft(110));
    tapeSpeed_.setBounds(switches.removeFromLeft(140).reduced(3, 4));
    area.removeFromTop(8);
    constexpr int columns = 5;
    constexpr int rows = 3;
    const int width = area.getWidth() / columns;
    const int height = area.getHeight() / rows;
    const std::array<Knob*, 15> controls{
        &overdub_, &feedback_,    &lengthBeats_, &freeLength_, &start_,
        &speed_,   &pitch_,       &splice_,      &wow_,        &flutter_,
        &drift_,   &degradation_, &amplifier_,   &mix_,        &output_};
    for (std::size_t index = 0; index < controls.size(); ++index) {
      controls[index]->setBounds(
          area.getX() + static_cast<int>(index % columns) * width,
          area.getY() + static_cast<int>(index / columns) * height, width,
          height);
    }
  }

 private:
  LoopLookAndFeel lookAndFeel_;
  LoopAudioProcessor& processor_;
  MemoryPanel memory_;
  Toggle capture_, sync_, reverse_;
  Knob overdub_, feedback_, lengthBeats_, freeLength_, start_, speed_, pitch_;
  Knob splice_, wow_, flutter_, drift_, degradation_, amplifier_, mix_, output_;
  juce::ComboBox tapeSpeed_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      tapeSpeedAttachment_;
  juce::ComboBox preset_;
};

}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout
LoopAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"capture", 1}, "Capture", false));
  addFloat(layout, "overdub", "Overdub", {0, 100, 0.01F}, 50, "%");
  addFloat(layout, "feedback", "Feedback", {0, 100, 0.01F}, 85, "%");
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"sync", 1}, "Host Sync", true));
  addFloat(layout, "length_beats", "Length Beats", {0.25F, 16, 0.25F}, 4,
           "beats");
  addFloat(layout, "free_length", "Free Length", skewed(0.05F, 16, 2, 0.01F), 2,
           "s");
  addFloat(layout, "start", "Start", {0, 100, 0.01F}, 0, "%");
  addFloat(layout, "speed", "Speed", skewed(0.125F, 4, 1, 0.001F), 1, "x");
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"reverse", 1}, "Reverse", false));
  addFloat(layout, "pitch", "Pitch", {-12, 12, 0.01F}, 0, "st");
  addFloat(layout, "splice", "Splice", {0, 25, 0.01F}, 3, "%");
  addFloat(layout, "wow", "Wow", {0, 100, 0.01F}, 8, "%");
  addFloat(layout, "flutter", "Flutter", {0, 100, 0.01F}, 3, "%");
  addFloat(layout, "drift", "Drift", {0, 100, 0.01F}, 2, "%");
  addFloat(layout, "degradation", "Loss", {0, 100, 0.01F}, 8, "%");
  addFloat(layout, "amplifier", "Record", {0, 100, 0.01F}, 25, "%");
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"tape_speed", 1}, "Tape Speed",
      juce::StringArray{"3 3/4 IPS", "7 1/2 IPS", "15 IPS"}, 1));
  addFloat(layout, "mix", "Mix", {0, 100, 0.01F}, 100, "%");
  addFloat(layout, "output", "Output", {-24, 12, 0.01F}, -3, "dB");
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"bypass", 1}, "Bypass", false));
  return layout;
}

LoopAudioProcessor::LoopAudioProcessor()
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

Parameters LoopAudioProcessor::currentParameters() const noexcept {
  const auto value = [this](ParameterIndex index) {
    return parameterValues_[index]->load(std::memory_order_relaxed);
  };
  double bpm = 120.0;
  if (const auto* playHead = getPlayHead()) {
    if (const auto position = playHead->getPosition())
      bpm = position->getBpm().orFallback(120.0);
  }
  const bool hostSync = value(sync) >= 0.5F;
  const float seconds = hostSync
                            ? static_cast<float>(60.0 * value(lengthBeats) /
                                                 std::clamp(bpm, 20.0, 400.0))
                            : value(freeLength);
  return {.capture = value(capture) >= 0.5F || midiCapture_,
          .overdub = value(overdub) * 0.01F,
          .feedback = value(feedback) * 0.01F,
          .loopLengthSeconds = seconds,
          .start = value(start) * 0.01F,
          .speed = value(speed),
          .reverse = value(reverse) >= 0.5F,
          .pitchSemitones = value(pitch),
          .splice = value(splice) * 0.01F,
          .wow = value(wow) * 0.01F,
          .flutter = value(flutter) * 0.01F,
          .drift = value(drift) * 0.01F,
          .degradation = value(degradation) * 0.01F,
          .amplifier = value(amplifier) * 0.01F,
          .tapeSpeed = std::array{0.5F, 1.0F, 2.0F}[static_cast<std::size_t>(
              juce::jlimit(0, 2, static_cast<int>(value(tapeSpeed))))],
          .mix = value(mix) * 0.01F,
          .outputDb = value(output),
          .bypass = value(bypass) >= 0.5F};
}

void LoopAudioProcessor::prepareToPlay(double sampleRate, int) {
  processor_.prepare(sampleRate, 16.0);
  midiCapture_ = false;
  setLatencySamples(0);
  publishMeters({});
}
void LoopAudioProcessor::releaseResources() { midiCapture_ = false; }

bool LoopAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
  const auto input = layouts.getMainInputChannelSet();
  return input == layouts.getMainOutputChannelSet() &&
         (input == juce::AudioChannelSet::mono() ||
          input == juce::AudioChannelSet::stereo());
}

void LoopAudioProcessor::processRange(juce::AudioBuffer<float>& buffer,
                                      int startSample, int frames,
                                      Parameters parameters) noexcept {
  if (frames <= 0) return;
  processor_.process(buffer.getWritePointer(0, startSample),
                     buffer.getNumChannels() > 1
                         ? buffer.getWritePointer(1, startSample)
                         : nullptr,
                     static_cast<std::size_t>(frames), parameters);
}

void LoopAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  if (clearRequested_.exchange(false, std::memory_order_acq_rel)) {
    processor_.discard();
  }
  const int navigation =
      generationNavigation_.exchange(0, std::memory_order_acq_rel);
  if (navigation < 0) processor_.previousGeneration();
  if (navigation > 0) processor_.nextGeneration();
  if (buffer.getNumChannels() < 1 || buffer.getNumSamples() == 0) {
    publishMeters({});
    return;
  }
  Parameters parameters = currentParameters();
  parameters.reloop =
      reloopRequested_.exchange(false, std::memory_order_acq_rel);
  int processed{};
  for (const auto metadata : midi) {
    const int position =
        juce::jlimit(0, buffer.getNumSamples(), metadata.samplePosition);
    processRange(buffer, processed, position - processed, parameters);
    if (position > processed) parameters.reloop = false;
    processed = position;
    const auto& message = metadata.getMessage();
    const int pitchClass =
        message.isNoteOnOrOff() ? message.getNoteNumber() % 12 : -1;
    if (message.isNoteOn() && pitchClass == 0) midiCapture_ = true;
    if (message.isNoteOff() && pitchClass == 0) midiCapture_ = false;
    if (message.isNoteOn() && pitchClass == 2) parameters.reloop = true;
    if (message.isNoteOn() && pitchClass == 11) processor_.previousGeneration();
    if (message.isNoteOn() && pitchClass == 1) processor_.nextGeneration();
    parameters.capture =
        parameterValues_[capture]->load(std::memory_order_relaxed) >= 0.5F ||
        midiCapture_;
  }
  processRange(buffer, processed, buffer.getNumSamples() - processed,
               parameters);
  publishMeters(processor_.meters());
}

void LoopAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer&) {
  float peak{};
  for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    peak =
        std::max(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
  publishMeters({peak, peak, loopPosition(), capturedAmount()});
}

void LoopAudioProcessor::publishMeters(const MeterSnapshot& meters) noexcept {
  inputPeak_.store(meters.inputPeak, std::memory_order_relaxed);
  outputPeak_.store(meters.outputPeak, std::memory_order_relaxed);
  loopPosition_.store(meters.position, std::memory_order_relaxed);
  capturedAmount_.store(meters.captured, std::memory_order_relaxed);
  printingProgress_.store(meters.printing, std::memory_order_relaxed);
  generation_.store(static_cast<int>(meters.generation),
                    std::memory_order_relaxed);
  retainedGenerations_.store(static_cast<int>(meters.retainedGenerations),
                             std::memory_order_relaxed);
  activeDeck_.store(static_cast<int>(meters.activeDeck),
                    std::memory_order_relaxed);
}
float LoopAudioProcessor::inputPeak() const noexcept {
  return inputPeak_.load(std::memory_order_relaxed);
}
float LoopAudioProcessor::outputPeak() const noexcept {
  return outputPeak_.load(std::memory_order_relaxed);
}
float LoopAudioProcessor::loopPosition() const noexcept {
  return loopPosition_.load(std::memory_order_relaxed);
}
float LoopAudioProcessor::capturedAmount() const noexcept {
  return capturedAmount_.load(std::memory_order_relaxed);
}
float LoopAudioProcessor::printingProgress() const noexcept {
  return printingProgress_.load(std::memory_order_relaxed);
}
int LoopAudioProcessor::generation() const noexcept {
  return generation_.load(std::memory_order_relaxed);
}
int LoopAudioProcessor::retainedGenerations() const noexcept {
  return retainedGenerations_.load(std::memory_order_relaxed);
}
int LoopAudioProcessor::activeDeck() const noexcept {
  return activeDeck_.load(std::memory_order_relaxed);
}
void LoopAudioProcessor::clearLoop() noexcept {
  clearRequested_.store(true, std::memory_order_release);
}
void LoopAudioProcessor::reloop() noexcept {
  reloopRequested_.store(true, std::memory_order_release);
}
void LoopAudioProcessor::previousGeneration() noexcept {
  generationNavigation_.store(-1, std::memory_order_release);
}
void LoopAudioProcessor::nextGeneration() noexcept {
  generationNavigation_.store(1, std::memory_order_release);
}

int LoopAudioProcessor::factoryPresetCount() noexcept {
  return static_cast<int>(kPresets.size());
}
juce::String LoopAudioProcessor::factoryPresetName(int index) {
  return juce::isPositiveAndBelow(index, factoryPresetCount())
             ? kPresets[static_cast<std::size_t>(index)].name
             : juce::String{};
}
void LoopAudioProcessor::loadFactoryPreset(int index) {
  if (!juce::isPositiveAndBelow(index, factoryPresetCount())) return;
  const auto& preset = kPresets[static_cast<std::size_t>(index)];
  for (std::size_t parameter = 0; parameter < preset.values.size(); ++parameter)
    if (auto* target = state_.getParameter(kParameterIds[parameter]))
      target->setValueNotifyingHost(
          target->convertTo0to1(preset.values[parameter]));
}

juce::AudioProcessorParameter* LoopAudioProcessor::getBypassParameter() const {
  return state_.getParameter("bypass");
}

void LoopAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
  auto state = state_.copyState();
  state.setProperty("schema", kStateSchema, nullptr);
  state.setProperty("product", kStateType, nullptr);
  if (const auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void LoopAudioProcessor::setStateInformation(const void* data, int size) {
  const auto xml = getXmlFromBinary(data, size);
  if (xml == nullptr) return;
  const auto restored = juce::ValueTree::fromXml(*xml);
  const int schema = static_cast<int>(restored.getProperty("schema", -1));
  if (!restored.isValid() || restored.getType().toString() != kStateType ||
      restored.getProperty("product").toString() != kStateType ||
      (schema != 1 && schema != kStateSchema))
    return;
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
    if (parameter == nullptr) continue;
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

juce::AudioProcessorEditor* LoopAudioProcessor::createEditor() {
  return new LoopEditor{*this};
}

}  // namespace aste::loop::plugin

#if !defined(ASTE_NO_PLUGIN_FACTORY)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::loop::plugin::LoopAudioProcessor{};
}
#endif
