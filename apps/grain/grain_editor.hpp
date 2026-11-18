// Included inside the product namespace, after the processor implementation.
namespace {
juce::String nodeName(std::size_t n) {
  return juce::String::charToString(static_cast<juce::juce_wchar>('A' + n));
}
bool feedbackRoute(const Controls& c, std::size_t destination,
                   std::size_t source) {
  if (destination == source) return true;
  bool reachable[3][3]{};
  for (std::size_t d = 0; d < 3; ++d)
    for (std::size_t s = 0; s < 3; ++s) reachable[s][d] = c.routes[d][s] > 0.F;
  for (std::size_t k = 0; k < 3; ++k)
    for (std::size_t s = 0; s < 3; ++s)
      for (std::size_t d = 0; d < 3; ++d)
        reachable[s][d] |= reachable[s][k] && reachable[k][d];
  return reachable[destination][source];
}

// A gesture owns only the value it last wrote. Host automation or state recall
// taking over a route must not be overwritten by a stale mouse-up.
class HoldSend final : public juce::TextButton {
 public:
  explicit HoldSend(GrainAudioProcessor& p) : TextButton{"HOLD SEND"}, p_(p) {
    setTitle("Momentary send");
    setComponentID("hold-route");
    setTooltip(
        "Hold mouse or Space to send. Drag up/down to change the amount. "
        "Release restores the previous connection, not the memory.");
    setWantsKeyboardFocus(true);
  }
  ~HoldSend() override { release(); }
  void route(std::size_t d, std::size_t s) {
    release();
    id_ = GrainAudioProcessor::routeId(d, s);
  }
  void mouseDown(const juce::MouseEvent& e) override {
    juce::TextButton::mouseDown(e);
    origin_ = e.position.y;
    engage();
  }
  void mouseDrag(const juce::MouseEvent& e) override {
    juce::TextButton::mouseDrag(e);
    if (parameter_)
      write(std::clamp(startAmount_ + (origin_ - e.position.y) / 120.F, 0.F,
                       1.5F));
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
    return juce::TextButton::keyPressed(key);
  }
  bool keyStateChanged(bool down) override {
    if (!juce::KeyPress::isKeyCurrentlyDown(juce::KeyPress::spaceKey))
      release();
    return parameter_ != nullptr || juce::TextButton::keyStateChanged(down);
  }
  void focusLost(FocusChangeType) override { release(); }
  void release() {
    if (!parameter_) return;
    if (parameter_->getValue() == lastWritten_)
      parameter_->setValueNotifyingHost(previous_);
    parameter_->endChangeGesture();
    parameter_ = nullptr;
    setToggleState(false, juce::dontSendNotification);
  }

