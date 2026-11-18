#include "click_plugin.hpp"
namespace aste::click::plugin {
using namespace aste::experimental;
namespace {
constexpr juce::uint32 accent = 0xffc2ab72, background = 0xff101417,
                       ink = 0xffe7ece8;
float parameter(Processor& p, const juce::String& id) {
  return p.state().getRawParameterValue(id)->load();
}
std::unique_ptr<Control> control(Processor& p, const juce::String& id) {
  for (auto spec : p.specs())
    if (spec.id == id) {
      const auto title = spec.name;
      auto split = spec.name.indexOf(" / ");
      if (split >= 0)
        spec.name = spec.name.substring(split + 3);
      else if (id.contains("_e") && spec.name.startsWith("O"))
        spec.name = spec.name.fromFirstOccurrenceOf(" ", false, false)
                        .fromFirstOccurrenceOf(" ", false, false);
      else if (id.contains("_e")) {
        const auto first = spec.name.indexOfChar(' ');
        const auto second = spec.name.indexOfChar(first + 1, ' ');
        if (second >= 0) spec.name = spec.name.substring(second + 1);
      }
      auto c = std::make_unique<Control>(p, spec, true);
      c->setTitle(title);
      c->setDescription(title);
      for (auto* child : c->getChildren()) {
        child->setTitle(title);
        child->setDescription(title);
        if (id.contains("_e") && id.endsWith("_lock"))
          if (auto* button = dynamic_cast<juce::ToggleButton*>(child))
            button->onClick = [&p, id, button] {
              if (!button->getToggleState()) return;
              const int object = juce::jlimit(
                            0, 3, id.substring(1).getIntValue() - 1),
                        step = juce::jlimit(
                            0, 31,
                            id.fromFirstOccurrenceOf("_e", false, false)
                                    .getIntValue() -
                                1);
              const int source = juce::jlimit(
                  0, 3,
                  static_cast<int>(
                      parameter(p, Processor::eventId(object, step, "sound"))) -
                      1);
              const auto sound = p.currentControls().object[source];
              p.set(Processor::eventId(object, step, "pitch"), 0);
              p.set(Processor::eventId(object, step, "start"), sound.start);
              p.set(Processor::eventId(object, step, "cutoff"), sound.cutoff);
              p.set(Processor::eventId(object, step, "morph"), sound.morph);
              p.set(Processor::eventId(object, step, "decay"), sound.decay);
            };
        if (auto* slider = dynamic_cast<juce::Slider*>(child)) {
          const auto previous = slider->onDragStart;
          slider->onDragStart = [&p, title, previous] {
            p.beginEdit(title);
            if (previous) previous();
          };
          const auto key = id.fromLastOccurrenceOf("_", false, false);
          const bool boundary = key == "start" || key == "end" || key == "mod";
          slider->textFromValueFunction = [slider](double value) {
            return aste::ui::formattedValue(*slider, value);
          };
          if (id.endsWith("_speed") || id == "repeat_free")
            slider->textFromValueFunction = [](double value) {
              return juce::String(value, 4);
            };
          slider->updateText();
          slider->valueFromTextFunction = [slider](const juce::String& text) {
            double parsed = 0;
            return aste::parameters::parseFiniteDecimal(
                       text.trim().toStdString(), parsed)
                       ? parsed
                       : slider->getValue();
          };
          if (boundary && id.startsWith("o")) {
            const auto source = [&p, id] {
              int object =
                  juce::jlimit(0, 3, id.substring(1).getIntValue() - 1);
              if (id.contains("_e")) {
                int step = juce::jlimit(
                    0, 31,
                    id.fromFirstOccurrenceOf("_e", false, false).getIntValue() -
                        1);
                object = juce::jlimit(
                    0, 3,
                    static_cast<int>(parameter(
                        p, Processor::eventId(object, step, "sound"))) -
                        1);
              }
              return object;
            };
            slider->textFromValueFunction = [&p, source](double value) {
              const auto length =
                  std::max<std::size_t>(2, p.sampleFrames(source()));
              return juce::String(static_cast<juce::int64>(
                         std::llround(value * (length - 1)))) +
                     " fr";
            };
            slider->updateText();
            slider->valueFromTextFunction = [&p, source, slider](
                                                const juce::String& input) {
              auto text = input.trim();
              const bool ms = text.endsWithIgnoreCase("ms");
              if (ms || text.endsWithIgnoreCase("fr"))
                text = text.dropLastCharacters(2).trimEnd();
              double value = 0;
              if (!aste::parameters::parseFiniteDecimal(text.toStdString(),
                                                        value))
                return slider->getValue();
              const auto object = source();
              if (ms) value *= p.sampleRate(object) * .001;
              return value /
                     static_cast<double>(
                         std::max<std::size_t>(2, p.sampleFrames(object)) - 1);
            };
            slider->setTooltip(
                "Sample-frame position. Type an exact frame number, or a value "
                "ending in ms. Shift-drag for fine editing.");
            slider->updateText();
          }
        }
      }
      return c;
    }
  jassertfalse;
  return {};
}
class Gate final : public juce::TextButton {
 public:
  Gate(Processor& processor, juce::String id, juce::String title,
       int division = -1)
      : p_{processor}, id_{std::move(id)}, division_{division} {
    setButtonText(title);
    setTitle(title);
    setTooltip(
        "Hold to perform. Shift-click to latch; click again to release. Host "
        "automation records this gesture.");
  }
  ~Gate() override {
    if (pressed_) release();
  }
  void mouseDown(const juce::MouseEvent& e) override {
    if (division_ >= 0)
      p_.set("repeat_division", static_cast<float>(division_));
    juce::TextButton::mouseDown(e);
    auto* parameter = p_.state().getParameter(id_);
    if (e.mods.isShiftDown()) {
      p_.set(id_, parameter->getValue() >= .5F ? 0.F : 1.F);
      return;
    }
    pressed_ = true;
    parameter->beginChangeGesture();
    parameter->setValueNotifyingHost(1);
  }
  void mouseUp(const juce::MouseEvent& e) override {
    juce::TextButton::mouseUp(e);
    if (pressed_) release();
  }
  bool keyPressed(const juce::KeyPress& key) override {
    if (key.getKeyCode() == juce::KeyPress::spaceKey) {
      if (division_ >= 0)
        p_.set("repeat_division", static_cast<float>(division_));
      p_.set(id_, parameter(p_, id_) >= .5F ? 0 : 1);
      return true;
    }
    return juce::TextButton::keyPressed(key);
  }
  void refresh() {
    const std::array<juce::String, 6> ids{"repeat", "nested", "void",
                                          "crush",  "shift",  "hold"};
    unsigned bit = 0;
    for (std::size_t i = 0; i < ids.size(); ++i)
      if (id_ == ids[i]) bit = 1U << i;
    setToggleState(
        (parameter(p_, id_) >= .5F ||
         (static_cast<unsigned>(p_.meter(19)) & bit) != 0) &&
            (division_ < 0 ||
             static_cast<int>(parameter(p_, "repeat_division")) == division_),
        juce::dontSendNotification);
  }

