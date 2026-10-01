#pragma once

#include <juce_audio_utils/juce_audio_utils.h>

inline bool writeMinimumEditorArtifact(juce::AudioProcessorEditor& editor,
                                       const juce::File& file) {
  const auto* limits = editor.getConstrainer();
  if (!limits) return false;
  editor.setSize(limits->getMinimumWidth(), limits->getMinimumHeight());
  for (auto* child : editor.getChildren())
    if (child->isVisible() &&
        !editor.getLocalBounds().contains(child->getBounds()))
      return false;
  const auto image = editor.createComponentSnapshot(editor.getLocalBounds());
  juce::FileOutputStream output{file};
  return output.openedOk() && output.setPosition(0) &&
         output.truncate().wasOk() &&
         juce::PNGImageFormat{}.writeImageToStream(image, output);
}
