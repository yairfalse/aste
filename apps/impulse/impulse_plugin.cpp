#include "impulse_plugin.hpp"

#include "decimal_parse.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <memory>
#include <string>
#include <vector>

namespace aste::impulse::plugin {
namespace {
constexpr auto kStateType = "impulse-i01";
constexpr int kStateSchema = 1;
constexpr auto kSurface = 0xff10100dU;
constexpr auto kPanel = 0xff1d1c16U;
constexpr auto kInk = 0xffece9dcU;
constexpr auto kMuted = 0xff888274U;
constexpr auto kAccent = 0xffd66a25U;
constexpr std::array<const char*, 8> kGlobalIds{
    "energy", "division", "variation", "mutation",
    "seed",   "output",   "sequence",  "bypass"};
constexpr std::array<const char*, kTrackCount> kTrackIds{"kick", "click",
                                                         "burst", "body"};
constexpr std::array<const char*, kTrackCount> kTrackNames{"KICK", "CLICK",
                                                           "BURST", "BODY"};
constexpr std::array<const char*, 13> kFieldIds{
    "level",  "pitch",     "decay",    "tone",        "drive",
    "length", "pulses",    "rotation", "probability", "ratchet",
    "timing", "condition", "accent"};
constexpr std::array<const char*, 13> kFieldNames{
    "LEVEL",  "PITCH", "DECAY",   "TONE",   "DRIVE",     "LENGTH", "PULSES",
    "ROTATE", "PROB",  "RATCHET", "TIMING", "CONDITION", "ACCENT"};

juce::String trackId(std::size_t track, std::size_t field) {
  return juce::String{kTrackIds[track]} + "_" + kFieldIds[field];
}
bool parseFiniteFloat(const juce::var& value, float& result) {
  double parsed{};
  if (!aste::parameters::parseFiniteDecimal(value.toString().toStdString(),
                                            parsed))
    return false;
  result = static_cast<float>(parsed);
  return std::isfinite(result);
}
juce::NormalisableRange<float> skewed(float low, float high, float centre,
                                      float interval) {
  juce::NormalisableRange<float> result{low, high, interval};
  result.setSkewForCentre(centre);
  return result;
}
void addFloat(juce::AudioProcessorValueTreeState::ParameterLayout& layout,
              const juce::String& id, const juce::String& name,
              juce::NormalisableRange<float> range, float initial,
              const char* unit) {
  layout.add(std::make_unique<juce::AudioParameterFloat>(
      juce::ParameterID{id, 1}, name, range, initial,
      juce::AudioParameterFloatAttributes{}.withLabel(unit)));
}

class ImpulseLookAndFeel final : public juce::LookAndFeel_V4 {
 public:
  ImpulseLookAndFeel() {
    setColour(juce::Slider::textBoxTextColourId, juce::Colour{kInk});
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colour{kSurface});
    setColour(juce::Slider::textBoxOutlineColourId,
              juce::Colours::transparentBlack);
    setColour(juce::Label::textColourId, juce::Colour{kInk});
  }
  void drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                        float position, float startAngle, float endAngle,
                        juce::Slider&) override {
    auto bounds =
        juce::Rectangle<float>{static_cast<float>(x), static_cast<float>(y),
                               static_cast<float>(width),
                               static_cast<float>(height)}
            .reduced(5);
    const float radius = std::min(bounds.getWidth(), bounds.getHeight()) * 0.5F;
    const auto centre = bounds.getCentre();
    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius, radius, 0, startAngle,
                        endAngle, true);
    g.setColour(juce::Colour{kMuted}.withAlpha(0.28F));
    g.strokePath(track, juce::PathStrokeType{2.5F});
    juce::Path active;
    active.addCentredArc(centre.x, centre.y, radius, radius, 0, startAngle,
                         startAngle + position * (endAngle - startAngle), true);
    g.setColour(juce::Colour{kAccent});
    g.strokePath(active, juce::PathStrokeType{3.5F});
  }
};

