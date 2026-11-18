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
    meters_[n * meterFields].store(m.storedRms);
    meters_[n * meterFields + 1].store(m.outputRms);
    meters_[n * meterFields + 2].store(static_cast<float>(m.active));
    meters_[n * meterFields + 3].store(static_cast<float>(m.dropped));
    meters_[n * meterFields + 4].store(static_cast<float>(m.captureStalls));
    meters_[n * meterFields + 5].store(m.frozen ? 1.F : 0.F);
    meters_[n * meterFields + 6].store(static_cast<float>(m.storedFrames));
    meters_[n * meterFields + 7].store(m.captureRms);
    meters_[n * meterFields + 8].store(m.capturing ? 1.F : 0.F);
    meters_[n * meterFields + 9].store(
        static_cast<float>(m.storedFrames / rate_));
    meters_[n * meterFields + 10].store(m.effectiveDensity);
  }
}
#include "grain_editor.hpp"
juce::AudioProcessorEditor* GrainAudioProcessor::createEditor() {
  return new Editor{*this};
}
}  // namespace aste::grain::plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::grain::plugin::GrainAudioProcessor;
}
