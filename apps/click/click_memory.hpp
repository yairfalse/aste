#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <vector>

namespace aste::click {
constexpr int memoryGenerations = 8;
struct FragmentControls {
  float start{}, end{1}, loopStart{}, loopEnd{1}, pitch{}, speed{1},
      cutoff{20000}, resonance{}, morph{};
  int playback{};  // Clean, 16-bit/44.1 kHz, 12-bit/22.05 kHz, 8-bit/11.025
                   // kHz.
  bool reverse{};
};
struct MemoryControls {
  std::array<FragmentControls, memoryGenerations> generation{};
  int selected{}, division{3};
  bool play{}, repeat{}, capture{}, resample{};
  double freeBeats{.125}, captureBeats{4}, captureAgoBeats{};
};
struct CapturedState {
  double rate{48000};
  int selected{};
  std::array<std::vector<float>, memoryGenerations> audio;
};
// Published generations are immutable for this object's lifetime. Replacement
// and destruction happen off audio, protected by the adapter's lifecycle lock.
class MasterMemory {
 public:
  enum Command { None, Capture, Resample, Previous, Next, Original, Select };
  bool prepare(double rate);
  void restore(const CapturedState&);  // Setup only.
  CapturedState state() const;  // Non-audio; concurrent capture/print is safe.
  std::array<float, 128> waveform(int generation) const;
  void process(float*, float*, std::size_t, const MemoryControls&, double bpm,
               Command = None, bool discontinuity = false) noexcept;
  int count() const noexcept { return count_.load(); }
  int selected() const noexcept { return selected_.load(); }
  float position() const noexcept { return position_.load(); }
  float progress() const noexcept { return progress_.load(); }
  int status() const noexcept {
    return status_.load();
  }  // 0 ready, 1 empty, 2 full, 3 printing.
  std::size_t heapBytes() const noexcept;

 private:
  struct Bank {
    std::array<std::vector<float>, 2> data;
    std::size_t start{}, length{};
  };
  bool capture(const MemoryControls&, double) noexcept;
  void publish() noexcept;
  std::array<Bank, memoryGenerations> bank_;
  Bank live_;
  std::size_t capacity_{}, written_{}, available_{}, printAt_{}, printLength_{};
  double rate_{48000}, cursor_{}, mix_{}, transition_{};
  std::array<double, 2> last_{}, ic1_{}, ic2_{}, ic3_{}, ic4_{};
  int requested_{}, playingGeneration_{-1};
  int printSource_{};
  double printTempo_{120};
  MemoryControls printSettings_{};
  FragmentControls smooth_{};
  bool printing_{}, wasCapture_{}, wasResample_{}, wasRepeat_{}, initialized_{};
  std::atomic<int> count_{}, selected_{}, status_{};
  std::atomic<float> position_{}, progress_{};
};
static_assert(std::atomic<int>::is_always_lock_free);
}  // namespace aste::click