class Knob final : public juce::Component {
 public:
  Knob(juce::AudioProcessorValueTreeState& state, const juce::String& id,
       const juce::String& name, const juce::String& suffix, double initial,
       int order)
      : attachment_{state, id, slider_} {
    slider_.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    slider_.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 68, 18);
    slider_.setTextValueSuffix(suffix);
    slider_.setDoubleClickReturnValue(true, initial);
    slider_.setTitle(name);
    slider_.setDescription(name + " parameter");
    slider_.setWantsKeyboardFocus(true);
    slider_.setExplicitFocusOrder(order);
    label_.setText(name, juce::dontSendNotification);
    label_.setJustificationType(juce::Justification::centred);
    label_.setFont(juce::FontOptions{9.0F, juce::Font::bold});
    addAndMakeVisible(slider_);
    addAndMakeVisible(label_);
  }
  void resized() override {
    auto area = getLocalBounds();
    label_.setBounds(area.removeFromTop(16));
    slider_.setBounds(area);
  }

 private:
  juce::Slider slider_;
  juce::Label label_;
  juce::AudioProcessorValueTreeState::SliderAttachment attachment_;
};

class TrackPanel final : public juce::Component, private juce::Timer {
 public:
  TrackPanel(ImpulseAudioProcessor& processor, std::size_t track, int order)
      : processor_{processor}, track_{track} {
    const Parameters defaults;
    const auto& p = defaults.tracks[track];
    const std::array<double, 13> values{p.level * 100,
                                        p.pitchHz,
                                        p.decayMs,
                                        p.tone * 100,
                                        p.drive * 100,
                                        static_cast<double>(p.length),
                                        static_cast<double>(p.pulses),
                                        static_cast<double>(p.rotation),
                                        p.probability * 100,
                                        static_cast<double>(p.ratchet),
                                        p.timing * 100,
                                        static_cast<double>(p.condition),
                                        p.accent * 100};
    const std::array<const char*, 13> suffix{
        " %", " Hz", " ms", " %", " %", "", "", "", " %", "", " %", "", " %"};
    for (std::size_t field = 0; field < knobs_.size(); ++field) {
      knobs_[field] = std::make_unique<Knob>(
          processor.state(), trackId(track, field),
          juce::String{kTrackNames[track]} + " " + kFieldNames[field],
          suffix[field], values[field], order + static_cast<int>(field));
      addAndMakeVisible(*knobs_[field]);
    }
    startTimerHz(20);
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{kPanel});
    g.setColour(juce::Colour{kInk});
    g.setFont(juce::FontOptions{14.0F, juce::Font::bold});
    g.drawText(kTrackNames[track_], 12, 8, getWidth() - 24, 22,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour{kAccent});
    g.setFont(juce::FontOptions{11.0F, juce::Font::bold});
    g.drawText(step_ < 0 ? "--" : juce::String{step_ + 1}.paddedLeft('0', 2),
               getWidth() - 52, 8, 40, 22, juce::Justification::centredRight);
  }
  void resized() override {
    auto area = getLocalBounds().reduced(8);
    area.removeFromTop(30);
    constexpr int columns = 3;
    const int width = area.getWidth() / columns;
    const int height = area.getHeight() / 5;
    for (std::size_t index = 0; index < knobs_.size(); ++index)
      knobs_[index]->setBounds(
          area.getX() + static_cast<int>(index % columns) * width,
          area.getY() + static_cast<int>(index / columns) * height, width,
          height);
  }

 private:
  void timerCallback() override {
    const int next = processor_.currentStep(track_);
    if (next != step_) {
      step_ = next;
      repaint();
    }
  }
  ImpulseAudioProcessor& processor_;
  std::size_t track_{};
  std::array<std::unique_ptr<Knob>, 13> knobs_{};
  int step_{-1};
};

