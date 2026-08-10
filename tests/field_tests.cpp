#include "field_processor.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <vector>

namespace {
std::atomic<std::size_t> allocations{};
int failures{};
}  // namespace

void* operator new(std::size_t size) {
  allocations.fetch_add(1, std::memory_order_relaxed);
  if (void* value = std::malloc(size)) return value;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* value) noexcept { std::free(value); }
void operator delete[](void* value) noexcept { std::free(value); }
void operator delete(void* value, std::size_t) noexcept { std::free(value); }
void operator delete[](void* value, std::size_t) noexcept { std::free(value); }

namespace {

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

std::vector<float> render(bool forever, float mass, float grain, float pitch,
                          const std::vector<std::size_t>& blocks) {
  constexpr std::size_t frames = 48000U * 4U;
  std::vector<float> audio(frames);
  audio[0] = 1.0F;
  auto processor = std::make_unique<aste::field::Processor>();
  processor->prepare(48000.0);
  aste::field::Parameters parameters;
  parameters.forever = forever;
  parameters.mass = mass;
  parameters.grain = grain;
  parameters.pitch = pitch;
  parameters.motion = 0.55F;
  parameters.distance = 0.5F;
  parameters.blend = 1.0F;
  parameters.outputDb = 0.0F;
  std::size_t position{};
  std::size_t blockIndex{};
  while (position < audio.size()) {
    const std::size_t block =
        std::min(blocks[blockIndex++ % blocks.size()], audio.size() - position);
    processor->process(audio.data() + position, nullptr, block, parameters);
    position += block;
  }
  return audio;
}

float rms(const std::vector<float>& audio, std::size_t begin) {
  double energy{};
  for (std::size_t sample = begin; sample < audio.size(); ++sample)
    energy += static_cast<double>(audio[sample]) * audio[sample];
  return static_cast<float>(
      std::sqrt(energy / static_cast<double>(audio.size() - begin)));
}

void testDryAndBypass() {
  require(sizeof(aste::field::Processor) < 1536U * 1024U,
          "Field processing state remains below its 1.5 MiB budget");
  auto processor = std::make_unique<aste::field::Processor>();
  processor->prepare(48000.0);
  std::array<float, 127> left{};
  std::array<float, 127> right{};
  for (std::size_t sample = 0; sample < left.size(); ++sample) {
    left[sample] = 0.4F * std::sin(static_cast<float>(sample) * 0.17F);
    right[sample] = -0.25F * std::cos(static_cast<float>(sample) * 0.11F);
  }
  const auto expectedLeft = left;
  const auto expectedRight = right;
  aste::field::Parameters parameters;
  parameters.blend = 0.0F;
  parameters.outputDb = 0.0F;
  processor->process(left.data(), right.data(), left.size(), parameters);
  require(left == expectedLeft && right == expectedRight,
          "Zero blend preserves the dry stereo signal exactly");
  parameters.bypass = true;
  processor->process(left.data(), right.data(), 0U, parameters);
  require(processor->latencySamples() == 0U,
          "Field reports zero algorithmic latency");
}

void testForeverAndPitchField() {
  const auto released = render(false, 0.2F, 0.0F, 0.0F, {127U});
  const auto held = render(true, 0.2F, 0.45F, 0.5F, {127U});
  const float releasedTail = rms(released, released.size() - 48000U);
  const float heldTail = rms(held, held.size() - 48000U);
  require(heldTail > releasedTail * 4.0F && heldTail > 1.0e-6F,
          "FOREVER retains materially more late energy than release mode");
  require(held != released,
          "Pitched grain feedback changes the field rather than relabeling it");
  for (float sample : held)
    require(std::isfinite(sample), "FOREVER output remains finite");
}

void testDeterminismAndBlockIndependence() {
  const auto whole = render(true, 0.7F, 0.8F, 0.65F, {192000U});
  const auto divided =
      render(true, 0.7F, 0.8F, 0.65F, {1U, 2U, 7U, 127U, 511U, 2048U});
  require(whole == divided,
          "Field motion is deterministic across variable block schedules");
}

void testMidiExcitationAndRealtimeSafety() {
  for (double rate : {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0}) {
    auto processor = std::make_unique<aste::field::Processor>();
    processor->prepare(rate);
    processor->noteOn(60, 0.8F);
    aste::field::Parameters parameters;
    parameters.forever = true;
    parameters.mass = 1.0F;
    parameters.grain = 1.0F;
    parameters.pitch = 1.0F;
    parameters.motion = 1.0F;
    parameters.distance = 1.0F;
    parameters.blend = 1.0F;
    parameters.outputDb = 24.0F;
    std::array<float, 511> left{};
    std::array<float, 511> right{};
    const auto before = allocations.load(std::memory_order_relaxed);
    for (int block = 0; block < 200; ++block) {
      if (block == 1) {
        parameters.mass = std::numeric_limits<float>::quiet_NaN();
        parameters.grain = std::numeric_limits<float>::infinity();
        parameters.outputDb = -std::numeric_limits<float>::infinity();
      }
      processor->process(left.data(), right.data(), left.size(), parameters);
    }
    const auto after = allocations.load(std::memory_order_relaxed);
    require(after == before, "Field processing performs no allocation");
    require(processor->meters().fieldEnergy > 0.0F,
            "MIDI excitation enters and energizes the field");
    for (std::size_t sample = 0; sample < left.size(); ++sample)
      require(std::isfinite(left[sample]) && std::isfinite(right[sample]),
              "Extreme and invalid controls still produce finite output");
  }
}

void testLongHeldStability() {
  auto processor = std::make_unique<aste::field::Processor>();
  processor->prepare(48000.0);
  aste::field::Parameters parameters;
  parameters.forever = true;
  parameters.mass = parameters.grain = parameters.pitch = 1.0F;
  parameters.motion = parameters.distance = parameters.blend = 1.0F;
  parameters.outputDb = 0.0F;
  std::array<float, 511> left{};
  std::array<float, 511> right{};
  left[0] = right[0] = 1.0F;
  float maximum{};
  constexpr std::size_t totalFrames = 60U * 48000U;
  for (std::size_t position = 0; position < totalFrames;
       position += left.size()) {
    const auto count = std::min(left.size(), totalFrames - position);
    processor->process(left.data(), right.data(), count, parameters);
    for (std::size_t sample = 0; sample < count; ++sample) {
      maximum = std::max(
          maximum, std::max(std::abs(left[sample]), std::abs(right[sample])));
      require(std::isfinite(left[sample]) && std::isfinite(right[sample]),
              "A minute-long fully pitched FOREVER field remains finite");
      left[sample] = right[sample] = 0.0F;
    }
  }
  require(maximum < 2.0F && processor->meters().fieldEnergy > 0.0F,
          "FOREVER remains bounded while retaining measurable energy");
}

void testStereoFieldFormation() {
  auto processor = std::make_unique<aste::field::Processor>();
  processor->prepare(48000.0);
  aste::field::Parameters parameters;
  parameters.mass = 0.7F;
  parameters.grain = 0.4F;
  parameters.pitch = 0.3F;
  parameters.motion = 0.4F;
  parameters.blend = 1.0F;
  parameters.outputDb = 0.0F;
  std::vector<float> left(48000U);
  std::vector<float> right(48000U);
  left[0] = right[0] = 1.0F;
  processor->process(left.data(), right.data(), left.size(), parameters);
  double energy{};
  double difference{};
  for (std::size_t sample = 0; sample < left.size(); ++sample) {
    energy += static_cast<double>(left[sample]) * left[sample] +
              static_cast<double>(right[sample]) * right[sample];
    const double delta = left[sample] - right[sample];
    difference += delta * delta;
  }
  require(energy > 1.0e-6 && difference > energy * 0.05,
          "Centered excitation forms a non-collapsed stereo field");
}

}  // namespace

int main() {
  testDryAndBypass();
  testForeverAndPitchField();
  testDeterminismAndBlockIndependence();
  testMidiExcitationAndRealtimeSafety();
  testLongHeldStability();
  testStereoFieldFormation();
  if (failures == 0) std::cout << "field_tests: ok\n";
  return failures == 0 ? 0 : 1;
}
