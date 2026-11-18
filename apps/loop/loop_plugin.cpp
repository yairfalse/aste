#include "loop_plugin.hpp"

#include "decimal_parse.hpp"
#include "plugin_ui.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>

namespace aste::loop::plugin {
namespace {

constexpr auto kStateType = "loop-l01";
constexpr int kStateSchema = 4;
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

constexpr std::array<Preset, 7> kPresets{{
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
    {"First take",
     {0, 0, 85, 1, 4, 2, 0, 1, 0, 0, 3, 8, 3, 2, 8, 25, 1, 100, -3, 0}},
    {"Evolving material",
     {0, 0, 90, 1, 4, 2, 0, 1, 0, 0, 3, 12, 4, 3, 28, 48, 1, 100, -6, 0}},
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
    slider_.textFromValueFunction = [this](double value) {
      return aste::ui::formattedValue(slider_, value);
    };
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 78, 20);
    slider_.setTextValueSuffix(suffix);
    slider_.setDoubleClickReturnValue(true, initial);
    slider_.setTitle(name);
    slider_.setDescription(
        aste::ui::explanation(id, state.processor.getName()));
    slider_.setTooltip(
        aste::ui::explanation(id, state.processor.getName()) +
        " Shift-drag for fine adjustment; type an exact value below.");
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
  aste::ui::PrecisionSlider slider_;
  juce::Label label_;
  juce::AudioProcessorValueTreeState::SliderAttachment attachment_;
};

class Toggle final : public juce::TextButton {
 public:
  Toggle(juce::AudioProcessorValueTreeState& state, const char* id,
         const char* name, int order)
      : juce::TextButton{name}, attachment_{state, id, *this} {
    setClickingTogglesState(true);
    setTitle(name);
    setWantsKeyboardFocus(true);
    setExplicitFocusOrder(order);
    setColour(juce::TextButton::textColourOffId, juce::Colour{kInk});
    setColour(juce::TextButton::buttonColourId, juce::Colour{kPanel});
    setColour(juce::TextButton::buttonOnColourId, juce::Colour{kAccent});
  }

