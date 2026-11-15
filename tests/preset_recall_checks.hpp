#pragma once

namespace aste::tests {
inline juce::ComboBox* presetMenu(juce::Component& component) {
  if (component.getTitle() == "PRESETS")
    return dynamic_cast<juce::ComboBox*>(&component);
  for (auto* child : component.getChildren())
    if (auto* menu = presetMenu(*child)) return menu;
  return nullptr;
}

template <class Processor, class Check>
void presetRecallChecks(Check require) {
  for (int index = 0; index < Processor::factoryPresetCount(); ++index) {
    Processor source;
    source.loadFactoryPreset(index);
    juce::MemoryBlock state;
    source.getStateInformation(state);
    Processor restored;
    restored.setStateInformation(state.getData(),
                                 static_cast<int>(state.getSize()));
    std::unique_ptr<juce::AudioProcessorEditor> editor{restored.createEditor()};
    auto* menu = presetMenu(*editor);
    require(menu && menu->getText() == Processor::factoryPresetName(index),
            "Reopened editor identifies the actual factory sound");
    juce::MemoryBlock after;
    restored.getStateInformation(after);
    require(state == after,
            "Identifying a preset does not modify session audio or state");
  }
  Processor changed;
  auto* output = changed.state().getParameter("output");
  output->setValueNotifyingHost(output->convertTo0to1(4.37F));
  juce::MemoryBlock state;
  changed.getStateInformation(state);
  Processor restored;
  restored.setStateInformation(state.getData(),
                               static_cast<int>(state.getSize()));
  std::unique_ptr<juce::AudioProcessorEditor> editor{restored.createEditor()};
  auto* menu = presetMenu(*editor);
  require(menu && menu->getText() == "CUSTOM",
          "Reopened edited sounds are honestly identified as Custom");
}
}  // namespace aste::tests
