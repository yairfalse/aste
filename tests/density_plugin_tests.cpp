#include "density_plugin.hpp"

#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <new>
#include <string_view>
#include <thread>
#include <vector>

#if JUCE_MAC
#include <fcntl.h>
#include <pthread.h>
#include <unistd.h>
#endif

namespace {
std::atomic<std::size_t> allocations{0};
#if JUCE_MAC
std::atomic<bool> allocationAuditActive{false};
std::atomic<std::uintptr_t> allocationAuditThread{0};

bool shouldCountAllocation() noexcept {
  return allocationAuditActive.load(std::memory_order_relaxed) &&
         allocationAuditThread.load(std::memory_order_relaxed) ==
             reinterpret_cast<std::uintptr_t>(pthread_self());
}

void setAllocationAuditActive(bool active) noexcept {
  if (active) {
    allocationAuditThread.store(
        reinterpret_cast<std::uintptr_t>(pthread_self()),
        std::memory_order_relaxed);
  }
  allocationAuditActive.store(active, std::memory_order_relaxed);
}

static_assert(std::atomic<bool>::is_always_lock_free);
static_assert(std::atomic<std::uintptr_t>::is_always_lock_free);
#else
bool shouldCountAllocation() noexcept { return true; }
void setAllocationAuditActive(bool) noexcept {}
#endif
}  // namespace

#if JUCE_MAC
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int active);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif

void* operator new(std::size_t size) {
  if (shouldCountAllocation()) {
    allocations.fetch_add(1, std::memory_order_relaxed);
  }
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

int failures = 0;

void require(bool condition, const char* message) {
  if (!condition) {
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
  }
}

void setValue(aste::density::plugin::DensityAudioProcessor& processor,
              const char* id, float plainValue) {
  auto* parameter = processor.state().getParameter(id);
  require(parameter != nullptr, "Parameter ID exists");
  if (parameter != nullptr) {
    parameter->setValueNotifyingHost(parameter->convertTo0to1(plainValue));
  }
}

float rawValue(aste::density::plugin::DensityAudioProcessor& processor,
               const char* id) {
  const auto* value = processor.state().getRawParameterValue(id);
  require(value != nullptr, "Raw parameter ID exists");
  return value == nullptr ? 0.0F : value->load();
}

juce::MemoryBlock binaryState(const juce::ValueTree& state) {
  juce::MemoryBlock binary;
  if (const auto xml = state.createXml()) {
    juce::AudioProcessor::copyXmlToBinary(*xml, binary);
  }
  return binary;
}

juce::Component* findTitled(juce::Component& component,
                            const juce::String& title) {
  if (component.getTitle() == title) {
    return &component;
  }
  for (int i = 0; i < component.getNumChildComponents(); ++i) {
    if (auto* match = findTitled(*component.getChildComponent(i), title)) {
      return match;
    }
  }
  return nullptr;
}

void testParameterTextContract() {
  struct Contract {
    const char* id;
    const char* name;
    const char* unit;
    float value;
    const char* input;
    float tolerance;
  };
  constexpr std::array<Contract, 9> contracts{{
      {"drive", "Drive", "dB", -6.5F, "-6.50 dB", 0.011F},
      {"crush", "Crush", "%", 82.5F, "82.50 %", 0.011F},
      {"attack", "Attack", "ms", 0.125F, "0.125 ms", 0.0011F},
      {"release", "Release", "ms", 777.7F, "777.7 ms", 0.11F},
      {"density", "Density", "%", 73.25F, "73.25 %", 0.011F},
      {"blend", "Blend", "%", 37.5F, "37.50 %", 0.011F},
      {"stereo", "Stereo", "%", 62.5F, "62.50 %", 0.011F},
      {"output", "Output", "dB", -3.25F, "-3.25 dB", 0.011F},
      {"detector_hpf", "Detector HPF", "Hz", 137.5F, "137.5 Hz", 0.11F},
  }};
  aste::density::plugin::DensityAudioProcessor processor;
  for (const auto& contract : contracts) {
    auto* parameter = processor.state().getParameter(contract.id);
    require(parameter != nullptr, "Every continuous parameter ID exists");
    if (parameter == nullptr) {
      continue;
    }
    require(parameter->getName(64) == contract.name,
            "Every continuous parameter has a stable visible name");
    require(parameter->getLabel() == contract.unit,
            "Every continuous parameter has a stable unit");
    const float parsed =
        parameter->convertFrom0to1(parameter->getValueForText(contract.input));
    require(std::abs(parsed - contract.value) <= contract.tolerance,
            "Every continuous parameter accepts exact host text input");
    const auto text =
        parameter->getText(parameter->convertTo0to1(contract.value), 64);
    const float roundTrip =
        parameter->convertFrom0to1(parameter->getValueForText(text));
    require(!text.isEmpty() &&
                std::abs(roundTrip - contract.value) <= contract.tolerance,
            "Every continuous parameter text representation round-trips");
  }
}

std::uint32_t fuzzNext(std::uint32_t& state) {
  state = state * 1664525U + 1013904223U;
  return state;
}

std::vector<std::byte> bytes(const juce::MemoryBlock& block) {
  if (block.isEmpty()) {
    return {};
  }
  const auto* begin = static_cast<const std::byte*>(block.getData());
  return {begin, begin + block.getSize()};
}

void reportFuzzFailure(std::size_t index, const char* reason,
                       const std::vector<std::byte>& input) {
  constexpr char hex[] = "0123456789abcdef";
  std::cerr << "state fuzz failure: seed=0xd01f022 case=" << index
            << " reason=" << reason << " bytes=";
  for (std::byte value : input) {
    const auto byte = std::to_integer<unsigned int>(value);
    std::cerr << hex[byte >> 4U] << hex[byte & 0x0fU];
  }
  std::cerr << '\n';
}

bool verifyFuzzedState(
    aste::density::plugin::DensityAudioProcessor& processor) {
  constexpr std::array<const char*, 11> ids{
      "drive",  "crush",  "attack",       "release",    "density", "blend",
      "stereo", "output", "detector_hpf", "protection", "bypass"};
  for (const char* id : ids) {
    const auto* parameter = processor.state().getParameter(id);
    const auto* value = processor.state().getRawParameterValue(id);
    if (parameter == nullptr || value == nullptr ||
        !std::isfinite(value->load())) {
      return false;
    }
    const auto& range = parameter->getNormalisableRange();
    if (value->load() < range.start || value->load() > range.end) {
      return false;
    }
  }

  juce::MemoryBlock firstState;
  juce::MemoryBlock secondState;
  processor.getStateInformation(firstState);
  processor.getStateInformation(secondState);
  if (firstState.isEmpty() || firstState != secondState) {
    return false;
  }

  std::array<float, 7> first{};
  std::array<float, 7> second{};
  for (std::size_t sample = 0; sample < first.size(); ++sample) {
    first[sample] = 0.8F * std::sin(static_cast<float>(sample) * 0.37F);
  }
  second = first;
  float* firstChannel[]{first.data()};
  float* secondChannel[]{second.data()};
  juce::AudioBuffer<float> firstAudio{firstChannel, 1,
                                      static_cast<int>(first.size())};
  juce::AudioBuffer<float> secondAudio{secondChannel, 1,
                                       static_cast<int>(second.size())};
  juce::MidiBuffer midi;
  processor.prepareToPlay(48000.0, static_cast<int>(first.size()));
  processor.processBlock(firstAudio, midi);
  processor.prepareToPlay(48000.0, static_cast<int>(second.size()));
  processor.processBlock(secondAudio, midi);
  for (std::size_t sample = 0; sample < first.size(); ++sample) {
    if (!std::isfinite(first[sample]) || first[sample] != second[sample]) {
      return false;
    }
  }
  return true;
}

void testStateRoundTrip() {
  aste::density::plugin::DensityAudioProcessor source;
  setValue(source, "density", 83.25F);
  setValue(source, "drive", 7.5F);
  setValue(source, "release", 432.1F);
  setValue(source, "stereo", 37.5F);
  setValue(source, "protection", 0.0F);
  setValue(source, "bypass", 1.0F);

  juce::MemoryBlock state;
  source.getStateInformation(state);
  require(!state.isEmpty(), "State serialization produces data");

  aste::density::plugin::DensityAudioProcessor restored;
  restored.setStateInformation(state.getData(),
                               static_cast<int>(state.getSize()));
  require(std::abs(rawValue(restored, "density") - 83.25F) < 0.011F,
          "Density restores exactly within parameter interval");
  require(std::abs(rawValue(restored, "drive") - 7.5F) < 0.011F,
          "Drive restores exactly within parameter interval");
  require(std::abs(rawValue(restored, "release") - 432.1F) < 0.11F,
          "Release restores exactly within parameter interval");
  require(std::abs(rawValue(restored, "stereo") - 37.5F) < 0.011F,
          "Stereo link restores exactly within parameter interval");
  require(rawValue(restored, "protection") < 0.5F, "Boolean state restores");
  require(rawValue(restored, "bypass") >= 0.5F, "Bypass state restores");

  juce::MemoryBlock repeated;
  restored.getStateInformation(repeated);
  require(state == repeated, "Equivalent state serialization is deterministic");

  const auto beforeMalformed = rawValue(restored, "density");
  constexpr char malformed[] = "not-density-state";
  restored.setStateInformation(malformed, static_cast<int>(sizeof(malformed)));
  require(rawValue(restored, "density") == beforeMalformed,
          "Malformed state leaves current state unchanged");

  auto futureSchema = source.state().copyState();
  futureSchema.setProperty("schema", 2, nullptr);
  futureSchema.setProperty("product", "density-d01", nullptr);
  futureSchema.getChildWithProperty("id", "density")
      .setProperty("value", 12.0, nullptr);
  const auto futureBinary = binaryState(futureSchema);
  restored.setStateInformation(futureBinary.getData(),
                               static_cast<int>(futureBinary.getSize()));
  require(rawValue(restored, "density") == beforeMalformed,
          "Unknown state schemas are rejected at the migration boundary");

  auto invalidValue = source.state().copyState();
  invalidValue.setProperty("schema", 1, nullptr);
  invalidValue.setProperty("product", "density-d01", nullptr);
  invalidValue.getChildWithProperty("id", "density")
      .setProperty("value", "nan", nullptr);
  const auto invalidBinary = binaryState(invalidValue);
  restored.setStateInformation(invalidBinary.getData(),
                               static_cast<int>(invalidBinary.getSize()));
  require(rawValue(restored, "density") == beforeMalformed,
          "Non-finite parameter state is rejected");

  auto duplicate = source.state().copyState();
  duplicate.setProperty("schema", 1, nullptr);
  duplicate.setProperty("product", "density-d01", nullptr);
  duplicate.addChild(
      duplicate.getChildWithProperty("id", "density").createCopy(), -1,
      nullptr);
  const auto duplicateBinary = binaryState(duplicate);
  restored.setStateInformation(duplicateBinary.getData(),
                               static_cast<int>(duplicateBinary.getSize()));
  require(rawValue(restored, "density") == beforeMalformed,
          "Duplicate parameter IDs are rejected");

  auto partial = source.state().copyState();
  partial.setProperty("schema", 1, nullptr);
  partial.setProperty("product", "density-d01", nullptr);
  for (int index = partial.getNumChildren(); --index >= 0;) {
    if (partial.getChild(index).getProperty("id").toString() != "density") {
      partial.removeChild(index, nullptr);
    }
  }
  const auto partialBinary = binaryState(partial);
  restored.setStateInformation(partialBinary.getData(),
                               static_cast<int>(partialBinary.getSize()));
  require(std::abs(rawValue(restored, "density") - 83.25F) < 0.011F,
          "Present parameters restore from partial state");
  require(std::abs(rawValue(restored, "drive")) < 0.011F,
          "Missing parameters restore documented defaults");
  require(std::abs(rawValue(restored, "stereo") - 100.0F) < 0.011F,
          "Older partial state receives the fully linked Stereo default");
}

void testStateFuzz() {
  constexpr std::size_t byteCases = 2048;
  constexpr std::size_t treeCases = 1024;
  std::uint32_t randomState = 0xD01F022U;
  std::size_t caseIndex = 0;
  aste::density::plugin::DensityAudioProcessor processor;
  setValue(processor, "density", 73.25F);
  setValue(processor, "drive", 11.5F);
  juce::MemoryBlock validState;
  processor.getStateInformation(validState);
  const auto validBytes = bytes(validState);
  require(!validBytes.empty(), "State fuzz has a valid mutation seed");

  auto apply = [&](const std::vector<std::byte>& input) {
    ++caseIndex;
    processor.setStateInformation(input.empty() ? nullptr : input.data(),
                                  static_cast<int>(input.size()));
    if (!verifyFuzzedState(processor)) {
      reportFuzzFailure(caseIndex, "state-or-audio-invariant", input);
      require(false, "Fuzzed state remains bounded, deterministic, and usable");
      return false;
    }
    return true;
  };

  for (std::size_t index = 0; index < byteCases; ++index) {
    std::vector<std::byte> input;
    switch (index % 4U) {
      case 0: {
        input.resize(fuzzNext(randomState) % 2049U);
        for (std::byte& value : input) {
          value = static_cast<std::byte>(fuzzNext(randomState) >> 24U);
        }
        break;
      }
      case 1: {
        const std::size_t length =
            fuzzNext(randomState) % (validBytes.size() + 1U);
        input.assign(validBytes.begin(), validBytes.begin() + length);
        break;
      }
      case 2: {
        input = validBytes;
        const std::size_t flips = 1U + fuzzNext(randomState) % 8U;
        for (std::size_t flip = 0; flip < flips; ++flip) {
          const std::size_t position = fuzzNext(randomState) % input.size();
          const auto mask =
              static_cast<std::byte>(1U << (fuzzNext(randomState) % 8U));
          input[position] ^= mask;
        }
        break;
      }
      default: {
        input = validBytes;
        const std::size_t extra = 1U + fuzzNext(randomState) % 64U;
        for (std::size_t byte = 0; byte < extra; ++byte) {
          input.push_back(static_cast<std::byte>(fuzzNext(randomState) >> 24U));
        }
        break;
      }
    }
    if (!apply(input)) {
      return;
    }
  }

  aste::density::plugin::DensityAudioProcessor templateProcessor;
  auto templateState = templateProcessor.state().copyState();
  templateState.setProperty("schema", 1, nullptr);
  templateState.setProperty("product", "density-d01", nullptr);
  constexpr std::array<const char*, 5> invalidValues{"nan", "inf", "-inf",
                                                     "1e999", "not-a-number"};
  for (std::size_t index = 0; index < treeCases; ++index) {
    auto tree = templateState.createCopy();
    const int childIndex =
        static_cast<int>(fuzzNext(randomState) %
                         static_cast<std::uint32_t>(tree.getNumChildren()));
    auto child = tree.getChild(childIndex);
    switch (index % 10U) {
      case 0:
        tree.setProperty("schema", static_cast<int>(fuzzNext(randomState) % 4U),
                         nullptr);
        break;
      case 1:
        tree.setProperty("product", "wrong-product", nullptr);
        break;
      case 2:
        child.setProperty("id", "unknown-parameter", nullptr);
        break;
      case 3:
        child.removeProperty("value", nullptr);
        break;
      case 4:
        tree.addChild(child.createCopy(), -1, nullptr);
        break;
      case 5:
        tree.removeChild(childIndex, nullptr);
        break;
      case 6:
        child.setProperty(
            "value",
            invalidValues[fuzzNext(randomState) % invalidValues.size()],
            nullptr);
        break;
      case 7:
        child.setProperty("value",
                          fuzzNext(randomState) % 2U == 0 ? 1.0e30 : -1.0e30,
                          nullptr);
        break;
      case 8:
        child.setProperty("value",
                          static_cast<double>(static_cast<std::int32_t>(
                              fuzzNext(randomState))) /
                              1000.0,
                          nullptr);
        break;
      default: {
        juce::ValueTree wrongRoot{"wrong-root"};
        wrongRoot.setProperty("schema", 1, nullptr);
        wrongRoot.setProperty("product", "density-d01", nullptr);
        for (int childToCopy = 0; childToCopy < tree.getNumChildren();
             ++childToCopy) {
          wrongRoot.addChild(tree.getChild(childToCopy).createCopy(), -1,
                             nullptr);
        }
        tree = std::move(wrongRoot);
        break;
      }
    }
    const auto input = bytes(binaryState(tree));
    if (!apply(input)) {
      return;
    }
  }
  require(caseIndex == byteCases + treeCases,
          "State fuzz executes every deterministic case");
}

void testLifecycleAndAudio() {
  aste::density::plugin::DensityAudioProcessor processor;
  require(processor.getNumPrograms() == 1 &&
              processor.getProgramName(0) == "Default",
          "The single factory program has a valid name");
  juce::AudioProcessor::BusesLayout stereo;
  stereo.inputBuses.add(juce::AudioChannelSet::stereo());
  stereo.outputBuses.add(juce::AudioChannelSet::stereo());
  require(processor.isBusesLayoutSupported(stereo),
          "Stereo layout is supported");
  processor.prepareToPlay(48000.0, 127);
  require(processor.getLatencySamples() == 0, "Host latency reports zero");

  juce::AudioBuffer<float> audio{2, 127};
  for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
    const float value = 0.8F * std::sin(static_cast<float>(sample) * 0.17F);
    audio.setSample(0, sample, value);
    audio.setSample(1, sample, value);
  }
  juce::MidiBuffer midi;
  processor.processBlock(audio, midi);
  for (int channel = 0; channel < audio.getNumChannels(); ++channel) {
    for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
      require(std::isfinite(audio.getSample(channel, sample)),
              "Plugin output remains finite");
    }
  }
  require(processor.inputPeak() > 0.0F, "Input meter receives audio");
  require(processor.outputPeak() > 0.0F, "Output meter receives audio");

  setValue(processor, "bypass", 1.0F);
  const float bypassInput = audio.getSample(0, 10);
  processor.processBlock(audio, midi);
  require(audio.getSample(0, 10) == bypassInput,
          "Owned bypass parameter leaves audio unchanged");
  processor.releaseResources();
}