 private:
  juce::AudioProcessorValueTreeState::ButtonAttachment attachment_;
};

#include "loop_waveform.hpp"

class MemoryPanel final : public juce::Component, private juce::Timer {
 public:
  explicit MemoryPanel(LoopAudioProcessor& processor)
      : processor_{processor}, waveform_{processor}, repeat_{processor} {
    addAndMakeVisible(waveform_);
    addAndMakeVisible(repeat_);
    undo_.setButtonText("UNDO");
    redo_.setButtonText("REDO");
    undo_.onClick = [this] { processor_.undoEdit(); };
    redo_.onClick = [this] { processor_.undoEdit(true); };
    addAndMakeVisible(undo_);
    addAndMakeVisible(redo_);
    latch_.setButtonText("LATCH");
    latch_.setTitle("LATCH REPEAT");
    latch_.onClick = [this] {
      auto* p = processor_.state().getParameter("repeat");
      p->beginChangeGesture();
      p->setValueNotifyingHost(p->getValue() < .5F ? 1.F : 0.F);
      p->endChangeGesture();
    };
    addAndMakeVisible(latch_);
    for (std::size_t i = 0; i < divisions_.size(); ++i) {
      auto& button = divisions_[i];
      button.setButtonText(std::array{"FREE", "1/4", "1/8", "1/16", "1/32"}[i]);
      button.setTitle("REPEAT " + button.getButtonText());
      button.onClick = [this, i] {
        auto* p = processor_.state().getParameter("repeat_division");
        p->beginChangeGesture();
        p->setValueNotifyingHost(p->convertTo0to1(static_cast<float>(i)));
        p->endChangeGesture();
      };
      addAndMakeVisible(button);
    }
    for (std::size_t i = 0; i < takes_.size(); ++i) {
      takes_[i].onClick = [this, i] { processor_.selectTake(takeIds_[i]); };
      takes_[i].setTooltip(
          "Listen to this recorded generation without applying the previous "
          "transforms twice. Undo restores your edits.");
      remove_[i].setButtonText("x");
      remove_[i].setTitle("REMOVE GENERATION " +
                          juce::String(static_cast<int>(i + 1)));
      remove_[i].setTooltip(
          "Remove this inactive take to free a slot. Undo recovers its audio.");
      remove_[i].onClick = [this, i] { processor_.removeTake(takeIds_[i]); };
      addAndMakeVisible(takes_[i]);
      addAndMakeVisible(remove_[i]);
    }
    reloop_.setButtonText("RELOOP / PRINT NEW TAKE");
    reloop_.setTitle("RELOOP");
    reloop_.setColour(juce::TextButton::buttonColourId, juce::Colour{kAccent});
    reloop_.setColour(juce::TextButton::textColourOffId,
                      juce::Colour{kSurface});
    reloop_.onClick = [this] { processor_.reloop(); };
    reloop_.setTooltip(
        "Print the transformed loop onto the next tape. Capture audio first.");
    previous_.setButtonText("OLDER TAKE");
    previous_.setTitle("PREVIOUS GENERATION");
    previous_.onClick = [this] {
      if (processor_.state().getRawParameterValue("safe_takes")->load() < .5F) {
        processor_.previousGeneration();
        return;
      }
      for (auto i = takeIds_.rbegin(); i != takeIds_.rend(); ++i)
        if (*i > 0 && *i < processor_.generation()) {
          processor_.selectTake(*i);
          break;
        }
    };
    next_.setButtonText("NEWER TAKE");
    next_.setTitle("NEXT GENERATION");
    next_.onClick = [this] {
      if (processor_.state().getRawParameterValue("safe_takes")->load() < .5F) {
        processor_.nextGeneration();
        return;
      }
      for (int id : takeIds_)
        if (id > processor_.generation()) {
          processor_.selectTake(id);
          break;
        }
    };
    clear_.setButtonText("CLEAR MEMORY");
    clear_.setTitle("CLEAR MEMORY");
    clear_.setColour(juce::TextButton::buttonColourId, juce::Colour{kPanel});
    clear_.setColour(juce::TextButton::textColourOffId, juce::Colour{kInk});
    clear_.onClick = [this] {
      if (auto* capture = processor_.state().getParameter("capture")) {
        capture->beginChangeGesture();
        capture->setValueNotifyingHost(0);
        capture->endChangeGesture();
      }
      processor_.clearWithUndo();
    };
    for (auto* button : {&previous_, &next_}) {
      button->setColour(juce::TextButton::buttonColourId,
                        juce::Colour{kSurface});
      button->setColour(juce::TextButton::textColourOffId, juce::Colour{kInk});
    }
    addAndMakeVisible(reloop_);
    addAndMakeVisible(previous_);
    addAndMakeVisible(next_);
    addAndMakeVisible(clear_);
    const std::array<const char*, 3> names{"PLAY", "STOP", "RELEASE"};
    const std::array<const char*, 3> hints{
        "Resume retained tape from its paused position.",
        "Pause playback and recording without erasing the tape.",
        "Stop recording and printing; let repeats decay. Feedback sets the "
        "tail length."};
    for (std::size_t i = 0; i < transport_.size(); ++i) {
      auto& button = transport_[i];
      button.setButtonText(names[i]);
      button.setTitle(names[i]);
      button.setTooltip(hints[i]);
      button.setColour(juce::TextButton::buttonColourId,
                       juce::Colour{kSurface});
      button.setColour(juce::TextButton::buttonOnColourId,
                       juce::Colour{kAccent});
      button.setColour(juce::TextButton::textColourOffId, juce::Colour{kInk});
      button.setWantsKeyboardFocus(true);
      button.onClick = [this, i] {
        processor_.setTapeTransport(static_cast<Transport>(i));
      };
      addAndMakeVisible(button);
    }
    timerCallback();
    startTimerHz(30);
  }
  void resized() override {
    waveform_.setBounds(12, 32, getWidth() - 344, 142);
    const int x = getWidth() - 320;
    const int width = 96;
    for (int i = 0; i < 3; ++i)
      transport_[static_cast<std::size_t>(i)].setBounds(x + i * (width + 6), 36,
                                                        width, 28);
    reloop_.setBounds(x, 72, 300, 34);
    previous_.setBounds(x, 114, 147, 26);
    next_.setBounds(x + 153, 114, 147, 26);
    clear_.setBounds(x, 148, 144, 26);
    undo_.setBounds(x + 150, 148, 72, 26);
    redo_.setBounds(x + 228, 148, 72, 26);
    const int takeWidth = (getWidth() - 36) / 3;
    for (int i = 0; i < 3; ++i) {
      takes_[static_cast<std::size_t>(i)].setBounds(12 + i * (takeWidth + 6),
                                                    196, takeWidth - 32, 32);
      remove_[static_cast<std::size_t>(i)].setBounds(
          12 + i * (takeWidth + 6) + takeWidth - 30, 196, 30, 32);
    }
    for (int i = 0; i < 5; ++i)
      divisions_[static_cast<std::size_t>(i)].setBounds(12 + i * 68, 250, 62,
                                                        30);
    repeat_.setBounds(356, 250, 180, 30);
    latch_.setBounds(542, 250, 80, 30);
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{kPanel});
    auto area = getLocalBounds().toFloat().reduced(12);
    g.setColour(juce::Colour{kMuted});
    g.setFont(juce::FontOptions{11.0F, juce::Font::bold});
    g.drawText("CAPTURE / TRANSFORM / RELOOP", area.removeFromTop(20),
               juce::Justification::centredLeft);
    area = {static_cast<float>(getWidth() - 320), 12, 300, 20};
    g.setColour(juce::Colour{kInk});
    g.setFont(juce::FontOptions{14.0F, juce::Font::bold});
    g.drawText(processor_.isBypassed() ? "BYPASSED / MEMORY RETAINED"
               : processor_.tapeTransport() == Transport::Stop
                   ? "STOPPED / TAPE RETAINED"
               : processor_.tapeTransport() == Transport::Release
                   ? "RELEASING / TAPE RETAINED"
               : processor_.isPrinting()
                   ? "RELOOP " + juce::String(printing_ * 100, 0) + "%"
               : processor_.isCapturing()
                   ? "CAPTURING " + juce::String(captured_ * 100, 0) + "%"
               : generation_ > 0 ? "PLAYING / GEN " + juce::String{generation_}
                                 : "CAPTURE AUDIO TO BEGIN",
               area.removeFromTop(28), juce::Justification::centred);
    g.setColour(juce::Colour{kMuted});
    g.setFont(juce::FontOptions{10.0F});
    g.drawText(
        processor_.historyFull()
            ? "HISTORY 3/3 / REMOVE AN INACTIVE TAKE TO FREE A SLOT (UNDOABLE)"
            : "HISTORY " + juce::String(retained_) +
                  "/3 / SELECT A TAKE TO LISTEN / EDIT",
        12, 176, getWidth() - 24, 18, juce::Justification::centredLeft);
    g.drawText("PERFORM / CONTINUING TIMELINE", 12, 230, 400, 18,
               juce::Justification::centredLeft);
    auto levels = juce::Rectangle<float>{static_cast<float>(getWidth() - 270),
                                         244, 250, 40};
    drawLevel(g, levels.removeFromTop(18), input_, "IN");
    drawLevel(g, levels.removeFromTop(18), output_, "OUT");
  }