class ImpulseEditor final : public juce::AudioProcessorEditor,
                            private juce::Timer {
 public:
  explicit ImpulseEditor(ImpulseAudioProcessor& processor)
      : AudioProcessorEditor{processor},
        processor_{processor},
        energy_{processor.state(), "energy", "ENERGY", " %", 45, 1},
        variation_{processor.state(), "variation", "VARIATION", " %", 12, 2},
        mutation_{processor.state(), "mutation", "MUTATION", " %", 0, 3},
        seed_{processor.state(), "seed", "SEED", "", 1701, 4},
        output_{processor.state(), "output", "OUTPUT", " dB", -6, 5},
        sequence_{"SEQUENCE"} {
    setLookAndFeel(&lookAndFeel_);
    division_.setTitle("DIVISION");
    division_.addItemList({"1/8", "1/16", "1/32"}, 1);
    divisionAttachment_ = std::make_unique<
        juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.state(), "division", division_);
    sequenceAttachment_ =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            processor.state(), "sequence", sequence_);
    sequence_.setTitle("SEQUENCE");
    sequence_.setColour(juce::ToggleButton::tickColourId,
                        juce::Colour{kAccent});
    for (std::size_t track = 0; track < kTrackCount; ++track) {
      tracks_[track] = std::make_unique<TrackPanel>(
          processor, track, 10 + static_cast<int>(track * 13));
      addAndMakeVisible(*tracks_[track]);
    }
    preset_.setTitle("PRESETS");
    preset_.setTextWhenNothingSelected("SNAPSHOTS");
    for (int index = 0; index < processor.factoryPresetCount(); ++index)
      preset_.addItem(processor.factoryPresetName(index), index + 1);
    preset_.onChange = [this] {
      if (preset_.getSelectedId() > 0) {
        processor_.loadFactoryPreset(preset_.getSelectedId() - 1);
        preset_.setSelectedId(0, juce::dontSendNotification);
      }
    };
    for (auto* component : std::array<juce::Component*, 8>{
             &energy_, &variation_, &mutation_, &seed_, &output_, &division_,
             &sequence_, &preset_})
      addAndMakeVisible(component);
    setResizable(true, true);
    setResizeLimits(1120, 700, 1800, 1120);
    setSize(1480, 900);
    startTimerHz(30);
  }
  ~ImpulseEditor() override { setLookAndFeel(nullptr); }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{kSurface});
    auto header = getLocalBounds().reduced(24).removeFromTop(48);
    g.setColour(juce::Colour{kInk});
    g.setFont(juce::FontOptions{22.0F, juce::Font::bold});
    g.drawText("IMPULSE", header.removeFromLeft(200),
               juce::Justification::centredLeft);
    g.setColour(juce::Colour{kAccent});
    g.setFont(juce::FontOptions{12.0F, juce::Font::bold});
    header.removeFromRight(220);
    g.drawText("I-01 / RHYTHMIC OBJECTS", header,
               juce::Justification::centredRight);
    g.setColour(juce::Colour{kMuted});
    g.drawText("OUT " + juce::String{peak_ * 100.0F, 0} + " %",
               getWidth() - 210, 70, 180, 20,
               juce::Justification::centredRight);
  }
  void resized() override {
    preset_.setBounds(getWidth() - 204, 25, 180, 28);
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(62);
    auto globals = area.removeFromTop(155);
    const int width = globals.getWidth() / 8;
    energy_.setBounds(globals.removeFromLeft(width * 2));
    variation_.setBounds(globals.removeFromLeft(width));
    mutation_.setBounds(globals.removeFromLeft(width));
    seed_.setBounds(globals.removeFromLeft(width));
    output_.setBounds(globals.removeFromLeft(width));
    division_.setBounds(globals.removeFromTop(32).reduced(8, 2));
    sequence_.setBounds(globals.removeFromTop(32).reduced(8, 2));
    area.removeFromTop(12);
    const int trackWidth = area.getWidth() / static_cast<int>(kTrackCount);
    for (std::size_t track = 0; track < kTrackCount; ++track)
      tracks_[track]->setBounds(
          area.getX() + static_cast<int>(track) * trackWidth, area.getY(),
          trackWidth - 8, area.getHeight());
  }

 private:
  void timerCallback() override {
    peak_ = std::max(processor_.outputPeak(), peak_ * 0.84F);
    repaint(getWidth() - 220, 65, 190, 30);
  }
  ImpulseLookAndFeel lookAndFeel_;
  ImpulseAudioProcessor& processor_;
  Knob energy_, variation_, mutation_, seed_, output_;
  juce::ComboBox division_, preset_;
  juce::ToggleButton sequence_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
      divisionAttachment_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
      sequenceAttachment_;
  std::array<std::unique_ptr<TrackPanel>, kTrackCount> tracks_{};
  float peak_{};
};
}  // namespace

