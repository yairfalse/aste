#include "resonator_plugin.hpp"

namespace aste::resonator::plugin {
using namespace aste::experimental;
std::vector<ControlSpec> ResonatorAudioProcessor::controls() {
  return {
      choice("source", "EXCITER",
             "White noise|Pink noise|Shaped noise|Impulse|Grain burst", 2),
      knob("burst", "BURST ms", 1, 150, 35, 35),
      knob("excitation", "EXCITATION %", 0, 100, 60),
      knob("root", "ROOT MIDI", 24, 84, 48, 0, 1),
      choice("chord", "STRUCTURE", "Minor 7|Suspended|Minor 9|Open"),
      choice("input_mode", "MIDI MODE",
             "Play chord roots|Play individual tones"),
      knob("decay", "DECAY seconds", .05F, 30, 4, 3),
      knob("damping", "DAMPING %", 0, 80, 25),
      knob("coupling", "ENERGY TRANSFER Hz", 0, 20, 3),
      knob("stretch", "HARMONIC STRETCH %", 0, 30, 0),
      knob("inharmonic", "INHARMONIC %", 0, 100, 0),
      choice("memory", "HARMONIC MEMORY", "Reset|Shared|Partial|Overlap", 1),
      knob("retain", "PARTIAL RETAIN %", 0, 100, 25),
      toggle("persist", "PERSIST"),
      knob("mutation", "MUTATION cents", 0, 50, 4),
      knob("seed", "SEED", 0, 65535, 101, 0, 1),
      toggle("gate", "KEY RELEASE DAMPING")};
}
ResonatorAudioProcessor::ResonatorAudioProcessor()
    : Instrument{"Resonator R-01", "resonator-r01", controls()} {
  scene(3);
}
bool ResonatorAudioProcessor::prepareEngine(double rate, int) {
  midiTuningActive_.store(false);
  lastRoot_ = value(root);
  lastStructure_ = value(chord);
  held_.fill(false);
  sustained_.fill(false);
  pedal_ = false;
  strikes_ = 0;
  Controls c;
  return exciter_.prepare(rate) &&
         engine_.prepare(rate, Architecture::Modal, Memory::Shared, c);
}
void ResonatorAudioProcessor::onRestore() noexcept {
  clearRequested_.store(true);
}
Chord ResonatorAudioProcessor::selectedChord() const noexcept {
  Chord c;
  c.root = value(root);
  constexpr std::array<std::array<double, 4>, 4> shapes{
      {{0, 3, 7, 10}, {0, 5, 7, 12}, {0, 3, 7, 14}, {0, 2, 7, 11}}};
  c.intervals = shapes[static_cast<std::size_t>(
      juce::jlimit(0, 3, static_cast<int>(value(chord))))];
  return c;
}
bool ResonatorAudioProcessor::key(int note, bool on) noexcept {
  if (note < 24 || note > 100) {
    rejected_.fetch_add(1);
    return false;
  }
  const auto index = static_cast<std::size_t>(note);
  if (!on) {
    if (held_[index] && pedal_) sustained_[index] = true;
    if (!pedal_) sustained_[index] = false;
    held_[index] = false;
    return true;
  }
  auto proposed = held_;
  proposed[index] = true;
  for (std::size_t i = 0; i < 128; ++i)
    proposed[i] = proposed[i] || sustained_[i];
  const int count =
      static_cast<int>(std::count(proposed.begin(), proposed.end(), true));
  const auto first =
      std::find(proposed.begin(), proposed.end(), true) - proposed.begin();
  const auto last = 127 - (std::find(proposed.rbegin(), proposed.rend(), true) -
                           proposed.rbegin());
  if (count > 4 || last - first > 24) {
    rejected_.fetch_add(1);
    return false;
  }
  held_[index] = true;
  sustained_[index] = false;
  return true;
}
Chord ResonatorAudioProcessor::heldChord() const noexcept {
  Chord c;
  c.count = 0;
  for (std::size_t n = 0; n < 128; ++n)
    if (held_[n] || sustained_[n]) {
      if (c.count == 0) c.root = static_cast<double>(n);
      if (c.count < 4) c.intervals[c.count++] = static_cast<double>(n) - c.root;
    }
  return c.count ? c : selectedChord();
}
void ResonatorAudioProcessor::scene(int index) {
  if (index == 3) useCurrentResponse();
  defaults();
  if (index == 3) {
    set("root", 36);
    set("decay", 6);
    set("source", 1);
    set("coupling", 7);
    set("damping", 18);
    set("burst", 18);
    set("excitation", 80);
    set("stretch", 4);
    set("inharmonic", 12);
  }
  if (index == 1) {
    set("decay", .45F);
    set("source", 3);
    set("damping", 40);
    set("memory", 2);
    set("retain", 40);
    set("excitation", 85);
    set("master", 6);
  } else if (index == 2) {
    set("decay", 12);
    set("coupling", 8);
    set("stretch", 8);
    set("inharmonic", 15);
    set("source", 1);
    set("memory", 3);
  }
}
void ResonatorAudioProcessor::render(juce::AudioBuffer<float>& audio,
                                     const juce::MidiBuffer& midi) noexcept {
  Controls c;
  c.t60 = value(decay);
  c.damping = value(damping) * .01;
  c.couplingHz = value(coupling);
  c.stretch = value(stretch) * .01;
  c.inharmonic = value(inharmonic) * .01;
  c.partialRetain = value(retain) * .01;
  c.persist = value(persist) >= .5F;
  c.mutationCents = value(mutation);
  c.seed = static_cast<std::uint64_t>(value(seed));
  c.material = usesMaterialResponse()
                   ? std::clamp(.35 + value(coupling) / 20., 0., 1.)
                   : 0.;
  engine_.setMemory(static_cast<Memory>(static_cast<int>(value(memory))));
  bool panic = clearRequested_.exchange(false),
       manual = exciteRequested_.exchange(false),
       retune = retuneRequested_.exchange(false);
  if (usesMaterialResponse() &&
      (lastRoot_ != value(root) || lastStructure_ != value(chord)))
    retune = true;
  lastRoot_ = value(root);
  lastStructure_ = value(chord);
  if (panic) {
    held_.fill(false);
    sustained_.fill(false);
    pedal_ = false;
    exciter_.prepare(rate_);
    strikes_ = 0;
    velocity_ = 1;
    engine_.reset();
    panic = false;
  }
  auto iterator = midi.begin();
  const auto end = midi.end();
  const int frames = audio.getNumSamples();
  for (int n = 0; n < frames; ++n) {
    bool strike = n == 0 && manual, change = n == 0 && retune, release = false;
    Chord selected = selectedChord();
    while (iterator != end && (*iterator).samplePosition <= n) {
      const auto metadata = *iterator;
      ++iterator;
      if (metadata.numBytes > 3) continue;
      const auto msg = metadata.getMessage();
      if (msg.isAllNotesOff() || msg.isAllSoundOff()) {
        panic = true;
        held_.fill(false);
        sustained_.fill(false);
        pedal_ = false;
        exciter_.prepare(rate_);
        continue;
      }
      if (msg.isController() && msg.getControllerNumber() == 64) {
        pedal_ = msg.getControllerValue() >= 64;
        if (!pedal_) {
          sustained_.fill(false);
          release = (c.persist || value(gate) >= .5F) &&
                    std::none_of(held_.begin(), held_.end(),
                                 [](bool v) { return v; });
        }
        continue;
      }
      if (!msg.isNoteOnOrOff()) continue;
      const bool on = msg.isNoteOn();
      if (value(inputMode) < .5F) {
        if (msg.getNoteNumber() < 24 || msg.getNoteNumber() > 100) {
          rejected_.fetch_add(1);
          continue;
        }
        held_[static_cast<std::size_t>(msg.getNoteNumber())] = on;
        if (on) {
          selected.root = msg.getNoteNumber();
          strike = true;
          velocity_ = msg.getFloatVelocity();
        } else
          release = (c.persist || value(gate) >= .5F) && !pedal_ &&
                    std::none_of(held_.begin(), held_.end(),
                                 [](bool v) { return v; });
      } else if (key(msg.getNoteNumber(), on)) {
        if (on) {
          strike = true;
          velocity_ = msg.getFloatVelocity();
        } else
          release = (c.persist || value(gate) >= .5F) && !pedal_ &&
                    std::none_of(held_.begin(), held_.end(),
                                 [](bool v) { return v; });
      }
    }
    if (value(inputMode) >= .5F) selected = heldChord();
    if (panic) {
      strike = false;
      change = false;
      exciter_.prepare(rate_);
      strikes_ = 0;
    }
    if (strike) {
      const auto kind = static_cast<Excitation>(
          juce::jlimit(0, 4, static_cast<int>(value(source))));
      excitationGain_ = kind == Excitation::Impulse ? 2.F : 1.F;
      exciter_.trigger(kind, c.seed + (++strikes_) * 7919U,
                       static_cast<std::size_t>(rate_ * value(burst) * .001));
    }
    std::array<Event, 3> events{};
    std::size_t count = 0;
    if (change || strike)
      events[count++] = {0, strike ? EventType::Strike : EventType::Retune,
                         selected};
    if (release && !strike) events[count++] = {0, EventType::Release};
    if (panic) {
      events[count++] = {0, EventType::Panic};
      panic = false;
    }
    float input = exciter_.next() * excitationGain_ * value(excitation) *
                  .005F * (.3F + .7F * velocity_);
    float l = 0, r = 0;
    engine_.process(&input, &l, &r, 1, c, std::span(events).first(count));
    l = std::tanh(3.F * l);
    r = std::tanh(3.F * r);
    audio.setSample(0, n, audio.getNumChannels() > 1 ? l : .5F * (l + r));
    if (audio.getNumChannels() > 1) audio.setSample(1, n, r);
  }
  midiTuningActive_.store(
      std::any_of(held_.begin(), held_.end(), [](bool v) { return v; }) ||
      std::any_of(sustained_.begin(), sustained_.end(),
                  [](bool v) { return v; }));
  const auto snapshot = engine_.snapshot();
  for (std::size_t b = 0; b < 2; ++b)
    for (std::size_t m = 0; m < 12; ++m) {
      frequencies_[b * 12 + m].store(
          static_cast<float>(snapshot.bankFrequencies[b][m]));
      energies_[b * 12 + m].store(
          static_cast<float>(snapshot.bankEnergies[b][m]));
    }
}

namespace {
class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit Editor(ResonatorAudioProcessor& p)
      : AudioProcessorEditor{p}, p_{p}, master_{p.state()} {
    setLookAndFeel(&look_);
    addAndMakeVisible(master_);
    response_.bind([this] { return p_.usesMaterialResponse(); },
                   [this] { p_.useCurrentResponse(); },
                   "Adopt the new material behavior. This changes sound and "
                   "interaction while preserving saved parameter values.");
    addAndMakeVisible(response_);
    workflow_.bind(*this, "EXCITE > TUNE THE STRUCTURE > SHAPE MEMORY > MASTER",
                   "Choose an exciter and chord, then EXCITE or play MIDI. "
                   "Retune moves existing energy; Clear Energy stops it. "
                   "Partial Retain works only in Partial memory mode.");
    addAndMakeVisible(workflow_);
    scenes_.addItemList({"Shared minor structure", "Short impacts",
                         "Moving memory", "Resonant body"},
                        1);
    scenes_.setTextWhenNothingSelected("STARTING SOUNDS");
    scenes_.setTitle("STARTING SOUNDS");
    for (int index = 0; index < 4; ++index) scenes_.addSceneSnapshot(p_, index);
    scenes_.watch(p_);
    scenes_.onChange = [this] {
      p_.scene(scenes_.getSelectedId() - 1);
      scenes_.remember(p_);
    };
    addAndMakeVisible(scenes_);
    excite_.setButtonText("EXCITE");
    excite_.setTooltip(
        "Inject new energy using the selected exciter and harmonic structure. "
        "Also works without MIDI.");
    excite_.onClick = [this] { p_.excite(); };
    addAndMakeVisible(excite_);
    retune_.setButtonText("RETUNE EXISTING ENERGY");
    retune_.setTooltip(
        "Move the tuning of the energy already in the resonators; this does "
        "not excite new sound.");
    retune_.onClick = [this] { p_.retune(); };
    addAndMakeVisible(retune_);
    clear_.setButtonText("CLEAR ENERGY");
    clear_.onClick = [this] { p_.clear(); };
    addAndMakeVisible(clear_);
    for (const auto& spec : p_.specs()) {
      controls_.push_back(std::make_unique<Control>(p_, spec));
      addAndMakeVisible(*controls_.back());
    }
    setResizable(true, true);
    setResizeLimits(1000, 740, 1800, 1200);
    setSize(1240, 840);
    timerCallback();
    startTimerHz(25);
  }
  ~Editor() override { setLookAndFeel(nullptr); }
  void resized() override {
    response_.setBounds(getWidth() - 224, getHeight() - 28, 200, 24);
    workflow_.setBounds(24, getHeight() - 80, getWidth() - 48, 48);
    master_.setBounds(getWidth() - 214, 16, 190, 46);
    scenes_.setBounds(getWidth() - 464, 28, 230, 28);
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(58);
    area.removeFromBottom(66);
    auto actions = area.removeFromTop(42);
    const int column = area.getWidth() / 3;
    excite_.setBounds(actions.removeFromLeft(column).reduced(6, 2));
    retune_.setBounds(actions.removeFromLeft(column).reduced(6, 2));
    clear_.setBounds(actions.reduced(6, 2));
    auto sections = area.removeFromTop(std::max(330, area.getHeight() - 140));
    const std::array<std::vector<std::size_t>, 3> groups{
        {{0, 1, 2}, {3, 4, 5, 9, 10, 14, 15}, {6, 7, 8, 11, 12, 13, 16}}};
    for (std::size_t group = 0; group < 3; ++group) {
      auto section =
          sections.removeFromLeft(group == 2 ? sections.getWidth() : column)
              .reduced(6, 0);
      groupTitles_[group] = section.removeFromTop(22);
      const int columns = group == 0 ? 1 : 2, rows = group == 0 ? 3 : 4;
      for (std::size_t item = 0; item < groups[group].size(); ++item)
        controls_[groups[group][item]]->setBounds(
            section.getX() +
                static_cast<int>(item % columns) * section.getWidth() / columns,
            section.getY() +
                static_cast<int>(item / columns) * section.getHeight() / rows,
            section.getWidth() / columns, section.getHeight() / rows);
    }
    energy_ = area;
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{0xff101417});
    g.setColour(juce::Colour{0xffe7ece8});
    g.setFont(juce::FontOptions{22.F, juce::Font::bold});
    g.drawText("RESONATOR", 24, 18, 280, 30, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions{11.F});
    g.drawText("R-01 / EXCITATION -> HARMONIC ENERGY", 24, 49, 430, 16,
               juce::Justification::centredLeft);
    g.setFont(juce::FontOptions{9.F});
    g.drawText("OUT " + aste::ui::levelText(displayedPeak_), getWidth() - 214,
               64, 190, 12, juce::Justification::centredRight);
    const std::array<const char*, 3> headings{
        "1 / EXCITATION", "2 / HARMONIC STRUCTURE", "3 / ENERGY + MEMORY"};
    g.setFont(juce::FontOptions{11.F, juce::Font::bold});
    for (std::size_t i = 0; i < 3; ++i)
      g.drawText(headings[i], groupTitles_[i],
                 juce::Justification::centredLeft);
    auto area = energy_;
    g.setColour(juce::Colour{0xff1c2729});
    g.fillRect(area);
    g.setColour(juce::Colour{0xffe7ece8});
    g.drawText("ACTUAL MODAL STATE / TWO MEMORY BANKS",
               area.removeFromTop(22).reduced(8, 0),
               juce::Justification::centredLeft);
    const int row = std::max(1, area.getHeight() / 2),
              width = std::max(1, (area.getWidth() - 24) / 12);
    for (std::size_t b = 0; b < 2; ++b)
      for (std::size_t m = 0; m < 12; ++m) {
        const float energy = p_.energy(b, m);
        const float height =
            std::clamp(std::sqrt(std::max(0.F, energy)) * .5F, 0.F, 1.F) *
            std::max(1, row - 24);
        const int x = area.getX() + 12 + static_cast<int>(m) * width,
                  y = area.getY() + static_cast<int>(b) * row;
        g.setColour(juce::Colour{b ? 0xff7ba995U : 0xffb8c9bfU});
        g.fillRect(static_cast<float>(x),
                   static_cast<float>(y + row - 20) - height,
                   static_cast<float>(width - 7), height);
        g.setFont(juce::FontOptions{9.F});
        g.drawText(juce::String(p_.frequency(b, m), 0) + " Hz", x, y + row - 18,
                   width - 4, 16, juce::Justification::centred);
      }
    g.setColour(juce::Colour{0xffe7ece8});
    g.setFont(juce::FontOptions{10.F});
    g.drawText(
        "Play MIDI or EXCITE. Individual-tone mode: four notes, two-octave "
        "span. Controls recall; live energy starts empty.  Rejected notes: " +
            juce::String(p_.rejected()),
        24, getHeight() - 28, getWidth() - 260, 22,
        juce::Justification::centredLeft);
  }