 private:
  static void drawLevel(juce::Graphics& g, juce::Rectangle<float> area,
                        float level, const char* label) {
    const auto text = area.removeFromLeft(104);
    g.setColour(juce::Colour{kMuted});
    g.drawText(juce::String{label} + " " + aste::ui::levelText(level), text,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour{kSurface});
    g.fillRect(area);
    g.setColour(juce::Colour{kInk});
    g.fillRect(
        area.withWidth(area.getWidth() * aste::ui::meterPosition(level)));
  }
  void timerCallback() override {
    const int latest = processor_.newestGeneration();
    if (latest != latest_ || processor_.generation() != generation_ ||
        processor_.retainedGenerations() != retained_ ||
        processor_.isCapturing() != wasRecording_ ||
        (processor_.isCapturing() && (++refresh_ % 6) == 0)) {
      auto tape = processor_.tapeSnapshot();
      waveform_.update(tape);
      std::size_t index{};
      for (auto generation : tape.generations)
        if (generation) takeIds_[index++] = static_cast<int>(generation);
      std::sort(takeIds_.begin(),
                takeIds_.begin() + static_cast<std::ptrdiff_t>(index));
      while (index < takeIds_.size()) takeIds_[index++] = 0;
      latest_ = latest;
    }
    wasRecording_ = processor_.isCapturing();
    input_ = std::max(processor_.inputPeak(), input_ * 0.84F);
    output_ = std::max(processor_.outputPeak(), output_ * 0.84F);
    position_ = processor_.loopPosition();
    captured_ = processor_.capturedAmount();
    printing_ = processor_.printingProgress();
    generation_ = processor_.generation();
    retained_ = processor_.retainedGenerations();
    activeDeck_ = processor_.activeDeck();
    const bool available = !processor_.isPrinting() &&
                           !processor_.isCapturing() &&
                           !processor_.isBypassed();
    const bool validRegion =
        processor_.state().getRawParameterValue("safe_takes")->load() < .5F ||
        processor_.state().getRawParameterValue("region_end")->load() >
            processor_.state().getRawParameterValue("start")->load();
    reloop_.setButtonText(validRegion ? "RELOOP / PRINT NEW TAKE"
                                      : "END MUST FOLLOW START");
    reloop_.setEnabled(validRegion && captured_ > 0 && available &&
                       !processor_.historyFull() &&
                       processor_.tapeTransport() == Transport::Play);
    for (std::size_t i = 0; i < transport_.size(); ++i)
      transport_[i].setToggleState(
          processor_.tapeTransport() == static_cast<Transport>(i),
          juce::dontSendNotification);
    reloop_.setDescription(
        captured_ <= 0 ? "Capture audio first. Printing makes a new take from "
                         "the transformed loop."
        : !available ? "Wait for printing to finish and turn bypass off before "
                       "printing another take."
                     : "Print the transformed loop onto a new take, then "
                       "compare it with Older / Newer Take.");
    previous_.setEnabled(generation_ > processor_.oldestGeneration() &&
                         available);
    next_.setEnabled(generation_ < processor_.newestGeneration() && available);
    clear_.setEnabled(generation_ > 0 && available);
    undo_.setEnabled(available && processor_.undoManager().canUndo());
    redo_.setEnabled(available && processor_.undoManager().canRedo());
    for (std::size_t i = 0; i < takes_.size(); ++i) {
      takes_[i].setButtonText(
          takeIds_[i]
              ? "GEN " + juce::String(takeIds_[i]) +
                    (takeIds_[i] == generation_ ? " / ACTIVE" : " / LISTEN")
              : "EMPTY SLOT");
      takes_[i].setTitle("GENERATION " + juce::String(static_cast<int>(i + 1)));
      takes_[i].setEnabled(takeIds_[i] > 0 && available);
      takes_[i].setToggleState(takeIds_[i] == generation_ && generation_ > 0,
                               juce::dontSendNotification);
      remove_[i].setEnabled(takeIds_[i] > 0 && takeIds_[i] != generation_ &&
                            available);
    }
    const bool safe =
        processor_.state().getRawParameterValue("safe_takes")->load() >= .5F;
    repeat_.setEnabled(safe && generation_ > 0);
    latch_.setEnabled(safe && generation_ > 0);
    latch_.setToggleState(
        processor_.state().getRawParameterValue("repeat")->load() >= .5F,
        juce::dontSendNotification);
    for (std::size_t i = 0; i < divisions_.size(); ++i) {
      divisions_[i].setEnabled(safe);
      divisions_[i].setToggleState(
          static_cast<int>(processor_.state()
                               .getRawParameterValue("repeat_division")
                               ->load()) == static_cast<int>(i),
          juce::dontSendNotification);
    }
    waveform_.setEnabled(safe);
    waveform_.repaint();
    repaint();
  }
  LoopAudioProcessor& processor_;
  TapeWaveform waveform_;
  MomentaryRepeat repeat_;
  std::array<juce::TextButton, 3> takes_, remove_;
  std::array<int, 3> takeIds_{};
  std::array<juce::TextButton, 5> divisions_;
  juce::TextButton undo_, redo_, latch_;
  int latest_{-1}, refresh_{};
  bool wasRecording_{};
  juce::TextButton reloop_, previous_, next_, clear_;
  std::array<juce::TextButton, 3> transport_;
  float input_{};
  float output_{};
  float position_{};
  float captured_{};
  float printing_{};
  int generation_{};
  int retained_{};
  int activeDeck_{};
};

class LoopEditor final : public juce::AudioProcessorEditor,
                         private juce::Timer {
 public:
  explicit LoopEditor(LoopAudioProcessor& processor)
      : AudioProcessorEditor{processor},
        processor_{processor},
        master_{processor.state()},
        memory_{processor},
        capture_{processor.state(), "capture", "CAPTURE", 1},
        safe_{processor.state(), "safe_takes", "SAFE TAKES", 2},
        monitor_{processor.state(), "monitor", "MONITOR INPUT", 3},
        sync_{processor.state(), "sync", "HOST SYNC", 2},
        reverse_{processor.state(), "reverse", "REVERSE", 3},
        overdub_{processor.state(), "overdub", "OVERDUB", " %", 50, 4},
        feedback_{processor.state(), "feedback", "FEEDBACK", " %", 85, 5},
        lengthBeats_{processor.state(), "length_beats", "BEATS", "", 4, 6},
        freeLength_{
            processor.state(), "free_length", "FREE LENGTH", " s", 2, 7},
        start_{processor.state(), "start", "START", " %", 0, 8},
        end_{processor.state(), "region_end", "END", " %", 100, 9},
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
    addAndMakeVisible(master_);
    workflow_.bind(
        *this, "CAPTURE > TRANSFORM > RELOOP > LISTEN TO THE NEW TAKE > REPEAT",
        "Safe Takes records once. Drag a waveform region and hold Repeat. "
        "RELOOP prints the wet tape, before monitoring and output gain. "
        "Click a generation to listen without double-processing. Three slots; "
        "removal is undoable.");
    addAndMakeVisible(workflow_);
    capture_.setButtonText("CAPTURE INPUT");
    capture_.onClick = [this] {
      captureRequestGeneration_ = processor_.generation();
    };
    capture_.setColour(juce::TextButton::buttonColourId, juce::Colour{kAccent});
    capture_.setTooltip(
        "Safe Takes: record one new take for the selected duration; click "
        "again to finish early. Legacy: continuous overdub. MIDI C holds "
        "Capture.");
    safe_.setTooltip(
        "On: one-shot capture, protected history and region performance. Off: "
        "original continuous overdub and recycling three-deck behavior for "
        "older sessions.");
    monitor_.setTooltip(
        "Add live input to tape playback. Monitoring and output gain are never "
        "printed by RELOOP.");
    sync_.setTooltip(
        "On: Beats determines duration. Off: Free Length determines duration.");
    reverse_.setTooltip("Play the captured audio backwards.");
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
    for (const auto& snapshot : kPresets)
      preset_.addSnapshot(processor_, processor_.state(), [&](auto set) {
        for (std::size_t i = 0; i < snapshot.values.size(); ++i)
          set(kParameterIds[i], snapshot.values[i]);
        set("safe_takes", snapshot.name == kPresets.back().name ? 1.F : 0.F);
      });
    preset_.watch(processor_);
    response_.bind([this] { return !processor_.usesLegacyBehavior(); },
                   [this] { processor_.useCurrentResponse(); },
                   "Use continuous tape motion without clearing or reprinting "
                   "any recorded tape.");
    addAndMakeVisible(response_);
    preset_.onChange = [this] {
      if (preset_.getSelectedId() > 0) {
        processor_.loadFactoryPreset(preset_.getSelectedId() - 1);
        preset_.remember(processor_);
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
    addAndMakeVisible(safe_);
    addAndMakeVisible(monitor_);
    addAndMakeVisible(end_);
    setResizable(true, true);
    setResizeLimits(1000, 780, 1840, 1440);
    setSize(1160, 820);
    timerCallback();
    startTimerHz(15);
  }
  ~LoopEditor() override { setLookAndFeel(nullptr); }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{kSurface});
    auto header = juce::Rectangle<int>{24, 24, 180, 28};
    g.setColour(juce::Colour{kInk});
    g.setFont(juce::FontOptions{22.0F, juce::Font::bold});
    g.drawText("LOOP", header.removeFromLeft(180),
               juce::Justification::centredLeft);
    g.setColour(juce::Colour{kAccent});
    g.setFont(juce::FontOptions{12.0F, juce::Font::bold});
    header = juce::Rectangle<int>{24, 53, 310, 16};
    g.drawText("L-01 / PLAYABLE MEMORY", header,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour{kMuted}.withAlpha(0.35F));
    g.fillRect(24, 72, getWidth() - 48, 1);
    g.setColour(juce::Colour{kInk});
    g.setFont(juce::FontOptions{11.0F});
  }
  void resized() override {
    workflow_.setBounds(24, getHeight() - 80, getWidth() - 48, 48);
    preset_.setBounds(getWidth() - 414, 28, 180, 28);
    master_.setBounds(getWidth() - 214, 18, 190, 46);
    response_.setBounds(getWidth() - 224, getHeight() - 30, 200, 24);
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(66);
    area.removeFromBottom(60);
    memory_.setBounds(area.removeFromTop(292));
    area.removeFromTop(8);
    auto switches = area.removeFromTop(36);
    const int switchWidth = switches.getWidth() / 6;
    capture_.setBounds(switches.removeFromLeft(switchWidth));
    sync_.setBounds(switches.removeFromLeft(switchWidth));
    reverse_.setBounds(switches.removeFromLeft(switchWidth));
    safe_.setBounds(switches.removeFromLeft(switchWidth));
    monitor_.setBounds(switches.removeFromLeft(switchWidth));
    tapeSpeed_.setBounds(switches.reduced(3, 4));
    area.removeFromTop(8);
    constexpr int columns = 8;
    constexpr int rows = 2;
    const int width = area.getWidth() / columns;
    const int height = area.getHeight() / rows;
    const std::array<Knob*, 16> controls{
        &start_,       &end_,        &speed_,   &pitch_,
        &degradation_, &amplifier_,  &splice_,  &output_,
        &lengthBeats_, &freeLength_, &wow_,     &flutter_,
        &drift_,       &feedback_,   &overdub_, &mix_};
    for (std::size_t index = 0; index < controls.size(); ++index) {
      controls[index]->setBounds(
          area.getX() + static_cast<int>(index % columns) * width,
          area.getY() + static_cast<int>(index / columns) * height, width,
          height);
    }
  }