juce::AudioProcessorValueTreeState::ParameterLayout
ImpulseAudioProcessor::createParameterLayout() {
  juce::AudioProcessorValueTreeState::ParameterLayout layout;
  addFloat(layout, "energy", "Energy", {0, 100, 0.01F}, 45, "%");
  layout.add(std::make_unique<juce::AudioParameterChoice>(
      juce::ParameterID{"division", 1}, "Division",
      juce::StringArray{"1/8", "1/16", "1/32"}, 1));
  addFloat(layout, "variation", "Variation", {0, 100, 0.01F}, 12, "%");
  addFloat(layout, "mutation", "Mutation", {0, 100, 0.01F}, 0, "%");
  addFloat(layout, "seed", "Seed", {0, 65535, 1}, 1701, "");
  addFloat(layout, "output", "Output", {-24, 12, 0.01F}, -6, "dB");
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"sequence", 1}, "Sequence", true));
  layout.add(std::make_unique<juce::AudioParameterBool>(
      juce::ParameterID{"bypass", 1}, "Bypass", false));
  const Parameters defaults;
  for (std::size_t track = 0; track < kTrackCount; ++track) {
    const auto prefix = juce::String{kTrackNames[track]} + " ";
    const auto& p = defaults.tracks[track];
    addFloat(layout, trackId(track, 0), prefix + "Level", {0, 100, 0.01F},
             p.level * 100, "%");
    addFloat(layout, trackId(track, 1), prefix + "Pitch",
             skewed(25, 10000, p.pitchHz, 0.1F), p.pitchHz, "Hz");
    addFloat(layout, trackId(track, 2), prefix + "Decay",
             skewed(5, 3000, p.decayMs, 0.1F), p.decayMs, "ms");
    addFloat(layout, trackId(track, 3), prefix + "Tone", {0, 100, 0.01F},
             p.tone * 100, "%");
    addFloat(layout, trackId(track, 4), prefix + "Drive", {0, 100, 0.01F},
             p.drive * 100, "%");
    addFloat(layout, trackId(track, 5), prefix + "Length", {1, 32, 1},
             static_cast<float>(p.length), "steps");
    addFloat(layout, trackId(track, 6), prefix + "Pulses", {0, 32, 1},
             static_cast<float>(p.pulses), "steps");
    addFloat(layout, trackId(track, 7), prefix + "Rotation", {0, 31, 1},
             static_cast<float>(p.rotation), "steps");
    addFloat(layout, trackId(track, 8), prefix + "Probability", {0, 100, 0.01F},
             p.probability * 100, "%");
    addFloat(layout, trackId(track, 9), prefix + "Ratchet", {1, 4, 1},
             static_cast<float>(p.ratchet), "");
    addFloat(layout, trackId(track, 10), prefix + "Timing", {-49, 49, 0.01F},
             p.timing * 100, "% step");
    addFloat(layout, trackId(track, 11), prefix + "Condition", {1, 4, 1},
             static_cast<float>(p.condition), "cycles");
    addFloat(layout, trackId(track, 12), prefix + "Accent", {0, 100, 0.01F},
             p.accent * 100, "%");
  }
  return layout;
}

ImpulseAudioProcessor::ImpulseAudioProcessor()
    : AudioProcessor{BusesProperties{}.withOutput(
          "Output", juce::AudioChannelSet::stereo(), true)},
      state_{*this, nullptr, kStateType, createParameterLayout()} {
  for (std::size_t index = 0; index < kGlobalIds.size(); ++index)
    globals_[index] = state_.getRawParameterValue(kGlobalIds[index]);
  for (std::size_t track = 0; track < kTrackCount; ++track)
    for (std::size_t field = 0; field < kFieldIds.size(); ++field)
      tracks_[track][field] =
          state_.getRawParameterValue(trackId(track, field));
}

