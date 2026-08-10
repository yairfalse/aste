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

int main() {
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
