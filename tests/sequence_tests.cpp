#include "sequence_processor.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <vector>

namespace {
std::atomic<std::size_t> allocations{};
int failures{};
}  // namespace

void* operator new(std::size_t size) {
  allocations.fetch_add(1, std::memory_order_relaxed);
  if (void* pointer = std::malloc(size)) {
    return pointer;
  }
  throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, std::size_t) noexcept {
  std::free(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
  std::free(pointer);
}

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

aste::sequence::MidiEvent noteOn(std::size_t offset = 0) {
  return {offset, aste::sequence::MidiEventType::noteOn, 48, 0.8F};
}

void testMidiVoiceAndRates() {
  for (double rate : {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0}) {
    aste::sequence::Parameters parameters;
    parameters.sequenceEnabled = false;
    aste::sequence::Processor processor;
    processor.prepare(rate, parameters);
    for (std::size_t block : {1U, 2U, 7U, 127U, 511U, 2048U}) {
      std::vector<float> left(block);
      std::vector<float> right(block);
      const std::array event{noteOn()};
      processor.process(left.data(), right.data(), block, parameters, {},
                        event);
      for (std::size_t sample = 0; sample < block; ++sample) {
        require(std::isfinite(left[sample]) && left[sample] == right[sample],
                "Sequence output is finite and mono-compatible");
      }
    }
    require(processor.meters().outputPeak > 0.0F,
            "MIDI note produces audible output at every supported rate");
    require(processor.latencySamples() == 0U,
            "Sequence core remains zero latency");
  }
}

void testBlockIndependenceAndReset() {
  constexpr std::size_t frames = 24000;
  aste::sequence::Parameters parameters;
  parameters.sequenceEnabled = false;
  std::vector<float> contiguous(frames);
  std::vector<float> divided(frames);
  aste::sequence::Processor first;
  aste::sequence::Processor second;
  first.prepare(48000.0, parameters);
  second.prepare(48000.0, parameters);
  const std::array event{noteOn()};
  first.process(contiguous.data(), nullptr, frames, parameters, {}, event);
  constexpr std::array blocks{1U, 2U, 7U, 127U, 511U, 2048U};
  std::size_t offset{};
  std::size_t blockIndex{};
  while (offset < frames) {
    const std::size_t size =
        std::min(static_cast<std::size_t>(blocks[blockIndex++ % blocks.size()]),
                 frames - offset);
    const std::span<const aste::sequence::MidiEvent> events =
        offset == 0 ? std::span<const aste::sequence::MidiEvent>{event}
                    : std::span<const aste::sequence::MidiEvent>{};
    second.process(divided.data() + offset, nullptr, size, parameters, {},
                   events);
    offset += size;
  }
  require(contiguous == divided,
          "Sequence render is independent of variable block boundaries");
  second.reset();
  std::fill(divided.begin(), divided.end(), 0.0F);
  second.process(divided.data(), nullptr, frames, parameters, {}, event);
  require(contiguous == divided, "Sequence reset is deterministic");
}

void testHostSequence() {
  aste::sequence::Parameters parameters;
  aste::sequence::Processor processor;
  processor.prepare(48000.0, parameters);
  std::vector<float> audio(13000);
  const aste::sequence::Transport transport{true, true, 120.0, 0.0};
  processor.process(audio.data(), nullptr, audio.size(), parameters, transport);
  require(processor.meters().outputPeak > 0.0F,
          "Host transport triggers the programmed pattern");
  require(static_cast<int>(processor.meters().currentStep) == 2,
          "Host PPQ selects the expected deterministic step");
  for (float sample : audio) {
    require(std::isfinite(sample), "Sequencer output remains finite");
  }

  const aste::sequence::Transport stopped{true, false, 120.0, 0.0};
  std::array<float, 1> stoppedAudio{};
  processor.process(stoppedAudio.data(), nullptr, 1, parameters, stopped);
  require(processor.meters().currentStep < 0.0F,
          "Transport stop clears the sequence position");
}

void testPressureMapping() {
  auto previous = aste::sequence::Processor::mapPressure(0.0F);
  for (float amount : {0.25F, 0.5F, 0.75F, 1.0F}) {
    const auto current = aste::sequence::Processor::mapPressure(amount);
    require(current.mixerDrive >= previous.mixerDrive &&
                current.envelopeDepth >= previous.envelopeDepth &&
                current.accentGain >= previous.accentGain &&
                current.filterLoading >= previous.filterLoading,
            "Pressure mapping is musically monotonic");
    previous = current;
  }
}

float renderFilterWeight(float weight) {
  constexpr std::size_t frames = 24000;
  aste::sequence::Parameters parameters;
  parameters.sequenceEnabled = false;
  parameters.filterMorph = weight;
  parameters.filterDrive = 0.65F;
  parameters.cutoffHz = 720.0F;
  parameters.resonance = 0.65F;
  parameters.sustain = 1.0F;
  aste::sequence::Processor processor;
  processor.prepare(48000.0, parameters);
  std::vector<float> audio(frames);
  const std::array event{noteOn()};
  processor.process(audio.data(), nullptr, audio.size(), parameters, {}, event);
  double energy{};
  for (std::size_t sample = 12000; sample < audio.size(); ++sample) {
    energy += static_cast<double>(audio[sample]) * audio[sample];
  }
  return static_cast<float>(
      std::sqrt(energy / static_cast<double>(audio.size() - 12000)));
}

void testCharacterFilterContinuity() {
  const float open = renderFilterWeight(0.0F);
  const float middle = renderFilterWeight(0.5F);
  const float weight = renderFilterWeight(1.0F);
  require(std::isfinite(open) && std::isfinite(middle) &&
              std::isfinite(weight) && open > 0.0F && middle > 0.0F &&
              weight > 0.0F,
          "Every character-filter weight is finite and audible");
  require(middle > 0.25F * std::min(open, weight),
          "Filter Weight has no destructive midpoint level collapse");
  require(std::abs(open - weight) > 1.0e-4F,
          "Filter Weight reaches materially different responses");
}

void testSlideTransition() {
  aste::sequence::Parameters plain;
  for (auto& step : plain.steps) {
    step.gate = false;
    step.accent = false;
    step.slide = false;
  }
  plain.steps[0] = {0, true, false, false};
  plain.steps[1] = {12, true, false, false};
  plain.glideMs = 120.0F;
  auto sliding = plain;
  sliding.steps[0].slide = true;
  aste::sequence::Processor plainProcessor;
  aste::sequence::Processor slideProcessor;
  plainProcessor.prepare(48000.0, plain);
  slideProcessor.prepare(48000.0, sliding);
  std::vector<float> plainAudio(7200);
  std::vector<float> slideAudio(7200);
  const aste::sequence::Transport transport{true, true, 120.0, 0.0};
  plainProcessor.process(plainAudio.data(), nullptr, plainAudio.size(), plain,
                         transport);
  slideProcessor.process(slideAudio.data(), nullptr, slideAudio.size(), sliding,
                         transport);
  float difference{};
  for (std::size_t sample = 6000; sample < plainAudio.size(); ++sample) {
    difference =
        std::max(difference, std::abs(plainAudio[sample] - slideAudio[sample]));
  }
  require(difference > 0.001F,
          "A marked slide holds its gate and changes the next transition");
}

void testNumericalAndRealtimeSafety() {
  aste::sequence::Parameters parameters;
  parameters.sequenceEnabled = false;
  parameters.pressure = std::numeric_limits<float>::infinity();
  parameters.cutoffHz = std::numeric_limits<float>::quiet_NaN();
  parameters.resonance = -1000.0F;
  aste::sequence::Processor processor;
  processor.prepare(48000.0, parameters);
  std::array<float, 127> left{};
  std::array<float, 127> right{};
  const std::array event{noteOn()};
  const auto before = allocations.load(std::memory_order_relaxed);
  processor.process(left.data(), right.data(), left.size(), parameters, {},
                    event);
  const auto after = allocations.load(std::memory_order_relaxed);
  require(after == before, "Sequence process performs no heap allocation");
  for (float sample : left) {
    require(std::isfinite(sample), "Hostile parameters retain finite output");
  }
}

}  // namespace

