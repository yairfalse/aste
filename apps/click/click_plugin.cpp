#include "click_plugin.hpp"
#include "audio_state.hpp"
#include <cmath>
#include <numbers>

namespace aste::click::plugin {
using namespace aste::experimental;
namespace {
std::array<float, 34> soundValues(const Object& o) {
  return std::array<float, 34>{static_cast<float>(o.source),
                               o.pitch,
                               o.tune,
                               o.excite,
                               o.body,
                               o.decay,
                               o.resonance,
                               o.noise,
                               o.character,
                               o.cutoff,
                               o.morph,
                               o.filterEnv,
                               o.level,
                               o.pan,
                               o.start,
                               o.end,
                               o.loopStart,
                               o.loopEnd,
                               o.loopMod,
                               o.bits,
                               o.colour,
                               static_cast<float>(o.interpolation),
                               o.reverse ? 1.F : 0.F,
                               o.loop ? 1.F : 0.F,
                               static_cast<float>(o.length),
                               static_cast<float>(o.division),
                               static_cast<float>(o.midi),
                               static_cast<float>(o.roll),
                               static_cast<float>(o.repeatSteps),
                               o.mute ? 1.F : 0.F,
                               o.solo ? 1.F : 0.F,
                               o.phaseReset ? 1.F : 0.F,
                               o.repeat ? 1.F : 0.F,
                               o.velocityResponse};
}
std::array<float, 19> eventValues(const Step& e) {
  return std::array<float, 19>{e.on ? 1.F : 0.F,
                               e.lock ? 1.F : 0.F,
                               e.velocity,
                               e.pitch,
                               e.start,
                               e.cutoff,
                               e.morph,
                               e.decay,
                               e.timing,
                               e.probability,
                               e.span,
                               e.curve,
                               e.endVelocity,
                               e.endPitch,
                               e.mutation,
                               e.irregular,
                               static_cast<float>(e.sound + 1),
                               static_cast<float>(e.condition),
                               static_cast<float>(e.count)};
}
}  // namespace

const std::array<const char*, Processor::objectFields> Processor::objectIds{
    "source",       "pitch",
    "tune",         "excite",
    "body",         "decay",
    "resonance",    "noise",
    "character",    "cutoff",
    "morph",        "filter_env",
    "level",        "pan",
    "start",        "end",
    "loop_start",   "loop_end",
    "loop_mod",     "bits",
    "colour",       "interpolation",
    "reverse",      "loop",
    "length",       "division",
    "midi",         "roll",
    "repeat_steps", "mute",
    "solo",         "phase_reset",
    "repeat",       "velocity_response"};
const std::array<const char*, Processor::eventFields> Processor::eventIds{
    "on",        "lock",  "velocity",     "pitch",     "start",
    "cutoff",    "morph", "decay",        "timing",    "probability",
    "span",      "curve", "end_velocity", "end_pitch", "mutation",
    "irregular", "sound", "condition",    "count"};
juce::String Processor::objectId(int n, const char* field) {
  return "o" + juce::String(n + 1) + "_" + field;
}
juce::String Processor::eventId(int n, int step, const char* field) {
  return "o" + juce::String(n + 1) + "_e" + juce::String(step + 1) + "_" +
         field;
}
const std::array<const char*, Processor::memoryFields> Processor::memoryIds{
    "start",  "end",       "loop_start", "loop_end", "pitch",  "speed",
    "cutoff", "resonance", "morph",      "playback", "reverse"};
juce::String Processor::memoryId(int n, const char* field) {
  return "m" + juce::String(n) + "_" + field;
}
std::vector<ControlSpec> Processor::controls() {
  std::vector<ControlSpec> specs{
      choice("mode", "PLAYBACK", "MIDI only|Host sequence", 1),
      knob("master_length", "MASTER STEPS", 1, 32, 16, 0, 1),
      knob("region_start", "FRAGMENT START", 0, 31, 0, 0, 1),
      knob("region_end", "FRAGMENT END", 1, 32, 16, 0, 1),
      toggle("repeat", "MASTER REPEAT"),
      toggle("nested", "INNER REPEAT"),
      toggle("reverse", "REVERSE FRAGMENT"),
      toggle("hold", "HOLD TIME"),
      choice("time_mode", "REPEAT TIME", "Continue|Hold"),
      toggle("void", "VOID"),
      toggle("crush", "CRUSH"),
      toggle("shift", "SHIFT"),
      knob("shift_steps", "SHIFT STEPS", -8, 8, 1, 0, 1),
      knob("inner_span", "INNER LOOP qn", .03125F, 4, .25F, 0, .03125F),
      knob("pressure", "PRESSURE %", 0, 100, 30),
      knob("recovery", "RECOVERY ms", 5, 2000, 180, 180),
      knob("dirt", "DIRT %", 0, 100, 20),
      knob("parallel", "PARALLEL %", 0, 100, 70),
      toggle("fill", "FILL"),
      knob("roll_span", "ROLL SPAN steps", .03125F, 8, 1, 0, .03125F),
      knob("swing", "SWING %", 0, 50, 0),
      knob("seed", "SEED", 1, 65535, 1701, 0, 1)};
  const auto defaults = factory(0);
  for (int n = 0; n < objects; ++n) {
    const auto& o = defaults.object[n];
    const auto k = [&](const char* id, const char* name, float lo, float hi,
                       float value, float centre = 0, float step = .01F) {
      specs.push_back(knob(objectId(n, id),
                           "OBJECT " + juce::String(n + 1) + " / " + name, lo,
                           hi, value, centre, step));
    };
    const auto t = [&](const char* id, const char* name) {
      specs.push_back(toggle(objectId(n, id), name));
    };
    specs.push_back(
        choice(objectId(n, "source"), "SOURCE", "Synthesis|Sample|Layer"));
    k("pitch", "FOUNDATION Hz", 18, 16000, o.pitch, 150);
    k("tune", "TUNE st", -48, 48, 0);
    k("excite", "EXCITATION", 0, 1, o.excite);
    k("body", "BODY", 0, 1, o.body);
    k("decay", "DECAY ms", .2F, 8000, o.decay, 150);
    k("resonance", "RESONANCE", 0, .95F, .25F);
    k("noise", "NOISE", 0, 1, o.noise);
    k("character", "CHARACTER", 0, 1, .2F);
    k("cutoff", "FILTER Hz", 30, 20000, o.cutoff, 2000);
    k("morph", "FILTER MORPH", 0, 1, 0);
    k("filter_env", "FILTER ENVELOPE", -1, 1, 0);
    k("level", "LEVEL", 0, 1, o.level);
    k("pan", "PAN", -1, 1, 0);
    k("start", "START", 0, .999999F, 0, 0, 0);
    k("end", "END", .000001F, 1, 1, 0, 0);
    k("loop_start", "LOOP START", 0, 1, 0, 0, 0);
    k("loop_end", "LOOP END", 0, 1, 1, 0, 0);
    k("loop_mod", "LOOP MOTION", -.5F, .5F, 0, 0, 0);
    k("bits", "BITS", 3, 24, 16, 0, 1);
    k("colour", "DIGITAL COLOUR", 0, 1, 0);
    specs.push_back(choice(objectId(n, "interpolation"), "INTERPOLATION",
                           "Nearest|Linear|Cubic", 1));
    t("reverse", "REVERSE SAMPLE");
    t("loop", "SAMPLE LOOP");
    k("length", "CYCLE STEPS", 1, 32, 16, 0, 1);
    specs.push_back(choice(objectId(n, "division"), "SUBDIVISION",
                           "1/32|1/16 triplet|1/16|1/8 triplet|1/8|1/4", 2));
    k("midi", "MIDI NOTE", 0, 127, static_cast<float>(o.midi), 0, 1);
    k("roll", "LOCAL ROLL", 1, 16, 1, 0, 1);
    k("repeat_steps", "VOICE FRAGMENT", 1, 8, 4, 0, 1);
    t("mute", "MUTE");
    t("solo", "SOLO");
    t("phase_reset", "RESET WITH MASTER");
    t("repeat", "VOICE REPEAT");
    k("velocity_response", "VELOCITY RESPONSE", 0, 1, 1);
  }
  for (int n = 0; n < objects; ++n)
    for (int s = 0; s < steps; ++s) {
      const auto& e = defaults.object[n].step[s];
      const auto k = [&](const char* id, const char* name, float lo, float hi,
                         float init, float increment = .01F) {
        specs.push_back(knob(
            eventId(n, s, id),
            "O" + juce::String(n + 1) + " E" + juce::String(s + 1) + " " + name,
            lo, hi, init, 0, increment));
      };
      specs.push_back(toggle(eventId(n, s, "on"), "EVENT ON", e.on));
      specs.push_back(toggle(eventId(n, s, "lock"), "SOUND LOCK"));
      k("velocity", "VELOCITY", 0, 1, e.velocity);
      k("pitch", "PITCH LOCK st", -48, 48, 0);
      k("start", "START LOCK", 0, .999999F, 0, 0);
      k("cutoff", "FILTER LOCK Hz", 30, 20000, e.cutoff);
      k("morph", "MORPH LOCK", 0, 1, 0);
      k("decay", "DECAY LOCK ms", .2F, 8000, e.decay);
      k("timing", "MICROTIMING steps", -.49F, .49F, 0);
      k("probability", "PROBABILITY", 0, 1, 1);
      k("span", "ROLL SPAN steps", .03125F, 8, 1, .03125F);
      k("curve", "ACCELERATION", -1, 1, 0);
      k("end_velocity", "END VELOCITY", 0, 1, e.endVelocity);
      k("end_pitch", "ROLL PITCH st", -48, 48, 0);
      k("mutation", "MORPH MUTATION", -1, 1, 0);
      k("irregular", "IRREGULARITY", 0, 1, 0);
      k("sound", "SOUND OBJECT", 1, 4, static_cast<float>(n + 1), 1);
      specs.push_back(choice(eventId(n, s, "condition"), "CONDITION",
                             "Always|Odd cycles|Even cycles|1 in 3|1 in "
                             "4|First cycle|After first|Fill"));
      k("count", "ROLL HITS", 1, 16, 1, 1);
    }
  for (int n = 0; n < 4; ++n)
    for (int f = 0; f < objectFields; ++f) {
      auto& spec =
          specs[static_cast<std::size_t>(globals + n * objectFields + f)];
      const auto split = spec.name.indexOf(" / ");
      if (split >= 0) spec.name = spec.name.substring(split + 3);
      spec.name = "O" + juce::String(n + 1) + " / " + spec.name;
    }
  for (int n = 0; n < 4; ++n)
    for (int step = 0; step < 32; ++step)
      for (int f = 0; f < eventFields; ++f) {
        auto& spec = specs[static_cast<std::size_t>(
            globals + 4 * objectFields + (n * 32 + step) * eventFields + f)];
        if (spec.name.startsWith("O"))
          spec.name = spec.name.fromFirstOccurrenceOf(" ", false, false)
                          .fromFirstOccurrenceOf(" ", false, false);
        spec.name = "O" + juce::String(n + 1) + " E" + juce::String(step + 1) +
                    " / " + spec.name;
      }
  const auto append = [&](ControlSpec spec) {
    spec.appended = true;
    specs.push_back(std::move(spec));
  };
  append(knob("skew", "SKEW", 0, 1, 0));
  append(choice("compressor_mode", "DYNAMICS", "Original|Impact"));
  append(toggle("memory_play", "PLAY CAPTURE"));
  append(toggle("memory_repeat", "REPEAT CAPTURE"));
  append(choice("repeat_division", "REPEAT DIVISION",
                "1/4|1/8|1/16|1/32|1/64|Free", 3));
  append(knob("repeat_free", "FREE REPEAT qn", .015625F, 4, .125F, 0,
              1.F / 65536));
  append(knob("memory_generation", "GENERATION", 0, 7, 0, 0, 1));
  append(toggle("capture_audio", "CAPTURE AUDIO"));
  append(toggle("resample_audio", "RESAMPLE AUDIO"));
  for (int n = 0; n < 4; ++n) {
    append(knob(objectId(n, "speed"), "O" + juce::String(n + 1) + " / SPEED",
                .125F, 4, 1, 1, 1.F / 8192));
    append(choice(objectId(n, "playback"), "O" + juce::String(n + 1) + " / PCM",
                  "Clean|16-bit / 44.1k|12-bit / 22k|8-bit / 11k"));
  }
  for (int n = 0; n < memoryGenerations; ++n) {
    const auto field = [&](ControlSpec spec) {
      spec.name = "G" + juce::String(n) + " / " + spec.name;
      append(std::move(spec));
    };
    field(knob(memoryId(n, "start"), "START", 0, .999999F, 0, 0, 0));
    field(knob(memoryId(n, "end"), "END", .000001F, 1, 1, 0, 0));
    field(knob(memoryId(n, "loop_start"), "LOOP START", 0, .999999F, 0, 0, 0));
    field(knob(memoryId(n, "loop_end"), "LOOP END", .000001F, 1, 1, 0, 0));
    field(knob(memoryId(n, "pitch"), "PITCH st", -48, 48, 0));
    field(knob(memoryId(n, "speed"), "SPEED", .125F, 4, 1, 1, 1.F / 8192));
    field(knob(memoryId(n, "cutoff"), "FILTER Hz", 30, 20000, 20000, 2000));
    field(knob(memoryId(n, "resonance"), "RESONANCE", 0, .95F, 0));
    field(knob(memoryId(n, "morph"), "FILTER MORPH", 0, 1, 0));
    field(choice(memoryId(n, "playback"), "PCM",
                 "Clean|16-bit / 44.1k|12-bit / 22k|8-bit / 11k"));
    field(toggle(memoryId(n, "reverse"), "REVERSE"));
  }
  return specs;
}
Processor::Processor()
    : Instrument{"Click K-01", "click-k01", controls(), false, &edits},
      Thread{"K01 sample loader"} {
  formats_.registerBasicFormats();
  for (int n = 0; n < 4; ++n) {
    paths_[n] = "builtin:" + juce::String(n);
    status_[n] = "Loading factory fragment";
    requested_[n] = 1;
  }
  storeScene(0);
  for (int i = 1; i < 4; ++i) {
    scene(i);
    storeScene(i);
  }
  scene(0);
  set("compressor_mode", 1);
  edits.clearUndoHistory();
  startThread();
  startTimerHz(25);
  setLatencySamples(Engine::latencySamples);
}
Processor::~Processor() {
  stopTimer();
  signalThreadShouldExit();
  notify();
  stopThread(-1);
  delete pendingMemory_.exchange(nullptr);
  delete memory_.exchange(nullptr);
  collectMemory();
}
bool Processor::prepareEngine(double rate, int) {
  const std::lock_guard lock{metadata_};
  adoptMemory();
  const auto saved = memory_.load()->dsp.state();
  auto prepared = std::make_unique<MemoryEngine>();
  if (!prepared->dsp.prepare(rate)) return false;
  prepared->dsp.restore(saved);
  delete memory_.exchange(prepared.release());
  collectMemory();
  memoryRate_ = rate;
  memoryPrepared_ = true;
  clockKnown_ = false;
  setLatencySamples(Engine::latencySamples);
  const bool ready = engine_.prepare(rate);
  meters_[18].store(static_cast<float>(engine_.polyphony()));
  return ready;
}
Controls Processor::currentControls() const noexcept {
  Controls c;
  c.skew = value(extensionBase);
  c.compressorMode = static_cast<int>(value(extensionBase + 1));
  c.mode = static_cast<int>(value(0));
  c.masterLength = static_cast<int>(value(1));
  c.regionStart = static_cast<int>(value(2));
  c.regionEnd = static_cast<int>(value(3));
  c.repeat = value(4) >= .5F;
  c.nested = value(5) >= .5F;
  c.reverse = value(6) >= .5F;
  c.hold = value(7) >= .5F;
  c.continueTime = value(8) < .5F;
  c.voided = value(9) >= .5F;
  c.crush = value(10) >= .5F;
  c.shift = value(11) >= .5F;
  c.shiftSteps = static_cast<int>(value(12));
  c.innerSpan = value(13);
  c.pressure = value(14) * .01F;
  c.recovery = value(15);
  c.dirt = value(16) * .01F;
  c.parallel = value(17) * .01F;
  c.fill = value(18) >= .5F;
  c.rollSpan = value(19);
  c.seed = static_cast<std::uint32_t>(value(21));
  for (int n = 0; n < 4; ++n) {
    auto& o = c.object[n];
    o.speed = value(extensionBase + 9 + n * 2);
    o.playback = static_cast<int>(value(extensionBase + 10 + n * 2));
    const auto at = [&](int field) {
      return value(globals + n * objectFields + field);
    };
    o.source = static_cast<int>(at(0));
    o.pitch = at(1);
    o.tune = at(2);
    o.excite = at(3);
    o.body = at(4);
    o.decay = at(5);
    o.resonance = at(6);
    o.noise = at(7);
    o.character = at(8);
    o.cutoff = at(9);
    o.morph = at(10);
    o.filterEnv = at(11);
    o.level = at(12);
    o.pan = at(13);
    o.start = at(14);
    o.end = at(15);
    o.loopStart = at(16);
    o.loopEnd = at(17);
    o.loopMod = at(18);
    o.bits = at(19);
    o.colour = at(20);
    o.interpolation = static_cast<int>(at(21));
    o.reverse = at(22) >= .5F;
    o.loop = at(23) >= .5F;
    o.length = static_cast<int>(at(24));
    o.division = static_cast<int>(at(25));
    o.midi = static_cast<int>(at(26));
    o.roll = static_cast<int>(at(27));
    o.repeatSteps = static_cast<int>(at(28));
    o.mute = at(29) >= .5F;
    o.solo = at(30) >= .5F;
    o.phaseReset = at(31) >= .5F;
    o.repeat = at(32) >= .5F;
    o.velocityResponse = at(33);
    for (int s = 0; s < 32; ++s) {
      auto& e = o.step[s];
      const auto get = [&](int field) {
        return value(globals + 4 * objectFields + (n * 32 + s) * eventFields +
                     field);
      };
      e.on = get(0) >= .5F;
      e.lock = get(1) >= .5F;
      e.velocity = get(2);
      e.pitch = get(3);
      e.start = get(4);
      e.cutoff = get(5);
      e.morph = get(6);
      e.decay = get(7);
      e.timing = get(8);
      e.probability = get(9);
      e.span = get(10) * value(19);
      e.curve = get(11);
      e.endVelocity = get(12);
      e.endPitch = get(13);
      e.mutation = get(14);
      e.irregular = get(15);
      e.sound = static_cast<int>(get(16)) - 1;
      e.condition = static_cast<int>(get(17));
      e.count = static_cast<int>(get(18));
      if (s % 2 == 1) e.timing = std::min(.49F, e.timing + value(20) * .01F);
    }
  }
  return c;
}
void Processor::processBlock(juce::AudioBuffer<float>& b,
                             juce::MidiBuffer& midi) {
  Instrument::processBlock(b, midi);
  float peak = 0;
  for (int ch = 0; ch < b.getNumChannels(); ++ch)
    for (int s = 0; s < b.getNumSamples(); ++s) {
      float x = b.getSample(ch, s);
      x = std::isfinite(x) ? juce::jlimit(-.98F, .98F, x) : 0;
      b.setSample(ch, s, x);
      peak = std::max(peak, std::abs(x));
    }
  finalPeak_.store(peak, std::memory_order_relaxed);
}
void Processor::render(juce::AudioBuffer<float>& b,
                       const juce::MidiBuffer& midi) noexcept {
  if (clearRequested_.exchange(false)) engine_.reset();
  adoptMemory();
  auto& memory = memory_.load()->dsp;
  auto controls = currentControls();
  auto captured = memoryControls();
  if (captured.repeat && !controls.continueTime) controls.hold = true;
  auto command =
      b.getNumSamples() > 0
          ? static_cast<MasterMemory::Command>(memoryCommand_.exchange(0))
          : MasterMemory::None;
  Transport t;
  if (auto* host = getPlayHead())
    if (auto p = host->getPosition()) {
      t.playing = p->getIsPlaying();
      if (auto q = p->getPpqPosition()) {
        t.ppq = *q;
        t.valid = true;
      }
      if (auto bpm = p->getBpm()) t.bpm = *bpm;
    }
  t.bpm = std::isfinite(t.bpm) ? juce::jlimit(20., 400., t.bpm) : 120.;
  const bool jump =
      clockKnown_ && t.valid &&
      std::abs(t.ppq - expectedQuarter_) > t.bpm / (60 * rate_) * .6;
  clockKnown_ = t.valid;
  expectedQuarter_ =
      t.ppq + (t.playing ? b.getNumSamples() * t.bpm / (60 * rate_) : 0);
  const double phrase = controls.masterLength * .25;
  captured.captureBeats = (controls.regionEnd - controls.regionStart) * .25;
  captured.captureAgoBeats = std::fmod(
      std::fmod(t.ppq - controls.regionEnd * .25, phrase) + phrase, phrase);
  int cursor = 0;
  engine_.process(b.getWritePointer(0), mono_.data(), 0, controls, t);
  const auto audition = audition_.exchange(0);
  for (int n = 0; n < 4; ++n)
    if (audition & (1U << n)) engine_.audition(n);
  const auto renderTo = [&](int end) {
    while (cursor < end) {
      auto count = std::min(512, end - cursor);
      auto clock = t;
      if (t.valid && t.playing) clock.ppq += cursor * t.bpm / (60 * rate_);
      engine_.process(
          b.getWritePointer(0) + cursor,
          b.getNumChannels() > 1 ? b.getWritePointer(1) + cursor : mono_.data(),
          static_cast<std::size_t>(count), controls, clock);
      memory.process(
          b.getWritePointer(0) + cursor,
          b.getNumChannels() > 1 ? b.getWritePointer(1) + cursor : mono_.data(),
          static_cast<std::size_t>(count), captured, t.bpm, command,
          jump && cursor == 0);
      command = MasterMemory::None;
      if (b.getNumChannels() == 1)
        for (int i = 0; i < count; ++i)
          b.getWritePointer(0)[cursor + i] =
              (b.getSample(0, cursor + i) +
               mono_[static_cast<std::size_t>(i)]) *
              .5F;
      cursor += count;
    }
  };
  for (auto metadata : midi) {
    renderTo(juce::jlimit(cursor, b.getNumSamples(), metadata.samplePosition));
    if (metadata.samplePosition >= b.getNumSamples() || metadata.numBytes < 1 ||
        metadata.numBytes > 3)
      continue;
    const auto* data = metadata.data;
    const int status = data[0] & 0xf0;
    if ((status == 0x90 || status == 0x80) && metadata.numBytes == 3)
      engine_.midi({0, data[1] & 127, (data[2] & 127) / 127.F,
                    status == 0x90 && data[2] > 0, -1});
    if (status == 0xb0 && metadata.numBytes == 3)
      engine_.midi({0, data[2] & 127, 0, true, data[1] & 127});
  }
  renderTo(b.getNumSamples());
  memoryCount_.store(memory.count());
  memorySelected_.store(memory.selected());
  memoryPosition_.store(memory.position());
  memoryProgress_.store(memory.progress());
  memoryStatus_.store(memory.status());
  const auto m = engine_.meter();
  for (int n = 0; n < 4; ++n) {
    meters_[n].store(m.position[n]);
    meters_[4 + n].store(m.energy[n]);
  }
  meters_[8].store(m.masterPosition);
  meters_[9].store(m.reduction);
  meters_[10].store(m.capturedSeconds);
  meters_[11].store(m.repeating ? 1.F : 0.F);
  meters_[12].store(m.nested ? 1.F : 0.F);
  meters_[13].store(m.holding ? 1.F : 0.F);
  meters_[14].store(m.empty ? 1.F : 0.F);
  meters_[15].store(static_cast<float>(m.dropped));
  meters_[16].store(static_cast<float>(m.stolen));
  meters_[17].store(static_cast<float>(m.hits));
  meters_[19].store(static_cast<float>(m.gestures));
}
class Processor::SampleEdit final : public juce::UndoableAction {
 public:
  SampleEdit(Processor& owner, int object, juce::String path,
             juce::ValueTree audio)
      : p{owner},
        n{object},
        nextPath{std::move(path)},
        nextAudio{std::move(audio)} {
    std::lock_guard lock(p.metadata_);
    previousPath = p.paths_[n];
    previousAudio = p.audio_[n];
  }
  bool perform() override {
    p.loadStoredSample(n, nextPath, nextAudio);
    return true;
  }
  bool undo() override {
    {
      std::lock_guard lock(p.metadata_);
      if (p.paths_[n] == nextPath && p.audio_[n].isValid())
        nextAudio = p.audio_[n];
    }
    p.loadStoredSample(n, previousPath, previousAudio);
    return true;
  }
  int getSizeInUnits() override {
    // Account for both possible full-size PCM payloads before async decoding.
    return 22000;
  }

