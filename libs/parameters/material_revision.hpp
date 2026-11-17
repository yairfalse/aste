#pragma once
#include <atomic>
#include <juce_data_structures/juce_data_structures.h>
namespace aste::parameters {
// Session sound revision, not an extra performance parameter. Old projects
// retain their exact graph until the musician explicitly adopts the new sound.
class MaterialRevision {
 public:
  bool active() const noexcept {
    return active_.load(std::memory_order_relaxed);
  }
  void adopt() noexcept { active_.store(true, std::memory_order_relaxed); }
  void set(bool active) noexcept {
    active_.store(active, std::memory_order_relaxed);
  }
  void write(juce::ValueTree& tree) const {
    tree.setProperty("material", active() ? 1 : 0, nullptr);
  }
  static bool parse(const juce::ValueTree& tree, bool& active) {
    if (!tree.hasProperty("material")) {
      active = false;
      return true;
    }
    const auto text = tree.getProperty("material").toString();
    if (text != "0" && text != "1") return false;
    active = text == "1";
    return true;
  }

 private:
  std::atomic<bool> active_{true};
};
}  // namespace aste::parameters