 private:
  void release() {
    auto* parameter = p_.state().getParameter(id_);
    parameter->setValueNotifyingHost(0);
    parameter->endChangeGesture();
    pressed_ = false;
  }
  Processor& p_;
  juce::String id_;
  int division_{-1};
  bool pressed_{};
};
class Grid final : public juce::Component {
 public:
  explicit Grid(Processor& processor) : p_{processor} {
    setWantsKeyboardFocus(true);
    setTitle("Four sound object event grid");
    setDescription(
        "Arrow keys select an event. Space toggles it. Delete removes. Shift "
        "arrows change roll length. Click empty to add; click event to select; "
        "Alt click removes. Drag body to move; drag right edge to roll.");
  }
  std::function<void(int, int)> selected;
  int object{}, event{};
  void paint(juce::Graphics& g) override {
    auto c = p_.currentControls();
    const float width = getWidth() / 32.F, height = getHeight() / 4.F;
    for (int n = 0; n < 4; ++n)
      for (int s = 0; s < 32; ++s) {
        auto bounds =
            juce::Rectangle<float>{s * width, n * height, width, height}
                .reduced(1.5F, 4);
        const auto& e = c.object[n].step[s];
        g.setColour(
            juce::Colour{s % 4 == 0 ? 0xff263034 : 0xff1b2326}.withAlpha(
                s < c.object[n].length ? 1.F : .35F));
        g.fillRoundedRectangle(bounds, 2);
        if (e.on) {
          const float alpha = s < c.object[n].length ? 1.F : .35F;
          g.setColour(juce::Colour{accent}.withAlpha(alpha));
          auto hit = bounds.reduced(3, 10);
          hit.setHeight(hit.getHeight() * (.3F + .7F * e.velocity));
          hit.setY(bounds.getBottom() - 10 - hit.getHeight());
          g.fillRoundedRectangle(hit, 2);
          if (e.count > 1) {
            g.setColour(juce::Colour{background});
            g.setFont(juce::FontOptions{10.F, juce::Font::bold});
            g.drawText(juce::String(e.count), bounds.toNearestInt(),
                       juce::Justification::centred);
          }
          if (e.lock) {
            g.setColour(juce::Colour{ink});
            g.fillRect(bounds.getX() + 4, bounds.getY() + 4, 4.F, 4.F);
          }
        }
        if (s == event && n == object) {
          g.setColour(juce::Colour{ink});
          g.drawRoundedRectangle(bounds, 2, 1.5F);
        }
        if (s == static_cast<int>(p_.meter(n))) {
          g.setColour(juce::Colour{ink}.withAlpha(.6F));
          g.fillRect(bounds.getX(), n * height + 2, width - 3, 2.F);
        }
      }
    const auto& roll = c.object[object].step[event];
    if (roll.on && roll.count > 1) {
      const float y = (object + 1) * height - 6;
      g.setColour(juce::Colour{ink});
      const float span = roll.span * width;
      for (int hit = 0; hit < roll.count; ++hit) {
        const float t = static_cast<float>(hit) / roll.count;
        const float x =
            event * width + std::pow(t, std::exp2(-2.F * roll.curve)) * span;
        g.drawVerticalLine(static_cast<int>(x), y - 9, y);
      }
      g.drawHorizontalLine(
          static_cast<int>(y), event * width,
          std::min(static_cast<float>(getWidth()), event * width + span));
    }
  }
  void mouseDown(const juce::MouseEvent& e) override {
    p_.beginEdit("Edit event / roll");
    grabKeyboardFocus();
    const int n =
        juce::jlimit(0, 3, static_cast<int>(e.position.y * 4 / getHeight()));
    const int s =
        juce::jlimit(0, 31, static_cast<int>(e.position.x * 32 / getWidth()));
    removeOnClick_ = object == n && event == s &&
                     parameter(p_, Processor::eventId(n, s, "on")) >= .5F;
    object = n;
    event = s;
    origin_ = s;
    drag_ = false;
    edge_ = std::fmod(e.position.x, getWidth() / 32.F) > getWidth() / 32.F - 6;
    const auto id = Processor::eventId(n, s, "on");
    if (e.mods.isAltDown() || e.mods.isRightButtonDown())
      p_.set(id, 0);
    else if (parameter(p_, id) < .5F)
      p_.set(id, 1);
    if (selected) selected(n, s);
    repaint();
  }
  void mouseDoubleClick(const juce::MouseEvent&) override {
    p_.set(Processor::eventId(object, event, "on"), 0);
    repaint();
  }
  void mouseDrag(const juce::MouseEvent& e) override {
    if (e.getDistanceFromDragStart() < 3) return;
    drag_ = true;
    if (edge_) {
      const float span = juce::jlimit(
          .125F, 8.F, 1 + e.getDistanceFromDragStartX() / (getWidth() / 32.F));
      p_.set(Processor::eventId(object, origin_, "count"),
             static_cast<float>(
                 juce::jlimit(2, 16, static_cast<int>(std::ceil(span * 4)))));
      p_.set(Processor::eventId(object, origin_, "span"), span);
      p_.set(Processor::eventId(object, origin_, "curve"),
             juce::jlimit(-1.F, 1.F, -e.getDistanceFromDragStartY() / 70.F));
    } else
      event =
          juce::jlimit(0, 31, static_cast<int>(e.position.x * 32 / getWidth()));
    repaint();
  }
  void mouseUp(const juce::MouseEvent&) override {
    if (drag_ && !edge_) {
      p_.moveEvent(object, origin_, event);
      if (selected) selected(object, event);
    }
    if (!drag_ && !edge_ && removeOnClick_)
      p_.set(Processor::eventId(object, event, "on"), 0);
    repaint();
  }
  bool keyPressed(const juce::KeyPress& key) override {
    if (key.getModifiers().isCommandDown() || key.getModifiers().isCtrlDown() ||
        key.getModifiers().isAltDown())
      return false;
    auto code = key.getKeyCode();
    if (code == juce::KeyPress::spaceKey) {
      auto id = Processor::eventId(object, event, "on");
      p_.set(id, parameter(p_, id) < .5F ? 1 : 0);
    } else if (code == juce::KeyPress::deleteKey ||
               code == juce::KeyPress::backspaceKey)
      p_.set(Processor::eventId(object, event, "on"), 0);
    else if (code == juce::KeyPress::leftKey ||
             code == juce::KeyPress::rightKey) {
      int direction = code == juce::KeyPress::leftKey ? -1 : 1;
      if (key.getModifiers().isShiftDown()) {
        auto id = Processor::eventId(object, event, "count");
        p_.set(id, juce::jlimit(1.F, 16.F, parameter(p_, id) + direction));
      } else
        event = juce::jlimit(0, 31, event + direction);
    } else if (code == juce::KeyPress::upKey)
      object = std::max(0, object - 1);
    else if (code == juce::KeyPress::downKey)
      object = std::min(3, object + 1);
    else
      return false;
    if (selected) selected(object, event);
    repaint();
    return true;
  }