void testFreshAttackDuringRelease() {
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    aste::sequence::Parameters p;
    p.sequenceEnabled = false;
    p.attackMs = 1;
    aste::sequence::Processor processor;
    processor.prepare(rate, p);
    float peak{};
    for (std::size_t i = 0; i < static_cast<std::size_t>(rate * .16); ++i) {
      std::array<aste::sequence::MidiEvent, 1> event{};
      std::size_t count{};
      if (i == 0 || i == static_cast<std::size_t>(rate * .12))
        event[count++] = {0, aste::sequence::MidiEventType::noteOn, 48, .8F};
      if (i == static_cast<std::size_t>(rate * .1))
        event[count++] = {0, aste::sequence::MidiEventType::noteOff, 48, 0};
      float sample{};
      processor.process(&sample, nullptr, 1, p, {}, {event.data(), count});
      if (i >= static_cast<std::size_t>(rate * .12))
        peak = std::max(peak, processor.meters().envelope);
    }
    require(peak > .95F,
            "A separated MIDI note attacks during the prior release");
  }
}

std::vector<float> hostPhrase(double rate, double bpm, std::size_t block) {
  const auto count = static_cast<std::size_t>(rate * 2);
  std::vector<float> audio(count);
  aste::sequence::Parameters p;
  aste::sequence::Processor processor;
  processor.prepare(rate, p);
  for (std::size_t at = 0; at < count;) {
    const auto epoch = at >= count / 2 ? count / 2 : 0U;
    const double tempo = epoch ? bpm * .73 : bpm;
    const double origin = epoch ? -1.125 : 0.;
    const auto boundary = std::min(count, epoch ? count : count / 2);
    const auto n = std::min({block, boundary - at, 512 - at % 512});
    p.cutoffHz = (at / 512) % 2 ? 6000 : 150;
    p.pressure = (at / 512) % 2 ? 1 : 0;
    const aste::sequence::Transport t{
        true, true, tempo, origin + (at - epoch) / rate * tempo / 60.};
    processor.process(audio.data() + at, nullptr, n, p, t);
    at += n;
  }
  return audio;
}

