#pragma once

namespace aste::tests {
inline juce::Button* responseButton(juce::Component& component) {
  if (component.getTitle() == "RESPONSE MODE")
    return dynamic_cast<juce::Button*>(&component);
  for (auto* child : component.getChildren())
    if (auto* button = responseButton(*child)) return button;
  return nullptr;
}

template <class Processor, class Check>
void responseUpgradeChecks(Check require, int oldSchema) {
  Processor processor;
  juce::MemoryBlock bytes;
  processor.getStateInformation(bytes);
  auto xml = juce::AudioProcessor::getXmlFromBinary(
      bytes.getData(), static_cast<int>(bytes.getSize()));
  xml->setAttribute("schema", oldSchema);
  juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
  processor.setStateInformation(bytes.getData(),
                                static_cast<int>(bytes.getSize()));
  processor.getStateInformation(bytes);
  auto expected = juce::AudioProcessor::getXmlFromBinary(
      bytes.getData(), static_cast<int>(bytes.getSize()));
  std::unique_ptr<juce::AudioProcessorEditor> editor{processor.createEditor()};
  auto* button = responseButton(*editor);
  require(button && button->isEnabled(),
          "Recalled response offers an explicit upgrade action");
  if (!button) return;
  button->onClick();
  require(!button->isEnabled() && button->getButtonText() == "CURRENT RESPONSE",
          "Upgrade immediately reports its resulting state");
  expected->setAttribute("behavior", 1);
  if (expected->hasAttribute("clock")) expected->setAttribute("clock", 1);
  processor.getStateInformation(bytes);
  auto upgraded = juce::AudioProcessor::getXmlFromBinary(
      bytes.getData(), static_cast<int>(bytes.getSize()));
  require(upgraded && upgraded->toString() == expected->toString(),
          "Upgrade changes only response metadata, preserving controls and "
          "tape payloads");
  Processor reopened;
  reopened.setStateInformation(bytes.getData(),
                               static_cast<int>(bytes.getSize()));
  std::unique_ptr<juce::AudioProcessorEditor> again{reopened.createEditor()};
  auto* current = responseButton(*again);
  require(current && !current->isEnabled(),
          "Explicit response upgrade survives session reopen");
}
}  // namespace aste::tests