Parameters ImpulseAudioProcessor::currentParameters() const noexcept {
  const auto global = [this](std::size_t index) {
    return globals_[index]->load(std::memory_order_relaxed);
  };
  Parameters result;
  result.energy = global(0) * 0.01F;
  result.division = static_cast<int>(global(1));
  result.variation = global(2) * 0.01F;
  result.mutation = global(3) * 0.01F;
  result.seed = static_cast<std::uint32_t>(global(4));
  result.outputDb = global(5);
  result.sequenceEnabled = global(6) >= 0.5F;
  result.bypass = global(7) >= 0.5F;
  for (std::size_t track = 0; track < kTrackCount; ++track) {
    const auto value = [this, track](std::size_t field) {
      return tracks_[track][field]->load(std::memory_order_relaxed);
    };
    auto& p = result.tracks[track];
    p.level = value(0) * 0.01F;
    p.pitchHz = value(1);
    p.decayMs = value(2);
    p.tone = value(3) * 0.01F;
    p.drive = value(4) * 0.01F;
    p.length = static_cast<int>(value(5));
    p.pulses = static_cast<int>(value(6));
    p.rotation = static_cast<int>(value(7));
    p.probability = value(8) * 0.01F;
    p.ratchet = static_cast<int>(value(9));
    p.timing = value(10) * 0.01F;
    p.condition = static_cast<int>(value(11));
    p.accent = value(12) * 0.01F;
  }
  return result;
}

Transport ImpulseAudioProcessor::currentTransport() const noexcept {
  Transport result{};
  if (const auto* playHead = getPlayHead())
    if (const auto position = playHead->getPosition()) {
      result.playing = position->getIsPlaying();
      result.bpm = position->getBpm().orFallback(120.0);
      result.ppq = position->getPpqPosition().orFallback(0.0);
    }
  return result;
}

std::span<const MidiEvent> ImpulseAudioProcessor::readMidi(
    const juce::MidiBuffer& midi, int frames) noexcept {
  midiCount_ = 0;
  for (const auto metadata : midi) {
    const auto& message = metadata.getMessage();
    if ((!message.isNoteOn() && !message.isNoteOff()) ||
        midiCount_ == midiScratch_.size())
      continue;
    const int pitchClass = message.getNoteNumber() % 12;
    const int mappedNote =
        pitchClass < 4 ? 36 + pitchClass : message.getNoteNumber();
    midiScratch_[midiCount_++] = {static_cast<std::size_t>(juce::jlimit(
                                      0, frames, metadata.samplePosition)),
                                  mappedNote, message.getFloatVelocity(),
                                  message.isNoteOn()};
  }
  return {midiScratch_.data(), midiCount_};
}

void ImpulseAudioProcessor::prepareToPlay(double sampleRate, int) {
  processor_.prepare(sampleRate, currentParameters());
  setLatencySamples(0);
  publishMeters({});
}
void ImpulseAudioProcessor::releaseResources() { processor_.reset(); }
bool ImpulseAudioProcessor::isBusesLayoutSupported(
    const BusesLayout& layouts) const {
  const auto output = layouts.getMainOutputChannelSet();
  return layouts.getMainInputChannelSet().isDisabled() &&
         (output == juce::AudioChannelSet::mono() ||
          output == juce::AudioChannelSet::stereo());
}
void ImpulseAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer,
                                         juce::MidiBuffer& midi) {
  juce::ScopedNoDenormals noDenormals;
  buffer.clear();
  if (buffer.getNumChannels() < 1 || buffer.getNumSamples() == 0) {
    publishMeters({});
    return;
  }
  processor_.process(
      buffer.getWritePointer(0),
      buffer.getNumChannels() > 1 ? buffer.getWritePointer(1) : nullptr,
      static_cast<std::size_t>(buffer.getNumSamples()), currentParameters(),
      currentTransport(), readMidi(midi, buffer.getNumSamples()));
  publishMeters(processor_.meters());
}
void ImpulseAudioProcessor::processBlockBypassed(
    juce::AudioBuffer<float>& buffer, juce::MidiBuffer&) {
  buffer.clear();
  publishMeters({});
}
void ImpulseAudioProcessor::publishMeters(
    const MeterSnapshot& meters) noexcept {
  outputPeak_.store(meters.outputPeak, std::memory_order_relaxed);
  for (std::size_t track = 0; track < kTrackCount; ++track)
    currentSteps_[track].store(static_cast<float>(meters.currentStep[track]),
                               std::memory_order_relaxed);
}
float ImpulseAudioProcessor::outputPeak() const noexcept {
  return outputPeak_.load(std::memory_order_relaxed);
}
int ImpulseAudioProcessor::currentStep(std::size_t track) const noexcept {
  return track < kTrackCount ? static_cast<int>(currentSteps_[track].load(
                                   std::memory_order_relaxed))
                             : -1;
}
juce::AudioProcessorParameter* ImpulseAudioProcessor::getBypassParameter()
    const {
  return state_.getParameter("bypass");
}

