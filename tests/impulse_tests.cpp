#include "impulse_processor.hpp"

#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <new>
#include <vector>

namespace {
std::atomic<std::size_t> allocations{};
int failures{};
}  // namespace
void* operator new(std::size_t size) {
  allocations.fetch_add(1);
  if (void* p = std::malloc(size)) return p;
  throw std::bad_alloc{};
}
void* operator new[](std::size_t size) { return ::operator new(size); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

namespace {
void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

std::vector<float> render(std::uint32_t seed,
                          const std::vector<std::size_t>& blocks,
                          int ratchet = 1) {
  aste::impulse::Parameters parameters;
  parameters.seed = seed;
  parameters.tracks[1].ratchet = ratchet;
  aste::impulse::Processor processor;
  processor.prepare(48000, parameters);
  std::vector<float> output(96000);
  std::size_t offset{}, blockIndex{};
  while (offset < output.size()) {
    const auto frames =
        std::min(blocks[blockIndex++ % blocks.size()], output.size() - offset);
    const double ppq = offset * 120.0 / (60.0 * 48000.0);
    processor.process(output.data() + offset, nullptr, frames, parameters,
                      {true, 120, ppq});
    offset += frames;
  }
  return output;
}

void testDeterminismAndBlocks() {
  require(render(1701, {127}, 3) == render(1701, {1, 7, 127, 511}, 3),
          "Triplet retriggers own the same samples across callback partitions");
  const auto first = render(1701, {127});
  const auto second = render(1701, {1, 7, 127, 511});
  const auto changed = render(1702, {127});
  require(first == second,
          "Impulse output is independent of block partitioning");
  require(first != changed, "Stored seed changes deterministic variation");
  float peak{};
  for (float sample : first) {
    require(std::isfinite(sample), "Impulse output remains finite");
    peak = std::max(peak, std::abs(sample));
  }
  require(peak > 0.01F, "Impulse produces clocked physical events");
}

void testRatesMidiAndRealtime() {
  for (double rate : {44100.0, 48000.0, 88200.0, 96000.0, 176400.0, 192000.0}) {
    aste::impulse::Parameters parameters;
    parameters.energy = 1;
    parameters.mutation = 1;
    for (auto& track : parameters.tracks) {
      track.ratchet = 4;
      track.drive = 1;
      track.probability = 1;
    }
    aste::impulse::Processor processor;
    processor.prepare(rate, parameters);
    std::array<float, 127> left{}, right{};
    constexpr std::array midi{aste::impulse::MidiEvent{23, 36, 1, true},
                              aste::impulse::MidiEvent{61, 43, 0.8F, true}};
    const auto before = allocations.load();
    processor.process(left.data(), right.data(), left.size(), parameters,
                      {false, 120, 0}, midi);
    require(allocations.load() == before, "Impulse process allocates nothing");
    require(processor.meters().triggered[0] && processor.meters().triggered[7],
            "Impulse applies all eight MIDI object triggers at sample offsets");
    for (std::size_t i = 0; i < left.size(); ++i)
      require(std::isfinite(left[i]) && std::isfinite(right[i]),
              "Impulse hostile mode remains finite");
    require(processor.latencySamples() == 0, "Impulse reports zero latency");
  }
}

void testVisiblePatternIsPlaybackSource() {
  aste::impulse::Parameters parameters;
  for (auto& track : parameters.tracks) track.pattern.fill(0U);
  parameters.tracks[0].length = 16;
  parameters.tracks[0].pattern[3] = 2U;
  aste::impulse::Processor processor;
  processor.prepare(48000.0, parameters);
  std::vector<float> audio(24000);
  processor.process(audio.data(), nullptr, audio.size(), parameters,
                    {true, 120.0, 0.0});
  std::size_t first = audio.size();
  for (std::size_t sample = 0; sample < audio.size(); ++sample) {
    if (std::abs(audio[sample]) > 1.0e-7F) {
      first = sample;
      break;
    }
  }
  require(first >= 17990U && first < 18100U,
          "The visible manual step pattern directly schedules playback");
}
}  // namespace

std::vector<float> midiPhrase(std::size_t block, float tone = .5F,
                              int note = 36) {
  aste::impulse::Parameters p;
  p.sequenceEnabled = false;
  p.variation = .6F;
  p.tracks[4].tone = tone;
  aste::impulse::Processor processor;
  processor.prepare(48000, p);
  std::vector<float> audio(48000);
  constexpr std::array<std::size_t, 3> times{4977, 21851, 38729};
  for (std::size_t at = 0; at < audio.size();) {
    const auto n = std::min(block, audio.size() - at);
    std::array<aste::impulse::MidiEvent, 3> events{};
    std::size_t count{};
    for (auto time : times)
      if (time >= at && time < at + n)
        events[count++] = {time - at, note, .8F, true};
    processor.process(audio.data() + at, nullptr, n, p, {},
                      {events.data(), count});
    at += n;
  }
  return audio;
}

std::vector<float> hostTransitions(double rate, std::size_t block, int ratchet,
                                   float timing) {
  aste::impulse::Parameters p;
  for (auto& track : p.tracks) track.pattern.fill(0);
  p.tracks[1].pattern.fill(1);
  p.tracks[1].ratchet = ratchet;
  p.tracks[1].timing = timing;
  p.tracks[1].condition = 2;
  p.variation = .6F;
  aste::impulse::Processor processor;
  processor.prepare(rate, p);
  std::vector<float> audio(static_cast<std::size_t>(rate));
  const auto quarter = audio.size() / 4;
  for (std::size_t at = 0; at < audio.size();) {
    const auto epoch = std::min<std::size_t>(at / quarter, 3);
    const auto end = epoch == 3 ? audio.size() : (epoch + 1) * quarter;
    const auto n = std::min(block, end - at);
    const double tempo = epoch == 3 ? 87 : 120;
    const double origin = epoch == 1 ? 8.125 : (epoch == 3 ? -.125 : 0);
    const double ppq = origin + (at - epoch * quarter) * tempo / (60 * rate);
    processor.process(audio.data() + at, nullptr, n, p,
                      {epoch != 2, tempo, ppq});
    at += n;
  }
  return audio;
}

void testAllRatchetsAndHostTransitions() {
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
    for (int ratchet : {1, 2, 3, 4})
      for (float timing : {-.49F, 0.F, .49F})
        require(
            hostTransitions(rate, 127, ratchet, timing) ==
                hostTransitions(rate, 511, ratchet, timing),
            "All ratchets/microtiming survive seeks, stop/restart and tempo "
            "changes sample-identically");
}

int main() {
  testAllRatchetsAndHostTransitions();
  require(midiPhrase(127) == midiPhrase(511),
          "MIDI variation belongs to events, not callback offsets");
  require(midiPhrase(127, 0, 40) != midiPhrase(127, 1, 40),
          "Low Tone changes the object's harmonic edge");
  testDeterminismAndBlocks();
  testRatesMidiAndRealtime();
  testVisiblePatternIsPlaybackSource();
  if (failures == 0) std::cout << "impulse_tests: ok\n";
  return failures == 0 ? 0 : 1;
}
