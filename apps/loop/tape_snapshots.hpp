#pragma once

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace aste::loop {

struct TapeState {
  double sampleRate{48000};
  std::size_t active{};
  std::array<std::uint32_t, 3> generations{};
  std::array<std::vector<float>, 3>
      audio;  // Interleaved stereo, portable on serialization.
};

// Preallocated copy-on-write pages keep the last published block immutable
// while a non-audio thread saves it. Only the audio thread changes page
// reference counts. No allocation, deletion, waiting or mutex is needed by
// write() or publish().
class TapeSnapshots {
 public:
  void prepare(std::size_t capacity) {
    enabled_ = false;
    dirty_ = true;
    latest_.store(-1);
    pagesPerDeck_ = (capacity + pageFrames - 1) / pageFrames;
    const auto count = pagesPerDeck_ * 3;
    pool_.assign(count * 4, {});
    free_.clear();
    free_.reserve(pool_.size());
    current_.resize(count);
    for (std::size_t i = 0; i < count; ++i) {
      current_[i] = i;
      pool_[i].references = 1;
    }
    for (std::size_t i = count; i < pool_.size(); ++i) free_.push_back(i);
    for (auto& slot : slots_) {
      slot.pages.assign(count, invalid);
      slot.busy.store(0);
    }
    latest_.store(-1);
    enabled_ = capacity > 0;
  }

  void write(std::size_t deck, std::size_t frame, float left,
             float right) noexcept {
    if (!enabled_ || current_.empty()) return;
    dirty_ = true;
    auto& index = current_[deck * pagesPerDeck_ + frame / pageFrames];
    if (pool_[index].references > 1) {
      const auto replacement = free_.back();
      free_.pop_back();
      pool_[replacement].samples = pool_[index].samples;
      --pool_[index].references;
      index = replacement;
      pool_[index].references = 1;
    }
    const auto offset = (frame % pageFrames) * 2;
    pool_[index].samples[offset] = left;
    pool_[index].samples[offset + 1] = right;
  }

  void publish(double rate, std::size_t active,
               std::array<std::size_t, 3> lengths,
               std::array<std::uint32_t, 3> generations) noexcept {
    if (!enabled_) return;
    const int previous = latest_.load();
    if (!dirty_ && previous >= 0) {
      const auto& slot = slots_[static_cast<std::size_t>(previous)];
      if (slot.rate == rate && slot.active == active &&
          slot.lengths == lengths && slot.generations == generations)
        return;
    }
    for (int i = 0; i < 3; ++i) {
      if (i == latest_.load()) continue;
      auto& slot = slots_[static_cast<std::size_t>(i)];
      int expected = 0;
      if (!slot.busy.compare_exchange_strong(expected, 1)) continue;
      for (std::size_t page = 0; page < current_.size(); ++page) {
        const auto old = slot.pages[page];
        if (old != invalid && --pool_[old].references == 0)
          free_.push_back(old);
        slot.pages[page] = current_[page];
        ++pool_[current_[page]].references;
      }
      slot.rate = rate;
      slot.active = active;
      slot.lengths = lengths;
      slot.generations = generations;
      slot.busy.store(0);
      latest_.store(i);
      dirty_ = false;
      return;
    }
  }

  TapeState read() const {
    for (;;) {
      const int index = latest_.load();
      if (index < 0) return {};
      const auto& slot = slots_[static_cast<std::size_t>(index)];
      int expected = 0;
      if (!slot.busy.compare_exchange_strong(expected, 2)) continue;
      // Release even if a large off-thread allocation fails.
      struct Release {
        std::atomic<int>& busy;
        ~Release() { busy.store(0); }
      } release{slot.busy};
      TapeState result;
      result.sampleRate = slot.rate;
      result.active = slot.active;
      result.generations = slot.generations;
      for (std::size_t deck = 0; deck < 3; ++deck) {
        // An unfinished RELOOP destination is not a retained generation yet.
        const auto length =
            slot.generations[deck] == 0 ? 0 : slot.lengths[deck];
        result.audio[deck].resize(length * 2);
        for (std::size_t frame = 0; frame < length; ++frame) {
          const auto page =
              slot.pages[deck * pagesPerDeck_ + frame / pageFrames];
          const auto offset = (frame % pageFrames) * 2;
          result.audio[deck][frame * 2] = pool_[page].samples[offset];
          result.audio[deck][frame * 2 + 1] = pool_[page].samples[offset + 1];
        }
      }
      return result;
    }
  }

 private:
  static constexpr std::size_t pageFrames = 1024;
  static constexpr std::size_t invalid = static_cast<std::size_t>(-1);
  struct Page {
    std::array<float, pageFrames * 2> samples{};
    unsigned references{};
  };
  struct Slot {
    mutable std::atomic<int> busy{};
    std::vector<std::size_t> pages;
    double rate{48000};
    std::size_t active{};
    std::array<std::size_t, 3> lengths{};
    std::array<std::uint32_t, 3> generations{};
  };
  std::size_t pagesPerDeck_{};
  bool enabled_{};
  bool dirty_{true};
  std::vector<Page> pool_;
  std::vector<std::size_t> current_, free_;
  std::array<Slot, 3> slots_;
  std::atomic<int> latest_{-1};
};
static_assert(std::atomic<int>::is_always_lock_free);

}  // namespace aste::loop
