// Product fork of the independent G-01 laboratory, 2026-11-16.
#pragma once
#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>
namespace aste::grain {
constexpr std::size_t nodes = 3, maxGrains = 48;
using Stereo = std::array<float, 2>;
enum class TimeMode { Independent, Varispeed };
enum class Window { Hann, Triangle, Fragment };
enum class Interpolation { Linear, Sinc16 };
struct NodeControls {
  float sizeMs{120}, density{16}, speed{1}, pitch{}, position{},
      variation{.15F};
  float temporalSpread{}, stereoSpread{}, width{1}, pan{}, panDrift{};
  float erosion{}, inheritance{1}, level{.65F}, captureSeconds{.5F},
      accumulate{};
  bool capture{true}, freeze{}, reverse{}, sync{};
  float division{.25F}, cluster{};
  TimeMode mode{TimeMode::Independent};
  Window window{Window::Hann};
  Interpolation interpolation{Interpolation::Linear};
  bool operator==(const NodeControls&) const = default;
};
struct Controls {
  std::array<NodeControls, nodes> node{};
  std::array<float, nodes> live{1, 0, 0};
  // routes[destination][source]; no topological sort, all edges delayed 1
  // sample.
  std::array<std::array<float, nodes>, nodes> routes{
      {{0, 0, 0}, {.85F, 0, 0}, {0, .85F, 0}}};
  float time{}, spread{}, erosion{}, inheritance{1}, master{.8F}, mix{1};
  bool freeze{};
  std::uint64_t seed{1701};
  bool operator==(const Controls&) const = default;
};
struct Transport {
  bool valid{}, playing{}, discontinuity{};
  double ppq{}, bpm{120};
};
enum class Action { Clear, CaptureNow, Mutate };
struct Event {
  std::size_t offset{};
  Action action{};
  std::uint8_t node{};
};
struct Change {
  std::size_t offset{};
  Controls controls{};
};
struct NodeMeter {
  std::uint64_t launched{}, dropped{}, commits{}, captureStalls{}, generation{};
  std::size_t active{}, storedFrames{};
  float storedRms{}, outputRms{}, captureRms{}, effectiveDensity{};
  bool frozen{}, capturing{};
};
struct Snapshot {
  std::array<NodeMeter, nodes> node{};
  std::array<float, nodes> routeScale{1, 1, 1};
  std::uint64_t sanitized{}, rejected{};
  float peak{};
};
class Engine {
 public:
  bool prepare(double rate, double memorySeconds = 4);
  void reset() noexcept;
  void clear(std::size_t node) noexcept;
  // Single audio-thread ownership. Events/changes must be sorted; invalid
  // batches are rejected before any partial change. Controls copied, never
  // retained.
  void process(const float* left, const float* right, float* outL, float* outR,
               std::size_t frames, const Controls&, Transport = {},
               std::span<const Event> events = {},
               std::span<const Change> changes = {}) noexcept;
  static bool validControls(const Controls& c) noexcept {
    std::uint64_t count = 0;
    sanitize(c, count);
    return count == 0;
  }
  Snapshot snapshot() const noexcept { return meter_; }
  std::size_t heapBytes() const noexcept;

 private:
  struct Bank {
    std::array<std::vector<float>, 2> data;
    std::size_t length{}, refs{};
    double power{};
    std::uint64_t generation{};
  };
  struct Grain {
    bool active{};
    unsigned bank{}, filter{};
    std::size_t age{}, length{};
    double cursor{}, step{};
    float pan{}, erosion{}, inheritance{}, width{1};
    Window window{};
    Interpolation interpolation{};
    Stereo low{};
  };
  struct Node {
    std::array<Bank, 3> bank;
    std::array<Grain, maxGrains> grains;
    int published{-1};
    unsigned recording{};
    std::size_t written{};
    double writePower{}, phase{1}, cursor{}, drift{};
    std::uint64_t random{1};
    float previousPosition{}, fade{1};
    bool previousFreeze{}, previousCapture{true}, previousSync{};
    float previousDivision{.25F};
    Stereo last{};
    NodeMeter meter{};
  };
  static Controls sanitize(Controls, std::uint64_t&) noexcept;
  bool commit(std::size_t) noexcept;
  void launch(std::size_t, const NodeControls&) noexcept;
  Stereo render(std::size_t, const NodeControls&) noexcept;
  float read(const Bank&, unsigned, double, Interpolation,
             unsigned) const noexcept;
  float random(Node&) noexcept;
  double rate_{48000}, routeSmooth_{}, levelSmooth_{};
  std::size_t capacity_{}, voiceLimit_{48};
  bool prepared_{};
  bool transportKnown_{}, wasPlaying_{};
  double expectedPpq_{};
  std::array<Node, nodes> node_{};
  Controls target_{};
  std::array<std::array<float, nodes>, nodes> routes_{};
  float master_{.8F}, mix_{1};
  std::array<float, nodes> live_{}, levels_{};
  std::array<Stereo, nodes> previous_{};
  std::array<float, 2049> hann_{};
  std::array<std::array<std::array<float, 16>, 64>, 6> sinc_{};
  Snapshot meter_{};
  bool initialized_{};
  std::uint64_t clock_{};
};
Controls scene(unsigned index) noexcept;
Controls morph(const Controls&, const Controls&, float amount) noexcept;
}  // namespace aste::grain