 private:
  Processor& p_;
  int origin_{};
  bool drag_{}, edge_{}, removeOnClick_{};
};
class Timeline final : public juce::Component {
 public:
  explicit Timeline(Processor& p) : p_{p} {
    setTitle("Master fragment selection");
    setDescription(
        "Drag across steps to select a master audio fragment, then hold Master "
        "Repeat after playing the phrase.");
  }
  void paint(juce::Graphics& g) override {
    const int length = static_cast<int>(parameter(p_, "master_length")),
              a = static_cast<int>(parameter(p_, "region_start")),
              b = static_cast<int>(parameter(p_, "region_end"));
    const float width = getWidth() / 32.F;
    for (int s = 0; s < 32; ++s) {
      g.setColour(juce::Colour{s >= a && s < b ? accent : 0xff263034}.withAlpha(
          s < length ? 1.F : .25F));
      g.fillRect(s * width + 1, 22.F, width - 2, getHeight() - 24.F);
      g.setColour(juce::Colour{ink});
      g.setFont(juce::FontOptions{10.F});
      g.drawText(juce::String(s + 1), static_cast<int>(s * width), 1,
                 static_cast<int>(width), 18, juce::Justification::centred);
    }
    g.setColour(juce::Colour{ink});
    g.fillRect(p_.meter(8) * width, 18.F, 2.F, getHeight() - 18.F);
  }
  void mouseDown(const juce::MouseEvent& e) override {
    anchor_ =
        juce::jlimit(0, static_cast<int>(parameter(p_, "master_length")) - 1,
                     static_cast<int>(e.position.x * 32 / getWidth()));
    update(anchor_);
  }
  void mouseDrag(const juce::MouseEvent& e) override {
    update(juce::jlimit(0, static_cast<int>(parameter(p_, "master_length")) - 1,
                        static_cast<int>(e.position.x * 32 / getWidth())));
  }

 private:
  void update(int end) {
    p_.set("region_start", static_cast<float>(std::min(anchor_, end)));
    p_.set("region_end", static_cast<float>(std::max(anchor_, end) + 1));
    repaint();
  }
  Processor& p_;
  int anchor_{};
};
class Wave final : public juce::Component, public juce::FileDragAndDropTarget {
 public:
  Wave(Processor& p, int& object) : p_{p}, object_{object} {
    setTitle("Sample trim and loop editor");
    setDescription(
        "Top handles edit sample start and end; bottom handles edit loop start "
        "and end. Drop audio to load.");
  }
  bool isInterestedInFileDrag(const juce::StringArray&) override {
    return true;
  }
  void filesDropped(const juce::StringArray& files, int, int) override {
    if (!files.isEmpty()) p_.loadSample(object_, files[0]);
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{0xff1b2326});
    const auto wave = p_.waveform(object_);
    const float h = getHeight() * .3F, y = getHeight() * .5F,
                w = getWidth() / 256.F;
    g.setColour(juce::Colour{accent}.withAlpha(.75F));
    for (int i = 0; i < 256; ++i) {
      float magnitude = std::min(1.F, wave[static_cast<std::size_t>(i)]);
      g.fillRect(i * w, y - magnitude * h, std::max(1.F, w), 2 * magnitude * h);
    }
    const std::array<const char*, 4> ids{"start", "end", "loop_start",
                                         "loop_end"};
    const std::array<const char*, 4> labels{"START", "END", "LOOP START",
                                            "LOOP END"};
    for (int i = 0; i < 4; ++i) {
      const float x =
          parameter(p_, Processor::objectId(object_,
                                            ids[static_cast<std::size_t>(i)])) *
          getWidth();
      g.setColour(juce::Colour{i < 2 ? ink : accent});
      g.drawVerticalLine(
          static_cast<int>(x), i < 2 ? 0.F : getHeight() * .5F,
          i < 2 ? getHeight() * .5F : static_cast<float>(getHeight()));
      g.drawText(labels[static_cast<std::size_t>(i)],
                 juce::jlimit(2, getWidth() - 86, static_cast<int>(x) - 40),
                 i < 2 ? 3 : getHeight() - 23, 84, 20,
                 juce::Justification::centred);
    }
  }
  void mouseDown(const juce::MouseEvent& e) override {
    const bool bottom = e.position.y > getHeight() * .5F;
    const auto a = Processor::objectId(object_,
                                       bottom ? "loop_start" : "start"),
               b = Processor::objectId(object_, bottom ? "loop_end" : "end");
    const float x = e.position.x / getWidth();
    dragId_ =
        std::abs(x - parameter(p_, a)) < std::abs(x - parameter(p_, b)) ? a : b;
    p_.state().getParameter(dragId_)->beginChangeGesture();
    mouseDrag(e);
  }
  void mouseDrag(const juce::MouseEvent& e) override {
    if (dragId_.isEmpty()) return;
    auto* v = p_.state().getParameter(dragId_);
    float value = juce::jlimit(0.F, 1.F, e.position.x / getWidth());
    if (dragId_.endsWith("loop_start"))
      value = std::min(
          value,
          parameter(p_, Processor::objectId(object_, "loop_end")) - .000001F);
    else if (dragId_.endsWith("loop_end"))
      value = std::max(
          value,
          parameter(p_, Processor::objectId(object_, "loop_start")) + .000001F);
    else if (dragId_.endsWith("start"))
      value = std::min(
          value, parameter(p_, Processor::objectId(object_, "end")) - .000001F);
    else
      value =
          std::max(value, parameter(p_, Processor::objectId(object_, "start")) +
                              .000001F);
    v->setValueNotifyingHost(v->convertTo0to1(value));
    repaint();
  }
  void mouseUp(const juce::MouseEvent&) override {
    if (dragId_.isNotEmpty())
      p_.state().getParameter(dragId_)->endChangeGesture();
    dragId_.clear();
  }