 private:
  void engage() {
    if (parameter_ || !isEnabled() || p_.value(6) > 0.F) return;
    parameter_ = p_.state().getParameter(id_);
    if (!parameter_) return;
    previous_ = parameter_->getValue();
    startAmount_ = std::max(.65F, parameter_->convertFrom0to1(previous_));
    parameter_->beginChangeGesture();
    write(startAmount_);
    setToggleState(true, juce::dontSendNotification);
  }
  void write(float amount) {
    lastWritten_ = parameter_->convertTo0to1(amount);
    parameter_->setValueNotifyingHost(lastWritten_);
  }
  GrainAudioProcessor& p_;
  juce::String id_;
  juce::RangedAudioParameter* parameter_{};
  float previous_{}, lastWritten_{}, startAmount_{}, origin_{};
};

class RoutingMap final : public juce::Component {
 public:
  explicit RoutingMap(GrainAudioProcessor& p) : p_(p) {
    setTitle("Directed routes after cycle morph and feedback limiting");
    setDescription(
        "Click an arrow row to edit it. Amber marks self-routing "
        "or a feedback cycle. Values are target gains, not audio meters.");
  }
  std::function<void(std::size_t, std::size_t)> edit;
  void mouseDown(const juce::MouseEvent& e) override {
    const int d = static_cast<int>(e.position.x) * 3 / std::max(1, getWidth());
    const int s = (static_cast<int>(e.position.y) - 25) / 32;
    if (e.position.y >= 25 && d >= 0 && d < 3 && s >= 0 && s < 3 && edit)
      edit(static_cast<std::size_t>(d), static_cast<std::size_t>(s));
  }
  void paint(juce::Graphics& g) override {
    const auto c = p_.currentControls();
    g.fillAll(juce::Colour{0xff1b2526});
    g.setFont(juce::FontOptions{11.F});
    for (std::size_t d = 0; d < 3; ++d) {
      const int x = static_cast<int>(d) * getWidth() / 3 + 10;
      const int w = getWidth() / 3 - 20;
      float sum = 0;
      for (float gain : c.routes[d]) sum += gain;
      const float scale = sum > .995F ? .995F / sum : 1;
      g.setColour(juce::Colour{0xffe7ece8});
      g.drawText("INTO " + nodeName(d), x, 4, w, 18,
                 juce::Justification::centredLeft);
      for (std::size_t s = 0; s < 3; ++s) {
        const float amount = c.routes[d][s] * scale;
        const int y = 28 + static_cast<int>(s) * 32;
        const bool feedback = feedbackRoute(c, d, s);
        g.setColour(amount > 0.F
                        ? juce::Colour{feedback ? 0xffd4b075 : 0xff83a99a}
                        : juce::Colour{0xff72807c});
        g.drawText(nodeName(s) + " > " + nodeName(d) + "  " +
                       (amount > 0.F
                            ? juce::String(amount, amount < .01F ? 4 : 2) + "x"
                            : "OFF"),
                   x, y, w, 16, juce::Justification::centredLeft);
        if (amount > 0.F) {
          g.drawArrow({static_cast<float>(x), static_cast<float>(y + 21),
                       static_cast<float>(x + w), static_cast<float>(y + 21)},
                      1, 4, 4);
          if (feedback)
            g.fillEllipse(static_cast<float>(x + w - 5),
                          static_cast<float>(y + 3), 4, 4);
        }
      }
    }
  }