void testHostPartitionWithAutomationAndSeek() {
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
    for (double bpm : {87., 120., 173.})
      require(hostPhrase(rate, bpm, 127) == hostPhrase(rate, bpm, 511),
              "Host gates are sample-identical across blocks, automation, "
              "tempo and seek");
}

void testWidePolyphony() {
  using namespace aste::sequence;
  for (double rate : {8000., 44100., 48000., 96000., 192000.}) {
    Parameters p;
    p.voiceMode = 1;
    p.sequenceEnabled = false;
    p.releaseMs = 30;
    p.cutoffHz = 2400;
    p.resonance = 1;
    p.filterDrive = 1;
    p.pressure = 1;
    p.outputDb = 6;
    Processor processor;
    processor.prepare(rate, p);
    std::array<float, 4096> l{}, r{};
    const std::array chord{MidiEvent{23, MidiEventType::noteOn, 48, .8F},
                           MidiEvent{23, MidiEventType::noteOn, 52, .8F},
                           MidiEvent{23, MidiEventType::noteOn, 55, .8F}};
    const auto before = allocations.load();
    processor.process(l.data(), r.data(), l.size(), p, {}, chord);
    require(allocations.load() == before,
            "Polyphonic rendering allocates nothing");
    bool stereo = false;
    for (std::size_t i = 0; i < l.size(); ++i) {
      require(std::isfinite(l[i]) && std::isfinite(r[i]) &&
                  std::abs(l[i]) <= 1 && std::abs(r[i]) <= 1,
              "Polyphonic chord stays finite and bounded at extreme settings");
      if (i < 23)
        require(l[i] == 0 && r[i] == 0, "Poly MIDI respects sample offsets");
      stereo |= std::abs(l[i] - r[i]) > .0001F;
    }
    require(stereo, "Chord voices occupy distinct stereo positions");
    // Releasing the last-played note must leave the other two sounding.
    const std::array off{MidiEvent{0, MidiEventType::noteOff, 55, 0}};
    processor.process(l.data(), r.data(), l.size(), p, {}, off);
    require(processor.meters().outputPeak > .001F,
            "Releasing one chord key does not release the other voices");
    const std::array panic{MidiEvent{0, MidiEventType::allNotesOff, 0, 0}};
    processor.process(l.data(), r.data(), l.size(), p, {}, panic);
    for (int i = 0; i < 20; ++i)
      processor.process(l.data(), r.data(), l.size(), p, {});
    require(processor.meters().outputPeak < 1.e-6F,
            "All notes off releases every voice");
  }
}