 private:
  void timerCallback() override {
    const bool safe =
        processor_.state().getRawParameterValue("safe_takes")->load() >= .5F;
    overdub_.setEnabled(!safe);
    end_.setEnabled(safe);
    // Only the message thread acknowledges one-shot completion to the host.
    // A high capture parameter is ignored by DSP until its next rising edge.
    if (safe && capture_.getToggleState() && !processor_.isCapturing() &&
        processor_.tapeTransport() == Transport::Play &&
        (wasRecording_ ||
         processor_.generation() != captureRequestGeneration_)) {
      auto* p = processor_.state().getParameter("capture");
      p->beginChangeGesture();
      p->setValueNotifyingHost(0);
      p->endChangeGesture();
    }
    wasRecording_ = processor_.isCapturing();
    const bool synced =
        processor_.state().getRawParameterValue("sync")->load() >= 0.5F;
    lengthBeats_.setDescription(
        synced ? "Capture duration in beats at the DAW tempo."
               : "Inactive: Host Sync is off. Free Length currently sets "
                 "capture duration.");
    freeLength_.setDescription(
        synced ? "Inactive: Host Sync is on. Beats currently sets capture "
                 "duration."
               : "Capture duration in seconds, independently of DAW tempo.");
    lengthBeats_.setEnabled(synced);
    freeLength_.setEnabled(!synced);
    lengthBeats_.setAlpha(synced ? 1.0F : 0.35F);
    freeLength_.setAlpha(synced ? 0.35F : 1.0F);
    capture_.setButtonText(processor_.isCapturing() ? "FINISH CAPTURE"
                           : safe                   ? "CAPTURE NEW TAKE"
                                                    : "CAPTURE / OVERDUB");
    capture_.setEnabled(
        !processor_.isPrinting() &&
        (!safe || !processor_.historyFull() || processor_.isCapturing()));
  }
  LoopLookAndFeel lookAndFeel_;
  LoopAudioProcessor& processor_;
  aste::ui::MasterOutput master_;
  aste::ui::WorkflowGuide workflow_;
  MemoryPanel memory_;
  Toggle capture_, safe_, monitor_, sync_, reverse_;
  Knob overdub_, feedback_, lengthBeats_, freeLength_, start_, end_, speed_,
      pitch_;
  Knob splice_, wow_, flutter_, drift_, degradation_, amplifier_, mix_, output_;
  juce::ComboBox tapeSpeed_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      tapeSpeedAttachment_;
  aste::ui::PresetMenu preset_;
  aste::ui::ResponseButton response_;
  juce::TooltipWindow tooltips_{this, 400};
  bool wasRecording_{};
  int captureRequestGeneration_{};
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
  aste::ui::MasterGain::addParameter(layout, 0.F);
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"transport", 1}, "Tape Transport",
      juce::StringArray{"Play", "Stop", "Release"}, 0));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"safe_takes", 1}, "Safe Takes", false));
  addFloat(layout, "region_end", "Region End", {0, 100, .01F}, 100, "%");
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"repeat", 1}, "Repeat Region", false));
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"repeat_division", 1}, "Repeat Division",
      juce::StringArray{"FREE", "1/4", "1/8", "1/16", "1/32"}, 0));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"monitor", 1}, "Monitor Input", false));
  return layout;
}

