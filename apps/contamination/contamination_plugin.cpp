#include "contamination_plugin.hpp"
namespace aste::contamination::plugin {
using namespace aste::experimental;
std::vector<ControlSpec> Processor::controls() {
  return {knob("pressure", "PRESSURE %", 0, 100, 35),
          knob("contamination", "CONTAMINATION %", 0, 100, 15),
          knob("instability", "INSTABILITY %", 0, 100, 10),
          knob("memory", "MEMORY %", 0, 100, 50),
          knob("motion", "MOTION %", 0, 100, 25),
          knob("colour", "COLOUR Hz", 80, 8000, 1100, 1100),
          knob("attack", "ATTACK ms", .5F, 4000, 3, 100),
          knob("decay", "DECAY ms", 10, 8000, 250, 500),
          knob("sustain", "SUSTAIN %", 0, 100, 55),
          knob("release", "RELEASE ms", 5, 12000, 200, 600)};
}
Processor::Processor()
    : Instrument{"Contamination C-01", "contamination-c01", controls()} {
  setLatencySamples(Engine::latency);
  scene(4);
}
bool Processor::prepareEngine(double rate, int) {
  playing_ = false;
  activeDense_ = usesDenseCircuit();
  return engine_.prepare(rate, Engine::recommendedFactor(rate)) &&
         revised_.prepare(rate, Engine::recommendedFactor(rate));
}
void Processor::writeProductMetadata(juce::ValueTree& tree) const {
  tree.setProperty("circuit", usesDenseCircuit() ? 2 : 1, nullptr);
}
bool Processor::validateProductMetadata(const juce::ValueTree& tree) const {
  auto value = tree.getProperty("circuit", 1).toString();
  return value == "1" || value == "2";
}
void Processor::restoreProductMetadata(const juce::ValueTree& tree) {
  circuit_.store(tree.getProperty("circuit", 1).toString() == "2" ? 2 : 1,
                 std::memory_order_release);
}
void Processor::adoptDenseCircuit() {
  circuit_.store(2, std::memory_order_release);
  clearRequested_.store(true);
  updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}
void Processor::scene(int index) {
  defaults();
  circuit_.store(index >= 4 ? 2 : 1, std::memory_order_release);
  if (index == 1) {
    set("pressure", 55);
    set("contamination", 25);
    set("memory", 70);
    set("sustain", 0);
    set("decay", 90);
    set("release", 65);
    set("colour", 750);
  }
  if (index == 2) {
    set("pressure", 65);
    set("contamination", 45);
    set("instability", 45);
    set("memory", 90);
    set("motion", 60);
    set("attack", 180);
    set("release", 2200);
  }
  if (index == 3) {
    set("pressure", 0);
    set("contamination", 0);
    set("instability", 0);
    set("memory", 0);
    set("motion", 0);
  }
  if (index >= 4) {
    constexpr std::array<std::array<float, 10>, 5> targets{
        {{60, 25, 12, 30, 22, 650, 4, 110, 0, 90},
         {85, 65, 32, 48, 45, 1100, 2, 70, 3, 80},
         {62, 40, 40, 85, 62, 900, 5, 80, 0, 70},
         {85, 55, 80, 72, 72, 600, 150, 600, 80, 2000},
         {70, 50, 45, 90, 55, 750, 3, 110, 0, 100}}};
    const auto& target =
        targets[static_cast<std::size_t>(juce::jlimit(4, 8, index) - 4)];
    for (std::size_t i = 0; i < target.size(); ++i)
      set(specs()[i].id, target[i]);
  }
  updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}
void Processor::render(juce::AudioBuffer<float>& audio,
                       const juce::MidiBuffer& midi) noexcept {
  const bool dense = usesDenseCircuit();
  if (clearRequested_.exchange(false) || dense != activeDense_) {
    engine_.reset();
    revised_.reset();
    activeDense_ = dense;
  }
  if (memoryRequested_.exchange(false)) {
    engine_.event({0, Kind::Clear, 0, 0});
    revised_.event({0, Kind::Clear, 0, 0});
  }
  const auto send = [&](const Event& e) {
    if (dense)
      revised_.event(e);
    else
      engine_.event(e);
  };
  if (auto* head = getPlayHead())
    if (auto position = head->getPosition()) {
      const bool playing = position->getIsPlaying();
      if (playing_ && !playing) {
        engine_.reset();
        revised_.reset();
      }
      playing_ = playing;
    }
  Controls c;
  c.pressure = value(0) * .01;
  c.contamination = value(1) * .01;
  c.instability = value(2) * .01;
  c.memory = value(3) * .01;
  c.motion = value(4) * .01;
  c.colour = value(5);
  c.attack = value(6) * .001;
  c.decay = value(7) * .001;
  c.sustain = value(8) * .01;
  c.release = value(9) * .001;
  int cursor = 0;
  const int frames = audio.getNumSamples();
  const auto renderTo = [&](int end) {
    while (cursor < end) {
      const int count = std::min(512, end - cursor);
      auto* left = audio.getWritePointer(0) + cursor;
      auto* right = audio.getNumChannels() > 1
                        ? audio.getWritePointer(1) + cursor
                        : monoRight_.data();
      if (dense)
        revised_.process(left, right, static_cast<std::size_t>(count), c);
      else
        engine_.process(left, right, static_cast<std::size_t>(count), c);
      if (audio.getNumChannels() == 1 && dense)
        for (int i = 0; i < count; ++i) left[i] = (left[i] + right[i]) * .5F;
      cursor += count;
    }
  };
  for (const auto metadata : midi) {
    renderTo(juce::jlimit(cursor, frames, metadata.samplePosition));
    if (metadata.samplePosition >= frames || metadata.numBytes < 1 ||
        metadata.numBytes > 3)
      continue;
    const auto* bytes = metadata.data;
    const int status = bytes[0] & 0xf0;
    if ((status == 0x90 || status == 0x80) && metadata.numBytes == 3)
      send({0, status == 0x90 && bytes[2] > 0 ? Kind::On : Kind::Off,
            bytes[1] & 127, (bytes[2] & 127) / 127.});
    else if (status == 0xb0 && metadata.numBytes == 3) {
      if (bytes[1] == 64) send({0, Kind::Pedal, 0, (bytes[2] & 127) / 127.});
      if (bytes[1] == 1) send({0, Kind::Wheel, 0, (bytes[2] & 127) / 127.});
      if (bytes[1] == 120 || bytes[1] == 123) {
        engine_.reset();
        revised_.reset();
      }
    } else if (status == 0xd0 && metadata.numBytes == 2)
      send({0, Kind::Touch, 0, (bytes[1] & 127) / 127.});
    else if (status == 0xe0 && metadata.numBytes == 3)
      send({0, Kind::Bend, 0,
            ((bytes[1] & 127) + 128 * (bytes[2] & 127) - 8192) / 8192.});
  }
  renderTo(frames);
  if (dense) {
    const auto state = revised_.observation();
    memoryLevel_.store(static_cast<float>(state.memory));
    interaction_.store(static_cast<float>(state.interaction));
    spaceLevel_.store(static_cast<float>(std::sqrt(state.spaceEnergy)));
  } else {
    const auto state = engine_.observation();
    memoryLevel_.store(static_cast<float>(state.memory));
    interaction_.store(static_cast<float>(state.interaction));
    spaceLevel_.store(0);
  }
}
namespace {
class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit Editor(Processor& p)
      : AudioProcessorEditor{p}, p_{p}, master_{p.state()} {
    look_.setColour(juce::Slider::thumbColourId, juce::Colour{0xff83a99a});
    setLookAndFeel(&look_);
    setResizable(true, true);
    const std::array<juce::String, 10> help{
        "Drive inside the oscillator return and voice integrators. Aftertouch "
        "adds pressure.",
        "Correlated noise enters the nonlinear oscillator return. Zero removes "
        "injection.",
        "Slow correlated frequency movement. Zero disables drift; timing stays "
        "fixed.",
        "Retain excitation-dependent local and shared operating state between "
        "notes.",
        "Envelope-driven phase depth and memory-dependent spectral opening. "
        "CC1 adds phase depth.",
        "Spectral centre of the nonlinear return, in Hz.",
        "Amplitude attack in milliseconds.",
        "Time to approach the held level, in milliseconds.",
        "Held amplitude as a percentage.",
        "Release to silence in milliseconds."};
    int index = 0;
    for (const auto& spec : p.specs()) {
      auto control = std::make_unique<Control>(p, spec, index >= 5);
      control->setDescription(help[static_cast<std::size_t>(index)]);
      for (auto* child : control->getChildren()) {
        child->setDescription(help[static_cast<std::size_t>(index)]);
        if (auto* tooltip = dynamic_cast<juce::SettableTooltipClient*>(child))
          tooltip->setTooltip(help[static_cast<std::size_t>(index)] +
                              " Shift-drag for fine adjustment; type an exact "
                              "value; double-click to reset.");
      }
      control->setExplicitFocusOrder(index + 1);
      addAndMakeVisible(*control);
      controls_.push_back(std::move(control));
      ++index;
    }
    addAndMakeVisible(master_);
    master_.setExplicitFocusOrder(11);
    response_.bind(
        [this] { return p_.usesDenseCircuit(); },
        [this] { p_.adoptDenseCircuit(); },
        "Adopt the dense source, nonlinear resonant filtering and interacting "
        "feedback. Existing projects keep their classic circuit until this "
        "action or a new target patch is selected.");
    addAndMakeVisible(response_);
    preset_.addItemList(
        {"First circuit", "Muted pressure", "Slow contamination", "Predictable",
         "Submerged chord", "Corroded stab", "Feedback ghost", "Unstable mass",
         "Dub residue"},
        1);
    preset_.setTitle("Starting sound");
    preset_.onChange = [this] {
      if (preset_.getSelectedId() > 0) {
        p_.scene(preset_.getSelectedId() - 1);
        last_ = preset_.getText();
      }
    };
    addAndMakeVisible(preset_);
    preset_.setExplicitFocusOrder(12);
    clear_.setButtonText("CLEAR MEMORY");
    clear_.setTitle("Clear accumulated memory");
    clear_.setTooltip(
        "Discharge stored operating state. Held notes keep sounding.");
    clear_.onClick = [this] { p_.clearMemory(); };
    addAndMakeVisible(clear_);
    clear_.setExplicitFocusOrder(13);
    panic_.setButtonText("ALL SOUND OFF");
    panic_.onClick = [this] { p_.panic(); };
    addAndMakeVisible(panic_);
    panic_.setExplicitFocusOrder(14);
    meter_.setTitle("Measured memory, oscillator interaction and output");
    addAndMakeVisible(meter_);
    cue_.setText(
        "PLAY MIDI CHORDS   /   CC1: phase depth   /   AFTERTOUCH: pressure",
        juce::dontSendNotification);
    addAndMakeVisible(cue_);
    setResizeLimits(900, 560, 1600, 1000);
    setSize(1100, 650);
    startTimerHz(20);
    timerCallback();
  }
  ~Editor() override {
    stopTimer();
    setLookAndFeel(nullptr);
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{0xff101417});
    g.setColour(juce::Colour{0xffe7ece8});
    g.setFont(juce::FontOptions{22.F, juce::Font::bold});
    g.drawText("ASTE  /  C-01", 28, 18, 300, 32,
               juce::Justification::centredLeft);
    g.setFont(juce::FontOptions{12.F});
    g.drawText("CONTAMINATION / CHORD + DIRTY RETURN", 28, 50, 360, 22,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour{0xff83a99a});
    g.fillRect(28, 85, getWidth() - 56, 1);
  }
  void resized() override {
    if (controls_.size() != 10) return;
    const int w = getWidth(), h = getHeight();
    master_.setBounds(w - 300, 20, 272, 50);
    preset_.setBounds(330, 28, 220, 30);
    cue_.setBounds(28, 100, w - 56, 28);
    for (int i = 0; i < 5; ++i)
      controls_[static_cast<std::size_t>(i)]->setBounds(
          28 + i * (w - 56) / 5, 145, (w - 56) / 5, (h - 270) / 2);
    for (int i = 5; i < 10; ++i)
      controls_[static_cast<std::size_t>(i)]->setBounds(
          28 + (i - 5) * (w - 56) / 5, 160 + (h - 270) / 2, (w - 56) / 5, 110);
    response_.setBounds(w - 292, h - 70, 264, 32);
    meter_.setBounds(28, h - 125, w - 56, 32);
    clear_.setBounds(28, h - 70, 190, 32);
    panic_.setBounds(232, h - 70, 190, 32);
  }

