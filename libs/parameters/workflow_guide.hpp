#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace aste::ui {
// UI-only guidance. Following focus or the pointer never changes audio state.
class WorkflowGuide final : public juce::Component, private juce::Timer {
 public:
  void bind(juce::Component& editor, juce::String path,
            juce::String firstAction) {
    owner_ = &editor;
    path_ = std::move(path);
    first_ = std::move(firstAction);
    setTitle("WORKFLOW GUIDE");
    setDescription(first_);
    setInterceptsMouseClicks(false, false);
    help_.setTitle("CONTROL HELP");
    help_.setJustificationType(juce::Justification::centredLeft);
    help_.setColour(juce::Label::textColourId, juce::Colour{0xffe7e9e4});
    help_.setFont(juce::FontOptions{13.F});
    help_.setMinimumHorizontalScale(1.F);
    addAndMakeVisible(help_);
    show(first_);
    editor.addMouseListener(this, true);
    startTimerHz(12);
  }
  ~WorkflowGuide() override {
    if (owner_) owner_->removeMouseListener(this);
  }
  void resized() override {
    help_.setBounds(getLocalBounds().withTrimmedTop(16).reduced(8, 0));
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{0xff192124});
    g.setColour(juce::Colour{0xffa9bbb5});
    g.setFont(juce::FontOptions{11.F, juce::Font::bold});
    g.drawText(path_, 8, 0, getWidth() - 16, 16,
               juce::Justification::centredLeft);
  }
  juce::String helpText() const { return help_.getText(); }
  void showControl(juce::Component& component) {
    target_ = &component;
    describe(target_);
  }

 private:
  void show(juce::String text) {
    if (help_.getText() != text)
      help_.setText(text, juce::dontSendNotification);
  }
  void describe(juce::Component* component) {
    juce::String text;
    while (component && component != owner_.getComponent()) {
      if (component == this || isParentOf(component)) return;
      juce::String explanation = component->getDescription();
      if (explanation.isEmpty())
        if (auto* tooltip = dynamic_cast<juce::TooltipClient*>(component))
          explanation = tooltip->getTooltip();
      if (explanation.isNotEmpty()) {
        const auto title = component->getTitle();
        text = (title.isEmpty() ? juce::String{} : title + ": ") + explanation;
      }
      component = component->getParentComponent();
    }
    show(text.isEmpty() ? first_ : text);
  }
  void mouseEnter(const juce::MouseEvent& e) override {
    target_ = e.originalComponent;
    describe(target_);
  }
  void mouseMove(const juce::MouseEvent& e) override {
    target_ = e.originalComponent;
    describe(target_);
  }
  void timerCallback() override {
    auto* focus = juce::Component::getCurrentlyFocusedComponent();
    if (focus != focused_.getComponent()) {
      focused_ = focus;
      if (owner_ && focus && owner_->isParentOf(focus)) target_ = focus;
    }
    if (target_) describe(target_);
  }
  juce::Component::SafePointer<juce::Component> owner_, focused_, target_;
  juce::String path_, first_;
  juce::Label help_;
};
}  // namespace aste::ui
