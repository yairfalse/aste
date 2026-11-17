#pragma once
#include "workflow_guide.hpp"
#include "material_revision_checks.hpp"
#include <juce_audio_processors/juce_audio_processors.h>
#include <memory>
namespace aste::tests {
template <class Processor, class Require>
void workflowChecks(Require require) {
  if constexpr (requires(Processor& p) { p.usesMaterialResponse(); })
    materialRevisionChecks<Processor>(require);
  Processor processor;
  juce::MemoryBlock before, after;
  processor.getStateInformation(before);
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  const auto find = [&](auto&& self, juce::Component& c) -> ui::WorkflowGuide* {
    if (auto* guide = dynamic_cast<ui::WorkflowGuide*>(&c)) return guide;
    for (auto* child : c.getChildren())
      if (auto* found = self(self, *child)) return found;
    return nullptr;
  };
  auto* guide = find(find, *editor);
  require(guide && guide->isVisible() && guide->helpText().isNotEmpty(),
          "Family first actions are visible when opening the editor");
  if (guide) {
    juce::Component container, control;
    container.setDescription("Inactive: choose the matching mode first.");
    control.setTitle("EXAMPLE CONTROL");
    control.setDescription("Generic child description");
    container.addAndMakeVisible(control);
    editor->addChildComponent(container);
    guide->showControl(control);
    require(guide->helpText().contains("Inactive:"),
            "Family guide reports the current inactive dependency rather than "
            "stale generic help");
    editor->removeChildComponent(&container);
  }
  processor.getStateInformation(after);
  require(before == after,
          "Opening and reading the family guide never changes sound, pattern "
          "or saved state");
}
}  // namespace aste::tests