void testWideSustainAndStealing() {
  using namespace aste::sequence;
  Parameters p;
  p.voiceMode = 1;
  p.sequenceEnabled = false;
  p.releaseMs = 10;
  Processor processor;
  processor.prepare(48000, p);
  std::array<float, 2048> l{}, r{};
  std::array<MidiEvent, 12> notes{};
  for (int i = 0; i < 12; ++i)
    notes[i] = {0, MidiEventType::noteOn, 48 + i, .8F};
  processor.process(l.data(), r.data(), l.size(), p, {}, notes);
  require(processor.meters().outputPeak > .01F,
          "Voice stealing keeps dense chords audible");
  const std::array pedal{MidiEvent{0, MidiEventType::sustain, 0, 1}};
  processor.process(l.data(), r.data(), l.size(), p, {}, pedal);
  for (int i = 0; i < 12; ++i)
    notes[i] = {0, MidiEventType::noteOff, 48 + i, 0};
  processor.process(l.data(), r.data(), l.size(), p, {}, notes);
  processor.process(l.data(), r.data(), l.size(), p, {});
  require(processor.meters().outputPeak > .001F,
          "Pedal holds released chord keys");
  const std::array up{MidiEvent{0, MidiEventType::sustain, 0, 0}};
  processor.process(l.data(), r.data(), l.size(), p, {}, up);
  for (int i = 0; i < 10; ++i)
    processor.process(l.data(), r.data(), l.size(), p, {});
  require(processor.meters().outputPeak < 1.e-6F,
          "Pedal up releases stolen and sustained notes");
  // A zero-velocity note-on has the same release semantics as note-off.
  const std::array on{MidiEvent{0, MidiEventType::noteOn, 60, .8F}};
  processor.process(l.data(), r.data(), l.size(), p, {}, on);
  const std::array zero{MidiEvent{0, MidiEventType::noteOn, 60, 0}};
  processor.process(l.data(), r.data(), l.size(), p, {}, zero);
  for (int i = 0; i < 10; ++i)
    processor.process(l.data(), r.data(), l.size(), p, {});
  require(processor.meters().outputPeak < 1.e-6F,
          "Zero velocity releases a polyphonic note");
}

void testChordFundamentals() {
  using namespace aste::sequence;
  Parameters p;
  p.voiceMode = 1;
  p.sequenceEnabled = false;
  p.detuneSemitones = 0;
  p.shape = 1;  // Sines isolate the played pitches from oscillator harmonics.
  p.subLevel = 0;
  p.cutoffHz = 8000;
  p.pressure = 0;
  p.filterDrive = 0;
  p.resonance = 0;
  p.attackMs = 1;
  p.sustain = 1;
  Processor processor;
  processor.prepare(48000, p);
  std::vector<float> audio(48000);
  const std::array chord{MidiEvent{0, MidiEventType::noteOn, 60, 1},
                         MidiEvent{0, MidiEventType::noteOn, 64, 1},
                         MidiEvent{0, MidiEventType::noteOn, 67, 1}};
  processor.process(audio.data(), nullptr, audio.size(), p, {}, chord);
  for (int note : {60, 64, 67}) {
    const double hz = 440 * std::exp2((note - 69.) / 12.);
    double real = 0, imaginary = 0;
    for (std::size_t n = 12000; n < audio.size(); ++n) {
      const double phase = 6.283185307179586 * hz * n / 48000.;
      real += audio[n] * std::cos(phase);
      imaginary += audio[n] * std::sin(phase);
    }
    require(
        std::hypot(real, imaginary) / 36000 > .02,
        "Each simultaneously held chord note has its own audible fundamental");
  }
}