 private:
  void timerCallback() override {
    const bool dense = p_.usesDenseCircuit();
    const std::array<juce::String, 6> labels =
        dense ? std::array<juce::String, 6>{"BODY / PRESSURE",
                                            "CORROSION",
                                            "DRIFT",
                                            "FEEDBACK",
                                            "EROSION / MOTION",
                                            "TONE Hz"}
              : std::array<juce::String, 6>{"PRESSURE %",    "CONTAMINATION %",
                                            "INSTABILITY %", "MEMORY %",
                                            "MOTION %",      "COLOUR Hz"};
    for (std::size_t i = 0; i < labels.size(); ++i)
      controls_[i]->setCaption(labels[i]);
    if (dense) {
      const std::array<juce::String, 6> help{
          "Harmonic body, nonlinear drive and resonant loading. Aftertouch "
          "adds pressure.",
          "Noise embedded in the harmonic source and nonlinear feedback "
          "returns.",
          "Detuned beating, voice drift and movement of stored material.",
          "Stored audio persistence and its influence on later chord "
          "excitation. Zero removes the audible return.",
          "Progressive return filtering and resonant movement. Repetitions "
          "change their material.",
          "Tone centre for the source resonances and the eroding return."};
      for (std::size_t i = 0; i < 6; ++i) controls_[i]->setHelp(help[i]);
      controls_[9]->setHelp(
          "Release of the chord source. Stored feedback decays independently; "
          "Clear Memory discharges it.");
    } else {
      const std::array<juce::String, 6> help{
          "Drive inside the oscillator return and voice integrators. "
          "Aftertouch adds pressure.",
          "Correlated noise inside the oscillator return.",
          "Slow correlated frequency movement.",
          "Local and shared operating-state memory, without an audio return.",
          "Envelope-driven phase depth; CC1 adds phase depth.",
          "Spectral centre of the two-pole return."};
      for (std::size_t i = 0; i < 6; ++i) controls_[i]->setHelp(help[i]);
      controls_[9]->setHelp("Amplitude release to silence in milliseconds.");
    }
    cue_.setText(dense ? "DENSE CIRCUIT / SOURCE + RESONANCE + DIRTY RETURN / "
                         "CC1: movement / AFTERTOUCH: pressure"
                       : "CLASSIC CIRCUIT / PLAY MIDI CHORDS / CC1: phase "
                         "depth / AFTERTOUCH: pressure",
                 juce::dontSendNotification);

    int match = -1;
    // Compare complete snapshots, including master/bypass, without altering the
    // processor.
    for (int scene = 0; scene < 9; ++scene) {
      if ((scene >= 4) != p_.usesDenseCircuit()) continue;
      std::array<float, 12> expected{35, 15,  10, 50,  25, 1100,
                                     3,  250, 55, 200, 0,  0};
      if (scene == 1) {
        expected[0] = 55;
        expected[1] = 25;
        expected[3] = 70;
        expected[5] = 750;
        expected[7] = 90;
        expected[8] = 0;
        expected[9] = 65;
      }
      if (scene == 2) {
        expected[0] = 65;
        expected[1] = 45;
        expected[2] = 45;
        expected[3] = 90;
        expected[4] = 60;
        expected[6] = 180;
        expected[9] = 2200;
      }
      if (scene == 3)
        for (int i = 0; i < 5; ++i) expected[static_cast<std::size_t>(i)] = 0;
      if (scene >= 4) {
        constexpr std::array<std::array<float, 10>, 5> targets{
            {{60, 25, 12, 30, 22, 650, 4, 110, 0, 90},
             {85, 65, 32, 48, 45, 1100, 2, 70, 3, 80},
             {62, 40, 40, 85, 62, 900, 5, 80, 0, 70},
             {85, 55, 80, 72, 72, 600, 150, 600, 80, 2000},
             {70, 50, 45, 90, 55, 750, 3, 110, 0, 100}}};
        std::copy(targets[static_cast<std::size_t>(scene - 4)].begin(),
                  targets[static_cast<std::size_t>(scene - 4)].end(),
                  expected.begin());
      }
      bool same = true;
      for (int i = 0; i < 10; ++i)
        same = same && std::abs(p_.value(static_cast<std::size_t>(i)) -
                                expected[static_cast<std::size_t>(i)]) < .02F;
      same =
          same &&
          std::abs(p_.state().getRawParameterValue("master")->load()) < .005F &&
          p_.state().getRawParameterValue("bypass")->load() < .5F;
      if (same) match = scene;
    }
    if (match >= 0) {
      preset_.setSelectedId(match + 1, juce::dontSendNotification);
      last_ = preset_.getText();
    } else {
      preset_.setSelectedId(0, juce::dontSendNotification);
      preset_.setText(last_.isEmpty() ? "CUSTOM" : last_ + " *",
                      juce::dontSendNotification);
    }
    const auto db = juce::Decibels::gainToDecibels(p_.outputPeak(), -100.F);
    meter_.setText("STORED STATE  " + juce::String(p_.memoryLevel(), 3) +
                       "    /    SPACE  " + juce::String(p_.spaceLevel(), 3) +
                       "    /    RETURN  " + juce::String(p_.interaction(), 3) +
                       "    /    OUTPUT  " + juce::String(db, 1) + " dBFS",
                   juce::dontSendNotification);
  }
  Processor& p_;
  Look look_;
  aste::ui::MasterOutput master_;
  std::vector<std::unique_ptr<Control>> controls_;
  aste::ui::ResponseButton response_;
  juce::ComboBox preset_;
  juce::TextButton clear_, panic_;
  juce::Label meter_, cue_;
  juce::String last_;
  juce::TooltipWindow tooltip_{this};
};
}  // namespace
juce::AudioProcessorEditor* Processor::createEditor() {
  return new Editor{*this};
}
}  // namespace aste::contamination::plugin
#ifndef ASTE_NO_PLUGIN_FACTORY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::contamination::plugin::Processor;
}
#endif