void testProcessingBoundary() {
  constexpr std::array<double, 6> rates{44100.0, 48000.0,  88200.0,
                                        96000.0, 176400.0, 192000.0};
  constexpr std::array<int, 14> blocks{0,   1,   2,   7,   16,  32,   64,
                                       127, 128, 256, 511, 512, 1024, 2048};
  std::array<float, 2048> left{};
  std::array<float, 2048> right{};
  float* channels[]{left.data(), right.data()};
  juce::MidiBuffer midi;
  aste::density::plugin::DensityAudioProcessor processor;
  std::size_t processAllocations = 0;
  bool allocationReported = false;

#if JUCE_MAC
  pthread_mutex_t calibrationMutex = PTHREAD_MUTEX_INITIALIZER;
  asteRealtimeAuditReset();
  asteRealtimeAuditSetActive(1);
  const int lockResult = pthread_mutex_lock(&calibrationMutex);
  if (lockResult == 0) {
    pthread_mutex_unlock(&calibrationMutex);
  }
  const int nullFile = open("/dev/null", O_WRONLY);
  if (nullFile >= 0) {
    write(nullFile, "", 0);
    close(nullFile);
  }
  asteRealtimeAuditSetActive(0);
  const auto calibrationLocks = asteRealtimeAuditLockCalls();
  const auto calibrationOpens = asteRealtimeAuditFileOpenCalls();
  const auto calibrationWrites = asteRealtimeAuditWriteCalls();
  const bool auditCalibrated = lockResult == 0 && nullFile >= 0 &&
                               calibrationLocks > 0 && calibrationOpens > 0 &&
                               calibrationWrites > 0;
  if (!auditCalibrated) {
    std::cerr << "audit calibration: locks=" << calibrationLocks
              << " opens=" << calibrationOpens
              << " writes=" << calibrationWrites << '\n';
  }
  require(auditCalibrated,
          "macOS forbidden-call interposition self-calibrates");
  asteRealtimeAuditReset();
#endif

  for (std::size_t cycle = 0; cycle < 64; ++cycle) {
    processor.prepareToPlay(rates[cycle % rates.size()], 2048);
    setValue(processor, "drive", cycle % 2 == 0 ? -12.0F : 24.0F);
    setValue(processor, "density", cycle % 3 == 0 ? 0.0F : 100.0F);
    setValue(processor, "stereo", cycle % 4 == 0 ? 0.0F : 100.0F);
    setValue(processor, "bypass", cycle % 5 == 0 ? 1.0F : 0.0F);

    for (int frames : blocks) {
      for (int sample = 0; sample < frames; ++sample) {
        const auto position =
            cycle * left.size() + static_cast<std::size_t>(sample);
        const float value =
            0.9F * std::sin(static_cast<float>(position) * 0.017F);
        left[static_cast<std::size_t>(sample)] = value;
        right[static_cast<std::size_t>(sample)] = -0.37F * value;
      }
      const int channelCount = cycle % 2 == 0 ? 1 : 2;
      juce::AudioBuffer<float> audio{channels, channelCount, frames};
      const auto before = allocations.load(std::memory_order_relaxed);
#if JUCE_MAC
      asteRealtimeAuditSetActive(1);
#endif
      setAllocationAuditActive(true);
      processor.processBlock(audio, midi);
      setAllocationAuditActive(false);
#if JUCE_MAC
      asteRealtimeAuditSetActive(0);
#endif
      const auto after = allocations.load(std::memory_order_relaxed);
      processAllocations += after - before;
      if (after != before && !allocationReported) {
        std::cerr << "first adapter allocation: cycle=" << cycle
                  << " frames=" << frames << " count=" << after - before
                  << '\n';
        allocationReported = true;
      }

      bool finite = true;
      for (int channel = 0; channel < channelCount; ++channel) {
        for (int sample = 0; sample < frames; ++sample) {
          finite = finite && std::isfinite(audio.getSample(channel, sample));
        }
      }
      require(finite, "Adapter lifecycle output remains finite");
    }
    processor.releaseResources();
  }

  require(processAllocations == 0,
          "Full plugin processing boundary performs no heap allocation");
#if JUCE_MAC
  require(asteRealtimeAuditLockCalls() == 0,
          "Audio callback acquires no audited POSIX or unfair locks");
  require(asteRealtimeAuditFileOpenCalls() == 0,
          "Audio callback opens no files");
  require(asteRealtimeAuditWriteCalls() == 0,
          "Audio callback performs no direct writes or logging");
#endif
}

