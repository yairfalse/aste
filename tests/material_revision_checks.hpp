#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
namespace aste::tests {
template <class Processor, class Check>
void materialRevisionChecks(Check require) {
  Processor p;
  juce::MemoryBlock bytes;
  p.getStateInformation(bytes);
  auto xml = juce::AudioProcessor::getXmlFromBinary(
      bytes.getData(), static_cast<int>(bytes.getSize()));
  require(xml && xml->getStringAttribute("material") == "1",
          "New instances select material behavior");
  xml->removeAttribute("material");
  juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
  p.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
  require(!p.usesMaterialResponse(),
          "States predating the revision retain their old sound");
  p.getStateInformation(bytes);
  const auto old = bytes;
  xml = juce::AudioProcessor::getXmlFromBinary(
      bytes.getData(), static_cast<int>(bytes.getSize()));
  xml->setAttribute("material", "1.0");
  juce::AudioProcessor::copyXmlToBinary(*xml, bytes);
  p.setStateInformation(bytes.getData(), static_cast<int>(bytes.getSize()));
  p.getStateInformation(bytes);
  require(bytes == old,
          "Malformed material revision rejects the entire restore");
  std::vector<float> controls;
  for (auto* parameter : p.getParameters())
    controls.push_back(parameter->getValue());
  p.useCurrentResponse();
  require(p.usesMaterialResponse(),
          "The explicit updated-sound action adopts material behavior");
  for (int i = 0; i < p.getParameters().size(); ++i)
    require(p.getParameters()[i]->getValue() ==
                controls[static_cast<std::size_t>(i)],
            "Adopting material behavior preserves saved parameter values");
  p.getStateInformation(bytes);
  Processor reopened;
  reopened.setStateInformation(bytes.getData(),
                               static_cast<int>(bytes.getSize()));
  juce::MemoryBlock again;
  reopened.getStateInformation(again);
  require(reopened.usesMaterialResponse() && again == bytes,
          "Material response recalls byte-stably");
}
}  // namespace aste::tests
