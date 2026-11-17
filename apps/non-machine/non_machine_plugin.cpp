#include "non_machine_plugin.hpp"

namespace aste::nonmachine::plugin {
using namespace aste::experimental;
const std::array<const char*, NonMachineAudioProcessor::fields>
    NonMachineAudioProcessor::fieldIds{
        "enabled",      "position",    "gain",       "pitch",
        "decay",        "color",       "repeats",    "interval",
        "acceleration", "repeat_gain", "pitch_step", "mutation",
        "probability",  "every",       "nudge",      "articulation"};
juce::String NonMachineAudioProcessor::eventId(std::size_t v, std::size_t e,
                                               const char* field) {
  return "v" + juce::String(static_cast<int>(v + 1)) + "_e" +
         juce::String(static_cast<int>(e + 1)) + "_" + field;
}
juce::String NonMachineAudioProcessor::trackId(std::size_t v,
                                               const char* field) {
  return "v" + juce::String(static_cast<int>(v + 1)) + "_" + field;
}
std::vector<ControlSpec> NonMachineAudioProcessor::controls() {
  std::vector<ControlSpec> s{
      toggle("sequence", "PATTERN (OFF: MIDI)", true),
      knob("seed", "SEED", 0, 65535, 1701, 0, 1),
      knob("phrase", "PHRASE qn", 1, 32, 8, 0, .25F),
      knob("void_start", "VOID START %", 0, 100, 81.25F),
      knob("void_length", "VOID LENGTH %", 0, 100, 18.75F),
      choice("time_mode", "TIME OWNER", "Continue|Suspend"),
      knob("fragment", "MASTER FRAGMENT ms", 5, 500, 150, 80),
      knob("lookback", "LOOKBACK ms", 0, 500, 0),
      toggle("hard", "HARD CUT"),
      toggle("hold", "HOLD MASTER"),
      toggle("reverse", "REVERSE MASTER")};
  const auto defaults = studyPattern();
  for (std::size_t v = 0; v < 3; ++v) {
    s.push_back(knob(trackId(v, "cycle"), "CYCLE qn", .25F, 32,
                     static_cast<float>(defaults.tracks[v].length), 0, .25F));
    s.push_back(knob(trackId(v, "phase"), "PHASE qn", 0, 32, 0, 0, .01F));
    s.push_back(knob(trackId(v, "fragment"), "LOCAL ms", 5, 250, 40, 40));
    s.push_back(toggle(trackId(v, "hold"), "LOCAL HOLD"));
  }
  for (std::size_t v = 0; v < 3; ++v)
    for (std::size_t e = 0; e < 8; ++e) {
      const auto& event = defaults.tracks[v].events[e];
      const auto id = [&](const char* field) { return eventId(v, e, field); };
      s.push_back(toggle(id("enabled"), "EVENT ON", event.enabled));
      s.push_back(knob(
          id("position"), "CYCLE POSITION %", 0, 99.99F,
          static_cast<float>(event.beat / defaults.tracks[v].length * 100)));
      s.push_back(knob(id("gain"), "LEVEL %", 0, 100, event.gain * 100));
      s.push_back(knob(id("pitch"), "PITCH Hz", 20, 8000, event.pitch, 300));
      s.push_back(
          knob(id("decay"), "DECAY ms", 2, 2000, event.decay * 1000, 120));
      s.push_back(knob(id("color"), "COLOR %", 0, 100, event.color * 100));
      s.push_back(knob(id("repeats"), "REPEATS", 1, 8,
                       static_cast<float>(event.repeats), 0, 1));
      s.push_back(knob(id("interval"), "INTERVAL qn", .001F, 1,
                       static_cast<float>(event.interval), .0625F, .001F));
      s.push_back(knob(id("acceleration"), "ACCELERATION", .5F, 2,
                       static_cast<float>(event.acceleration)));
      s.push_back(knob(id("repeat_gain"), "REPEAT LEVEL %", 0, 100,
                       event.repeatGain * 100));
      s.push_back(
          knob(id("pitch_step"), "REPEAT PITCH st", -12, 12, event.pitchStep));
      s.push_back(
          knob(id("mutation"), "MUTATION %", 0, 100, event.mutation * 100));
      s.push_back(knob(id("probability"), "PROBABILITY %", 0, 100,
                       event.probability * 100));
      s.push_back(knob(id("every"), "EVERY N CYCLES", 1, 8,
                       static_cast<float>(event.every), 0, 1));
      s.push_back(knob(id("nudge"), "NUDGE qn", -.125F, .125F,
                       static_cast<float>(event.microtiming), 0, .001F));
      s.push_back(choice(id("articulation"), "ARTICULATION",
                         "Restart|Carry|Crossfade", 2));
    }
  for (std::size_t v = 0; v < 3; ++v) {
    const auto append = [&](ControlSpec control) {
      control.appended = true;
      s.push_back(std::move(control));
    };
    append(choice(
        trackId(v, "material"), "MATERIAL",
        "Original|Pressure|Resonant metal|Noise object|Broken fragment"));
    append(knob(trackId(v, "structure"), "STRUCTURE %", 0, 100, 50));
    append(knob(trackId(v, "strike"), "STRIKE %", 0, 100, 50));
    append(knob(trackId(v, "bend"), "PITCH FALL st", -24, 24, 0));
    append(knob(trackId(v, "roughness"), "ROUGHNESS %", 0, 100, 0));
  }
  for (std::size_t v = 0; v < 3; ++v)
    for (std::size_t e = 1; e < 8; ++e) {
      auto lock = toggle(eventId(v, e, "sound_lock"), "LOCK HIT SOUND");
      lock.appended = true;
      s.push_back(std::move(lock));
    }
  return s;
}
NonMachineAudioProcessor::NonMachineAudioProcessor()
    : Instrument{"Non-Machine N-01", "non-machine-n01", controls()} {
  scene(3);
}
Pattern NonMachineAudioProcessor::pattern() const noexcept {
  Pattern p;
  p.materialBehavior = usesMaterialResponse();
  p.seed = static_cast<std::uint64_t>(value(seed));
  p.phraseLength = value(phrase);
  p.voidStart = p.phraseLength * value(voidStart) * .01;
  p.voidEnd = std::min(p.phraseLength,
                       p.voidStart + p.phraseLength * value(voidLength) * .01);
  for (std::size_t v = 0; v < 3; ++v) {
    auto& t = p.tracks[v];
    t.length = value(trackBase + v * 4);
    t.phase = value(trackBase + v * 4 + 1);
    const auto material = materialIndex(v);
    t.material = static_cast<unsigned>(value(material));
    t.structure = value(material + 1) * .01F;
    t.strike = value(material + 2) * .01F;
    t.bend = value(material + 3);
    t.roughness = value(material + 4) * .01F;
    for (std::size_t e = 0; e < 8; ++e) {
      const auto i = eventIndex(v, e);
      auto& a = t.events[e];
      a.enabled = value(i) >= .5F;
      a.beat = value(i + 1) * .01 * t.length;
      a.gain = value(i + 2) * .01F;
      a.pitch = value(i + 3);
      a.decay = value(i + 4) * .001F;
      a.color = value(i + 5) * .01F;
      a.repeats = static_cast<unsigned>(std::round(value(i + 6)));
      a.interval = value(i + 7);
      a.acceleration = value(i + 8);
      a.repeatGain = value(i + 9) * .01F;
      a.pitchStep = value(i + 10);
      a.mutation = value(i + 11) * .01F;
      a.probability = value(i + 12) * .01F;
      a.every = static_cast<unsigned>(std::round(value(i + 13)));
      a.microtiming = value(i + 14);
      a.articulation =
          static_cast<Articulation>(static_cast<int>(value(i + 15)));
      if (usesMaterialResponse() && e != 0 && !hitLocked(v, e)) {
        const auto base = eventIndex(v, 0);
        a.gain = value(base + 2) * .01F;
        a.pitch = value(base + 3);
        a.decay = value(base + 4) * .001F;
        a.color = value(base + 5) * .01F;
      }
    }
  }
  return p;
}
bool NonMachineAudioProcessor::prepareEngine(double rate, int) {
  samples_ = 0;
  wasRunning_ = reversed_ = false;
  auditions_.store(0);
  const auto p = pattern();
  invalid_.store(!valid(p));
  previewRemaining_ = previewElapsed_ = 0;
  previewSample_ = 0;
  ignoredMidi_.store(0);
  return engine_.prepare(rate, valid(p) ? p : studyPattern()) &&
         preview_.prepare(rate, valid(p) ? p : studyPattern());
}
void NonMachineAudioProcessor::onRestore() noexcept {
  clearRequested_.store(true);
}
void NonMachineAudioProcessor::clear() {
  set("hold", 0);
  set("reverse", 0);
  for (std::size_t v = 0; v < 3; ++v) set(trackId(v, "hold"), 0);
  clearRequested_.store(true);
}
void NonMachineAudioProcessor::scene(int index) {
  if (index == 3) useCurrentResponse();
  editing_.store(true);
  defaults();
  clear();
  if (index == 1) {
    const std::array<float, 3> cycles{3, 4, 2.5F};
    const std::array<std::array<float, 4>, 3> positions{
        {{0, 37.5F, 75, -1}, {12.5F, 50, 87.5F, -1}, {25, 62.5F, -1, -1}}};
    for (std::size_t v = 0; v < 3; ++v) {
      set(trackId(v, "cycle"), cycles[v]);
      for (std::size_t e = 0; e < 8; ++e) {
        set(eventId(v, e, "enabled"),
            e < 4 && positions[v][e] >= 0 ? 1.F : 0.F);
        if (e < 4 && positions[v][e] >= 0)
          set(eventId(v, e, "position"), positions[v][e]);
      }
    }
    set("void_start", 62.5F);
    set("void_length", 37.5F);
    set(eventId(1, 0, "repeats"), 6);
    set(eventId(1, 0, "interval"), .04F);
    set(eventId(1, 0, "acceleration"), .65F);
    set(eventId(1, 0, "decay"), 180);
  } else if (index == 2) {
    const std::array<float, 3> cycles{5, 3.5F, 4};
    const std::array<std::array<float, 4>, 3> positions{
        {{0, 60, -1, -1},
         {14.2857F, 57.1429F, 85.7143F, -1},
         {0, 12.5F, 43.75F, 75}}};
    for (std::size_t v = 0; v < 3; ++v) {
      set(trackId(v, "cycle"), cycles[v]);
      for (std::size_t e = 0; e < 8; ++e) {
        set(eventId(v, e, "enabled"),
            e < 4 && positions[v][e] >= 0 ? 1.F : 0.F);
        if (e < 4 && positions[v][e] >= 0)
          set(eventId(v, e, "position"), positions[v][e]);
      }
    }
    set("void_start", 75);
    set("void_length", 25);
    set("seed", 1937);
    set("time_mode", 1);
    set("fragment", 85);
    set(eventId(0, 0, "pitch"), 35);
    set(eventId(0, 0, "decay"), 450);
    set(eventId(2, 0, "color"), 90);
    set(eventId(2, 0, "repeats"), 6);
    set(eventId(2, 0, "interval"), .035F);
  }
  if (index == 3) {
    set("seed", 2317);
    if (usesMaterialResponse()) {
      for (std::size_t v = 0; v < 3; ++v) {
        set(trackId(v, "cycle"), 8);
        for (std::size_t e = 0; e < 8; ++e) set(eventId(v, e, "enabled"), 0);
        set(eventId(v, 0, "enabled"), 1);
        set(eventId(v, 0, "position"), v == 0 ? 0.F : v == 1 ? 37.5F : 62.5F);
      }
      set(eventId(0, 1, "enabled"), 1);
      set(eventId(0, 1, "position"), 50);
    } else {
      generateRhythm(false);
      editing_.store(true);
    }
    set("void_start", 72);
    set("void_length", 28);
    for (std::size_t v = 0; v < 3; ++v) {
      set(trackId(v, "material"), v == 0 ? 1.F : v == 1 ? 2.F : 4.F);
      set(trackId(v, "structure"), v == 1 ? 73.F : 35.F);
      set(trackId(v, "strike"), v == 2 ? 90.F : 65.F);
      set(trackId(v, "bend"), v == 0 ? 19.F : -7.F);
      set(trackId(v, "roughness"), v == 2 ? 70.F : 15.F);
    }
  }
  editing_.store(false);
}
void NonMachineAudioProcessor::applyHitSoundToObject(std::size_t voice,
                                                     std::size_t event) {
  if (voice >= 3 || event >= 8) return;
  editing_.store(true);
  const auto base = eventIndex(voice, event);
  const std::array<std::size_t, 5> fields{2, 3, 4, 5, 15};
  std::array<float, 5> sound{};
  for (std::size_t i = 0; i < fields.size(); ++i)
    sound[i] = value(base + fields[i]);
  for (std::size_t e = 0; e < 8; ++e)
    for (std::size_t i = 0; i < fields.size(); ++i)
      set(eventId(voice, e, fieldIds[fields[i]]), sound[i]);
  editing_.store(false);
}
void NonMachineAudioProcessor::lockHitSound(std::size_t voice,
                                            std::size_t event, bool enabled) {
  if (voice >= 3 || event == 0 || event >= 8) return;
  editing_.store(true);
  if (enabled && !hitLocked(voice, event))
    for (std::size_t field = 2; field < 6; ++field)
      set(eventId(voice, event, fieldIds[field]),
          value(eventIndex(voice, 0) + field));
  set(eventId(voice, event, "sound_lock"), enabled ? 1.F : 0.F);
  editing_.store(false);
}
void NonMachineAudioProcessor::emptyPattern() {
  editing_.store(true);
  clear();
  for (std::size_t v = 0; v < 3; ++v)
    for (std::size_t e = 0; e < 8; ++e) set(eventId(v, e, "enabled"), 0);
  editing_.store(false);
}
void NonMachineAudioProcessor::generateRhythm(bool advanceSeed) {
  editing_.store(true);
  clear();
  auto seedValue = static_cast<unsigned>(value(seed));
  if (advanceSeed) seedValue = (seedValue + 1U) & 65535U;
  set("seed", static_cast<float>(seedValue));
  std::uint32_t random = seedValue ^ 0x9e3779b9U;
  const auto next = [&]() {
    random ^= random << 13;
    random ^= random >> 17;
    random ^= random << 5;
    return random;
  };
  for (std::size_t v = 0; v < 3; ++v) {
    std::array<bool, 16> steps{};
    const auto count = 2U + next() % 3U;
    unsigned filled = 0;
    while (filled < count) {
      const auto step = next() % 16U;
      if (!steps[step]) {
        steps[step] = true;
        ++filled;
      }
    }
    const auto base = eventIndex(v, 0);
    const std::array<float, 4> tone{value(base + 2), value(base + 3),
                                    value(base + 4), value(base + 5)};
    std::size_t slot = 0;
    for (std::size_t step = 0; step < 16; ++step)
      if (steps[step]) {
        set(eventId(v, slot, "enabled"), 1);
        set(eventId(v, slot, "position"), static_cast<float>(step) * 6.25F);
        set(eventId(v, slot, "gain"), tone[0]);
        set(eventId(v, slot, "pitch"), tone[1]);
        set(eventId(v, slot, "decay"), tone[2]);
        set(eventId(v, slot, "color"), tone[3]);
        set(eventId(v, slot, "repeats"), next() % 4U == 0 ? 2.F : 1.F);
        set(eventId(v, slot, "interval"), .0625F);
        set(eventId(v, slot, "acceleration"), 1);
        set(eventId(v, slot, "repeat_gain"), 75);
        set(eventId(v, slot, "pitch_step"), 0);
        set(eventId(v, slot, "probability"), 100);
        set(eventId(v, slot, "every"), 1);
        set(eventId(v, slot, "nudge"), 0);
        ++slot;
      }
    for (; slot < 8; ++slot) set(eventId(v, slot, "enabled"), 0);
  }
  editing_.store(false);
}
void NonMachineAudioProcessor::render(juce::AudioBuffer<float>& audio,
                                      const juce::MidiBuffer& midi) noexcept {
  const auto frames = static_cast<std::size_t>(audio.getNumSamples());
  if (!editing_.load()) {
    const auto p = pattern();
    invalid_.store(!engine_.updatePattern(p));
    preview_.updatePattern(p);
    bool limited = false;
    for (const auto& track : p.tracks)
      for (const auto& e : track.events)
        if (e.enabled) {
          double duration = 0, interval = e.interval;
          for (unsigned r = 1; r < e.repeats; ++r) {
            duration += interval;
            interval *= e.acceleration;
          }
          limited |= duration >= track.length;
        }
    limited_.store(limited);
  }
  bool sequencing = false;
  double bpm = 120, ppq = samples_ * 120. / (60. * rate_);
  if (const auto* head = getPlayHead())
    if (const auto pos = head->getPosition()) {
      const auto tempo = pos->getBpm(), beat = pos->getPpqPosition();
      if (tempo && beat && std::isfinite(*tempo) && *tempo >= 20 &&
          *tempo <= 400 && std::isfinite(*beat)) {
        sequencing = value(sequence) >= .5F && pos->getIsPlaying();
        if (sequencing) {
          bpm = *tempo;
          ppq = *beat;
        }
      }
    }
  std::array<Command, 16> commands{};
  std::size_t count = 0;
  const bool clear = clearRequested_.exchange(false) ||
                     rejoin_.exchange(false) || sequencing != wasRunning_;
  if (clear) previewRemaining_ = 0;
  if (clear) commands[count++] = {0, 3, Action::Rejoin};
  bool waiting = false;
  bool masterCaptured = false;
  for (std::size_t target = 0; target < 4; ++target) {
    const bool wanted =
        value(target == 3 ? hold : trackBase + target * 4 + 3) >= .5F;
    const auto length = static_cast<std::size_t>(
        rate_ * .001 *
        value(target == 3 ? fragment : trackBase + target * 4 + 2));
    const auto back =
        target == 3 ? static_cast<std::size_t>(rate_ * .001 * value(lookback))
                    : 0;
    const bool active = !clear && engine_.fragmentActive(target);
    if (wanted && !active) {
      if (!clear && engine_.history(target) >= length + back) {
        capturedLength_[target] = length;
        masterCaptured |= target == 3;
        commands[count++] = {
            0,
            target,
            Action::Capture,
            length,
            back,
            0,
            value(timeMode) >= .5F ? Mode::Suspend : Mode::Continue,
            value(hard) >= .5F};
      } else
        waiting = true;
    } else if (wanted && active && length < capturedLength_[target]) {
      commands[count++] = {0, target, Action::Shorten, length};
      capturedLength_[target] = length;
    } else if (!wanted && active)
      commands[count++] = {0, target, Action::Release, 0,
                           0, 0,      Mode::Continue,  value(hard) >= .5F};
  }
  const bool reversed = value(reverse) >= .5F;
  if ((engine_.fragmentActive(3) || masterCaptured) &&
      reversed != (masterCaptured ? false : reversed_) && !clear)
    commands[count++] = {0, 3, Action::Reverse};
  if ((engine_.fragmentActive(3) || masterCaptured) && !clear)
    reversed_ = reversed;
  else
    reversed_ = false;
  std::array<ManualHit, 256> manual{};
  std::size_t hits = 0;
  const auto requests = auditions_.exchange(0);
  for (std::size_t i = 0; i < 24; ++i)
    if (!usesMaterialResponse() && (requests & (1U << i)))
      manual[hits++] = {0, i / 8, i % 8, 1};
  for (const auto metadata : midi) {
    if (metadata.numBytes > 3) continue;
    const auto msg = metadata.getMessage();
    if (usesMaterialResponse() && value(sequence) >= .5F && msg.isNoteOn()) {
      ignoredMidi_.fetch_add(1);
      continue;
    }
    if (msg.isNoteOn() && msg.getNoteNumber() % 12 < 3 && hits < manual.size())
      manual[hits++] = {
          static_cast<std::size_t>(juce::jlimit(0, audio.getNumSamples() - 1,
                                                metadata.samplePosition)),
          static_cast<std::size_t>(msg.getNoteNumber() % 12), 0,
          msg.getFloatVelocity()};
  }
  engine_.process(
      audio.getWritePointer(0),
      audio.getNumChannels() > 1 ? audio.getWritePointer(1) : nullptr, frames,
      {sequencing, true, sequencing != wasRunning_, samples_, ppq, bpm,
       !sequencing},
      nullptr, std::span(commands).first(count), std::span(manual).first(hits));
  unsigned owner = value(sequence) >= .5F ? (sequencing ? 1U : 0U) : 2U;
  if (engine_.fragmentActive(3))
    owner = 4;
  else
    for (std::size_t v = 0; v < 3; ++v)
      if (engine_.fragmentActive(v)) owner = 3;
  if (usesMaterialResponse() && (requests || previewRemaining_ > 0)) {
    std::array<ManualHit, 24> previewHits{};
    std::size_t previewCount = 0;
    if (requests) {
      double seconds = .2;
      for (std::size_t i = 0; i < 24; ++i)
        if (requests & (1U << i)) {
          previewHits[previewCount++] = {0, i / 8, i % 8, 1};
          seconds = std::max(
              seconds,
              static_cast<double>(value(eventIndex(i / 8, i % 8) + 4)) * .006);
        }
      previewRemaining_ =
          static_cast<std::size_t>(rate_ * std::min(seconds, 8.));
    }
    std::array<float, 256> l{}, r{};
    for (std::size_t at = 0; at < frames; at += 256) {
      const auto n = std::min(std::size_t{256}, frames - at);
      preview_.process(l.data(), r.data(), n,
                       {false, true, requests != 0 && at == 0, previewSample_,
                        previewSample_ * 2. / rate_, 120, true},
                       nullptr, {},
                       at == 0 ? std::span(previewHits).first(previewCount)
                               : std::span<const ManualHit>{});
      previewSample_ += static_cast<std::int64_t>(n);
      for (std::size_t i = 0; i < n && previewRemaining_ > 0;
           ++i, --previewRemaining_) {
        const float fade = std::min(
            1.F,
            static_cast<float>(std::min(previewRemaining_, previewElapsed_++)) /
                static_cast<float>(std::max(1., rate_ * .003)));
        audio.setSample(
            0, static_cast<int>(at + i),
            l[i] * fade +
                (1.F - fade) * audio.getSample(0, static_cast<int>(at + i)));
        if (audio.getNumChannels() > 1)
          audio.setSample(
              1, static_cast<int>(at + i),
              r[i] * fade +
                  (1.F - fade) * audio.getSample(1, static_cast<int>(at + i)));
      }
    }
    owner = 5;
  }
  soundOwner_.store(owner);
  // Instrument output level, independently adjustable by the clean Master
  // stage.
  for (int ch = 0; ch < audio.getNumChannels(); ++ch)
    for (int n = 0; n < audio.getNumSamples(); ++n)
      audio.setSample(ch, n, std::tanh(2.F * audio.getSample(ch, n)));
  phrase_.store(static_cast<float>(engine_.phraseBeat()));
  waiting_.store(waiting);
  running_.store(sequencing);
  unsigned held = 0;
  for (std::size_t target = 0; target < 4; ++target)
    if (engine_.fragmentActive(target)) held |= 1U << target;
  held_.store(held);
  wasRunning_ = sequencing;
  samples_ += static_cast<std::int64_t>(frames);
}

namespace {
class EventLane final : public juce::Component,
                        public juce::SettableTooltipClient {
 public:
  EventLane(NonMachineAudioProcessor& p, std::size_t voice,
            std::function<void(std::size_t)> select)
      : p_(p), voice_(voice), select_(std::move(select)) {
    setWantsKeyboardFocus(true);
    setTitle("Voice " + juce::String(static_cast<int>(voice + 1)) +
             " event timeline");
    setDescription(
        "Click to place or select an event; drag to move. Double-click "
        "removes. Left/right move, Tab selects numbered event buttons, Delete "
        "removes. Exact controls below.");
    setTooltip(
        "Click space to add, drag a marker to move, double-click to remove");
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{0xff202a2c});
    for (int i = 0; i <= 16; ++i) {
      g.setColour(juce::Colour{0xff405052}.withAlpha(i % 4 == 0 ? .7F : .25F));
      g.drawVerticalLine(6 + (getWidth() - 12) * i / 16, 0.F,
                         static_cast<float>(getHeight()));
    }
    const double length =
        p_.value(NonMachineAudioProcessor::trackBase + voice_ * 4);
    for (std::size_t e = 0; e < 8; ++e) {
      const auto base = NonMachineAudioProcessor::eventIndex(voice_, e);
      if (p_.value(base) < .5F) continue;
      const float position = p_.value(base + 1) * .01F;
      g.setColour(juce::Colour{e == selected_ ? 0xfff1c078 : 0xffc4d1ca});
      const float x = 6 + (getWidth() - 12) * position;
      g.fillEllipse(x - 4, 8.F, 8.F, 8.F);
      double offset = 0, interval = p_.value(base + 7);
      for (int r = 1; r < static_cast<int>(p_.value(base + 6)); ++r) {
        offset += interval;
        interval *= p_.value(base + 8);
        if (offset >= length) break;
        const float repeat =
            6 + (getWidth() - 12) * static_cast<float>(std::fmod(
                                        position + offset / length, 1.));
        g.drawVerticalLine(static_cast<int>(repeat), 19.F, 25.F);
      }
    }
    if (p_.running()) {
      const double phase =
          p_.value(NonMachineAudioProcessor::trackBase + voice_ * 4 + 1);
      double at = std::fmod(p_.phrasePosition() - phase, length);
      if (at < 0) at += length;
      g.setColour(juce::Colour{0xff7ca5b2});
      g.drawVerticalLine(6 + static_cast<int>((getWidth() - 12) * at / length),
                         0.F, static_cast<float>(getHeight()));
    }
    if (hasKeyboardFocus(true)) {
      g.setColour(juce::Colours::white);
      g.drawRect(getLocalBounds(), 1);
    }
  }
  void mouseDown(const juce::MouseEvent& e) override {
    grabKeyboardFocus();
    const float position = at(e.position.x);
    float nearest = 9.F;
    int found = -1;
    for (std::size_t slot = 0; slot < 8; ++slot) {
      const auto base = NonMachineAudioProcessor::eventIndex(voice_, slot);
      const float distance =
          std::abs(p_.value(base + 1) - position) * (getWidth() - 12) * .01F;
      if (p_.value(base) >= .5F && distance < nearest) {
        nearest = distance;
        found = static_cast<int>(slot);
      }
    }
    if (found < 0)
      for (std::size_t slot = 0; slot < 8; ++slot)
        if (p_.value(NonMachineAudioProcessor::eventIndex(voice_, slot)) <
            .5F) {
          found = static_cast<int>(slot);
          p_.set(NonMachineAudioProcessor::eventId(voice_, slot, "position"),
                 position);
          p_.set(NonMachineAudioProcessor::eventId(voice_, slot, "enabled"), 1);
          break;
        }
    dragging_ = found >= 0;
    if (dragging_) {
      selected_ = static_cast<std::size_t>(found);
      select_(selected_);
    }
  }
  void mouseDrag(const juce::MouseEvent& e) override {
    if (dragging_) move(at(e.position.x));
  }
  void mouseUp(const juce::MouseEvent&) override { dragging_ = false; }
  void mouseDoubleClick(const juce::MouseEvent&) override {
    p_.set(NonMachineAudioProcessor::eventId(voice_, selected_, "enabled"), 0);
    repaint();
  }
  bool keyPressed(const juce::KeyPress& key) override {
    if (key.getKeyCode() == juce::KeyPress::leftKey ||
        key.getKeyCode() == juce::KeyPress::rightKey) {
      const float delta = key.getModifiers().isShiftDown() ? .1F : 6.25F;
      move(std::clamp(
          p_.value(NonMachineAudioProcessor::eventIndex(voice_, selected_) +
                   1) +
              (key.getKeyCode() == juce::KeyPress::leftKey ? -delta : delta),
          0.F, 99.99F));
      return true;
    }
    if (key.getKeyCode() == juce::KeyPress::deleteKey ||
        key.getKeyCode() == juce::KeyPress::backspaceKey) {
      p_.set(NonMachineAudioProcessor::eventId(voice_, selected_, "enabled"),
             0);
      repaint();
      return true;
    }
    return false;
  }
  void selected(std::size_t event) {
    selected_ = event;
    repaint();
  }