void testEditorContract() {
  aste::density::plugin::DensityAudioProcessor processor;
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  require(editor != nullptr, "Editor can be created");
  if (editor == nullptr) {
    return;
  }

  require(editor->isResizable(), "Editor is host-resizable");
  const auto* constrainer = editor->getConstrainer();
  require(constrainer != nullptr && constrainer->getMinimumWidth() == 760 &&
              constrainer->getMinimumHeight() == 420 &&
              constrainer->getMaximumWidth() == 1520 &&
              constrainer->getMaximumHeight() == 840,
          "Editor exposes its supported scale range");

  struct SliderContract {
    const char* title;
    double defaultValue;
    const char* input;
    double parsedValue;
  };
  constexpr std::array<SliderContract, 9> sliderContracts{{
      {"DENSITY", 50.0, "73.25 %", 73.25},
      {"DRIVE", 0.0, "-6.50 dB", -6.50},
      {"CRUSH", 65.0, "82.50 %", 82.50},
      {"ATTACK", 1.0, "0.125 ms", 0.125},
      {"RELEASE", 180.0, "777.7 ms", 777.7},
      {"BLEND", 50.0, "37.50 %", 37.50},
      {"STEREO", 100.0, "62.50 %", 62.50},
      {"DETECTOR HPF", 90.0, "137.5 Hz", 137.5},
      {"OUTPUT", 0.0, "-3.25 dB", -3.25},
  }};
  for (const auto& contract : sliderContracts) {
    auto* slider =
        dynamic_cast<juce::Slider*>(findTitled(*editor, contract.title));
    require(slider != nullptr, "Every slider has a stable accessibility title");
    if (slider == nullptr) {
      continue;
    }
    require(slider->isTextBoxEditable(),
            "Every slider accepts exact numeric entry");
    require(slider->isDoubleClickReturnEnabled() &&
                slider->getDoubleClickReturnValue() == contract.defaultValue,
            "Every slider resets to its documented default");
    require(std::abs(slider->getValueFromText(contract.input) -
                     contract.parsedValue) < 0.001,
            "Every slider parses an exact value with its unit");
    require(slider->isAccessible(),
            "Every slider is exposed to accessibility clients");
  }

  auto* protection = findTitled(*editor, "PROTECTION");
  require(protection != nullptr && protection->getWantsKeyboardFocus() &&
              protection->isAccessible(),
          "Protection is keyboard and accessibility reachable");

  constexpr std::array<const char*, 10> focusTitles{
      "DENSITY", "DRIVE",  "CRUSH",        "ATTACK", "RELEASE",
      "BLEND",   "STEREO", "DETECTOR HPF", "OUTPUT", "PROTECTION"};
  for (std::size_t index = 0; index < focusTitles.size(); ++index) {
    auto* control = findTitled(*editor, focusTitles[index]);
    require(
        control != nullptr && control->getWantsKeyboardFocus() &&
            control->getExplicitFocusOrder() == static_cast<int>(index + 1U),
        "Every essential control has deterministic keyboard focus order");
  }

  auto* output = dynamic_cast<juce::Slider*>(findTitled(*editor, "OUTPUT"));
  require(output != nullptr && output->getTextFromValue(-0.000001) == "0.00 dB",
          "Rounded zero never displays a negative sign");
  if (output != nullptr) {
    auto* textBox = dynamic_cast<juce::Label*>(output->getChildComponent(0));
    require(textBox != nullptr && textBox->getText() == "0.00 dB",
            "Output text box initially displays canonical zero");
  }

  editor->setSize(760, 420);
  juce::Image image{juce::Image::RGB, editor->getWidth(), editor->getHeight(),
                    true};
  juce::Graphics graphics{image};
  editor->paintEntireComponent(graphics, true);
  require(image.getPixelAt(0, 0).getAlpha() == 255,
          "Minimum-size editor paints an opaque panel");
}

