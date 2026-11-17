#include "grain_plugin.hpp"
#include "workflow_guide.hpp"
#include "audio_state.hpp"
namespace aste::grain::plugin {
using namespace aste::experimental;
const std::array<const char*, GrainAudioProcessor::nodeFields>
    GrainAudioProcessor::fieldIds{
        "size",        "density",   "speed",           "pitch",
        "position",    "variation", "time_scatter",    "stereo_scatter",
        "width",       "pan",       "pan_drift",       "erosion",
        "inheritance", "level",     "capture_seconds", "accumulate",
        "division",    "cluster",   "capture",         "freeze",
        "reverse",     "sync",      "time_mode",       "window"};
juce::String GrainAudioProcessor::nodeId(std::size_t n, const char* field) {
  return "n" + juce::String(static_cast<int>(n + 1)) + "_" + field;
}
juce::String GrainAudioProcessor::routeId(std::size_t d, std::size_t s) {
  return "route_" + juce::String(static_cast<int>(s + 1)) + "_to_" +
         juce::String(static_cast<int>(d + 1));
}
std::vector<ControlSpec> GrainAudioProcessor::controls() {
  std::vector<ControlSpec> specs{
      knob("time", "TIME", -1, 1, 0),
      knob("spread", "SPREAD", 0, 1, .15F),
      knob("erosion", "EROSION", 0, 1, .1F),
      knob("inheritance", "INHERITANCE", 0, 1, 1),
      knob("mix", "DRY / NETWORK", 0, 1, .65F),
      toggle("freeze_all", "FREEZE ALL"),
      knob("cycle_morph", "MORPH TO CYCLE", 0, 1, 0),
      knob("seed", "SEED / NEXT CLEAR", 0, 65535, 1701, 0, 1)};
  for (std::size_t n = 0; n < 3; ++n)
    specs.push_back(knob(
        nodeId(n, "live"),
        "LIVE > " +
            juce::String::charToString(static_cast<juce::juce_wchar>('A' + n)),
        0, 1.5F, n == 0 ? 1.F : 0.F));
  for (std::size_t d = 0; d < 3; ++d)
    for (std::size_t s = 0; s < 3; ++s)
      specs.push_back(knob(
          routeId(d, s),
          juce::String::charToString(static_cast<juce::juce_wchar>('A' + s)) +
              " > " +
              juce::String::charToString(
                  static_cast<juce::juce_wchar>('A' + d)),
          0, 1.5F, d == s + 1 ? .85F : 0.F));
  for (std::size_t n = 0; n < 3; ++n) {
    auto add = [&](ControlSpec c) { specs.push_back(std::move(c)); };
    auto k = [&](const char* id, const char* title, float lo, float hi,
                 float init, float centre = 0) {
      add(knob(nodeId(n, id), title, lo, hi, init, centre));
    };
    k("size", "GRAIN ms", 1, 2000,
      n == 0   ? 120.F
      : n == 1 ? 250.F
               : 700.F,
      120);
    k("density", "GRAINS / SECOND", 0, 240,
      n == 0   ? 18.F
      : n == 1 ? 12.F
               : 6.F,
      16);
    k("speed", "SPEED", 0, 8, n == 0 ? 1.F : n == 1 ? .45F : .08F, 1);
    k("pitch", "PITCH st", -24, 24, n == 1 ? 7.F : n == 2 ? -12.F : 0.F);
    k("position", "POSITION", 0, 1, 0);
    k("variation", "POSITION VARIATION", 0, 1, .15F);
    k("time_scatter", "TIME SCATTER", 0, 1, .1F);
    k("stereo_scatter", "STEREO SCATTER", 0, 1, n == 1 ? .6F : .15F);
    k("width", "SOURCE WIDTH", 0, 1, 1);
    k("pan", "BALANCE", -1, 1, 0);
    k("pan_drift", "PAN DRIFT", 0, 1, .1F);
    k("erosion", "NODE EROSION", 0, 1, 0);
    k("inheritance", "NODE INHERITANCE", 0, 1, 1);
    k("level", "MONITOR LEVEL", 0, 1, n == 0 ? .4F : n == 1 ? .65F : .8F);
    k("capture_seconds", "MEMORY seconds", .02F, 4, .5F, .5F);
    k("accumulate", "ACCUMULATE", 0, .995F, 0);
    add(knob(nodeId(n, "division"), "SYNC qn", 1.F / 64, 4, .25F, 0, 1.F / 64));
    k("cluster", "CLUSTER", 0, 1, 0);
    add(toggle(nodeId(n, "capture"), "CAPTURE", true));
    add(toggle(nodeId(n, "freeze"), "FREEZE NODE"));
    add(toggle(nodeId(n, "reverse"), "REVERSE"));
    add(toggle(nodeId(n, "sync"), "DAW SYNC"));
    add(choice(nodeId(n, "time_mode"), "TIME MODE", "Independent|Varispeed"));
    add(choice(nodeId(n, "window"), "GRAIN EDGE", "Smooth|Triangle|Fragment"));
  }
  return specs;
}
GrainAudioProcessor::GrainAudioProcessor()
    : Instrument{"Grain G-01", "grain-g01", controls(), true} {}
Controls GrainAudioProcessor::currentControls() const noexcept {
  Controls c;
  c.time = value(0);
  c.spread = value(1);
  c.erosion = value(2);
  c.inheritance = value(3);
  c.mix = value(4);
  c.freeze = value(5) >= .5F;
  c.seed = static_cast<std::uint64_t>(value(7));
  c.master = 1;
  for (std::size_t n = 0; n < 3; ++n) {
    c.live[n] = value(liveBase + n);
    auto& a = c.node[n];
    auto i = nodeBase + n * nodeFields;
    a.sizeMs = value(i);
    a.density = value(i + 1);
    a.speed = value(i + 2);
    a.pitch = value(i + 3);
    a.position = value(i + 4);
    a.variation = value(i + 5);
    a.temporalSpread = value(i + 6);
    a.stereoSpread = value(i + 7);
    a.width = value(i + 8);
    a.pan = value(i + 9);
    a.panDrift = value(i + 10);
    a.erosion = value(i + 11);
    a.inheritance = value(i + 12);
    a.level = value(i + 13);
    a.captureSeconds = value(i + 14);
    a.accumulate = value(i + 15);
    a.division = value(i + 16);
    a.cluster = value(i + 17);
    a.capture = value(i + 18) >= .5F;
    a.freeze = value(i + 19) >= .5F;
    a.reverse = value(i + 20) >= .5F;
    a.sync = value(i + 21) >= .5F;
    a.mode = static_cast<TimeMode>(static_cast<int>(value(i + 22)));
    a.window = static_cast<Window>(static_cast<int>(value(i + 23)));
    a.interpolation = Interpolation::Sinc16;
    for (std::size_t s = 0; s < 3; ++s)
      c.routes[n][s] = value(routeBase + n * 3 + s);
  }
  const auto cycle = aste::grain::scene(3);
  const float amount = value(6);
  for (std::size_t d = 0; d < 3; ++d)
    for (std::size_t s = 0; s < 3; ++s)
      c.routes[d][s] += amount * (cycle.routes[d][s] - c.routes[d][s]);
  return c;
}
void GrainAudioProcessor::mutate(std::size_t n) {
  if (n < 3)
    set(nodeId(n, "position"),
        std::fmod(value(nodeBase + n * nodeFields + 4) + .38196601125F, 1.F));
}
bool GrainAudioProcessor::prepareEngine(double rate, int) {
  const std::lock_guard lock{lifecycle_};
  adoptPending();
  auto memory = engine_.load()->dsp.memoryState();
  auto ready = std::make_unique<OwnedEngine>();
  if (!ready->dsp.prepare(rate)) return false;
  ready->dsp.restoreMemory(memory);
  delete engine_.exchange(ready.release());
  collectRetired();
  hostRate_ = rate;
  prepared_ = true;
  for (auto& m : meters_) m.store(0);
  return true;
}
GrainAudioProcessor::~GrainAudioProcessor() {
  delete pending_.exchange(nullptr);
  delete engine_.exchange(nullptr);
  collectRetired();
}
void GrainAudioProcessor::adoptPending() noexcept {
  if (auto* ready = pending_.exchange(nullptr)) {
    auto* old = engine_.exchange(ready);
    auto* head = retired_.load();
    do {
      old->next = head;
    } while (!retired_.compare_exchange_weak(head, old));
  }
}
void GrainAudioProcessor::collectRetired() {
  auto* old = retired_.exchange(nullptr);
  while (old) {
    auto* next = old->next;
    delete old;
    old = next;
  }
}
void GrainAudioProcessor::writeProductMetadata(juce::ValueTree& tree) const {
  const std::lock_guard lock{lifecycle_};
  auto* source = pending_.load();
  if (!source) source = engine_.load();
  const auto memory = source->dsp.memoryState();
  juce::ValueTree saved{"MEMORY"};
  saved.setProperty("format", 1, nullptr);
  for (std::size_t i = 0; i < nodes; ++i)
    saved.appendChild(
        aste::parameters::encodeAudio(
            juce::Identifier{"NODE" + juce::String(static_cast<int>(i))},
            memory.rate, memory.audio[i]),
        nullptr);
  tree.removeChild(tree.getChildWithName("MEMORY"), nullptr);
  tree.appendChild(saved, nullptr);
}
bool GrainAudioProcessor::validateProductMetadata(
    const juce::ValueTree& tree) const {
  int roots = 0;
  for (auto child : tree)
    if (child.hasType("MEMORY")) ++roots;
  if (roots == 0) return true;  // Original controls-only sessions.
  const auto saved = tree.getChildWithName("MEMORY");
  if (roots != 1 || saved.getNumChildren() != 3 ||
      saved.getProperty("format").toString() != "1")
    return false;
  double rate = 0;
  for (int i = 0; i < 3; ++i) {
    const auto audio = aste::parameters::decodeAudio(
        saved.getChildWithName("NODE" + juce::String(i)), 192000 * 4, 44100);
    if (!audio || audio->stereo.size() / 2 > audio->rate * 4 ||
        (i > 0 && rate != audio->rate))
      return false;
    rate = audio->rate;
  }
  return true;
}
void GrainAudioProcessor::restoreProductMetadata(const juce::ValueTree& tree) {
  const std::lock_guard lock{lifecycle_};
  MemoryState memory;
  const auto saved = tree.getChildWithName("MEMORY");
  if (saved.isValid())
    for (int i = 0; i < 3; ++i) {
      auto audio = aste::parameters::decodeAudio(
          saved.getChildWithName("NODE" + juce::String(i)), 192000 * 4, 44100);
      memory.rate = audio->rate;
      memory.audio[static_cast<std::size_t>(i)] = std::move(audio->stereo);
    }
  auto ready = std::make_unique<OwnedEngine>();
  if (!ready->dsp.prepare(prepared_ ? hostRate_ : memory.rate)) return;
  ready->dsp.restoreMemory(memory);
  clearRequested_.store(false);
  clearMask_.store(0);
  delete pending_.exchange(ready.release());
  collectRetired();
}
void GrainAudioProcessor::setStateInformation(const void* data, int size) {
  if (!data || size <= 0 || size > 64 * 1024 * 1024) return;
  Instrument::setStateInformation(data, size);
}
void GrainAudioProcessor::scene(int index) {
  const auto c =
      aste::grain::scene(static_cast<unsigned>(juce::jlimit(0, 3, index)));
  set("cycle_morph", 0);
  for (std::size_t d = 0; d < 3; ++d) {
    set(nodeId(d, "live"), c.live[d]);
    for (std::size_t s = 0; s < 3; ++s) set(routeId(d, s), c.routes[d][s]);
  }
  updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}
void GrainAudioProcessor::render(juce::AudioBuffer<float>& b,
                                 const juce::MidiBuffer&) noexcept {
  adoptPending();
  auto& engine = engine_.load()->dsp;
  if (clearRequested_.exchange(false)) engine.reset();
  const auto mask = clearMask_.exchange(0);
  if (mask & 8U)
    engine.reset();
  else
    for (std::size_t n = 0; n < 3; ++n)
      if (mask & (1U << n)) engine.clear(n);
  Transport t;
  if (auto* host = getPlayHead())
    if (auto p = host->getPosition()) {
      if (auto bpm = p->getBpm()) t.bpm = *bpm;
      if (auto ppq = p->getPpqPosition()) {
        t.valid = true;
        t.ppq = *ppq;
      }
      t.playing = p->getIsPlaying();
    }
  // In-place stereo: core reads both input channels before writing either
  // output.
  auto* l = b.getWritePointer(0);
  if (b.getNumChannels() > 1) {
    auto* r = b.getWritePointer(1);
    engine.process(l, r, l, r, static_cast<std::size_t>(b.getNumSamples()),
                   currentControls(), t, std::span<const Event>{});
  } else {
    std::array<float, 256> right{};
    const auto c = currentControls();
    for (std::size_t at = 0; at < static_cast<std::size_t>(b.getNumSamples());
         at += right.size()) {
      auto frames = std::min(right.size(),
                             static_cast<std::size_t>(b.getNumSamples()) - at);
      auto clock = t;
      if (clock.valid && clock.playing)
        clock.ppq += at * clock.bpm / (60 * rate_);
      engine.process(l + at, nullptr, l + at, right.data(), frames, c, clock,
                     std::span<const Event>{});
      for (std::size_t i = 0; i < frames; ++i)
        l[at + i] = (l[at + i] + right[i]) * .5F;
    }
  }
  const auto s = engine.snapshot();
  for (std::size_t n = 0; n < 3; ++n) {
    const auto& m = s.node[n];
    meters_[n * 8].store(m.storedRms);
    meters_[n * 8 + 1].store(m.outputRms);
    meters_[n * 8 + 2].store(static_cast<float>(m.active));
    meters_[n * 8 + 3].store(static_cast<float>(m.dropped));
    meters_[n * 8 + 4].store(static_cast<float>(m.captureStalls));
    meters_[n * 8 + 5].store(m.frozen ? 1.F : 0.F);
    meters_[n * 8 + 6].store(static_cast<float>(m.storedFrames));
    meters_[n * 8 + 7].store(m.captureRms);
  }
}
namespace {
static_assert(std::atomic<float>::is_always_lock_free);
class Graph final : public juce::Component {
 public:
  Graph(GrainAudioProcessor& p) : p_(p) {
    setTitle("Granular routing graph");
    setDescription(
        "Select A, B or C using the buttons beside the graph. Matrix fields "
        "edit the same directed routes. Graph shows audible routing after "
        "morph and normalization.");
  }
  std::function<void(int)> select;
  int selected{};
  std::array<juce::Point<float>, 3> positions() const {
    return {{{getWidth() * .18F, getHeight() * .55F},
             {getWidth() * .5F, getHeight() * .22F},
             {getWidth() * .82F, getHeight() * .55F}}};
  }
  void mouseDown(const juce::MouseEvent& e) override {
    auto xy = positions();
    for (int i = 0; i < 3; ++i)
      if (xy[static_cast<std::size_t>(i)].getDistanceFrom(e.position) < 36 &&
          select)
        select(i);
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{0xff1b2526});
    auto xy = positions();
    const auto c = p_.currentControls();
    for (std::size_t d = 0; d < 3; ++d) {
      float sum = 0;
      for (auto x : c.routes[d]) sum += x;
      float scale = sum > .995F ? .995F / sum : 1;
      for (std::size_t s = 0; s < 3; ++s) {
        float amount = c.routes[d][s] * scale;
        if (amount < .005F) continue;
        g.setColour(juce::Colour{0xffb6ba77}.withAlpha(
            .35F + .65F * std::min(1.F, amount)));
        if (d == s) {
          auto p = xy[d];
          g.drawEllipse(p.x - 38, p.y - 42, 76, 66, 1 + 3 * amount);
          g.drawText("SELF", static_cast<int>(p.x) - 20,
                     static_cast<int>(p.y) - 59, 50, 16,
                     juce::Justification::centred);
          continue;
        }
        auto direction = (xy[d] - xy[s]);
        auto length = direction.getDistanceFromOrigin();
        direction /= length;
        auto normal = juce::Point<float>{-direction.y, direction.x} * 7;
        auto from = xy[s] + direction * 32 + normal,
             to = xy[d] - direction * 34 + normal;
        g.drawArrow({from, to}, 1 + 3 * amount, 8, 8);
      }
    }
    for (std::size_t n = 0; n < 3; ++n) {
      const auto p = xy[n];
      g.setColour(n == static_cast<std::size_t>(selected)
                      ? juce::Colour{0xff67704d}
                      : juce::Colour{0xff263735});
      g.fillEllipse(p.x - 27, p.y - 27, 54, 54);
      g.setColour(juce::Colour{0xffe7ece8});
      g.setFont(juce::FontOptions{19.F, juce::Font::bold});
      g.drawText(
          juce::String::charToString(static_cast<juce::juce_wchar>('A' + n)),
          static_cast<int>(p.x) - 20, static_cast<int>(p.y) - 17, 40, 34,
          juce::Justification::centred);
      g.setFont(juce::FontOptions{10.F});
      juce::String status = p_.nodeMeter(n, 6) > 0 ? "STORED" : "EMPTY";
      if (p_.nodeMeter(n, 5) > .5F)
        status += " / FROZEN";
      else if ((c.freeze || c.node[n].freeze) && p_.nodeMeter(n, 6) == 0)
        status = "ARMED / FEED AUDIO";
      g.drawText(status, static_cast<int>(p.x) - 65, static_cast<int>(p.y) + 31,
                 130, 16, juce::Justification::centred);
      g.drawText(juce::String(static_cast<int>(p_.nodeMeter(n, 2))) + " GRAINS",
                 static_cast<int>(p.x) - 65, static_cast<int>(p.y) + 48, 130,
                 16, juce::Justification::centred);
    }
  }