 private:
  float at(float x) const {
    return std::clamp((x - 6) / std::max(1, getWidth() - 12) * 100.F, 0.F,
                      99.99F);
  }
  void move(float position) {
    p_.set(NonMachineAudioProcessor::eventId(voice_, selected_, "position"),
           position);
    repaint();
  }
  NonMachineAudioProcessor& p_;
  std::size_t voice_{}, selected_{};
  bool dragging_{};
  std::function<void(std::size_t)> select_;
};
class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit Editor(NonMachineAudioProcessor& p)
      : AudioProcessorEditor{p}, p_{p}, master_{p.state()} {
    setLookAndFeel(&look_);
    addAndMakeVisible(master_);
    response_.bind([this] { return p_.usesMaterialResponse(); },
                   [this] { p_.useCurrentResponse(); },
                   "Adopt the new material behavior. This changes sound and "
                   "interaction while preserving saved parameter values.");
    addAndMakeVisible(response_);
    workflow_.bind(*this,
                   "CHOOSE MATERIAL > HIT > PLACE EVENTS > EDIT HIT LOCKS > "
                   "REPEAT / RELEASE",
                   "Select an object and HIT to hear it. Material shapes all "
                   "its hits; the event inspector changes one selected hit. "
                   "EMPTY PATTERN starts from silence.");
    addAndMakeVisible(workflow_);
    scenes_.addItemList({"Sparse time", "Accelerating cuts", "Suspended dust",
                         "Objects / build"},
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
    applySound_.setButtonText("USE HIT SOUND FOR ALL HITS");
    applySound_.setTitle("APPLY HIT SOUND TO OBJECT");
    applySound_.setTooltip(
        "Copy only Level, Pitch, Decay, Color and Articulation to this "
        "object's hits. Positions, repeats, conditions and enable states stay "
        "as programmed.");
    soundLock_.setButtonText("LOCK THIS HIT SOUND");
    soundLock_.setTitle("HIT SOUND LOCK");
    addAndMakeVisible(soundLock_);
    applySound_.onClick = [this] {
      if (p_.usesMaterialResponse())
        p_.audition(selected_ / 8, selected_ % 8);
      else
        p_.applyHitSoundToObject(selected_ / 8, selected_ % 8);
    };
    addAndMakeVisible(applySound_);
    empty_.setButtonText("EMPTY PATTERN");
    empty_.onClick = [this] { p_.emptyPattern(); };
    addAndMakeVisible(empty_);
    fresh_.setButtonText("NEW RHYTHM");
    fresh_.onClick = [this] { p_.generateRhythm(); };
    addAndMakeVisible(fresh_);
    clear_.setButtonText("CLEAR / RELEASE");
    clear_.onClick = [this] { p_.clear(); };
    addAndMakeVisible(clear_);
    rejoin_.setButtonText("REJOIN HOST");
    rejoin_.onClick = [this] { p_.rejoin(); };
    addAndMakeVisible(rejoin_);
    for (std::size_t i = 0; i < NonMachineAudioProcessor::globalCount; ++i) {
      globals_.push_back(std::make_unique<Control>(p_, p_.specs()[i], true));
      addAndMakeVisible(*globals_.back());
    }
    globals_[NonMachineAudioProcessor::timeMode]->setDescription(
        "Continue advances the phrase behind a held master fragment; Suspend "
        "pauses the underlying phrase until release.");
    globals_[NonMachineAudioProcessor::reverse]->setDescription(
        "Reverse the held master audio fragment. Hold Master must capture "
        "audio before a reversal is audible.");
    globals_[NonMachineAudioProcessor::lookback]->setDescription(
        "Choose how far back in audio history the captured master fragment "
        "begins.");
    for (std::size_t v = 0; v < 3; ++v) {
      eventLanes_[v] = std::make_unique<EventLane>(
          p_, v, [this, v](std::size_t e) { select(v * 8 + e); });
      addAndMakeVisible(*eventLanes_[v]);
      for (std::size_t f = 0; f < 4; ++f) {
        tracks_[v][f] = std::make_unique<Control>(
            p_, p_.specs()[NonMachineAudioProcessor::trackBase + v * 4 + f],
            true);
        addAndMakeVisible(*tracks_[v][f]);
      }
      auditions_[v].setButtonText("HIT");
      auditions_[v].setTitle(juce::String{"Audition "} + names_[v]);
      auditions_[v].onClick = [this, v] {
        p_.audition(v, p_.usesMaterialResponse()
                           ? 0
                           : (selected_ / 8 == v ? selected_ % 8 : 0));
      };
      addAndMakeVisible(auditions_[v]);
      for (std::size_t e = 0; e < 8; ++e) {
        auto& b = events_[v][e];
        b.setButtonText(juce::String(static_cast<int>(e + 1)));
        b.setTitle(juce::String{names_[v]} + " event " +
                   juce::String(static_cast<int>(e + 1)));
        b.setTooltip("Select event; Shift-click enables or disables it");
        b.onClick = [this, v, e] {
          if (juce::ModifierKeys::getCurrentModifiersRealtime().isShiftDown())
            p_.set(NonMachineAudioProcessor::eventId(v, e, "enabled"),
                   p_.value(NonMachineAudioProcessor::eventIndex(v, e)) >= .5F
                       ? 0.F
                       : 1.F);
          select(v * 8 + e);
        };
        addAndMakeVisible(b);
      }
    }
    setResizable(true, true);
    setResizeLimits(1080, 780, 1800, 1200);
    setSize(1320, 860);
    select(0);
    timerCallback();
    startTimerHz(25);
  }
  ~Editor() override { setLookAndFeel(nullptr); }
  void resized() override {
    response_.setBounds(24, getHeight() - 28, 180, 24);
    workflow_.setBounds(24, getHeight() - 80, getWidth() - 48, 48);
    master_.setBounds(getWidth() - 214, 16, 190, 46);
    scenes_.setBounds(getWidth() - 444, 28, 210, 28);
    empty_.setBounds(getWidth() - 724, 28, 130, 28);
    fresh_.setBounds(getWidth() - 584, 28, 130, 28);
    auto area = getLocalBounds().reduced(24);
    area.removeFromTop(54);
    area.removeFromBottom(66);
    const int w = area.getWidth() / 6;
    auto global = area.removeFromTop(104);
    for (std::size_t i = 0; i < globals_.size(); ++i)
      globals_[i]->setBounds(global.getX() + static_cast<int>(i % 6) * w,
                             global.getY() + static_cast<int>(i / 6) * 50, w,
                             48);
    timeline_ = area.removeFromTop(58);
    area.removeFromTop(8);
    for (std::size_t v = 0; v < 3; ++v) {
      auto row = area.removeFromTop(62);
      lanes_[v] = row;
      row.removeFromLeft(86);
      for (std::size_t f = 0; f < 4; ++f)
        tracks_[v][f]->setBounds(row.removeFromLeft(78));
      auditions_[v].setBounds(row.removeFromLeft(46).reduced(2, 15));
      eventLanes_[v]->setBounds(row.withTrimmedTop(28).reduced(2, 0));
      auto buttons = row.removeFromTop(26);
      const int bw = buttons.getWidth() / 8;
      for (std::size_t e = 0; e < 8; ++e)
        events_[v][e].setBounds(buttons.removeFromLeft(bw).reduced(2, 0));
    }
    auto header = area.removeFromTop(30);
    applySound_.setBounds(header.removeFromRight(210));
    soundLock_.setBounds(header.removeFromRight(220));
    inspectorHeader_ = header;
    auto material = area.removeFromTop(p_.usesMaterialResponse() ? 56 : 64);
    for (auto& control : materials_)
      control->setBounds(material.removeFromLeft(area.getWidth() / 5));
    if (p_.usesMaterialResponse()) {
      auto object = area.removeFromTop(52);
      const int width = object.getWidth() / 4;
      for (auto& control : objectSound_)
        control->setBounds(object.removeFromLeft(width));
    }
    const int cw = area.getWidth() / 4,
              ch = area.getHeight() / (p_.usesMaterialResponse() ? 3 : 4);
    for (std::size_t i = 0; i < inspector_.size(); ++i)
      inspector_[i]->setBounds(area.getX() + static_cast<int>(i % 4) * cw,
                               area.getY() + static_cast<int>(i / 4) * ch, cw,
                               ch);
    clear_.setBounds(getWidth() - 310, getHeight() - 28, 160, 24);
    rejoin_.setBounds(getWidth() - 146, getHeight() - 28, 122, 24);
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{0xff101417});
    g.setColour(juce::Colour{0xffe7ece8});
    g.setFont(juce::FontOptions{22.F, juce::Font::bold});
    g.drawText("NON-MACHINE", 24, 18, 300, 30,
               juce::Justification::centredLeft);
    g.setFont(juce::FontOptions{11.F});
    g.drawText("N-01 / THREE OBJECTS / EXPLICIT TIME", 24, 49, 380, 16,
               juce::Justification::centredLeft);
    g.setFont(juce::FontOptions{9.F});
    g.drawText("OUT " + aste::ui::levelText(displayedPeak_), getWidth() - 214,
               64, 190, 12, juce::Justification::centredRight);
    g.setColour(juce::Colour{0xff202a2c});
    g.fillRect(timeline_);
    const float phrase = p_.value(NonMachineAudioProcessor::phrase);
    const float start = p_.value(NonMachineAudioProcessor::voidStart) * .01F;
    const float length = std::min(
        1.F - start, p_.value(NonMachineAudioProcessor::voidLength) * .01F);
    g.setColour(juce::Colour{0xff514448});
    g.fillRect(
        timeline_.getX() + static_cast<int>(start * timeline_.getWidth()),
        timeline_.getY(), static_cast<int>(length * timeline_.getWidth()),
        timeline_.getHeight());
    g.setColour(juce::Colour{0xffe7ece8});
    g.drawText("MASTER PHRASE / VOID", timeline_.reduced(8),
               juce::Justification::topLeft);
    const float phase =
        std::fmod(std::max(0.F, p_.phrasePosition()), phrase) / phrase;
    const int x =
        timeline_.getX() + static_cast<int>(phase * timeline_.getWidth());
    if (p_.running())
      g.drawVerticalLine(x, static_cast<float>(timeline_.getY() + 20),
                         static_cast<float>(timeline_.getBottom()));
    for (std::size_t v = 0; v < 3; ++v)
      g.drawText(names_[v], lanes_[v].withWidth(86),
                 juce::Justification::centredLeft);
    g.drawText(juce::String{names_[selected_ / 8]} + " / EVENT " +
                   juce::String(static_cast<int>(selected_ % 8 + 1)) +
                   " / MATERIAL: whole object / LOCKS: selected hit",
               inspectorHeader_, juce::Justification::centredLeft);
    juce::String status =
        p_.invalidPattern()   ? "PATTERN INVALID: check timing values"
        : p_.heldTargets()    ? "CAPTURE HELD: release to hear pattern edits"
        : p_.limitedRepeats() ? "Repeats limited to each voice cycle"
        : p_.waiting()        ? "HOLD waiting for enough history; selection + "
                                "lookback must fit 500 ms"
        : p_.running() ? "DAW CLOCK / MIDI C, C#, D excite objects A, B, C"
                       : "Start DAW playback, or use HIT / MIDI to audition";

    if (p_.usesMaterialResponse()) {
      const std::array<const char*, 6> causes{"PATTERN / WAITING FOR DAW",
                                              "PATTERN",
                                              "MIDI OBJECT INPUT",
                                              "LOCAL CAPTURE + OBJECTS",
                                              "MASTER CAPTURE",
                                              "AUDITION / ISOLATED HIT"};
      status = causes[std::min(5U, p_.soundOwner())];
      if (p_.ignoredMidi() > 0 &&
          p_.value(NonMachineAudioProcessor::sequence) >= .5F)
        status += " / MIDI ignored in Pattern mode";
    }
    g.setFont(juce::FontOptions{10.F});
    g.drawText(status, 212, getHeight() - 27, getWidth() - 550, 22,
               juce::Justification::centredLeft);
  }