int ImpulseAudioProcessor::factoryPresetCount() noexcept { return 5; }
juce::String ImpulseAudioProcessor::factoryPresetName(int index) {
  constexpr std::array names{"Default Objects", "Deep Cycle",
                             "Microscopic Cuts", "Polymetric Field",
                             "Manual Surface"};
  return juce::isPositiveAndBelow(index, factoryPresetCount())
             ? names[static_cast<std::size_t>(index)]
             : juce::String{};
}
void ImpulseAudioProcessor::loadFactoryPreset(int index) {
  if (!juce::isPositiveAndBelow(index, factoryPresetCount())) return;
  const auto set = [this](const juce::String& id, float plain) {
    if (auto* p = state_.getParameter(id))
      p->setValueNotifyingHost(p->convertTo0to1(plain));
  };
  set("energy", std::array{45.0F, 72.0F, 38.0F, 58.0F,
                           50.0F}[static_cast<std::size_t>(index)]);
  set("variation", std::array{12.0F, 8.0F, 28.0F, 20.0F,
                              15.0F}[static_cast<std::size_t>(index)]);
  set("mutation", std::array{0.0F, 0.0F, 18.0F, 10.0F,
                             0.0F}[static_cast<std::size_t>(index)]);
  set("sequence", index == 4 ? 0.0F : 1.0F);
  constexpr std::array<std::array<int, 4>, 5> lengths{{{{15, 23, 11, 16}},
                                                       {{16, 19, 13, 16}},
                                                       {{7, 11, 5, 9}},
                                                       {{15, 23, 11, 16}},
                                                       {{16, 16, 16, 16}}}};
  constexpr std::array<std::array<int, 4>, 5> pulses{{{{4, 7, 4, 5}},
                                                      {{4, 5, 3, 4}},
                                                      {{3, 7, 2, 4}},
                                                      {{5, 9, 4, 7}},
                                                      {{0, 0, 0, 0}}}};
  for (std::size_t track = 0; track < kTrackCount; ++track) {
    set(trackId(track, 5),
        static_cast<float>(lengths[static_cast<std::size_t>(index)][track]));
    set(trackId(track, 6),
        static_cast<float>(pulses[static_cast<std::size_t>(index)][track]));
  }
}

void ImpulseAudioProcessor::getStateInformation(
    juce::MemoryBlock& destination) {
  auto state = state_.copyState();
  state.setProperty("schema", kStateSchema, nullptr);
  state.setProperty("product", kStateType, nullptr);
  if (const auto xml = state.createXml()) copyXmlToBinary(*xml, destination);
}
void ImpulseAudioProcessor::setStateInformation(const void* data, int size) {
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
    if (auto* p = state_.getParameter(child.getProperty("id").toString()))
      child.setProperty("value", p->convertFrom0to1(p->getDefaultValue()),
                        nullptr);
  }
  juce::StringArray seen;
  for (int index = 0; index < restored.getNumChildren(); ++index) {
    const auto child = restored.getChild(index);
    const auto id = child.getProperty("id").toString();
    auto* p = state_.getParameter(id);
    if (!p) continue;
    if (seen.contains(id) || !child.hasProperty("value")) return;
    seen.add(id);
    float value{};
    if (!parseFiniteFloat(child.getProperty("value"), value)) return;
    auto destination = validated.getChildWithProperty("id", id);
    if (!destination.isValid()) return;
    const auto& range = p->getNormalisableRange();
    destination.setProperty(
        "value", juce::jlimit(range.start, range.end, value), nullptr);
  }
  validated.setProperty("schema", kStateSchema, nullptr);
  validated.setProperty("product", kStateType, nullptr);
  state_.replaceState(validated);
}
juce::AudioProcessorEditor* ImpulseAudioProcessor::createEditor() {
  return new ImpulseEditor{*this};
}
}  // namespace aste::impulse::plugin

#if !defined(ASTE_NO_PLUGIN_FACTORY)
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::impulse::plugin::ImpulseAudioProcessor{};
}
#endif
