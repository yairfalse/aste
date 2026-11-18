#pragma once

// Included within the editor's private namespace. Audio is copied from the
// published immutable tape snapshot on the message thread, never from a deck
// being written by the audio callback.
class TapeWaveform final : public juce::Component {
 public:
  explicit TapeWaveform(LoopAudioProcessor& processor) : processor_{processor} {
    setTitle("LOOP REGION");
    setDescription(
        "Drag a region; drag its left or right boundary to resize. Arrow keys "
        "move the region; plus and minus resize it.");
    setWantsKeyboardFocus(true);
  }
  void update(const TapeState& tape) {
    minimum_.fill(0);
    maximum_.fill(0);
    const auto& audio = tape.audio[tape.active];
    frames_ = audio.size() / 2;
    seconds_ = frames_ / tape.sampleRate;
    for (std::size_t i = 0; i < frames_; ++i) {
      const auto bin =
          std::min(minimum_.size() - 1, i * minimum_.size() / frames_);
      minimum_[bin] = std::min({minimum_[bin], audio[i * 2], audio[i * 2 + 1]});
      maximum_[bin] = std::max({maximum_[bin], audio[i * 2], audio[i * 2 + 1]});
    }
    repaint();
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{kSurface});
    auto area = getLocalBounds().toFloat().reduced(10);
    g.setFont(juce::FontOptions{11.F});
    g.setColour(juce::Colour{kMuted});
    g.drawText(frames_
                   ? juce::String(seconds_, 2) + " s / " +
                         (get("reverse") > .5F ? "REVERSE " : "FORWARD ") +
                         juce::String(get("speed"), 3) + "x / DRAG TO SELECT"
                   : "LIVE INPUT > CAPTURE > YOUR WAVEFORM",
               area.removeFromTop(22), juce::Justification::centredLeft);
    const float left = get("start") * .01F;
    const float right = get("region_end") * .01F;
    const auto selected = juce::Rectangle<float>{
        area.getX() + left * area.getWidth(), area.getY(),
        std::max(0.F, right - left) * area.getWidth(), area.getHeight()};
    g.setColour(juce::Colour{kAccent}.withAlpha(.18F));
    g.fillRect(selected);
    g.setColour(juce::Colour{kInk});
    const float scale = area.getHeight() * .45F;
    for (std::size_t i = 0; i < minimum_.size(); ++i) {
      const float x = area.getX() +
                      static_cast<float>(i) * area.getWidth() / minimum_.size();
      g.drawVerticalLine(static_cast<int>(x),
                         area.getCentreY() - maximum_[i] * scale,
                         area.getCentreY() - minimum_[i] * scale + 1);
    }
    g.setColour(juce::Colour{kAccent});
    g.fillRect(selected.getX(), area.getY(), 2.F, area.getHeight());
    g.fillRect(selected.getRight() - 2.F, area.getY(), 2.F, area.getHeight());
    g.setColour(juce::Colour{kInk});
    const float x = area.getX() + processor_.loopPosition() * area.getWidth();
    g.fillRect(x, area.getY(), 1.F, area.getHeight());
    g.setColour(juce::Colour{kMuted});
    g.drawRect(getLocalBounds(), hasKeyboardFocus(true) ? 2 : 1);
  }
  void mouseDown(const juce::MouseEvent& e) override {
    if (isShowing()) grabKeyboardFocus();
    processor_.undoManager().beginNewTransaction("Edit loop region");
    for (auto id : {"start", "region_end"})
      processor_.state().getParameter(id)->beginChangeGesture();
    anchor_ = fraction(e.position.x);
    edge_ = std::abs(anchor_ - get("start")) < 2        ? 1
            : std::abs(anchor_ - get("region_end")) < 2 ? 2
                                                        : 0;
  }
  void mouseDrag(const juce::MouseEvent& e) override {
    const float position = fraction(e.position.x);
    if (edge_ == 1)
      set("start", std::min(position, get("region_end") - .1F));
    else if (edge_ == 2)
      set("region_end", std::max(position, get("start") + .1F));
    else {
      set("start", std::min(anchor_, position));
      set("region_end", std::max(anchor_ + .1F, position));
    }
    repaint();
  }
  void mouseUp(const juce::MouseEvent&) override {
    for (auto id : {"start", "region_end"})
      processor_.state().getParameter(id)->endChangeGesture();
  }
  bool keyPressed(const juce::KeyPress& key) override {
    const int code = key.getKeyCode();
    const float step = key.getModifiers().isShiftDown() ? .1F : 1.F;
    float first = get("start"), last = get("region_end");
    if (code == juce::KeyPress::leftKey || code == juce::KeyPress::rightKey) {
      const float move = std::clamp(
          code == juce::KeyPress::leftKey ? -step : step, -first, 100.F - last);
      first += move;
      last += move;
    } else if (code == '+' || code == '=')
      last = std::min(100.F, last + step);
    else if (code == '-')
      last = std::max(first + .1F, last - step);
    else
      return false;
    processor_.undoManager().beginNewTransaction("Nudge loop region");
    for (auto id : {"start", "region_end"})
      processor_.state().getParameter(id)->beginChangeGesture();
    set("start", first);
    set("region_end", last);
    for (auto id : {"start", "region_end"})
      processor_.state().getParameter(id)->endChangeGesture();
    repaint();
    return true;
  }

 private:
  float get(const char* id) const {
    return processor_.state().getRawParameterValue(id)->load();
  }
  void set(const char* id, float value) {
    auto* p = processor_.state().getParameter(id);
    p->setValueNotifyingHost(p->convertTo0to1(std::clamp(value, 0.F, 100.F)));
  }
  float fraction(float x) const {
    return std::clamp((x - 10) / std::max(1, getWidth() - 20) * 100.F, 0.F,
                      99.9F);
  }
  LoopAudioProcessor& processor_;
  std::array<float, 512> minimum_{}, maximum_{};
  std::size_t frames_{};
  double seconds_{};
  float anchor_{};
  int edge_{};
};