 private:
  void select(std::size_t selected) {
    selected_ = selected;
    if (eventLanes_[selected / 8])
      eventLanes_[selected / 8]->selected(selected % 8);
    inspector_.clear();
    materials_.clear();
    objectSound_.clear();
    lastMaterial_ = p_.usesMaterialResponse();
    lastLock_ = p_.hitLocked(selected / 8, selected % 8);
    soundLock_.setVisible(lastMaterial_);
    soundLock_.setEnabled(selected % 8 != 0);
    soundLock_.setTooltip(
        "When off, this hit uses the object sound above. When on, the sound "
        "controls edit only this hit. Timing is always per hit.");
    soundLock_.setToggleState(lastLock_, juce::dontSendNotification);
    soundLock_.onClick = [this] {
      p_.lockHitSound(selected_ / 8, selected_ % 8,
                      soundLock_.getToggleState());
    };
    if (lastMaterial_) {
      const auto bodyEvent = selected % 8 != 0 && lastLock_ ? selected % 8 : 0;
      for (std::size_t field = 2; field < 6; ++field) {
        auto spec = p_.specs()[NonMachineAudioProcessor::eventIndex(
                                   selected / 8, bodyEvent) +
                               field];
        spec.name = juce::String(bodyEvent ? "HIT " : "OBJECT ") + spec.name;
        objectSound_.push_back(std::make_unique<Control>(p_, spec, true));
        objectSound_.back()->setDescription(
            bodyEvent ? "Sound lock for the selected hit only."
                      : "Base object sound, used by object HIT, MIDI, and "
                        "every hit without a sound lock.");
        addAndMakeVisible(*objectSound_.back());
      }
    }
    for (std::size_t f = 0; f < 5; ++f) {
      materials_.push_back(std::make_unique<Control>(
          p_,
          p_.specs()[NonMachineAudioProcessor::materialIndex(selected / 8) + f],
          true));
      addAndMakeVisible(*materials_.back());
    }
    const auto i =
        NonMachineAudioProcessor::eventIndex(selected / 8, selected % 8);
    const std::array<std::size_t, 12> timing{0,  1,  6,  7,  8,  9,
                                             10, 11, 12, 13, 14, 15};
    for (std::size_t f = 0; f < (lastMaterial_ ? timing.size() : 16); ++f) {
      const auto field = lastMaterial_ ? timing[f] : f;
      inspector_.push_back(
          std::make_unique<Control>(p_, p_.specs()[i + field], true));
      addAndMakeVisible(*inspector_.back());
    }
    resized();
    repaint();
  }
  void timerCallback() override {
    const bool locked = p_.hitLocked(selected_ / 8, selected_ % 8);
    soundLock_.setToggleState(locked, juce::dontSendNotification);
    if (lastMaterial_ != p_.usesMaterialResponse() || lastLock_ != locked)
      select(selected_);
    applySound_.setButtonText(p_.usesMaterialResponse()
                                  ? "HEAR SELECTED HIT"
                                  : "USE HIT SOUND FOR ALL HITS");
    displayedPeak_ = std::max(p_.outputPeak(), displayedPeak_ * .85F);
    for (std::size_t v = 0; v < 3; ++v)
      for (std::size_t e = 0; e < 8; ++e) {
        auto& b = events_[v][e];
        b.setButtonText(juce::String(static_cast<int>(e + 1)));
        b.setToggleState(selected_ == v * 8 + e, juce::dontSendNotification);
        b.setAlpha(p_.value(NonMachineAudioProcessor::eventIndex(v, e)) >= .5F
                       ? 1.F
                       : .38F);
      }
    const bool material =
        p_.value(NonMachineAudioProcessor::materialIndex(selected_ / 8)) > 0.F;
    for (std::size_t i = 1; i < materials_.size(); ++i) {
      materials_[i]->setEnabled(material);
      materials_[i]->setDescription(
          material
              ? aste::ui::explanation(
                    p_.specs()[NonMachineAudioProcessor::materialIndex(
                                   selected_ / 8) +
                               i]
                        .id,
                    p_.getName())
              : "Inactive: choose a new Material. Original retains the earlier "
                "source formula; event Pitch, Decay and Color still work.");
    }
    for (auto& lane : eventLanes_) lane->repaint();
    repaint();
  }
  float displayedPeak_{};
  Look look_;
  NonMachineAudioProcessor& p_;
  aste::ui::MasterOutput master_;
  aste::ui::WorkflowGuide workflow_;
  aste::ui::ResponseButton response_;
  aste::ui::PresetMenu scenes_;
  juce::ToggleButton soundLock_;
  bool lastMaterial_{}, lastLock_{};
  juce::TextButton clear_, rejoin_, fresh_, empty_, applySound_;
  std::vector<std::unique_ptr<Control>> globals_, inspector_, materials_,
      objectSound_;
  std::array<std::array<std::unique_ptr<Control>, 4>, 3> tracks_;
  std::array<std::array<juce::TextButton, 8>, 3> events_;
  std::array<std::unique_ptr<EventLane>, 3> eventLanes_;
  std::array<juce::TextButton, 3> auditions_;
  std::size_t selected_{};
  juce::Rectangle<int> timeline_, inspectorHeader_;
  std::array<juce::Rectangle<int>, 3> lanes_;
  const std::array<const char*, 3> names_{"OBJECT A", "OBJECT B", "OBJECT C"};
};
}  // namespace
juce::AudioProcessorEditor* NonMachineAudioProcessor::createEditor() {
  return new Editor{*this};
}
}  // namespace aste::nonmachine::plugin
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::nonmachine::plugin::NonMachineAudioProcessor;
}