LoopAudioProcessor::LoopAudioProcessor()
    : AudioProcessor{
          BusesProperties{}
              .withInput("Input", juce::AudioChannelSet::stereo(), true)
              .withOutput("Output", juce::AudioChannelSet::stereo(), true)},
      state_{*this, &undo_, kStateType, createParameterLayout()} {
  masterGain_.bind(state_);
  transportValue_ = state_.getRawParameterValue("transport");
  const std::array ids{"safe_takes", "region_end", "repeat", "repeat_division",
                       "monitor"};
  for (std::size_t i = 0; i < ids.size(); ++i)
    performanceValues_[i] = state_.getRawParameterValue(ids[i]);
  for (std::size_t index = 0; index < kParameterIds.size(); ++index) {
    parameterValues_[index] = state_.getRawParameterValue(kParameterIds[index]);
    jassert(parameterValues_[index] != nullptr);
  }
  loadFactoryPreset(6);
  state_.getParameter("safe_takes")->setValueNotifyingHost(1);
  undo_.clearUndoHistory();
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
  if (!std::isfinite(bpm) || bpm <= 0) bpm = 120;
  const bool hostSync = value(sync) >= 0.5F;
  const float seconds = hostSync
                            ? static_cast<float>(60.0 * value(lengthBeats) /
                                                 std::clamp(bpm, 20.0, 400.0))
                            : value(freeLength);
  return {.capture = !editingHistory_.load() &&
                     (value(capture) >= 0.5F || midiCapture_),
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
          .bypass = value(bypass) >= 0.5F,
          .legacyBehavior = engine_.load()->legacyBehavior,
          .transport = tapeTransport(),
          .safeWorkflow = performanceValues_[0]->load() >= .5F,
          .end = performanceValues_[1]->load() * .01F,
          .repeat = performanceValues_[2]->load() >= .5F,
          .repeatSeconds =
              performanceValues_[3]->load() < .5F
                  ? 0.F
                  : static_cast<float>(
                        60.0 / std::clamp(bpm, 20.0, 400.0) /
                        std::exp2(std::clamp(performanceValues_[3]->load(), 1.F,
                                             4.F) -
                                  1)),
          .monitor = performanceValues_[4]->load() >= .5F};
}

LoopAudioProcessor::~LoopAudioProcessor() {
  delete pending_.exchange(nullptr);
  delete engine_.exchange(nullptr);
  collectRetired();
}

void LoopAudioProcessor::collectRetired() {
  auto* next = retired_.exchange(nullptr);
  while (next) {
    auto* old = next;
    next = old->retiredNext;
    delete old;
  }
}

void LoopAudioProcessor::adoptPending() noexcept {
  if (auto* ready = pending_.exchange(nullptr)) {
    if (ready->preservePlayback)
      ready->processor.continuePlaybackFrom(engine_.load()->processor);
    auto* old = engine_.exchange(ready);
    auto* head = retired_.load();
    do {
      old->retiredNext = head;
    } while (!retired_.compare_exchange_weak(head, old));
    midiCapture_ = false;
  }
}

void LoopAudioProcessor::prepareToPlay(double sampleRate, int) {
  masterGain_.prepare(sampleRate);
  const std::lock_guard lock{lifecycle_};
  adoptPending();
  auto tape = engine_.load()->processor.tapeState();
  auto ready = std::make_unique<Engine>();
  ready->legacyBehavior = engine_.load()->legacyBehavior.load();
  hostRate_ = juce::jlimit(8000.0, 384000.0,
                           std::isfinite(sampleRate) ? sampleRate : 48000.0);
  hostPrepared_ = true;
  ready->processor.prepare(hostRate_, 16.0, true);
  ready->processor.restoreTape(tape);
  delete engine_.exchange(ready.release());
  collectRetired();
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
  engine_.load()->processor.process(
      buffer.getWritePointer(0, startSample),
      buffer.getNumChannels() > 1 ? buffer.getWritePointer(1, startSample)
                                  : nullptr,
      static_cast<std::size_t>(frames), parameters, false);
}

void LoopAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                      juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  adoptPending();
  if (clearRequested_.exchange(false, std::memory_order_acq_rel)) {
    engine_.load()->processor.discard();
    midiCapture_ = false;
  }
  const int navigation =
      generationNavigation_.exchange(0, std::memory_order_acq_rel);
  if (navigation < 0) engine_.load()->processor.previousGeneration();
  if (navigation > 0) engine_.load()->processor.nextGeneration();
  if (const int take = selectedTake_.exchange(0); take > 0)
    engine_.load()->processor.selectGeneration(
        static_cast<std::uint32_t>(take));
  if (buffer.getNumChannels() < 1 || buffer.getNumSamples() == 0) {
    publishMeters({});
    return;
  }
  Parameters parameters = currentParameters();
  bypassed_.store(parameters.bypass);
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
    if (message.isAllNotesOff() || message.isAllSoundOff())
      midiCapture_ = false;
    if (message.isNoteOn() && pitchClass == 2) parameters.reloop = true;
    if (message.isNoteOn() && pitchClass == 11)
      engine_.load()->processor.previousGeneration();
    if (message.isNoteOn() && pitchClass == 1)
      engine_.load()->processor.nextGeneration();
    parameters.capture =
        !editingHistory_.load() &&
        (parameterValues_[capture]->load(std::memory_order_relaxed) >= 0.5F ||
         midiCapture_);
  }
  processRange(buffer, processed, buffer.getNumSamples() - processed,
               parameters);
  captureActive_.store(engine_.load()->processor.meters().recording &&
                       parameters.transport == Transport::Play);
  engine_.load()->processor.publishTape();
  publishMeters(engine_.load()->processor.meters());
  outputPeak_.store(masterGain_.process(buffer), std::memory_order_relaxed);
}

void LoopAudioProcessor::processBlockBypassed(juce::AudioBuffer<float>& buffer,
                                              juce::MidiBuffer& midi) {
  adoptPending();
  bypassed_.store(true);
  if (clearRequested_.exchange(false)) {
    engine_.load()->processor.discard();
    midiCapture_ = false;
    publishMeters({});
  }
  for (const auto metadata : midi) {
    const auto message = metadata.getMessage();
    if (message.isAllNotesOff() || message.isAllSoundOff() ||
        (message.isNoteOff() && message.getNoteNumber() % 12 == 0))
      midiCapture_ = false;
  }
  captureActive_.store(false);
  float peak{};
  for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    peak =
        std::max(peak, buffer.getMagnitude(channel, 0, buffer.getNumSamples()));
  inputPeak_.store(peak);
  outputPeak_.store(peak);
}

