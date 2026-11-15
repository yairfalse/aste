#pragma once

// Exercise the adapter's real binary state boundary, not its private flags.
namespace aste::tests {
template <class Processor, class Check>
void behaviorStateChecks(Check require, int previousSchema, int currentSchema) {
  Processor fresh;
  juce::MemoryBlock bytes;
  fresh.getStateInformation(bytes);
  auto xml = juce::AudioProcessor::getXmlFromBinary(
      bytes.getData(), static_cast<int>(bytes.getSize()));
  require(xml && xml->getIntAttribute("schema") == currentSchema &&
              xml->getStringAttribute("behavior") == "1",
          "Fresh instance records current sound behavior");
  if (!xml) return;
  auto old = std::make_unique<juce::XmlElement>(*xml);
  old->setAttribute("schema", previousSchema);
  juce::AudioProcessor::copyXmlToBinary(*old, bytes);
  Processor migrated;
  migrated.setStateInformation(bytes.getData(),
                               static_cast<int>(bytes.getSize()));
  migrated.getStateInformation(bytes);
  auto saved = juce::AudioProcessor::getXmlFromBinary(
      bytes.getData(), static_cast<int>(bytes.getSize()));
  require(saved && saved->getIntAttribute("schema") == currentSchema &&
              saved->getStringAttribute("behavior") == "0",
          "Prior schema is migrated without reinterpreting its sound");
  Processor reopened;
  reopened.setStateInformation(bytes.getData(),
                               static_cast<int>(bytes.getSize()));
  juce::MemoryBlock repeated;
  reopened.getStateInformation(repeated);
  require(repeated == bytes,
          "Legacy behavior survives a byte-stable new-schema round trip");
  for (const char* value : {"", "2", "-1", "1.0", "true", "NaN"}) {
    auto bad = std::make_unique<juce::XmlElement>(*xml);
    if (*value)
      bad->setAttribute("behavior", value);
    else
      bad->removeAttribute("behavior");
    juce::MemoryBlock invalid;
    juce::AudioProcessor::copyXmlToBinary(*bad, invalid);
    reopened.setStateInformation(invalid.getData(),
                                 static_cast<int>(invalid.getSize()));
    reopened.getStateInformation(repeated);
    require(repeated == bytes,
            "Invalid behavior rejects the complete restore without mutation");
  }
}
}  // namespace aste::tests