 private:
  GrainAudioProcessor& p_;
};

class Editor final : public juce::AudioProcessorEditor, private juce::Timer {
 public:
  explicit Editor(GrainAudioProcessor& p)
      : AudioProcessorEditor{p}, p_(p), master_{p.state()}, map_{p}, hold_{p} {
    setLookAndFeel(&look_);
    addAndMakeVisible(master_);
    addAndMakeVisible(map_);
    addAndMakeVisible(hold_);
    hold_.setExplicitFocusOrder(20);
    for (std::size_t i = 0; i < p.specs().size(); ++i) {
      controls_.push_back(std::make_unique<Control>(p, p.specs()[i], true));
      controls_.back()->setComponentID(p.specs()[i].id);
      controls_.back()->setExplicitFocusOrder(static_cast<int>(i + 100));
      addChildComponent(*controls_.back());
    }
    for (std::size_t n = 0; n < 3; ++n) {
      const auto letter = nodeName(n);
      setup(nodes_[n], "NODE " + letter, "node-" + letter.toLowerCase(),
            [this, n] { select(n); });
      setup(sends_[n], "SEND " + letter + " TO...",
            "send-" + letter.toLowerCase(), [this, n] {
              select(n);
              message_ =
                  "Choose a destination below. SEND routes live node output "
                  "into its capture input.";
              repaint();
            });
      setup(clear_[n], "CLEAR", "clear-node-" + letter.toLowerCase(),
            [this, n] { confirmClear(static_cast<int>(n)); });
      clear_[n].setTooltip(
          "Click twice within three seconds to erase this node. "
          "Other nodes can feed new material back into it.");
      setup(destinations_[n], "TO " + letter,
            "destination-" + letter.toLowerCase(),
            [this, n] { chooseDestination(n, true); });
      controls_[nodeIndex(n, 2)]->setCaption("SPEED x");
      controls_[nodeIndex(n, 2)]->setHelp(
          "Source traversal speed. Pitch stays independent unless Time Mode is "
          "Varispeed. Zero holds the source position.");
      controls_[nodeIndex(n, 13)]->setHelp(
          "Audible monitor gain only. Outgoing sends keep feeding even at "
          "zero.");
      controls_[nodeIndex(n, 18)]->setHelp(
          "Continuously replace memory from live input and incoming routes. "
          "Turning off seals the captured fragment; it does not mute "
          "playback.");
      controls_[nodeIndex(n, 19)]->setHelp(
          "Hold existing memory while grains keep playing. Empty memory arms "
          "on incoming audio when Capture is on.");
      controls_[nodeIndex(n, 14)]->setHelp(
          "Duration of each replacement memory. New audio becomes a grain "
          "source when this capture completes.");
    }
    setup(remove_, "REMOVE ROUTE", "remove-route", [this] {
      hold_.release();
      if (editable()) p_.set(routeId(), 0);
      refresh();
    });
    setup(latch_, "LATCH SEND", "latch-route", [this] {
      hold_.release();
      if (editable()) p_.set(routeId(), std::max(.65F, baseAmount()));
      refresh();
    });
    setup(commit_, "EDIT CURRENT ROUTES", "edit-routing", [this] {
      hold_.release();
      const auto c = p_.currentControls();
      for (std::size_t d = 0; d < 3; ++d)
        for (std::size_t s = 0; s < 3; ++s)
          p_.set(GrainAudioProcessor::routeId(d, s), c.routes[d][s]);
      p_.set("cycle_morph", 0);
      message_ =
          "Current morph printed into route levels. Audio memory is unchanged.";
      refresh();
    });
    commit_.setTooltip(
        "Round the current routing to the existing 0.01 matrix steps and "
        "reset Cycle Morph. No audio is copied or erased.");
    setup(details_, "DETAILS", "details", [this] {
      detail_ = !detail_;
      layoutControls();
      resized();
      repaint();
    });
    setup(matrix_, "MATRIX", "matrix", [this] {
      matrixVisible_ = !matrixVisible_;
      layoutControls();
      resized();
      repaint();
    });
    setup(mutate_, "MUTATE POSITION", "mutate", [this] { p_.mutate(source_); });
    setup(reset_, "CLEAR NETWORK", "clear-network",
          [this] { confirmClear(3); });
    scene_.addItemList(
        {"Single A", "Cascade A > B > C", "Parallel", "Feedback cycle"}, 1);
    scene_.setTitle("Routing starting points");
    scene_.setExplicitFocusOrder(21);
    scene_.onChange = [this] {
      hold_.release();
      p_.scene(scene_.getSelectedId() - 1);
      refresh();
    };
    addAndMakeVisible(scene_);
    map_.edit = [this](std::size_t d, std::size_t s) {
      select(s);
      chooseDestination(d, false);
    };
    workflow_.bind(*this, "CAPTURE > FREEZE > SEND > TRANSFORM",
                   "SEND is continuous audio into a destination's capture "
                   "input, not a copy. "
                   "Freeze or stop Capture to keep its memory. Monitor level "
                   "does not change outgoing sends. "
                   "Captured memories and routing save with the project.");
    addAndMakeVisible(workflow_);
    setResizable(true, true);
    setResizeLimits(1120, 800, 1800, 1200);
    setSize(1280, 850);
    select(0);
    startTimerHz(20);
  }
  ~Editor() override {
    hold_.release();
    setLookAndFeel(nullptr);
  }
  void resized() override {
    const int w = getWidth(), column = (w - 56) / 3;
    master_.setBounds(w - 220, 16, 196, 46);
    scene_.setBounds(w - 480, 24, 240, 28);
    const std::array<std::size_t, 6> globals{0, 1, 2, 4, 5, 6};
    for (std::size_t i = 0; i < globals.size(); ++i)
      controls_[globals[i]]->setBounds(24 + static_cast<int>(i) * (w - 48) / 6,
                                       82, (w - 48) / 6, 54);
    details_.setBounds(w - 132, 145, 108, 26);
    for (std::size_t n = 0; n < 3; ++n) {
      const int x = 24 + static_cast<int>(n) * (column + 4);
      nodes_[n].setBounds(x + 8, 184, 110, 28);
      sends_[n].setBounds(x + column - 154, 184, 146, 28);
      clear_[n].setBounds(x + column - 92, 246, 84, 24);
      for (std::size_t i = 0; i < primary_.size(); ++i)
        controls_[nodeIndex(n, primary_[i])]->setBounds(
            x + 4 + static_cast<int>(i % 2) * (column - 8) / 2,
            278 + static_cast<int>(i / 2) * 48, (column - 8) / 2, 46);
      controls_[GrainAudioProcessor::liveBase + n]->setBounds(
          x + 4 + (column - 8) / 2, 422, (column - 8) / 2, 46);
      for (std::size_t i = 0; i < 3; ++i)
        controls_[nodeIndex(n, 18 + i)]->setBounds(
            x + 4 + static_cast<int>(i) * (column - 8) / 3, 475,
            (column - 8) / 3, 26);
    }
    if (detail_) {
      for (std::size_t f = 0; f < GrainAudioProcessor::nodeFields; ++f)
        controls_[nodeIndex(source_, f)]->setBounds(
            24 + static_cast<int>(f % 6) * (w - 48) / 6,
            277 + static_cast<int>(f / 6) * 54, (w - 48) / 6, 52);
    }
    const int split = w * 45 / 100;
    map_.setBounds(24, 553, split - 36, 128);
    matrix_.setBounds(24, 515, 100, 28);
    for (std::size_t d = 0; d < 3; ++d)
      for (std::size_t s = 0; s < 3; ++s)
        controls_[GrainAudioProcessor::routeBase + d * 3 + s]->setBounds(
            24 + static_cast<int>(s) * (split - 36) / 3,
            550 + static_cast<int>(d) * 46, (split - 36) / 3, 44);
    const int x = split + 12, right = w - x - 24;
    for (std::size_t d = 0; d < 3; ++d)
      destinations_[d].setBounds(x + static_cast<int>(d) * 76, 548, 70, 28);
    hold_.setBounds(x, 590, 124, 30);
    latch_.setBounds(x + 132, 590, 124, 30);
    remove_.setBounds(x + 264, 590, 132, 30);
    if (amount_) amount_->setBounds(x + 232, 535, right - 232, 48);
    commit_.setBounds(x, 630, right, 28);
    mutate_.setBounds(24, 700, 150, 28);
    controls_[3]->setBounds(184, 691, 165, 46);
    controls_[7]->setBounds(359, 691, 165, 46);
    reset_.setBounds(w - 180, 700, 156, 28);
    workflow_.setBounds(24, getHeight() - 60, w - 48, 44);
  }
  void paint(juce::Graphics& g) override {
    g.fillAll(juce::Colour{0xff101617});
    g.setColour(juce::Colour{0xffe7ece8});
    g.setFont(juce::FontOptions{24.F, juce::Font::bold});
    g.drawText("GRAIN", 24, 16, 200, 32, juce::Justification::centredLeft);
    g.setFont(juce::FontOptions{11.F});
    g.drawText("G-01 / THREE INTERACTING MEMORIES", 24, 50, 460, 18,
               juce::Justification::centredLeft);
    g.drawText(detail_ ? "DETAILS / EDITING NODE " + nodeName(source_)
                       : "CAPTURE INPUT / SHAPE EACH CLOUD / SEND IT SOMEWHERE",
               24, 148, getWidth() - 180, 20, juce::Justification::centredLeft);
    const auto c = p_.currentControls();
    const int column = (getWidth() - 56) / 3;
    for (std::size_t n = 0; n < 3; ++n) {
      const int x = 24 + static_cast<int>(n) * (column + 4);
      g.setColour(juce::Colour{n == source_ ? 0xff83a99a : 0xff40514b});
      g.drawRect(x, 178, column, detail_ ? 96 : 330, n == source_ ? 2 : 1);
      g.setColour(juce::Colour{0xffe7ece8});
      const bool stored = p_.nodeMeter(n, 6) > 0;
      juce::String status =
          stored ? "STORED " + juce::String(p_.nodeMeter(n, 9), 2) + "s"
                 : "EMPTY";
      if (p_.nodeMeter(n, 5) > .5F)
        status += " / FROZEN";
      else if ((c.freeze || c.node[n].freeze) && !stored)
        status +=
            c.node[n].capture ? " / ARMED: FEED AUDIO" : " / CAPTURE IS OFF";
      else
        status += p_.nodeMeter(n, 8) > .5F ? " / CAPTURING"
                  : c.node[n].capture      ? " / CAPTURE READY"
                                           : " / CAPTURE OFF";
      g.drawText(status, x + 10, 216, column - 110, 18,
                 juce::Justification::centredLeft);
      const auto capture =
          p_.nodeMeter(n, 8) > .5F
              ? "REC " +
                    juce::String(juce::Decibels::gainToDecibels(
                                     p_.nodeMeter(n, 7), -100.F),
                                 0) +
                    "dB"
              : juce::String{"REC OFF"};
      g.drawText(capture, x + column - 102, 216, 94, 18,
                 juce::Justification::centredRight);
      const auto activity = juce::String(static_cast<int>(p_.nodeMeter(n, 2))) +
                            " grains / OUT " +
                            aste::ui::levelText(p_.nodeMeter(n, 1));
      g.drawText(activity, x + 10, 236, column - 110, 16,
                 juce::Justification::centredLeft);
      juce::String incoming, outgoing;
      for (std::size_t other = 0; other < 3; ++other) {
        if (c.routes[n][other] > 0.F) incoming += nodeName(other);
        if (c.routes[other][n] > 0.F) outgoing += nodeName(other);
      }
      g.setColour(juce::Colour{0xffaebdb5});
      g.drawText(
          "IN " + (c.live[n] > 0 ? juce::String{"LIVE "} : juce::String{}) +
              (incoming.isEmpty() ? "-" : incoming) + " / SEND " +
              (outgoing.isEmpty() ? "-" : outgoing),
          x + 10, 255, column - 110, 16, juce::Justification::centredLeft);
    }
    g.setColour(juce::Colour{0xffaebdb5});
    g.drawText(matrixVisible_ ? "BASE LEVELS / BEFORE MORPH + LIMIT"
                              : "TARGET GAINS / AMBER = FEEDBACK",
               136, 518, getWidth() * 45 / 100 - 148, 22,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour{0xffe7ece8});
    g.drawText("SEND FROM " + nodeName(source_) + "  >  " +
                   nodeName(destination_) +
                   (feedbackRoute(c, destination_, source_) ? " / FEEDBACK"
                                                            : " / LIVE ROUTE"),
               getWidth() * 45 / 100 + 12, 514, getWidth() / 2, 22,
               juce::Justification::centredLeft);
    g.setColour(juce::Colour{0xffaebdb5});
    g.drawFittedText(message_, getWidth() * 45 / 100 + 12, 662,
                     getWidth() * 55 / 100 - 36, 34,
                     juce::Justification::topLeft, 2);
  }