void LoopAudioProcessor::publishMeters(const MeterSnapshot& meters) noexcept {
  printActive_.store(meters.busy);
  historyFull_.store(meters.historyFull);
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
  oldestGeneration_.store(static_cast<int>(meters.oldestGeneration));
  newestGeneration_.store(static_cast<int>(meters.newestGeneration));
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

void LoopAudioProcessor::setTapeTransport(Transport transport) {
  if (auto* parameter = state_.getParameter("transport")) {
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(
        parameter->convertTo0to1(static_cast<float>(transport)));
    parameter->endChangeGesture();
  }
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

TapeState LoopAudioProcessor::tapeSnapshot() const {
  const std::lock_guard lock{lifecycle_};
  auto* source = pending_.load();
  if (!source) source = engine_.load();
  return source->processor.tapeState();
}

bool LoopAudioProcessor::undoEdit(bool redo) {
  if (isCapturing() || isPrinting()) return false;
  (void)state_.copyState();
  editingHistory_.store(true);
  struct Release {
    std::atomic<bool>& flag;
    ~Release() { flag.store(false); }
  } release{editingHistory_};
  const bool changed = redo ? undo_.redo() : undo_.undo();
  // Capture is a command. An undo transaction may restore its previous
  // value, but must never create another take as a side effect. Set the
  // tree without an undo action so the redo chain remains intact.
  state_.state.getChildWithProperty("id", "capture")
      .setProperty("value", 0.F, nullptr);
  state_.getParameter("capture")->setValueNotifyingHost(0);
  return changed;
}

void LoopAudioProcessor::neutralTransforms() {
  for (const auto& [id, value] :
       std::array<std::pair<const char*, float>, 12>{{{"start", 0},
                                                      {"region_end", 100},
                                                      {"speed", 1},
                                                      {"reverse", 0},
                                                      {"pitch", 0},
                                                      {"wow", 0},
                                                      {"flutter", 0},
                                                      {"drift", 0},
                                                      {"degradation", 0},
                                                      {"amplifier", 0},
                                                      {"repeat", 0},
                                                      {"splice", 0}}}) {
    auto* p = state_.getParameter(id);
    p->beginChangeGesture();
    p->setValueNotifyingHost(p->convertTo0to1(value));
    p->endChangeGesture();
  }
}

void LoopAudioProcessor::selectTake(int generation, bool neutral) {
  if (generation <= 0 || isPrinting() || isCapturing()) return;
  (void)state_.copyState();
  undo_.beginNewTransaction("Listen to generation");
  if (neutral) neutralTransforms();
  (void)state_.copyState();
  class Selection final : public juce::UndoableAction {
   public:
    Selection(LoopAudioProcessor& owner, int before, int after)
        : owner_{owner}, before_{before}, after_{after} {}
    bool perform() override {
      owner_.selectedTake_.store(after_);
      return true;
    }
    bool undo() override {
      owner_.selectedTake_.store(before_);
      return true;
    }

   private:
    LoopAudioProcessor& owner_;
    int before_, after_;
  };
  undo_.perform(new Selection{*this, this->generation(), generation});
  updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}

namespace {
class TapeEdit final : public juce::UndoableAction {
 public:
  TapeEdit(LoopAudioProcessor& owner, juce::MemoryBlock before,
           juce::MemoryBlock after)
      : owner_{owner}, before_{std::move(before)}, after_{std::move(after)} {}
  bool perform() override { return owner_.restoreHistory(before_, after_); }
  bool undo() override { return owner_.restoreHistory(after_, before_); }
  int getSizeInUnits() override {
    return static_cast<int>((before_.getSize() + after_.getSize()) / 1024 + 1);
  }

 private:
  LoopAudioProcessor& owner_;
  juce::MemoryBlock before_, after_;
};
}  // namespace

void LoopAudioProcessor::removeTake(int generation) {
  if (generation == this->generation() || isCapturing() || isPrinting()) return;
  juce::MemoryBlock before, after;
  getStateInformation(before);
  auto xml =
      getXmlFromBinary(before.getData(), static_cast<int>(before.getSize()));
  if (!xml) return;
  auto* tapes = xml->getChildByName("TAPES");
  if (!tapes) return;
  for (auto* tape : tapes->getChildIterator())
    if (tape->getIntAttribute("generation") == generation) {
      tape->setAttribute("generation", 0);
      tape->setAttribute("frames", 0);
      tape->setAttribute("audio", "");
    }
  copyXmlToBinary(*xml, after);
  undo_.beginNewTransaction("Remove generation");
  undo_.perform(new TapeEdit{*this, std::move(before), std::move(after)});
  updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}

void LoopAudioProcessor::clearWithUndo() {
  if (isCapturing() || isPrinting()) return;
  juce::MemoryBlock before, after;
  getStateInformation(before);
  auto xml =
      getXmlFromBinary(before.getData(), static_cast<int>(before.getSize()));
  if (!xml) return;
  if (auto* tapes = xml->getChildByName("TAPES")) {
    tapes->setAttribute("active", 0);
    for (auto* tape : tapes->getChildIterator()) {
      tape->setAttribute("generation", 0);
      tape->setAttribute("frames", 0);
      tape->setAttribute("audio", "");
    }
  }
  copyXmlToBinary(*xml, after);
  undo_.beginNewTransaction("Clear memory");
  undo_.perform(new TapeEdit{*this, std::move(before), std::move(after)});
  updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}

int LoopAudioProcessor::factoryPresetCount() noexcept {
  return static_cast<int>(kPresets.size());
}
juce::String LoopAudioProcessor::factoryPresetName(int index) {
  return juce::isPositiveAndBelow(index, factoryPresetCount())
             ? kPresets[static_cast<std::size_t>(index)].name
             : juce::String{};
}
bool LoopAudioProcessor::usesLegacyBehavior() const {
  const std::lock_guard lock{lifecycle_};
  auto* source = pending_.load();
  if (!source) source = engine_.load();
  return source->legacyBehavior.load();
}
void LoopAudioProcessor::useCurrentResponse() {
  {
    const std::lock_guard lock{lifecycle_};
    auto* source = pending_.load();
    if (!source) source = engine_.load();
    source->legacyBehavior.store(false);
  }
  updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}
void LoopAudioProcessor::loadFactoryPreset(int index) {
  if (!juce::isPositiveAndBelow(index, factoryPresetCount())) return;
  const auto& preset = kPresets[static_cast<std::size_t>(index)];
  undo_.beginNewTransaction("Load preset");
  state_.getParameter("safe_takes")
      ->setValueNotifyingHost(index == 6 ? 1.F : 0.F);
  for (auto id : {"region_end", "repeat", "repeat_division", "monitor"}) {
    auto* p = state_.getParameter(id);
    p->setValueNotifyingHost(p->getDefaultValue());
  }
  for (std::size_t parameter = 0; parameter < preset.values.size(); ++parameter)
    if (auto* target = state_.getParameter(kParameterIds[parameter]))
      target->setValueNotifyingHost(
          target->convertTo0to1(preset.values[parameter]));
}

juce::AudioProcessorParameter* LoopAudioProcessor::getBypassParameter() const {
  return state_.getParameter("bypass");
}

void LoopAudioProcessor::getStateInformation(juce::MemoryBlock& destination) {
  const std::lock_guard lock{lifecycle_};
  collectRetired();
  auto state = state_.copyState();
  // Capture is a one-shot command in Safe Takes, not an instruction to
  // record again when a host restores a project saved mid-take.
  if (performanceValues_[0]->load() >= .5F)
    state.getChildWithProperty("id", "capture")
        .setProperty("value", 0.F, nullptr);
  state.setProperty("schema", kStateSchema, nullptr);
  state.setProperty("product", kStateType, nullptr);
  auto* source = pending_.load();
  if (!source) source = engine_.load();
  state.setProperty("behavior", source->legacyBehavior ? 0 : 1, nullptr);
  const auto tape = source->processor.tapeState();
  juce::ValueTree tapes{"TAPES"};
  tapes.setProperty("sample_rate", tape.sampleRate, nullptr);
  tapes.setProperty("active", static_cast<int>(tape.active), nullptr);
  for (std::size_t deck = 0; deck < 3; ++deck) {
    juce::MemoryOutputStream bytes;
    for (float sample : tape.audio[deck]) bytes.writeFloat(sample);
    juce::ValueTree child{"TAPE"};
    child.setProperty("index", static_cast<int>(deck), nullptr);
    child.setProperty("generation",
                      static_cast<juce::int64>(tape.generations[deck]),
                      nullptr);
    child.setProperty("frames", static_cast<int>(tape.audio[deck].size() / 2),
                      nullptr);
    child.setProperty(
        "audio", juce::Base64::toBase64(bytes.getData(), bytes.getDataSize()),
        nullptr);
    tapes.addChild(child, -1, nullptr);
  }
  state.removeChild(state.getChildWithName("TAPES"), nullptr);
  state.addChild(tapes, -1, nullptr);
  if (const auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}

void LoopAudioProcessor::setStateInformation(const void* data, int size) {
  restoreState(data, size, false);
}

bool LoopAudioProcessor::restoreHistory(const juce::MemoryBlock& expected,
                                        const juce::MemoryBlock& replacement) {
  if (isCapturing() || isPrinting()) return false;
  juce::MemoryBlock current;
  getStateInformation(current);
  auto currentXml =
      getXmlFromBinary(current.getData(), static_cast<int>(current.getSize()));
  auto expectedXml = getXmlFromBinary(expected.getData(),
                                      static_cast<int>(expected.getSize()));
  if (!currentXml || !expectedXml) return false;
  auto* actualTapes = currentXml->getChildByName("TAPES");
  auto* expectedTapes = expectedXml->getChildByName("TAPES");
  if (!actualTapes || !expectedTapes ||
      !actualTapes->isEquivalentTo(expectedTapes, false))
    return false;
  restoreState(replacement.getData(), static_cast<int>(replacement.getSize()),
               true);
  return true;
}

void LoopAudioProcessor::restoreState(const void* data, int size,
                                      bool historyOnly) {
  if (!data || size <= 0 || size > 256 * 1024 * 1024) return;
  const std::lock_guard lock{lifecycle_};
  const auto xml = getXmlFromBinary(data, size);
  if (xml == nullptr) return;
  const auto restored = juce::ValueTree::fromXml(*xml);
  const auto schemaText = restored.getProperty("schema").toString();
  if (schemaText != "1" && schemaText != "2" && schemaText != "3" &&
      schemaText != "4")
    return;
  const int schema = static_cast<int>(restored.getProperty("schema", -1));
  if (!restored.isValid() || restored.getType().toString() != kStateType ||
      restored.getProperty("product").toString() != kStateType ||
      (schema < 1 || schema > kStateSchema))
    return;
  TapeState tape;
  const auto behavior = restored.getProperty("behavior").toString();
  if (schema == 4 && behavior != "0" && behavior != "1") return;
  const bool legacy = schema < 4 || behavior == "0";
  if (schema >= 3) {
    const auto tapes = restored.getChildWithName("TAPES");
    if (!tapes.isValid() || tapes.getNumChildren() != 3) return;
    int tapeRoots{};
    for (int i = 0; i < restored.getNumChildren(); ++i)
      if (restored.getChild(i).hasType("TAPES")) ++tapeRoots;
    if (tapeRoots != 1) return;
    const auto integer = [](const juce::var& input, double maximum,
                            std::size_t& output) {
      double parsed{};
      if (!aste::parameters::parseFiniteDecimal(input.toString().toStdString(),
                                                parsed) ||
          parsed < 0 || parsed > maximum || std::floor(parsed) != parsed)
        return false;
      output = static_cast<std::size_t>(parsed);
      return true;
    };
    if (!aste::parameters::parseFiniteDecimal(
            tapes.getProperty("sample_rate").toString().toStdString(),
            tape.sampleRate) ||
        tape.sampleRate < 8000 || tape.sampleRate > 384000 ||
        !integer(tapes.getProperty("active"), 2, tape.active))
      return;
    std::array<bool, 3> seenDecks{};
    for (int i = 0; i < 3; ++i) {
      const auto child = tapes.getChild(i);
      std::size_t deck{}, frames{}, generation{};
      if (!child.hasType("TAPE") ||
          !integer(child.getProperty("index"), 2, deck) || seenDecks[deck] ||
          !integer(child.getProperty("frames"), tape.sampleRate * 16, frames) ||
          !integer(child.getProperty("generation"), 4294967295.0, generation) ||
          (frames == 0) != (generation == 0))
        return;
      seenDecks[deck] = true;
      const auto encoded = child.getProperty("audio").toString();
      const auto byteCount = frames * 2 * sizeof(float);
      if (static_cast<std::size_t>(encoded.length()) !=
          ((byteCount + 2) / 3) * 4)
        return;
      juce::MemoryOutputStream bytes;
      if (!juce::Base64::convertFromBase64(bytes, encoded) ||
          bytes.getDataSize() != byteCount)
        return;
      juce::MemoryInputStream input{bytes.getData(), bytes.getDataSize(),
                                    false};
      tape.generations[deck] = static_cast<std::uint32_t>(generation);
      tape.audio[deck].resize(frames * 2);
      for (auto& sample : tape.audio[deck]) {
        sample = input.readFloat();
        if (!std::isfinite(sample) || std::abs(sample) > 8.0F) return;
      }
    }
    const bool any =
        std::any_of(tape.generations.begin(), tape.generations.end(),
                    [](auto g) { return g != 0; });
    if (any && tape.generations[tape.active] == 0) return;
    for (std::size_t i = 0; i < 3; ++i)
      for (std::size_t j = i + 1; j < 3; ++j)
        if (tape.generations[i] != 0 &&
            tape.generations[i] == tape.generations[j])
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
  if (!seen.contains("master"))
    validated.getChildWithProperty("id", "master")
        .setProperty("value", 0.F, nullptr);
  validated.setProperty("schema", kStateSchema, nullptr);
  validated.setProperty("product", kStateType, nullptr);
  validated.setProperty("behavior", legacy ? 0 : 1, nullptr);
  auto ready = std::make_unique<Engine>();
  ready->legacyBehavior = legacy;
  ready->preservePlayback = historyOnly;
  // Hosts may restore before announcing their rate. Keep the source samples
  // intact until prepareToPlay rather than resampling through a guessed rate.
  ready->processor.prepare(hostPrepared_ ? hostRate_ : tape.sampleRate, 16.0,
                           true);
  if (ready->processor.capacitySamples() == 0) return;
  ready->processor.restoreTape(tape);
  if (!historyOnly) state_.replaceState(validated);
  delete pending_.exchange(ready.release());
  collectRetired();
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