 private:
  void timerCallback() override {
    const bool partial =
        static_cast<int>(p_.value(ResonatorAudioProcessor::memory)) == 2;
    controls_[12]->setEnabled(partial);
    controls_[12]->setDescription(
        partial
            ? "Energy kept before each new excitation in Partial memory mode."
            : "Inactive: choose Partial in Harmonic Memory to use Partial "
              "Retain.");
    const bool roots = p_.value(ResonatorAudioProcessor::inputMode) < .5F ||
                       !p_.midiTuningActive();
    for (const auto i : {3, 4}) {
      controls_[i]->setEnabled(roots);
      controls_[i]->setDescription(
          roots ? "Tune the chord structure. Play a root note or press EXCITE."
                : "Inactive: individual-tone MIDI mode takes pitches from the "
                  "notes you play.");
    }
    displayedPeak_ = std::max(p_.outputPeak(), displayedPeak_ * .85F);
    repaint();
  }
  float displayedPeak_{};
  Look look_;
  ResonatorAudioProcessor& p_;
  aste::ui::MasterOutput master_;
  aste::ui::WorkflowGuide workflow_;
  aste::ui::ResponseButton response_;
  std::vector<std::unique_ptr<Control>> controls_;
  aste::ui::PresetMenu scenes_;
  juce::TextButton excite_, retune_, clear_;
  std::array<juce::Rectangle<int>, 3> groupTitles_;
  juce::Rectangle<int> energy_;
};
}  // namespace
juce::AudioProcessorEditor* ResonatorAudioProcessor::createEditor() {
  return new Editor{*this};
}
}  // namespace aste::resonator::plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::resonator::plugin::ResonatorAudioProcessor;
}