 private:
  Processor& p_;
  int& object_;
  juce::String dragId_;
};
class Fields final : public juce::Component {
 public:
  explicit Fields(Processor& p) : p_{p} {}
  void bind(const std::vector<juce::String>& ids, int columns = 6,
            int height = 80) {
    fields_.clear();
    ids_ = ids;
    columns_ = columns;
    rowHeight_ = height;
    int order = 1;
    for (const auto& id : ids) {
      auto item = control(p_, id);
      item->setExplicitFocusOrder(order++);
      addAndMakeVisible(*item);
      fields_.push_back(std::move(item));
    }
    resized();
  }
  void resized() override {
    int i = 0;
    for (auto& item : fields_) {
      item->setBounds((i % columns_) * getWidth() / columns_,
                      (i / columns_) * rowHeight_, getWidth() / columns_,
                      rowHeight_);
      ++i;
    }
  }
  void sourceMode(int source) {
    for (std::size_t i = 0; i < fields_.size(); ++i) {
      const auto key = ids_[i].fromFirstOccurrenceOf("_", false, false);
      const bool synthesis =
          key == "pitch" || key == "excite" || key == "noise";
      const bool sample = key == "start" || key == "end" ||
                          key == "loop_start" || key == "loop_end" ||
                          key == "loop" || key == "reverse" ||
                          key == "loop_mod" || key == "interpolation";
      fields_[i]->setEnabled(!(synthesis && source == 1) &&
                             !(sample && source == 0));
      if (sample)
        for (auto* child : fields_[i]->getChildren())
          if (auto* slider = dynamic_cast<juce::Slider*>(child))
            slider->updateText();
    }
  }
  int neededHeight() const {
    return (static_cast<int>(fields_.size()) + columns_ - 1) / columns_ *
           rowHeight_;
  }

 private:
  Processor& p_;
  std::vector<std::unique_ptr<Control>> fields_;
  std::vector<juce::String> ids_;
  int columns_{6}, rowHeight_{80};
};
class CaptureWave final : public juce::Component {
 public:
  explicit CaptureWave(Processor& p) : p_{p} {
    setTitle("Captured master waveform");
    setWantsKeyboardFocus(true);
    setDescription(
        "Drag across the waveform to select a loop. Drag the upper trim "
        "handles to change start and end. Arrow keys move loop start; Shift "
        "arrows move loop end.");
  }
  void refresh() {
    if (count_ != p_.memoryCount() || selected_ != p_.memorySelection() ||
        epoch_ != p_.memoryEpoch()) {
      count_ = p_.memoryCount();
      selected_ = p_.memorySelection();
      wave_ = p_.memoryWave(selected_);
      epoch_ = p_.memoryEpoch();
    }
    repaint();
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{0xff1b2326});
    if (count_ == 0) {
      g.setColour(juce::Colour{ink});
      g.drawText("PLAY A PHRASE  /  CAPTURE  /  HOLD A DIVISION  /  RESAMPLE",
                 getLocalBounds(), juce::Justification::centred);
      return;
    }
    const auto c = p_.memoryControls().generation[selected_];
    g.setColour(juce::Colour{accent}.withAlpha(.18F));
    g.fillRect(c.loopStart * getWidth(), 0.F,
               (c.loopEnd - c.loopStart) * getWidth(),
               static_cast<float>(getHeight()));
    g.setColour(juce::Colour{accent});
    const float peak = *std::max_element(wave_.begin(), wave_.end());
    const float displayScale = .9F / std::max(.000001F, peak);
    for (int n = 0; n < 128; ++n) {
      const float x = n * getWidth() / 128.F,
                  height = std::min(1.F, wave_[n] * displayScale) *
                           (getHeight() - 10) * .5F;
      g.drawVerticalLine(static_cast<int>(x), getHeight() * .5F - height,
                         getHeight() * .5F + height);
    }
    for (float x : {c.start, c.end}) {
      g.fillRect(x * (getWidth() - 6), 0.F, 6.F, 9.F);
    }
    g.setColour(juce::Colour{ink});
    for (float x : {c.loopStart, c.loopEnd})
      g.fillRect(x * (getWidth() - 3), getHeight() - 10.F, 3.F, 10.F);
    g.drawVerticalLine(static_cast<int>(p_.memoryPosition() * getWidth()), 0,
                       static_cast<float>(getHeight()));
    g.setFont(juce::FontOptions{10.F});
    g.drawText(
        "AUTO SCALE / PEAK " +
            juce::String(juce::Decibels::gainToDecibels(peak, -100.F), 1) +
            " dBFS",
        getLocalBounds().reduced(8, 2).removeFromTop(14),
        juce::Justification::right);
  }
  void mouseDown(const juce::MouseEvent& e) override {
    grabKeyboardFocus();
    p_.beginEdit("Edit captured fragment");
    origin_ = juce::jlimit(0.F, 1.F, e.position.x / getWidth());
    const auto c = p_.memoryControls().generation[selected_];
    trim_ = e.position.y < 12;
    end_ = std::abs(origin_ - c.end) < std::abs(origin_ - c.start);
    mouseDrag(e);
  }
  void mouseDrag(const juce::MouseEvent& e) override {
    if (count_ == 0) return;
    const float at = juce::jlimit(0.F, 1.F, e.position.x / getWidth());
    const auto c = p_.memoryControls().generation[selected_];
    if (trim_)
      p_.set(Processor::memoryId(selected_, end_ ? "end" : "start"),
             end_ ? std::max(c.start + .000001F, at)
                  : std::min(c.end - .000001F, at));
    else {
      p_.set(Processor::memoryId(selected_, "loop_start"),
             std::min(origin_, std::max(0.F, at - .000001F)));
      p_.set(Processor::memoryId(selected_, "loop_end"),
             std::max(origin_ + .000001F, at));
    }
    repaint();
  }
  bool keyPressed(const juce::KeyPress& k) override {
    if (k.getKeyCode() != juce::KeyPress::leftKey &&
        k.getKeyCode() != juce::KeyPress::rightKey)
      return false;
    p_.beginEdit("Nudge captured region");
    const auto id = Processor::memoryId(
        selected_, k.getModifiers().isShiftDown() ? "loop_end" : "loop_start");
    p_.set(id,
           parameter(p_, id) +
               (k.getKeyCode() == juce::KeyPress::leftKey ? -.001F : .001F));
    return true;
  }