 private:
  static std::size_t nodeIndex(std::size_t n, std::size_t field) {
    return GrainAudioProcessor::nodeBase + n * GrainAudioProcessor::nodeFields +
           field;
  }
  void setup(juce::TextButton& button, const juce::String& text,
             const juce::String& id, std::function<void()> action) {
    button.setButtonText(text);
    button.setTitle(text);
    button.setComponentID(id);
    button.setWantsKeyboardFocus(true);
    button.setExplicitFocusOrder(nextFocus_++);
    button.onClick = std::move(action);
    addAndMakeVisible(button);
  }
  juce::String routeId() const {
    return GrainAudioProcessor::routeId(destination_, source_);
  }
  float baseAmount() const {
    return p_.value(GrainAudioProcessor::routeBase + destination_ * 3 +
                    source_);
  }
  bool editable() const { return p_.value(6) <= 0.F; }
  void select(std::size_t n) {
    hold_.release();
    source_ = n;
    clearArmed_ = -1;
    for (std::size_t i = 0; i < 3; ++i)
      nodes_[i].setToggleState(i == n, juce::dontSendNotification);
    chooseDestination(destination_, false);
    layoutControls();
    resized();
  }
  void chooseDestination(std::size_t n, bool connect) {
    hold_.release();
    destination_ = n;
    hold_.route(n, source_);
    if (connect && editable() && baseAmount() <= 0.F) p_.set(routeId(), .65F);
    amount_ = std::make_unique<Control>(
        p_, p_.specs()[GrainAudioProcessor::routeBase + n * 3 + source_], true);
    amount_->setCaption("SEND AMOUNT x");
    amount_->setComponentID("send-amount");
    amount_->setExplicitFocusOrder(22);
    amount_->setHelp(
        "Continuous gain into the destination capture input. Incoming route "
        "totals are bounded at 0.995 for safe feedback. The map shows the "
        "resulting target gain.");
    addAndMakeVisible(*amount_);
    for (std::size_t d = 0; d < 3; ++d)
      destinations_[d].setToggleState(d == n, juce::dontSendNotification);
    refresh();
    resized();
  }
  void layoutControls() {
    for (auto& control : controls_) control->setVisible(false);
    for (auto i : {0, 1, 2, 4, 5, 6})
      controls_[static_cast<std::size_t>(i)]->setVisible(true);
    for (std::size_t n = 0; n < 3; ++n) {
      if (!detail_) {
        for (auto f : primary_) controls_[nodeIndex(n, f)]->setVisible(true);
        controls_[GrainAudioProcessor::liveBase + n]->setVisible(true);
        for (std::size_t f = 18; f <= 20; ++f)
          controls_[nodeIndex(n, f)]->setVisible(true);
      }
    }
    if (detail_) {
      for (std::size_t f = 0; f < GrainAudioProcessor::nodeFields; ++f)
        controls_[nodeIndex(source_, f)]->setVisible(true);
      controls_[3]->setVisible(true);
      controls_[7]->setVisible(true);
    }
    mutate_.setVisible(detail_);
    reset_.setVisible(detail_);
    details_.setToggleState(detail_, juce::dontSendNotification);
    matrix_.setToggleState(matrixVisible_, juce::dontSendNotification);
    map_.setVisible(!matrixVisible_);
    for (std::size_t i = 11; i < 20; ++i)
      controls_[i]->setVisible(matrixVisible_);
  }
  void confirmClear(int n) {
    const double now = juce::Time::getMillisecondCounterHiRes();
    if (clearArmed_ == n && now < clearDeadline_) {
      if (n == 3)
        p_.clearAll();
      else
        p_.clearNode(static_cast<std::size_t>(n));
      clearArmed_ = -1;
    } else {
      clearArmed_ = n;
      clearDeadline_ = now + 3000;
    }
    refresh();
  }
  void refresh() {
    const bool canEdit = editable();
    if (!canEdit) hold_.release();
    hold_.setEnabled(canEdit);
    latch_.setEnabled(canEdit);
    remove_.setEnabled(canEdit);
    if (amount_) amount_->setEnabled(canEdit);
    commit_.setVisible(!canEdit);
    const auto c = p_.currentControls();
    if (!canEdit)
      message_ =
          "Cycle Morph is shaping the routes. EDIT CURRENT ROUTES rounds to "
          "0.01 "
          "gain steps and enables direct SEND editing.";
    else if (c.freeze || c.node[destination_].freeze)
      message_ = "Destination " + nodeName(destination_) +
                 " is FROZEN: incoming sends do not replace held memory. "
                 "Release Freeze to receive new material.";
    else if (!c.node[destination_].capture)
      message_ = "Destination " + nodeName(destination_) +
                 " CAPTURE is OFF. Enable it to record the incoming send; "
                 "existing grains keep playing.";
    else if (p_.nodeMeter(source_, 6) == 0)
      message_ = "Source " + nodeName(source_) +
                 " has no stored audio yet. Feed its live input or another "
                 "node, with Capture enabled.";
    else
      message_ = "Continuous audio into " + nodeName(destination_) +
                 " memory. HOLD is temporary; LATCH keeps the route. Removing "
                 "a send keeps recorded material.";
    if (clearArmed_ >= 0 &&
        juce::Time::getMillisecondCounterHiRes() >= clearDeadline_)
      clearArmed_ = -1;
    for (std::size_t n = 0; n < 3; ++n) {
      clear_[n].setButtonText(clearArmed_ == static_cast<int>(n) ? "CONFIRM?"
                                                                 : "CLEAR");
      clear_[n].setTitle(clear_[n].getButtonText() + " NODE " + nodeName(n));
      nodes_[n].setDescription(
          "Node " + nodeName(n) + (n == source_ ? ", selected. " : ". ") +
          (p_.nodeMeter(n, 6) > 0 ? "Stored audio. " : "Empty memory. ") +
          (p_.nodeMeter(n, 5) > .5F ? "Frozen. " : "") +
          (p_.nodeMeter(n, 8) > .5F ? "Capturing input. " : "Not capturing. ") +
          juce::String(static_cast<int>(p_.nodeMeter(n, 2))) +
          " active grains.");
    }
    reset_.setButtonText(clearArmed_ == 3 ? "CONFIRM CLEAR?" : "CLEAR NETWORK");
    int match = 0;
    if (canEdit)
      for (unsigned i = 0; i < 4; ++i) {
        const auto candidate = aste::grain::scene(i);
        const auto sameParameter = [this](const juce::String& id, float actual,
                                          float expected) {
          const auto* parameter = p_.state().getParameter(id);
          return parameter->convertTo0to1(actual) ==
                 parameter->convertTo0to1(expected);
        };
        bool same = true;
        for (std::size_t d = 0; d < 3; ++d) {
          same &= sameParameter(GrainAudioProcessor::nodeId(d, "live"),
                                c.live[d], candidate.live[d]);
          for (std::size_t s = 0; s < 3; ++s)
            same &= sameParameter(GrainAudioProcessor::routeId(d, s),
                                  c.routes[d][s], candidate.routes[d][s]);
        }
        if (same) match = static_cast<int>(i + 1);
      }
    scene_.setTextWhenNothingSelected(canEdit ? "CUSTOM ROUTES"
                                              : "MORPHED ROUTES");
    scene_.setSelectedId(match, juce::dontSendNotification);
    map_.repaint();
    repaint();
  }
  void timerCallback() override {
    if (!juce::Process::isForegroundProcess()) hold_.release();
    refresh();
  }
  GrainAudioProcessor& p_;
  Look look_;
  aste::ui::MasterOutput master_;
  RoutingMap map_;
  HoldSend hold_;
  std::vector<std::unique_ptr<Control>> controls_;
  std::unique_ptr<Control> amount_;
  std::array<juce::TextButton, 3> nodes_, sends_, clear_, destinations_;
  juce::TextButton remove_, latch_, commit_, details_, matrix_, mutate_, reset_;
  juce::ComboBox scene_;
  aste::ui::WorkflowGuide workflow_;
  std::size_t source_{}, destination_{1};
  bool detail_{}, matrixVisible_{};
  int clearArmed_{-1};
  int nextFocus_{1};
  double clearDeadline_{};
  juce::String message_;
  static constexpr std::array<std::size_t, 7> primary_{0, 1, 2, 3, 7, 13, 14};
};
}  // namespace