 private:
  Processor& p;
  int n;
  juce::String previousPath, nextPath;
  juce::ValueTree previousAudio, nextAudio;
};
void Processor::editSample(int n, const juce::String& path,
                           const juce::ValueTree& audio) {
  if (n < 0 || n >= 4) return;
  edits.perform(new SampleEdit(*this, n, path, audio));
}
void Processor::loadSample(int n, const juce::String& path) {
  beginEdit("Load sample");
  editSample(n, path, {});
}
void Processor::loadStoredSample(int n, const juce::String& path,
                                 const juce::ValueTree& audio) {
  if (n < 0 || n >= 4) return;
  {
    std::lock_guard lock(metadata_);
    engine_.unloadSample(n);
    paths_[n] = path;
    audio_[n] = audio;
    frames_[n] = 0;
    status_[n] = path.isEmpty() ? "No sample" : "Loading...";
    ++requested_[n];
  }
  notify();
  updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}
juce::String Processor::sampleStatus(int n) const {
  std::lock_guard lock(metadata_);
  return status_[n];
}
juce::String Processor::samplePath(int n) const {
  std::lock_guard lock(metadata_);
  return paths_[n];
}
std::size_t Processor::sampleFrames(int n) const {
  std::lock_guard lock(metadata_);
  return frames_[n];
}
double Processor::sampleRate(int n) const {
  std::lock_guard lock(metadata_);
  return sampleRates_[n];
}
std::array<float, 256> Processor::waveform(int n) const {
  std::lock_guard lock(metadata_);
  return waves_[n];
}
void Processor::run() {
  while (!threadShouldExit()) {
    bool work = false;
    for (int n = 0; n < 4 && !threadShouldExit(); ++n) {
      juce::String path;
      std::uint64_t generation;
      juce::ValueTree embedded;
      {
        std::lock_guard lock(metadata_);
        generation = requested_[n];
        if (generation == completed_[n]) continue;
        path = paths_[n];
        embedded = audio_[n];
      }
      work = true;
      int bank = -1;
      std::size_t frames = 0;
      double rate = 48000;
      juce::String message;
      std::array<float, 256> wave{};
      if (!path.isEmpty() || embedded.isValid()) {
        bank = engine_.reserveSample(n);
        if (bank < 0) {
          wait(15);
          continue;
        }
        auto* l = engine_.sampleData(n, bank, 0);
        auto* r = engine_.sampleData(n, bank, 1);
        if (embedded.isValid()) {
          if (const auto saved =
                  aste::parameters::decodeAudio(embedded, sampleCapacity)) {
            frames = saved->stereo.size() / 2;
            rate = saved->rate;
            for (std::size_t i = 0; i < frames; ++i) {
              l[i] = saved->stereo[i * 2];
              r[i] = saved->stereo[i * 2 + 1];
            }
            message = "Embedded / " + juce::File{path}.getFileName();
          }
        } else if (path.startsWith("builtin:")) {
          const int source =
              juce::jlimit(0, 3, path.substring(8).getIntValue());
          frames = 12000;
          std::uint64_t rng = static_cast<std::uint64_t>(source + 1) * 1234567;
          const double frequency = 70 + source * 413.;
          for (std::size_t i = 0; i < frames; ++i) {
            rng ^= rng << 13;
            rng ^= rng >> 7;
            rng ^= rng << 17;
            const double random = (rng >> 11) * 0x1.0p-52 - 1;
            const double time = i / rate;
            const float x = static_cast<float>(
                (.65 * std::sin(2 * std::numbers::pi * frequency * time *
                                (1 + 3 * std::exp(-time * 110))) +
                 .35 * random) *
                std::exp(-time * (22 + n * 17)));
            l[i] = x;
            r[i] = x;
          }
          message =
              "Factory fragment " + juce::String(source + 1) + " / 250 ms";
        } else {
          const juce::File file{path};
          std::unique_ptr<juce::AudioFormatReader> reader(
              formats_.createReaderFor(file));
          if (!reader)
            message =
                "MISSING / UNREADABLE - LOAD to relink: " + file.getFileName();
          else if (reader->lengthInSamples < 2 ||
                   reader->lengthInSamples >
                       static_cast<juce::int64>(sampleCapacity) ||
                   reader->sampleRate < 8000 || reader->sampleRate > 192000)
            message =
                "Sample exceeds limit: 1,048,576 frames; use 8–192 kHz audio";
          else {
            frames = static_cast<std::size_t>(reader->lengthInSamples);
            rate = reader->sampleRate;
            juce::AudioBuffer<float> buffer(2, static_cast<int>(frames));
            if (reader->read(&buffer, 0, static_cast<int>(frames), 0, true,
                             true)) {
              for (std::size_t i = 0; i < frames; ++i) {
                l[i] =
                    std::isfinite(buffer.getSample(0, static_cast<int>(i)))
                        ? juce::jlimit(-8.F, 8.F,
                                       buffer.getSample(0, static_cast<int>(i)))
                        : 0;
                r[i] = reader->numChannels > 1
                           ? buffer.getSample(1, static_cast<int>(i))
                           : l[i];
                if (!std::isfinite(r[i])) r[i] = 0;
                r[i] = juce::jlimit(-8.F, 8.F, r[i]);
              }
              message = file.getFileName() + " / " +
                        juce::String(frames / rate, 2) + " s";
            } else {
              frames = 0;
              message = "Read failed - LOAD to retry";
            }
          }
        }
        if (frames > 0)
          for (std::size_t i = 0; i < frames; ++i) {
            auto bin = std::min<std::size_t>(255, i * 256 / frames);
            wave[bin] =
                std::max(wave[bin], std::max(std::abs(l[i]), std::abs(r[i])));
          }
        if (frames >= 2 && !path.startsWith("builtin:") &&
            !embedded.isValid()) {
          std::vector<float> stereo(frames * 2);
          for (std::size_t i = 0; i < frames; ++i) {
            stereo[i * 2] = l[i];
            stereo[i * 2 + 1] = r[i];
          }
          embedded = aste::parameters::encodeAudio("AUDIO", rate, stereo);
        }
      } else
        message = "No sample - LOAD or drop an audio file";
      {
        std::lock_guard lock(metadata_);
        if (generation != requested_[n]) {
          if (bank >= 0) engine_.cancelSample(n, bank);
          continue;
        }
        if (bank >= 0) {
          if (frames >= 2)
            engine_.publishSample(n, bank, frames, rate);
          else
            engine_.cancelSample(n, bank);
        }
        audio_[n] = embedded;
        status_[n] = message;
        waves_[n] = wave;
        frames_[n] = frames;
        sampleRates_[n] = rate;
        completed_[n] = generation;
      }
    }
    if (!work) wait(100);
  }
}
void Processor::applySound(int n, const Object& o) {
  const auto v = soundValues(o);
  for (std::size_t i = 0; i < v.size(); ++i)
    set(objectId(n, objectIds[i]), v[i]);
  set(objectId(n, "speed"), o.speed);
  set(objectId(n, "playback"), static_cast<float>(o.playback));
}
void Processor::scene(int index) {
  beginEdit("Factory starting point");
  const auto c = factory(juce::jlimit(0, 5, index));
  defaults();
  set("compressor_mode", 1);
  set("pressure", c.pressure * 100);
  set("recovery", c.recovery);
  set("dirt", c.dirt * 100);
  set("parallel", c.parallel * 100);
  for (int n = 0; n < 4; ++n) {
    applySound(n, c.object[n]);
    editSample(n, "builtin:" + juce::String(n), {});
    for (int s = 0; s < 32; ++s) {
      const auto& e = c.object[n].step[s];
      const auto v = eventValues(e);
      for (std::size_t f = 0; f < v.size(); ++f)
        set(eventId(n, s, eventIds[f]), v[f]);
    }
  }
}

int Processor::factoryIndex() const {
  {
    std::lock_guard lock(metadata_);
    for (int n = 0; n < 4; ++n)
      if (paths_[n] != "builtin:" + juce::String(n)) return -1;
  }
  const auto match = [&](int index, float expected) {
    auto* parameter =
        dynamic_cast<juce::RangedAudioParameter*>(getParameters()[index]);
    const auto normalized =
        parameter->convertFrom0to1(parameter->convertTo0to1(expected));
    return std::abs(value(static_cast<std::size_t>(index)) - normalized) <
           1e-6F;
  };
  auto* self = const_cast<Processor*>(this);
  if (std::abs(self->state().getRawParameterValue("master")->load()) > .001F ||
      self->state().getRawParameterValue("bypass")->load() >= .5F)
    return -1;
  for (int preset = 0; preset < 6; ++preset) {
    const auto c = factory(preset);
    bool same = true;
    for (int i = 0; i < globals && same; ++i) {
      float expected = specs()[static_cast<std::size_t>(i)].initial;
      if (i == 14) expected = c.pressure * 100;
      if (i == 15) expected = c.recovery;
      if (i == 16) expected = c.dirt * 100;
      if (i == 17) expected = c.parallel * 100;
      same = match(i, expected);
    }
    for (int n = 0; n < 4 && same; ++n) {
      const auto v = soundValues(c.object[n]);
      for (int i = 0; i < objectFields && same; ++i)
        same = match(globals + n * objectFields + i,
                     v[static_cast<std::size_t>(i)]);
    }
    for (int n = 0; n < 4 && same; ++n)
      for (int step = 0; step < 32 && same; ++step) {
        const auto v = eventValues(c.object[n].step[step]);
        for (int i = 0; i < eventFields && same; ++i)
          same = match(
              globals + 4 * objectFields + (n * 32 + step) * eventFields + i,
              v[static_cast<std::size_t>(i)]);
      }
    if (same) return preset;
  }
  return -1;
}
void Processor::moveEvent(int n, int from, int to) {
  if (from == to || from < 0 || to < 0 || from >= 32 || to >= 32) return;
  const auto& source = specs();
  std::array<float, eventFields> values{};
  for (std::size_t f = 0; f < eventFields; ++f)
    values[f] =
        value(globals + 4 * objectFields + (n * 32 + from) * eventFields + f);
  for (std::size_t f = 0; f < eventFields; ++f)
    set(eventId(n, to, eventIds[f]), values[f]);
  set(eventId(n, from, "on"), 0);
  (void)source;
}
void Processor::storeScene(int index) {
  if (index < 0 || index > 3) return;
  auto tree = state().copyState();
  {
    std::lock_guard lock(metadata_);
    writeSamples(tree);
    scenes_[index] = tree;
  }
  updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}
bool Processor::hasScene(int n) const {
  if (n < 0 || n >= 4) return false;
  std::lock_guard lock(metadata_);
  return scenes_[n].isValid();
}
bool Processor::recallScene(int index) {
  if (index < 0 || index >= 4) return false;
  juce::ValueTree tree;
  {
    std::lock_guard lock(metadata_);
    tree = scenes_[index].createCopy();
  }
  if (!tree.isValid()) return false;
  beginEdit("Recall scene");
  for (auto child : tree)
    if (auto* p = state().getParameter(child.getProperty("id").toString()))
      set(p->paramID, static_cast<float>(child.getProperty("value")));
  for (int n = 0; n < 4; ++n)
    editSample(n, tree.getProperty("sample" + juce::String(n)).toString(),
               tree.getChildWithName("AUDIO" + juce::String(n)));
  panic();
  return true;
}
void Processor::writeSamples(juce::ValueTree& tree) const {
  for (int n = 0; n < 4; ++n) {
    tree.setProperty("sample" + juce::String(n), paths_[n], nullptr);
    const juce::Identifier type{"AUDIO" + juce::String(n)};
    tree.removeChild(tree.getChildWithName(type), nullptr);
    if (audio_[n].isValid()) {
      juce::ValueTree child{type};
      child.copyPropertiesFrom(audio_[n], nullptr);
      tree.appendChild(child, nullptr);
    }
  }
}
bool Processor::validSamples(const juce::ValueTree& tree) {
  for (int n = 0; n < 4; ++n) {
    const juce::Identifier type{"AUDIO" + juce::String(n)};
    int count = 0;
    for (auto child : tree)
      if (child.hasType(type)) ++count;
    if (count > 1) return false;
    if (count == 1) {
      const auto saved = aste::parameters::decodeAudio(
          tree.getChildWithName(type), sampleCapacity);
      if (!saved || saved->stereo.size() < 4) return false;
    }
  }
  return true;
}
void Processor::writeProductMetadata(juce::ValueTree& tree) const {
  std::lock_guard lock(metadata_);
  writeSamples(tree);
  const auto* pending = pendingMemory_.load();
  const auto saved = (pending ? pending : memory_.load())->dsp.state();
  juce::ValueTree captures{"MASTER_MEMORY"};
  captures.setProperty("format", 1, nullptr);
  captures.setProperty("selected", saved.selected, nullptr);
  for (int n = 0; n < memoryGenerations && !saved.audio[n].empty(); ++n)
    captures.appendChild(
        aste::parameters::encodeAudio(juce::Identifier{"GEN" + juce::String(n)},
                                      saved.rate, saved.audio[n]),
        nullptr);
  tree.removeChild(tree.getChildWithName("MASTER_MEMORY"), nullptr);
  tree.appendChild(captures, nullptr);
  tree.getChildWithProperty("id", "memory_generation")
      .setProperty("value", saved.selected, nullptr);
  juce::ValueTree scenes{"SCENES"};
  for (int n = 0; n < 4; ++n)
    if (scenes_[n].isValid()) {
      auto entry = scenes_[n].createCopy();
      entry.setProperty("slot", n, nullptr);
      scenes.appendChild(entry, nullptr);
    }
  tree.appendChild(scenes, nullptr);
}
bool Processor::validateProductMetadata(const juce::ValueTree& tree) const {
  if (!validSamples(tree)) return false;
  int memories = 0;
  for (auto child : tree)
    if (child.hasType("MASTER_MEMORY")) ++memories;
  if (memories > 1) return false;
  const auto captures = tree.getChildWithName("MASTER_MEMORY");
  if (captures.isValid()) {
    double selected = 0, rate = 0;
    if (captures.getProperty("format").toString() != "1" ||
        captures.getNumChildren() > memoryGenerations ||
        !aste::parameters::parseFiniteDecimal(
            captures.getProperty("selected").toString().toStdString(),
            selected) ||
        selected != std::floor(selected) || selected < 0 ||
        selected > std::max(0, captures.getNumChildren() - 1))
      return false;
    for (int n = 0; n < captures.getNumChildren(); ++n) {
      const auto child = captures.getChild(n);
      if (!child.hasType(juce::Identifier{"GEN" + juce::String(n)}))
        return false;
      const auto audio =
          aste::parameters::decodeAudio(child, 192000 * 8, 22050);
      if (!audio || audio->stereo.size() < 4 ||
          audio->stereo.size() / 2 > audio->rate * 8 ||
          (n > 0 && audio->rate != rate))
        return false;
      rate = audio->rate;
    }
  }
  for (int n = 0; n < 4; ++n)
    if (tree.getProperty("sample" + juce::String(n)).toString().length() > 4096)
      return false;
  const auto scenes = tree.getChildWithName("SCENES");
  if (scenes.getNumChildren() > 4) return false;
  std::array<bool, 4> seen{};
  for (auto entry : scenes) {
    double parsedSlot = 0;
    if (!validSamples(entry) || !entry.hasType("click-k01") ||
        entry.getNumChildren() > getParameters().size() + 4 ||
        !aste::parameters::parseFiniteDecimal(
            entry.getProperty("slot", -1).toString().toStdString(),
            parsedSlot) ||
        parsedSlot != std::floor(parsedSlot) || parsedSlot < 0 ||
        parsedSlot > 3)
      return false;
    int slot = static_cast<int>(parsedSlot);
    if (seen[slot]) return false;
    seen[slot] = true;
    for (int n = 0; n < 4; ++n)
      if (entry.getProperty("sample" + juce::String(n)).toString().length() >
          4096)
        return false;
    juce::StringArray ids;
    for (auto parameter : entry) {
      if (parameter.hasType("AUDIO0") || parameter.hasType("AUDIO1") ||
          parameter.hasType("AUDIO2") || parameter.hasType("AUDIO3"))
        continue;
      const auto id = parameter.getProperty("id").toString();
      if (!const_cast<Processor*>(this)->state().getParameter(id) ||
          ids.contains(id))
        return false;
      ids.add(id);
      double v = 0;
      if (!aste::parameters::parseFiniteDecimal(
              parameter.getProperty("value").toString().toStdString(), v))
        return false;
    }
  }
  return true;
}
void Processor::restoreProductMetadata(const juce::ValueTree& tree) {
  {
    std::lock_guard lock(metadata_);
    CapturedState saved;
    const auto captures = tree.getChildWithName("MASTER_MEMORY");
    saved.selected = static_cast<int>(captures.getProperty("selected", 0));
    for (int n = 0; n < captures.getNumChildren(); ++n) {
      auto audio = aste::parameters::decodeAudio(captures.getChild(n),
                                                 192000 * 8, 22050);
      if (audio) {
        saved.rate = audio->rate;
        saved.audio[n] = std::move(audio->stereo);
      }
    }
    auto replacement = std::make_unique<MemoryEngine>();
    replacement->dsp.prepare(memoryPrepared_ ? memoryRate_ : saved.rate);
    replacement->dsp.restore(saved);
    delete pendingMemory_.exchange(replacement.release());
    collectMemory();
    scenes_ = {};
    for (auto entry : tree.getChildWithName("SCENES")) {
      int slot = static_cast<int>(entry.getProperty("slot"));
      scenes_[slot] = entry.createCopy();
    }
  }
  for (int n = 0; n < 4; ++n)
    loadStoredSample(
        n,
        tree.hasProperty("sample" + juce::String(n))
            ? tree.getProperty("sample" + juce::String(n)).toString()
            : "builtin:" + juce::String(n),
        tree.getChildWithName("AUDIO" + juce::String(n)));
}
void Processor::setStateInformation(const void* data, int size) {
  if (!data || size <= 0 || size > 512 * 1024 * 1024) return;
  Instrument::setStateInformation(data, size);
}

void Processor::adoptMemory() noexcept {
  if (auto* next = pendingMemory_.exchange(nullptr)) {
    auto* old = memory_.exchange(next);
    memoryEpoch_.fetch_add(1);
    auto* head = retiredMemory_.load();
    do {
      old->next = head;
    } while (!retiredMemory_.compare_exchange_weak(head, old));
  }
}
void Processor::collectMemory() {
  auto* old = retiredMemory_.exchange(nullptr);
  while (old) {
    auto* next = old->next;
    delete old;
    old = next;
  }
}
void Processor::timerCallback() {
  syncMemorySelection();
  std::lock_guard lock(metadata_);
  collectMemory();
}
MemoryControls Processor::memoryControls() const noexcept {
  MemoryControls c;
  c.play = value(extensionBase + 2) >= .5F;
  c.repeat = value(extensionBase + 3) >= .5F;
  c.division = static_cast<int>(value(extensionBase + 4));
  c.freeBeats = value(extensionBase + 5);
  c.selected = static_cast<int>(value(extensionBase + 6));
  c.capture = value(extensionBase + 7) >= .5F;
  c.resample = value(extensionBase + 8) >= .5F;
  for (int n = 0; n < memoryGenerations; ++n) {
    auto& p = c.generation[n];
    const int base = memoryBase + n * memoryFields;
    p.start = value(base);
    p.end = value(base + 1);
    p.loopStart = value(base + 2);
    p.loopEnd = value(base + 3);
    p.pitch = value(base + 4);
    p.speed = value(base + 5);
    p.cutoff = value(base + 6);
    p.resonance = value(base + 7);
    p.morph = value(base + 8);
    p.playback = static_cast<int>(value(base + 9));
    p.reverse = value(base + 10) >= .5F;
  }
  return c;
}
void Processor::memoryAction(MasterMemory::Command command) {
  memoryCommand_.store(static_cast<int>(command));
}
void Processor::clearMemory() {
  std::lock_guard lock(metadata_);
  auto replacement = std::make_unique<MemoryEngine>();
  replacement->dsp.prepare(memoryRate_);
  delete pendingMemory_.exchange(replacement.release());
  collectMemory();
  set("memory_play", 0);
  set("memory_repeat", 0);
  set("memory_generation", 0);
  for (int n = 0; n < memoryGenerations; ++n)
    for (int f = 0; f < memoryFields; ++f) {
      auto* parameter = state().getParameter(memoryId(n, memoryIds[f]));
      parameter->setValueNotifyingHost(parameter->getDefaultValue());
    }
  updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
}
std::array<float, 128> Processor::memoryWave(int n) const {
  std::lock_guard lock(metadata_);
  return memory_.load()->dsp.waveform(n);
}
void Processor::syncMemorySelection() {
  const int count = memoryCount(), selected = memorySelection();
  if (count != mirroredCount_ || selected != mirroredSelection_) {
    beginEdit(count > mirroredCount_ ? "Capture / resample generation"
                                     : "Select generation");
    set("memory_generation", static_cast<float>(selected));
    mirroredCount_ = count;
    mirroredSelection_ = selected;
    updateHostDisplay(ChangeDetails{}.withNonParameterStateChanged(true));
  }
}
void Processor::beginEdit(const juce::String& name) {
  state().copyState();
  edits.beginNewTransaction(name);
}
void Processor::undoEdit() {
  state().copyState();
  edits.undo();
  memoryAction(MasterMemory::Select);
}
void Processor::redoEdit() {
  state().copyState();
  edits.redo();
  memoryAction(MasterMemory::Select);
}

juce::ValueTree Processor::soundPreset(int object) {
  juce::ValueTree tree{"click-sound"};
  tree.setProperty("schema", 1, nullptr);
  if (object < 0 || object >= 4) return {};
  {
    std::lock_guard lock(metadata_);
    tree.setProperty("sample", paths_[object], nullptr);
    if (audio_[object].isValid()) {
      juce::ValueTree audio{"AUDIO"};
      audio.copyPropertiesFrom(audio_[object], nullptr);
      tree.appendChild(audio, nullptr);
    }
  }
  for (int field = 0; field < objectFields; ++field)
    if (field < 24 || field == 27 || field == 33) {
      juce::ValueTree child{"PARAM"};
      child.setProperty("id", objectIds[static_cast<std::size_t>(field)],
                        nullptr);
      child.setProperty("value", value(globals + object * objectFields + field),
                        nullptr);
      tree.appendChild(child, nullptr);
    }
  for (const char* field : {"speed", "playback"}) {
    juce::ValueTree child{"PARAM"};
    child.setProperty("id", field, nullptr);
    child.setProperty(
        "value", state().getRawParameterValue(objectId(object, field))->load(),
        nullptr);
    tree.appendChild(child, nullptr);
  }
  return tree;
}
bool Processor::restoreSound(int object, const juce::ValueTree& tree) {
  if (object < 0 || object >= 4 || !tree.hasType("click-sound") ||
      tree.getProperty("schema").toString() != "1" ||
      tree.getProperty("sample").toString().length() > 4096 ||
      tree.getNumChildren() > 29)
    return false;
  const auto audio = tree.getChildWithName("AUDIO");
  if (audio.isValid()) {
    const auto decoded = aste::parameters::decodeAudio(audio, sampleCapacity);
    if (!decoded || decoded->stereo.size() < 4) return false;
  }
  bool sawAudio = false;
  std::array<bool, objectFields + 2> seen{};
  std::array<float, objectFields + 2> values{};
  for (auto child : tree) {
    if (child.hasType("AUDIO")) {
      if (sawAudio) return false;
      sawAudio = true;
      continue;
    }
    const auto id = child.getProperty("id").toString();
    int field = -1;
    for (int i = 0; i < objectFields; ++i)
      if (id == objectIds[static_cast<std::size_t>(i)] &&
          (i < 24 || i == 27 || i == 33))
        field = i;
    if (id == "speed") field = objectFields;
    if (id == "playback") field = objectFields + 1;
    if (field < 0 || seen[static_cast<std::size_t>(field)]) return false;
    double v = 0;
    if (!aste::parameters::parseFiniteDecimal(
            child.getProperty("value").toString().toStdString(), v))
      return false;
    values[static_cast<std::size_t>(field)] = static_cast<float>(v);
    seen[static_cast<std::size_t>(field)] = true;
  }
  beginEdit("Import sound");
  for (int i = 0; i < objectFields; ++i)
    if (seen[static_cast<std::size_t>(i)])
      set(objectId(object, objectIds[static_cast<std::size_t>(i)]),
          values[static_cast<std::size_t>(i)]);
  set(objectId(object, "speed"), seen[objectFields] ? values[objectFields] : 1);
  set(objectId(object, "playback"),
      seen[objectFields + 1] ? values[objectFields + 1] : 0);
  editSample(object, tree.getProperty("sample").toString(), audio);
  return true;
}
void Processor::replaceSound(int n, int preset) {
  Object sound =
      factory(0).object[static_cast<std::size_t>(std::clamp(preset, 0, 3))];
  if (preset == 4) {
    sound.pitch = 650;
    sound.body = .95F;
    sound.excite = .1F;
    sound.decay = 90;
    sound.noise = .02F;
    sound.resonance = .8F;
    sound.morph = .35F;
  }
  if (preset == 5) {
    sound.pitch = 2800;
    sound.excite = .8F;
    sound.character = .9F;
    sound.body = .1F;
    sound.noise = .7F;
    sound.decay = 12;
  }
  if (preset == 6) {
    sound.pitch = 80;
    sound.noise = 1;
    sound.decay = 85;
    sound.morph = .5F;
    sound.resonance = .7F;
  }
  if (preset == 7) {
    sound.source = 2;
    sound.loop = true;
    sound.loopEnd = .08F;
    sound.decay = 350;
    sound.colour = .3F;
    sound.bits = 9;
  }
  const auto current = currentControls().object[n];
  sound.length = current.length;
  sound.division = current.division;
  sound.midi = current.midi;
  sound.mute = current.mute;
  sound.solo = current.solo;
  sound.roll = current.roll;
  sound.repeat = current.repeat;
  sound.repeatSteps = current.repeatSteps;
  sound.phaseReset = current.phaseReset;
  applySound(n, sound);
}
void Processor::duplicateSound(int from, int to) {
  if (from < 0 || from >= 4 || to < 0 || to >= 4 || from == to) return;
  beginEdit("Duplicate sound");
  auto copy = currentControls().object[from];
  const auto destination = currentControls().object[to];
  copy.length = destination.length;
  copy.division = destination.division;
  copy.midi = destination.midi;
  copy.mute = destination.mute;
  copy.solo = destination.solo;
  juce::String path;
  juce::ValueTree audio;
  {
    std::lock_guard lock(metadata_);
    path = paths_[from];
    audio = audio_[from];
  }
  applySound(to, copy);
  editSample(to, path, audio);
}
}  // namespace aste::click::plugin
#ifndef ASTE_NO_PLUGIN_FACTORY
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() {
  return new aste::click::plugin::Processor;
}
#endif