 private:
  Processor& p_;
  std::array<float, 128> wave_{};
  int count_{}, selected_{};
  unsigned epoch_{};
  float origin_{};
  bool trim_{}, end_{};
};
class CaptureDock final : public juce::Component {
 public:
  explicit CaptureDock(Processor& p) : p_{p}, wave_{p}, fields_{p} {
    addAndMakeVisible(wave_);
    addAndMakeVisible(fields_);
    addAndMakeVisible(status_);
    const std::array<const char*, 8> names{
        "CAPTURE", "RESAMPLE", "ORIGINAL", "PREVIOUS",
        "NEXT",    "UNDO",     "REDO",     "PLAY CAPTURE"};
    for (int n = 0; n < 8; ++n) {
      buttons_[n].setButtonText(names[n]);
      addAndMakeVisible(buttons_[n]);
    }
    buttons_[0].onClick = [this] { p_.memoryAction(MasterMemory::Capture); };
    buttons_[1].onClick = [this] { p_.memoryAction(MasterMemory::Resample); };
    buttons_[2].onClick = [this] { p_.memoryAction(MasterMemory::Original); };
    buttons_[3].onClick = [this] { p_.memoryAction(MasterMemory::Previous); };
    buttons_[4].onClick = [this] { p_.memoryAction(MasterMemory::Next); };
    buttons_[5].onClick = [this] { p_.undoEdit(); };
    buttons_[6].onClick = [this] { p_.redoEdit(); };
    buttons_[7].setClickingTogglesState(true);
    play_ =
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
            p.state(), "memory_play", buttons_[7]);
    buttons_[0].setTooltip(
        "Freeze the selected master phrase from recent rendered audio, after "
        "compression, before capture processing and Master output.");
    buttons_[1].setTooltip(
        "Print one processed cycle into a new PCM generation. The new "
        "generation starts with neutral controls. Original audio stays "
        "available.");
    const std::array<const char*, 6> names2{"1/4",  "1/8",  "1/16",
                                            "1/32", "1/64", "FREE"};
    for (int n = 0; n < 6; ++n) {
      divisions_[n] = std::make_unique<Gate>(p, "memory_repeat", names2[n], n);
      addAndMakeVisible(*divisions_[n]);
    }
    bind(0);
  }
  void refresh() {
    p_.syncMemorySelection();
    const int generation = p_.memorySelection();
    if (generation != generation_) bind(generation);
    wave_.refresh();
    for (auto& button : divisions_) button->refresh();
    status_.setText(
        p_.memoryStatus() == 3
            ? "PRINTING " + juce::String(p_.memoryProgress() * 100, 0) + "%"
        : p_.memoryStatus() == 4 ? "PRINT > 8s / SHORTEN OR SPEED UP"
        : p_.memoryStatus() == 2 ? "HISTORY FULL / 8"
        : p_.memoryStatus() == 1
            ? "PLAY BEFORE CAPTURE"
            : "GEN " + juce::String(p_.memoryCount() ? generation + 1 : 0) +
                  " / " + juce::String(p_.memoryCount()) +
                  "   /   HOLD TO REPEAT",
        juce::dontSendNotification);
  }
  void resized() override {
    const int w = getWidth();
    for (int n = 0; n < 8; ++n)
      buttons_[n].setBounds(n * w / 8, 0, w / 8 - 5, 26);
    wave_.setBounds(0, 32, w - 350, 58);
    status_.setBounds(w - 340, 30, 340, 24);
    for (int n = 0; n < 6; ++n)
      divisions_[n]->setBounds(w - 340 + n * 57, 57, 52, 32);
    fields_.setBounds(0, 95, w, 112);
  }

 private:
  void bind(int generation) {
    generation_ = generation;
    std::vector<juce::String> ids;
    for (const char* id : {"pitch", "speed", "cutoff", "resonance", "morph",
                           "playback", "reverse"})
      ids.push_back(Processor::memoryId(generation, id));
    for (const char* id : {"skew", "pressure", "recovery", "dirt", "parallel",
                           "time_mode", "repeat_free"})
      ids.emplace_back(id);
    fields_.bind(ids, 7, 55);
  }
  Processor& p_;
  CaptureWave wave_;
  Fields fields_;
  juce::Label status_;
  std::array<juce::TextButton, 8> buttons_;
  std::array<std::unique_ptr<Gate>, 6> divisions_;
  std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> play_;
  int generation_{};
};
class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit Editor(Processor& p)
      : AudioProcessorEditor{p},
        p_{p},
        master_{p.state()},
        grid_{p},
        timeline_{p},
        wave_{p, object_},
        eventFields_{p},
        soundFields_{p},
        performanceFields_{p},
        capture_{p} {
    look_.setColour(juce::Slider::thumbColourId, juce::Colour{accent});
    look_.setColour(juce::Slider::trackColourId, juce::Colour{accent});
    look_.setColour(juce::TextButton::buttonOnColourId,
                    juce::Colour{0xff665a3c});
    setLookAndFeel(&look_);
    for (int i = 0; i < 3; ++i) {
      view_[i].setButtonText(std::array{
          "SEQUENCE", "SOUND", "PERFORM"}[static_cast<std::size_t>(i)]);
      view_[i].onClick = [this, i] { setView(i); };
      addAndMakeVisible(view_[i]);
    }
    addAndMakeVisible(master_);
    addAndMakeVisible(capture_);
    preset_.addItemList({"First impacts", "Microscopic", "Sub pressure",
                         "Broken lattice", "Fragment material", "Sample ready"},
                        1);
    preset_.setTitle("Factory starting points");
    preset_.onChange = [this] {
      if (preset_.getSelectedId() > 0) {
        p_.scene(preset_.getSelectedId() - 1);
        lastPreset_ = preset_.getText();
      }
    };
    addAndMakeVisible(preset_);
    save_.setButtonText("SAVE");
    save_.onClick = [this] { choosePreset(true); };
    load_.setButtonText("OPEN");
    load_.onClick = [this] { choosePreset(false); };
    addAndMakeVisible(save_);
    addAndMakeVisible(load_);
    for (const char* id : {"mode", "master_length", "swing"}) {
      auto c = control(p_, id);
      addAndMakeVisible(*c);
      header_.push_back(std::move(c));
    }
    addAndMakeVisible(grid_);
    addAndMakeVisible(timeline_);
    addAndMakeVisible(eventFields_);
    addAndMakeVisible(selected_);
    addAndMakeVisible(cue_);
    grid_.selected = [this](int n, int step) {
      object_ = n;
      event_ = step;
      eventInspector();
    };
    for (int n = 0; n < 4; ++n) {
      auto& row = row_[n];
      row.name.setButtonText(juce::String(n + 1) + " / " +
                             std::array{"PRESSURE", "HOLLOW", "PARTICLE",
                                        "TICK"}[static_cast<std::size_t>(n)]);
      row.name.onClick = [this, n] {
        object_ = n;
        soundInspector();
        setView(1);
      };
      row.audition.setButtonText("PLAY");
      row.audition.onClick = [this, n] { p_.audition(n); };
      row.mute.setButtonText("M");
      row.solo.setButtonText("S");
      row.mute.setClickingTogglesState(true);
      row.solo.setClickingTogglesState(true);
      row.mute.setTitle("Mute object " + juce::String(n + 1));
      row.solo.setTitle("Solo object " + juce::String(n + 1));
      row.muteAttachment = std::make_unique<
          juce::AudioProcessorValueTreeState::ButtonAttachment>(
          p.state(), Processor::objectId(n, "mute"), row.mute);
      row.soloAttachment = std::make_unique<
          juce::AudioProcessorValueTreeState::ButtonAttachment>(
          p.state(), Processor::objectId(n, "solo"), row.solo);
      row.length.setSliderStyle(juce::Slider::IncDecButtons);
      row.length.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 30, 20);
      row.length.setTitle("Object cycle length in steps");
      row.length.setTooltip(
          "Independent cycle length in steps. Type or use +/-.");
      row.span.setSliderStyle(juce::Slider::IncDecButtons);
      row.span.setTextBoxStyle(juce::Slider::TextBoxLeft, false, 25, 20);
      row.span.setTitle("Voice repeat length in steps");
      row.span.setTooltip(
          "Length of the local phrase repeated by VOICE. Release returns to "
          "the running cycle.");
      row.repeat =
          std::make_unique<Gate>(p_, Processor::objectId(n, "repeat"), "VOICE");
      for (const auto& spec : p_.specs())
        if (spec.id == Processor::objectId(n, "division"))
          row.division.addItemList(
              juce::StringArray::fromTokens(spec.choices, "|", ""), 1);
      row.division.setTitle("Object step division");
      row.lengthAttachment = std::make_unique<
          juce::AudioProcessorValueTreeState::SliderAttachment>(
          p.state(), Processor::objectId(n, "length"), row.length);
      row.spanAttachment = std::make_unique<
          juce::AudioProcessorValueTreeState::SliderAttachment>(
          p.state(), Processor::objectId(n, "repeat_steps"), row.span);
      row.divisionAttachment = std::make_unique<
          juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
          p.state(), Processor::objectId(n, "division"), row.division);
      for (auto* c : std::initializer_list<juce::Component*>{
               &row.name, &row.audition,
               static_cast<juce::Component*>(&row.mute),
               static_cast<juce::Component*>(&row.solo), &row.length,
               &row.division, row.repeat.get(), &row.span})
        addAndMakeVisible(c);
    }
    more_.setButtonText("MORE EVENT");
    more_.setClickingTogglesState(true);
    more_.onClick = [this] { eventInspector(); };
    addAndMakeVisible(more_);
    addAndMakeVisible(soundSelect_);
    soundSelect_.addItemList(
        {"01 / Pressure", "02 / Hollow", "03 / Particle", "04 / Tick"}, 1);
    soundSelect_.setSelectedId(1, juce::dontSendNotification);
    soundSelect_.onChange = [this] {
      object_ = soundSelect_.getSelectedId() - 1;
      soundInspector();
    };
    replace_.addItemList(
        {"Sub impact", "Hollow pulse", "Electrical tick", "High particle",
         "Metal shell", "Sharp crack", "Filtered noise", "Hybrid fragment"},
        1);
    replace_.addSeparator();
    replace_.addItem("SAVE SOUND...", 100);
    replace_.addItem("OPEN SOUND...", 101);
    replace_.setTextWhenNothingSelected("REPLACE / SAVE SOUND");
    replace_.onChange = [this] {
      const int id = replace_.getSelectedId();
      if (id == 100 || id == 101)
        chooseSound(id == 100);
      else if (id > 0)
        p_.replaceSound(object_, id - 1);
      replace_.setSelectedId(0, juce::dontSendNotification);
    };
    addAndMakeVisible(replace_);
    sampleLoad_.setButtonText("LOAD SAMPLE / RELINK");
    sampleLoad_.onClick = [this] { chooseSample(); };
    addAndMakeVisible(sampleLoad_);
    duplicate_.setButtonText("COPY TO NEXT OBJECT");
    duplicate_.onClick = [this] {
      p_.duplicateSound(object_, (object_ + 1) % 4);
    };
    addAndMakeVisible(duplicate_);
    advanced_.setButtonText("ADVANCED SOUND");
    advanced_.setClickingTogglesState(true);
    advanced_.onClick = [this] { soundInspector(); };
    addAndMakeVisible(advanced_);
    addAndMakeVisible(sampleLabel_);
    addAndMakeVisible(wave_);
    soundViewport_.setViewedComponent(&soundFields_, false);
    soundViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(soundViewport_);
    eventViewport_.setViewedComponent(&eventFields_, false);
    eventViewport_.setScrollBarsShown(true, false);
    addAndMakeVisible(eventViewport_);
    performanceFields_.bind(
        {"pressure", "recovery", "dirt", "parallel", "time_mode", "reverse",
         "shift_steps", "inner_span", "fill", "roll_span", "seed",
         "compressor_mode"},
        6, 60);
    addAndMakeVisible(performanceFields_);
    const std::array<const char*, 6> ids{"repeat", "nested", "void",
                                         "crush",  "shift",  "hold"};
    const std::array<const char*, 6> titles{"MASTER REPEAT", "INNER REPEAT",
                                            "VOID",          "CRUSH",
                                            "SHIFT",         "HOLD / RELEASE"};
    for (int i = 0; i < 6; ++i) {
      gates_[i] = std::make_unique<Gate>(p_, ids[static_cast<std::size_t>(i)],
                                         titles[static_cast<std::size_t>(i)]);
      largeGates_[i] =
          std::make_unique<Gate>(p_, ids[static_cast<std::size_t>(i)],
                                 titles[static_cast<std::size_t>(i)]);
      addAndMakeVisible(*gates_[i]);
      addAndMakeVisible(*largeGates_[i]);
    }
    store_.setButtonText("STORE SCENE");
    store_.setClickingTogglesState(true);
    addAndMakeVisible(store_);
    panic_.setButtonText("ALL SOUND OFF");
    panic_.onClick = [this] {
      p_.panic();
      for (const char* id : {"repeat", "nested", "hold", "void", "crush",
                             "shift", "memory_play", "memory_repeat"})
        p_.set(id, 0);
    };
    addAndMakeVisible(panic_);
    addAndMakeVisible(status_);
    store_.setClickingTogglesState(true);
    for (int i = 0; i < 4; ++i) {
      scenes_[i].setTitle("Scene " + juce::String(i + 1));
      scenes_[i].onClick = [this, i] {
        if (store_.getToggleState()) {
          p_.storeScene(i);
          store_.setToggleState(false, juce::dontSendNotification);
        } else
          p_.recallScene(i);
      };
      addAndMakeVisible(scenes_[i]);
    }
    eventInspector();
    soundInspector();
    setResizable(true, true);
    setResizeLimits(1100, 840, 1800, 1200);
    setSize(1360, 900);
    setView(0);
    startTimerHz(25);
    timerCallback();
  }
  ~Editor() override {
    stopTimer();
    soundViewport_.setViewedComponent(nullptr, false);
    eventViewport_.setViewedComponent(nullptr, false);
    setLookAndFeel(nullptr);
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{background});
    g.setColour(juce::Colour{ink});
    g.setFont(juce::FontOptions{24.F, juce::Font::bold});
    g.drawText("ASTE / K-01", 24, 18, 240, 34,
               juce::Justification::centredLeft);
    g.setFont(juce::FontOptions{11.F});
    g.drawText("CLICK  /  IMPACTS / FRAGMENTS / SILENCE", 26, 52, 300, 22,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour{accent});
    g.fillRect(24, 132, getWidth() - 48, 1);
  }
  void resized() override {
    if (header_.size() != 3) return;
    const int w = getWidth(), h = getHeight();
    for (int i = 0; i < 3; ++i) view_[i].setBounds(330 + i * 110, 25, 105, 34);
    master_.setBounds(w - 275, 20, 250, 54);
    preset_.setBounds(24, 86, 210, 30);
    save_.setBounds(244, 86, 60, 30);
    load_.setBounds(310, 86, 60, 30);
    header_[0]->setBounds(400, 73, 210, 55);
    header_[1]->setBounds(625, 73, 175, 55);
    header_[2]->setBounds(810, 73, 175, 55);
    capture_.setBounds(24, h - 308, w - 48, 208);
    const int timelineY = 162, laneTop = 208, laneBottom = h - 430,
              laneHeight = (laneBottom - laneTop) / 4;
    cue_.setBounds(24, 138, w - 48, 24);
    timeline_.setBounds(282, timelineY, w - 310, 46);
    grid_.setBounds(282, laneTop, w - 310, laneHeight * 4);
    for (int n = 0; n < 4; ++n) {
      auto& row = row_[n];
      int y = laneTop + n * laneHeight;
      row.name.setBounds(24, y + 4, 152, 25);
      row.audition.setBounds(181, y + 4, 48, 25);
      row.mute.setBounds(232, y + 4, 23, 25);
      row.solo.setBounds(256, y + 4, 23, 25);
      row.length.setBounds(24, y + 31, 63, 20);
      row.division.setBounds(91, y + 31, 69, 20);
      row.repeat->setBounds(164, y + 31, 56, 20);
      row.span.setBounds(224, y + 31, 55, 20);
    }
    selected_.setBounds(24, laneBottom + 10, w - 220, 24);
    more_.setBounds(w - 180, laneBottom + 8, 154, 28);
    eventViewport_.setBounds(24, laneBottom + 36, w - 48, 80);
    eventFields_.setSize(w - 64, eventFields_.neededHeight());
    soundSelect_.setBounds(24, 145, 190, 32);
    replace_.setBounds(226, 145, 200, 32);
    sampleLoad_.setBounds(438, 145, 195, 32);
    duplicate_.setBounds(645, 145, 190, 32);
    advanced_.setBounds(w - 215, 145, 190, 32);
    sampleLabel_.setBounds(24, 184, w - 48, 24);
    wave_.setBounds(24, 214, w - 48, 80);
    soundViewport_.setBounds(24, 306, w - 48, h - 624);
    soundFields_.setSize(w - 64, soundFields_.neededHeight());
    for (int i = 0; i < 6; ++i)
      largeGates_[i]->setBounds(24 + (i % 3) * (w - 48) / 3, 166 + (i / 3) * 60,
                                (w - 72) / 3, 50);
    performanceFields_.setBounds(24, 292, w - 48,
                                 performanceFields_.neededHeight());
    store_.setBounds(24, h - 355, 160, 32);
    for (int i = 0; i < 4; ++i)
      scenes_[i].setBounds(200 + i * 150, h - 355, 138, 32);
    const int gateWidth = (w - 210) / 6;
    for (int i = 0; i < 6; ++i)
      gates_[i]->setBounds(24 + i * gateWidth, h - 62, gateWidth - 7, 34);
    panic_.setBounds(w - 175, h - 62, 150, 34);
    status_.setBounds(24, h - 94, w - 48, 26);
  }
  bool keyPressed(const juce::KeyPress& key) override {
    if ((key.getModifiers().isCommandDown() ||
         key.getModifiers().isCtrlDown()) &&
        juce::CharacterFunctions::toLowerCase(key.getTextCharacter()) == 'z') {
      if (key.getModifiers().isShiftDown())
        p_.redoEdit();
      else
        p_.undoEdit();
      return true;
    }
    return false;
  }

 private:
  void eventInspector() {
    std::vector<juce::String> ids;
    const std::vector<const char*> main{
        "on",       "velocity",    "count",        "span",
        "curve",    "probability", "end_velocity", "end_pitch",
        "mutation", "irregular",   "sound",        "lock",
        "pitch",    "cutoff",      "decay",        "condition"};
    if (more_.getToggleState())
      for (auto* id : Processor::eventIds)
        ids.push_back(Processor::eventId(object_, event_, id));
    else
      for (auto* id : main)
        ids.push_back(Processor::eventId(object_, event_, id));
    eventFields_.bind(ids, 6, 70);
    selected_.setText(
        "OBJECT " + juce::String(object_ + 1) + " / EVENT " +
            juce::String(event_ + 1) +
            "    Sound locks affect Pitch / Start / Filter / Decay",
        juce::dontSendNotification);
    resized();
  }
  void soundInspector() {
    soundSelect_.setSelectedId(object_ + 1, juce::dontSendNotification);
    std::vector<juce::String> ids;
    const std::vector<const char*> essential{
        "source",    "tune",  "speed",   "playback", "cutoff",
        "resonance", "morph", "start",   "end",      "loop_start",
        "loop_end",  "loop",  "reverse", "colour",   "pitch",
        "decay",     "body",  "excite",  "noise",    "character",
        "level",     "roll",  "pan",     "repeat",   "repeat_steps"};
    const std::vector<const char*> advanced{
        "filter_env",  "loop_mod", "interpolation",    "bits",
        "phase_reset", "midi",     "velocity_response"};
    for (auto* id : essential) ids.push_back(Processor::objectId(object_, id));
    if (advanced_.getToggleState())
      for (auto* id : advanced) ids.push_back(Processor::objectId(object_, id));
    soundFields_.bind(ids, 7, 65);
    resized();
    wave_.repaint();
  }
  void setView(int view) {
    currentView_ = view;
    cue_.setVisible(view != 1);
    for (int i = 0; i < 3; ++i)
      view_[i].setToggleState(view == i, juce::dontSendNotification);
    for (auto* c : std::initializer_list<juce::Component*>{
             static_cast<juce::Component*>(&grid_), &timeline_, &eventViewport_,
             &selected_, &more_})
      c->setVisible(view == 0);
    for (auto& row : row_)
      for (auto* c : std::initializer_list<juce::Component*>{
               static_cast<juce::Component*>(&row.name), &row.audition,
               &row.mute, &row.solo, &row.length, &row.division,
               row.repeat.get(), &row.span})
        c->setVisible(view == 0);
    for (auto* c : std::initializer_list<juce::Component*>{
             static_cast<juce::Component*>(&soundSelect_), &replace_,
             &sampleLoad_, &duplicate_, &advanced_, &sampleLabel_, &wave_,
             &soundViewport_})
      c->setVisible(view == 1);
    for (auto& gate : largeGates_) gate->setVisible(view == 2);
    performanceFields_.setVisible(view == 2);
    store_.setVisible(view == 2);
    for (auto& scene : scenes_) scene.setVisible(view == 2);
    cue_.setText(
        view == 0   ? "Click: add/select / double-click: remove / drag: move / "
                      "right edge: roll / drag master timeline: fragment"
        : view == 1 ? "Synthesis, sample or layer / drag top handles: trim / "
                      "bottom handles: loop / drop audio to load"
                    : "Hold a gesture to perform / Shift-click to latch / "
                      "CONTINUE runs underneath; HOLD suspends musical time",
        juce::dontSendNotification);
    resized();
  }
  void chooseSound(bool save) {
    chooser_ = std::make_unique<juce::FileChooser>(
        save ? "Save selected sound" : "Open sound", juce::File{},
        "*.k01sound");
    juce::Component::SafePointer<Editor> safe{this};
    const int object = object_;
    chooser_->launchAsync(
        (save ? juce::FileBrowserComponent::saveMode |
                    juce::FileBrowserComponent::warnAboutOverwriting
              : juce::FileBrowserComponent::openMode) |
            juce::FileBrowserComponent::canSelectFiles,
        [safe, object, save](const juce::FileChooser& chooser) {
          if (!safe || chooser.getResult() == juce::File{}) return;
          auto file = chooser.getResult();
          bool success = false;
          if (save) {
            auto xml = safe->p_.soundPreset(object).createXml();
            success = file.withFileExtension("k01sound")
                          .replaceWithText(xml->toString());
          } else if (file.getSize() < 256 * 1024)
            if (auto xml = juce::parseXML(file))
              success =
                  safe->p_.restoreSound(object, juce::ValueTree::fromXml(*xml));
          if (!success)
            juce::AlertWindow::showMessageBoxAsync(
                juce::MessageBoxIconType::WarningIcon, "Click K-01",
                "The sound preset could not be read or written.");
        });
  }
  void chooseSample() {
    chooser_ = std::make_unique<juce::FileChooser>(
        "Load an audio sample", juce::File{},
        "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");
    juce::Component::SafePointer<Editor> safe{this};
    const int object = object_;
    chooser_->launchAsync(
        juce::FileBrowserComponent::openMode |
            juce::FileBrowserComponent::canSelectFiles,
        [safe, object](const juce::FileChooser& chooser) {
          if (safe && chooser.getResult().existsAsFile())
            safe->p_.loadSample(object, chooser.getResult().getFullPathName());
        });
  }
  void choosePreset(bool save) {
    chooser_ = std::make_unique<juce::FileChooser>(
        save ? "Save Click preset" : "Open Click preset", juce::File{},
        "*.k01");
    juce::Component::SafePointer<Editor> safe{this};
    chooser_->launchAsync(
        (save ? juce::FileBrowserComponent::saveMode |
                    juce::FileBrowserComponent::warnAboutOverwriting
              : juce::FileBrowserComponent::openMode) |
            juce::FileBrowserComponent::canSelectFiles,
        [safe, save](const juce::FileChooser& chooser) {
          if (!safe || chooser.getResult() == juce::File{}) return;
          auto file = chooser.getResult();
          if (save) {
            file = file.withFileExtension("k01");
            juce::MemoryBlock bytes;
            safe->p_.getStateInformation(bytes);
            if (!file.replaceWithData(bytes.getData(), bytes.getSize()))
              safe->status_.setText("Preset could not be saved",
                                    juce::dontSendNotification);
          } else {
            juce::MemoryBlock bytes;
            if (file.getSize() > 2 * 1024 * 1024 ||
                !file.loadFileAsData(bytes)) {
              safe->status_.setText("Preset could not be read",
                                    juce::dontSendNotification);
              return;
            }
            safe->p_.setStateInformation(bytes.getData(),
                                         static_cast<int>(bytes.getSize()));
          }
        });
  }
  void timerCallback() override {
    capture_.refresh();
    const int match = p_.factoryIndex();
    if (match >= 0) {
      preset_.setSelectedId(match + 1, juce::dontSendNotification);
      lastPreset_ = preset_.getText();
    } else {
      preset_.setSelectedId(0, juce::dontSendNotification);
      preset_.setText(lastPreset_.isEmpty() ? "CUSTOM" : lastPreset_ + " *",
                      juce::dontSendNotification);
    }
    sampleLabel_.setText("SAMPLE / " + p_.sampleStatus(object_),
                         juce::dontSendNotification);
    sampleLabel_.setTooltip(p_.samplePath(object_));
    grid_.repaint();
    timeline_.repaint();
    if (currentView_ == 1) {
      wave_.repaint();
      soundFields_.sourceMode(static_cast<int>(
          parameter(p_, Processor::objectId(object_, "source"))));
    }
    for (auto& g : gates_) g->refresh();
    for (auto& g : largeGates_) g->refresh();
    for (auto& row : row_) row.repeat->refresh();
    for (int i = 0; i < 4; ++i)
      scenes_[i].setButtonText(
          "SCENE " +
          juce::String::charToString(static_cast<juce::juce_wchar>('A' + i)) +
          (p_.hasScene(i) ? "" : " / EMPTY"));
    const auto db = juce::Decibels::gainToDecibels(p_.outputPeak(), -100.F);
    status_.setText(
        "OUTPUT " + juce::String(db, 1) + " dBFS   /   PRESSURE REDUCTION " +
            juce::String(std::max(0.F, p_.meter(9)), 1) + " dB   /   " +
            (p_.meter(14) >= .5F ? juce::String("BUFFER EMPTY - PLAY A PHRASE")
             : p_.meter(11) >= .5F ? juce::String("MASTER REPEAT")
                                   : juce::String("LIVE")) +
            (p_.meter(12) >= .5F ? " + INNER" : "") +
            (p_.meter(13) >= .5F ? " / TIME HELD" : "") + "   /   " +
            juce::String(p_.meter(10), 2) + " s captured   /   " +
            juce::String(static_cast<int>(p_.meter(18))) + " voices",
        juce::dontSendNotification);
  }
  struct Row {
    juce::TextButton name, audition;
    juce::TextButton mute, solo;
    juce::Slider length, span;
    juce::ComboBox division;
    std::unique_ptr<Gate> repeat;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>
        lengthAttachment, spanAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>
        divisionAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>
        muteAttachment, soloAttachment;
  };
  Processor& p_;
  Look look_;
  aste::ui::MasterOutput master_;
  int object_{}, event_{}, currentView_{};
  std::array<juce::TextButton, 3> view_;
  juce::ComboBox preset_, soundSelect_, replace_;
  juce::TextButton save_, load_, more_, sampleLoad_, duplicate_, advanced_,
      panic_, store_;
  std::array<Row, 4> row_;
  std::vector<std::unique_ptr<Control>> header_;
  Grid grid_;
  Timeline timeline_;
  Wave wave_;
  Fields eventFields_, soundFields_, performanceFields_;
  CaptureDock capture_;
  juce::Viewport soundViewport_, eventViewport_;
  std::array<std::unique_ptr<Gate>, 6> gates_, largeGates_;
  std::array<juce::TextButton, 4> scenes_;
  juce::Label selected_, cue_, sampleLabel_, status_;
  juce::String lastPreset_;
  std::unique_ptr<juce::FileChooser> chooser_;
  juce::TooltipWindow tooltip_{this};
};
}  // namespace
juce::AudioProcessorEditor* Processor::createEditor() {
  return new Editor{*this};
}
}  // namespace aste::click::plugin