class MomentaryRepeat final : public juce::TextButton {
 public:
  explicit MomentaryRepeat(LoopAudioProcessor& processor)
      : juce::TextButton{"HOLD TO REPEAT"}, processor_{processor} {
    setTitle("HOLD TO REPEAT");
    setTooltip(
        "Hold to repeat. Drag left to shorten, up to speed up. Release "
        "restores "
        "your region and speed on the continuing tape timeline. Space also "
        "repeats; Latch allows hands-free editing.");
    setWantsKeyboardFocus(true);
  }
  ~MomentaryRepeat() override { release(); }
  void mouseDown(const juce::MouseEvent& e) override {
    juce::TextButton::mouseDown(e);
    origin_ = e.position;
    engage();
  }
  void mouseDrag(const juce::MouseEvent& e) override {
    juce::TextButton::mouseDrag(e);
    if (!held_) return;
    const auto plain = [this](std::size_t i) {
      return processor_.state()
          .getParameter(temporaryIds_[i])
          ->convertFrom0to1(previousTransforms_[i]);
    };
    const float first = std::min(plain(0), 99.9F);
    auto* end = processor_.state().getParameter("region_end");
    end->setValueNotifyingHost(end->convertTo0to1(
        std::clamp(first + std::max(.1F, plain(1) - first) *
                               std::exp2((e.position.x - origin_.x) / 100.F),
                   first + .1F, 100.F)));
    auto* speed = processor_.state().getParameter("speed");
    speed->setValueNotifyingHost(speed->convertTo0to1(std::clamp(
        plain(2) * std::exp2((origin_.y - e.position.y) / 100.F), .125F, 4.F)));
  }
  void mouseUp(const juce::MouseEvent& e) override {
    release();
    juce::TextButton::mouseUp(e);
  }
  bool keyPressed(const juce::KeyPress& key) override {
    if (key == juce::KeyPress::spaceKey) {
      engage();
      return true;
    }
    return false;
  }
  bool keyStateChanged(bool) override {
    if (!juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey))
      release();
    return held_;
  }
  void focusLost(FocusChangeType) override { release(); }

 private:
  void engage() {
    if (held_) return;
    auto* p = processor_.state().getParameter("repeat");
    previous_ = p->getValue();
    for (std::size_t i = 0; i < temporaryIds_.size(); ++i)
      previousTransforms_[i] =
          processor_.state().getParameter(temporaryIds_[i])->getValue();
    processor_.state().getParameter("region_end")->beginChangeGesture();
    processor_.state().getParameter("speed")->beginChangeGesture();
    p->beginChangeGesture();
    p->setValueNotifyingHost(1);
    held_ = true;
  }
  void release() {
    if (!held_) return;
    auto* p = processor_.state().getParameter("repeat");
    p->setValueNotifyingHost(previous_);
    p->endChangeGesture();
    for (std::size_t i = 0; i < temporaryIds_.size(); ++i) {
      auto* transform = processor_.state().getParameter(temporaryIds_[i]);
      if (transform->getValue() != previousTransforms_[i]) {
        if (i != 1 && i != 2) transform->beginChangeGesture();
        transform->setValueNotifyingHost(previousTransforms_[i]);
        if (i != 1 && i != 2) transform->endChangeGesture();
      }
    }
    processor_.state().getParameter("region_end")->endChangeGesture();
    processor_.state().getParameter("speed")->endChangeGesture();
    held_ = false;
  }
  LoopAudioProcessor& processor_;
  bool held_{};
  float previous_{};
  juce::Point<float> origin_{};
  static constexpr std::array temporaryIds_{"start", "region_end", "speed",
                                            "pitch", "reverse"};
  std::array<float, 5> previousTransforms_{};
};