void testWideBlockAndMonoSequence() {
  using namespace aste::sequence;
  for (int mode : {1, 2}) {
    Parameters p;
    p.voiceMode = mode;
    Processor a, b;
    a.prepare(48000, p);
    b.prepare(48000, p);
    constexpr std::size_t frames = 32000;
    std::vector<float> first(frames), second(frames), right(frames);
    a.process(first.data(), right.data(), frames, p, {true, true, 120, 0});
    for (std::size_t at = 0; at < frames;) {
      const auto n = std::min<std::size_t>(127, frames - at);
      b.process(second.data() + at, nullptr, n, p,
                {true, true, 120, at / 24000.});
      at += n;
    }
    require(first == right, "New sequencer voice remains centred in stereo");
    require(first == second,
            "New slides and accents are independent of block size");
    a.reset();
    a.process(second.data(), nullptr, frames, p, {true, true, 120, 0});
    require(first == second, "New voice resets deterministically");
  }
}

void testWaveMorphBodyAndFilterContour() {
  using namespace aste::sequence;
  std::array<double, 3> levels{};
  for (int setting = 0; setting < 3; ++setting) {
    Parameters p;
    p.voiceMode = 2;
    p.sequenceEnabled = false;
    p.oscillatorMix = p.subLevel = p.resonance = p.pressure = p.filterDrive = 0;
    p.shape = setting * .25F;
    p.cutoffHz = 14000;
    p.sustain = 1;
    Processor synth;
    synth.prepare(48000, p);
    std::vector<float> audio(24000);
    const std::array midi{noteOn()};
    synth.process(audio.data(), nullptr, audio.size(), p, {}, midi);
    for (std::size_t i = 12000; i < audio.size(); ++i)
      levels[setting] += audio[i] * audio[i];
  }
  require(
      levels[1] > .7 * std::min(levels[0], levels[2]),
      "Saw/pulse blend preserves body instead of cancelling its fundamental");
  Parameters p;
  p.voiceMode = 2;
  p.sequenceEnabled = false;
  p.cutoffHz = 100;
  p.envelopeAmount = .8F;
  p.sustain = 1;
  p.filterAttackMs = 1;
  p.filterDecayMs = 25;
  p.filterSustain = 0;
  Processor a, b;
  a.prepare(48000, p);
  auto slow = p;
  slow.filterDecayMs = 2000;
  b.prepare(48000, slow);
  std::vector<float> x(24000), y(24000);
  const std::array midi{noteOn()};
  a.process(x.data(), nullptr, x.size(), p, {}, midi);
  b.process(y.data(), nullptr, y.size(), slow, {}, midi);
  double difference = 0;
  for (std::size_t i = 12000; i < x.size(); ++i)
    difference += std::abs(x[i] - y[i]);
  require(difference > 10,
          "Independent filter decay audibly changes a sustained amplifier "
          "envelope");
}

int main() {
  testWaveMorphBodyAndFilterContour();
  testWidePolyphony();
  testWideSustainAndStealing();
  testChordFundamentals();
  testWideBlockAndMonoSequence();
  testFreshAttackDuringRelease();
  testHostPartitionWithAutomationAndSeek();
  testMidiVoiceAndRates();
  testBlockIndependenceAndReset();
  testHostSequence();
  testPressureMapping();
  testCharacterFilterContinuity();
  testSlideTransition();
  testNumericalAndRealtimeSafety();
  if (failures == 0) {
    std::cout << "sequence_tests: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
