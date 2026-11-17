#pragma once

namespace aste::tests {
template <class Processor, class Check>
void masterOutputChecks(Check require) {
  Processor a, b;
  const auto set = [](auto& p, const char* id, float value) {
    if (auto* parameter = p.state().getParameter(id))
      parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
  };
  require(a.state().getParameter("master") != nullptr,
          "Every product exposes an independent final master level");
  set(a, "master", 0);
  set(a, "blend", 0);
  set(a, "mix", 0);
  set(a, "sequence", 0);
  juce::MemoryBlock state;
  a.getStateInformation(state);
  b.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  set(b, "master", 6);
  a.prepareToPlay(48000, 511);
  b.prepareToPlay(48000, 511);
  juce::AudioBuffer<float> x{2, 511}, y{2, 511};
  for (int ch = 0; ch < 2; ++ch)
    for (int i = 0; i < 511; ++i) x.setSample(ch, i, .02F * std::sin(i * .13F));
  y.makeCopyOf(x);
  juce::MidiBuffer ma, mb;
  ma.addEvent(juce::MidiMessage::noteOn(1, 60, .8F), 0);
  mb.addEvents(ma, 0, -1, 0);
  a.processBlock(x, ma);
  b.processBlock(y, mb);
  float peak = 0;
  const float ratio = juce::Decibels::decibelsToGain(6.F);
  for (int ch = 0; ch < 2; ++ch)
    for (int i = 0; i < 511; ++i) {
      require(
          std::abs(y.getSample(ch, i) - ratio * x.getSample(ch, i)) < 2.e-6F,
          "Master gain raises final level without changing sound shaping");
      peak = std::max(peak, std::abs(y.getSample(ch, i)));
    }
  require(peak > .0001F && std::abs(b.outputPeak() - peak) < 2.e-6F,
          "Output meter reports final audible master level");
  auto xml = juce::AudioProcessor::getXmlFromBinary(
      state.getData(), static_cast<int>(state.getSize()));
  auto old = juce::ValueTree::fromXml(*xml);
  old.removeChild(old.getChildWithProperty("id", "master"), nullptr);
  juce::AudioProcessor::copyXmlToBinary(*old.createXml(), state);
  b.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
  require(std::abs(b.state().getRawParameterValue("master")->load()) < .001F,
          "Sessions predating master gain restore unity rather than a "
          "fresh-instance boost");
  std::unique_ptr<juce::AudioProcessorEditor> editor{b.createEditor()};
  bool visible = false;
  for (auto* child : editor->getChildren())
    visible |= child->getTitle() == "MASTER OUTPUT" && child->isVisible() &&
               child->getWidth() > 0;
  require(visible, "Master Output is visible in every editor header");
}
}  // namespace aste::tests