double benchmarkPaint(juce::AudioProcessorEditor& editor) {
  constexpr int frames = 120;
  juce::Image image{juce::Image::RGB, editor.getWidth(), editor.getHeight(),
                    true};
  const auto start = std::chrono::steady_clock::now();
  for (int frame = 0; frame < frames; ++frame) {
    juce::Graphics graphics{image};
    editor.paintEntireComponent(graphics, true);
  }
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  return elapsed * 1000.0 / frames;
}

int createEditorArtifacts(const char* directoryPath) {
  aste::density::plugin::DensityAudioProcessor processor;
  processor.prepareToPlay(48000.0, 512);
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  if (editor == nullptr) {
    return 1;
  }
  editor->setSize(980, 540);

  std::this_thread::sleep_for(std::chrono::milliseconds{40});
  juce::Timer::callPendingTimersSynchronously();
  const double idleMs = benchmarkPaint(*editor);

  setValue(processor, "drive", 12.0F);
  setValue(processor, "density", 80.0F);
  juce::AudioBuffer<float> audio{2, 512};
  juce::MidiBuffer midi;
  for (int block = 0; block < 32; ++block) {
    for (int sample = 0; sample < audio.getNumSamples(); ++sample) {
      const float value =
          0.85F * std::sin(static_cast<float>(block * 512 + sample) * 0.031F);
      audio.setSample(0, sample, value);
      audio.setSample(1, sample, value);
    }
    processor.processBlock(audio, midi);
  }
  std::this_thread::sleep_for(std::chrono::milliseconds{40});
  juce::Timer::callPendingTimersSynchronously();
  const double activeMs = benchmarkPaint(*editor);

  const auto snapshot = editor->createComponentSnapshot(
      editor->getLocalBounds(), true, 2.0F, juce::SoftwareImageType{});
  const juce::File directory{juce::String::fromUTF8(directoryPath)};
  if (directory.createDirectory().failed()) {
    return 2;
  }
  juce::FileOutputStream output{
      directory.getChildFile("density-d01-editor-2x.png")};
  if (!output.openedOk() || !output.setPosition(0) ||
      output.truncate().failed() ||
      !juce::PNGImageFormat{}.writeImageToStream(snapshot, output)) {
    return 2;
  }
  output.flush();
  std::cout << "{\"width\":" << snapshot.getWidth()
            << ",\"height\":" << snapshot.getHeight()
            << ",\"paint_frames\":120,\"idle_ms_per_frame\":" << idleMs
            << ",\"active_ms_per_frame\":" << activeMs << "}\n";
  processor.releaseResources();
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  juce::ScopedJuceInitialiser_GUI juce;
  if (argc == 3 && std::string_view{argv[1]} == "--editor-artifacts") {
    return createEditorArtifacts(argv[2]);
  }
  if (argc != 1) {
    return 2;
  }
  testParameterTextContract();
  testStateRoundTrip();
  testStateFuzz();
  testLifecycleAndAudio();
  testProcessingBoundary();
  testEditorContract();
  if (failures == 0) {
    std::cout << "density_plugin_tests: ok\n";
  }
  return failures == 0 ? 0 : 1;
}