 private:
  GrainAudioProcessor& p_;
};
class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  Editor(GrainAudioProcessor& p)
      : AudioProcessorEditor{p}, p_(p), master_{p.state()}, graph_{p} {
    setLookAndFeel(&look_);
    addAndMakeVisible(master_);
    addAndMakeVisible(graph_);
    scene_.addItemList(
        {"Single A", "Cascade A > B > C", "Parallel", "Feedback cycle"}, 1);
    scene_.setTitle("ROUTING STARTING POINT");
    scene_.setTextWhenNothingSelected("ROUTING STARTING POINTS");
    scene_.onChange = [this] { p_.scene(scene_.getSelectedId() - 1); };
    addAndMakeVisible(scene_);
    for (std::size_t i = 0; i < p_.specs().size(); ++i) {
      controls_.push_back(std::make_unique<Control>(p_, p_.specs()[i], i >= 4));
      addAndMakeVisible(*controls_.back());
      controls_.back()->setExplicitFocusOrder(static_cast<int>(i + 10));
    }
    for (int i = 0; i < 3; ++i) {
      auto& button = nodes_[static_cast<std::size_t>(i)];
      button.setButtonText(
          "OBJECT " +
          juce::String::charToString(static_cast<juce::juce_wchar>('A' + i)));
      button.onClick = [this, i] { select(i); };
      button.setClickingTogglesState(false);
      addAndMakeVisible(button);
    }
    graph_.select = [this](int n) { select(n); };
    clear_.setButtonText("CLEAR SELECTED");
    clear_.onClick = [this] {
      p_.clearNode(static_cast<std::size_t>(selected_));
    };
    clear_.setTooltip(
        "Forget this node. Other nodes can send material back into it.");
    addAndMakeVisible(clear_);
    mutate_.setButtonText("MUTATE POSITION");
    mutate_.onClick = [this] {
      p_.mutate(static_cast<std::size_t>(selected_));
    };
    addAndMakeVisible(mutate_);
    reset_.setButtonText("CLEAR NETWORK");
    reset_.onClick = [this] { p_.clearAll(); };
    addAndMakeVisible(reset_);
    workflow_.bind(
        *this,
        "INPUT > CAPTURE > CONNECT > FRAGMENT > FREEZE / TRANSFORM > MASTER",
        "Feed audio; select A/B/C to edit its sound. Freeze retains that node. "
        "Output is monitor level: routes keep feeding. Clear Network forgets "
        "all material. Captured source banks save with the project. Render "
        "to audio when you need to preserve the exact running performance.");
    addAndMakeVisible(workflow_);
    setResizable(true, true);
    setResizeLimits(1100, 880, 1800, 1300);
    setSize(1280, 960);
    select(0);
    timerCallback();
    startTimerHz(20);
  }
  ~Editor() override { setLookAndFeel(nullptr); }
  void select(int n) {
    selected_ = n;
    graph_.selected = n;
    for (std::size_t i = 0; i < 3; ++i) {
      nodes_[i].setToggleState(static_cast<int>(i) == n,
                               juce::dontSendNotification);
      for (std::size_t f = 0; f < GrainAudioProcessor::nodeFields; ++f)
        controls_[GrainAudioProcessor::nodeBase +
                  i * GrainAudioProcessor::nodeFields + f]
            ->setVisible(static_cast<int>(i) == n);
    }
    resized();
    graph_.repaint();
  }
  void resized() override {
    master_.setBounds(getWidth() - 220, 18, 196, 46);
    scene_.setBounds(getWidth() - 510, 28, 270, 28);
    for (std::size_t i = 0; i < 4; ++i)
      controls_[i]->setBounds(24 + static_cast<int>(i) * (getWidth() - 48) / 4,
                              82, (getWidth() - 48) / 4, 95);
    for (std::size_t i = 4; i < 8; ++i)
      controls_[i]->setBounds(
          24 + static_cast<int>(i - 4) * (getWidth() - 48) / 4, 180,
          (getWidth() - 48) / 4, 54);
    int leftWidth = (getWidth() - 60) * 43 / 100;
    graph_.setBounds(24, 246, leftWidth, 210);
    for (std::size_t i = 0; i < 3; ++i)
      controls_[GrainAudioProcessor::liveBase + i]->setBounds(
          24 + static_cast<int>(i) * leftWidth / 3, 477, leftWidth / 3, 56);
    for (std::size_t d = 0; d < 3; ++d)
      for (std::size_t s = 0; s < 3; ++s)
        controls_[GrainAudioProcessor::routeBase + d * 3 + s]->setBounds(
            24 + static_cast<int>(s) * leftWidth / 3,
            538 + static_cast<int>(d) * 56, leftWidth / 3, 54);
    reset_.setBounds(24, 712, leftWidth, 28);
    int x = 36 + leftWidth, w = getWidth() - x - 24;
    for (std::size_t n = 0; n < 3; ++n)
      nodes_[n].setBounds(x + static_cast<int>(n) * w / 3, 246, w / 3 - 4, 30);
    clear_.setBounds(x, 282, w / 2 - 4, 28);
    mutate_.setBounds(x + w / 2, 282, w / 2, 28);
    int height = (getHeight() - 430) / 6;
    for (std::size_t f = 0; f < GrainAudioProcessor::nodeFields; ++f)
      controls_[GrainAudioProcessor::nodeBase +
                static_cast<std::size_t>(selected_) *
                    GrainAudioProcessor::nodeFields +
                f]
          ->setBounds(x + static_cast<int>(f % 4) * w / 4,
                      322 + static_cast<int>(f / 4) * height, w / 4,
                      height - 2);
    workflow_.setBounds(24, getHeight() - 76, getWidth() - 48, 52);
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{0xff101617});
    g.setColour(juce::Colour{0xffe7ece8});
    g.setFont(juce::FontOptions{24.F, juce::Font::bold});
    g.drawText("GRAIN", 24, 18, 260, 30, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions{11.F});
    g.drawText("G-01 / THREE MEMORIES / OPEN FRAGMENT NETWORK", 24, 51, 500, 18,
               juce::Justification::centredLeft);
    g.setFont(juce::FontOptions{10.F});
    g.drawText(
        "ROUTE INPUTS / EDIT BASE LEVELS / GRAPH SHOWS MORPH TARGET + LIMIT",
        24, 458, (getWidth() - 60) * 43 / 100, 18,
        juce::Justification::centredLeft);
    juce::String status;
    for (std::size_t n = 0; n < 3; ++n)
      status +=
          juce::String::charToString(static_cast<juce::juce_wchar>('A' + n)) +
          ": " + aste::ui::levelText(p_.nodeMeter(n, 1)) + "  " +
          (p_.nodeMeter(n, 4) > 0
               ? "waits:" + juce::String(p_.nodeMeter(n, 4), 0) + " "
               : "") +
          (p_.nodeMeter(n, 3) > 0
               ? "drops:" + juce::String(p_.nodeMeter(n, 3), 0) + " "
               : "") +
          "  ";
    g.drawFittedText(status, 24, 748, (getWidth() - 60) * 43 / 100, 45,
                     juce::Justification::topLeft, 3);
    g.drawText("OUT " + aste::ui::levelText(p_.outputPeak()), getWidth() - 220,
               65, 196, 14, juce::Justification::centredRight);
  }

 private:
  void timerCallback() override {
    int match = -1;
    if (p_.value(6) < .0001F)
      for (int i = 0; i < 4; ++i) {
        auto candidate = aste::grain::scene(static_cast<unsigned>(i));
        bool same = true;
        for (std::size_t d = 0; d < 3; ++d) {
          same &= std::abs(p_.value(GrainAudioProcessor::liveBase + d) -
                           candidate.live[d]) < .0001F;
          for (std::size_t s = 0; s < 3; ++s)
            same &=
                std::abs(p_.value(GrainAudioProcessor::routeBase + d * 3 + s) -
                         candidate.routes[d][s]) < .0001F;
        }
        if (same) match = i;
      }
    scene_.setTextWhenNothingSelected(
        p_.value(6) > .0001F ? "MORPH / CUSTOM ROUTES" : "CUSTOM ROUTES");
    scene_.setSelectedId(match + 1, juce::dontSendNotification);
    graph_.repaint();
    repaint();
  }
  GrainAudioProcessor& p_;
  Look look_;
  aste::ui::MasterOutput master_;
  Graph graph_;
  juce::ComboBox scene_;
  std::vector<std::unique_ptr<Control>> controls_;
  std::array<juce::TextButton, 3> nodes_;
  juce::TextButton clear_, mutate_, reset_;
  aste::ui::WorkflowGuide workflow_;
  int selected_{};
};
}  // namespace
juce::AudioProcessorEditor* GrainAudioProcessor::createEditor() {
  return new Editor{*this};
}
}  // namespace aste::grain::plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::grain::plugin::GrainAudioProcessor;
}
