#pragma once
#include "libs/dsp/material_body.hpp"
// Tests causal behavior, rather than mistaking changed output level for memory.
inline void behaviorChecks() {
  using namespace aste;
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    dsp::MaterialBody body;
    body.prepare(rate);
    double tail = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(rate); ++i) {
      const float y = body.process(i == 0 ? 1.F : 0.F, 180, 1, .5F, .8F);
      require(std::isfinite(y) && std::abs(y) <= 1,
              "Excited body bounded at supported rate");
      if (i > 100) tail += y * y;
    }
    require(tail > 1.e-6, "Body retains energy after exciter stops");
    body.reset();
    require(body.process(0, 180, 1, .5F, .8F) == 0, "Body reset is silent");
  }
  // Moving/pitched holds must retain actual audio, then permit release.
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    auto e = std::make_unique<field::Processor>();
    e->prepare(rate);
    field::Parameters p;
    p.materialBehavior = true;
    p.forever = true;
    p.mass = .88F;
    p.grain = .6F;
    p.pitch = .4F;
    p.motion = .55F;
    p.texture = .85F;
    p.focus = .45F;
    p.blend = 1;
    double early = 0, late = 0, released = 0;
    for (std::size_t i = 0; i < static_cast<std::size_t>(rate * 8); ++i) {
      float l = i < rate * .3 ? .15F * std::sin(i * 2 * power::pi * 435 / rate)
                              : 0,
            r = l;
      p.forever = i < rate * 5;
      e->process(&l, &r, 1, p);
      require(std::isfinite(l) && std::isfinite(r) && std::abs(l) < 8 &&
                  std::abs(r) < 8,
              "Moving hold remains bounded");
      const double power = (l * l + r * r) * .5;
      if (i >= rate && i < rate * 1.5) early += power;
      if (i >= rate * 4 && i < rate * 4.5) late += power;
      if (i >= rate * 7 && i < rate * 7.5) released += power;
    }
    require(late > early * .1 && early > 1.e-6,
            "Moving hold preserves a measurable tail");
    require(released < late * .01, "Releasing hold restores decay");
    std::cout << "field " << rate << " held power ratio " << late / early
              << " released ratio " << released / late << '\n';
  }
  // A second excitation should interact with state, beyond linear overlap.
  auto memory = [](double material, bool first, bool second) {
    resonator::Controls c;
    c.material = material;
    c.t60 = 6;
    c.damping = .18;
    c.couplingHz = 7;
    resonator::Engine e;
    require(e.prepare(48000, resonator::Architecture::Modal,
                      resonator::Memory::Shared, c),
            "Memory prepare");
    Audio a(48000, 48000);
    for (std::size_t i = 0; i < a.left.size(); ++i) {
      const float input =
          ((i == 0 && first) || (i == 12000 && second)) ? 1.F : 0.F;
      std::array<resonator::Event, 1> event{
          {{0, resonator::EventType::Strike, {36}}}};
      e.process(&input, &a.left[i], &a.right[i], 1, c,
                i == 0 ? std::span<const resonator::Event>(event)
                       : std::span<const resonator::Event>{});
    }
    return a;
  };
  for (double material : {0., .7}) {
    auto both = memory(material, true, true), a = memory(material, true, false),
         b = memory(material, false, true);
    double residual = 0, energy = 0;
    for (std::size_t i = 12000; i < both.left.size(); ++i) {
      double d = both.left[i] - a.left[i] - b.left[i];
      residual += d * d;
      energy += both.left[i] * both.left[i];
    }
    const double ratio = std::sqrt(residual / std::max(energy, 1.e-30));
    std::cout << "resonator memory=" << material << " nonadditive residual "
              << 20 * std::log10(std::max(ratio, 1.e-15)) << " dB\n";
    require(material == 0 ? ratio < 1.e-5 : ratio > .001,
            "Energy-dependent memory differs from linear superposition");
  }
  for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.}) {
    resonator::Controls c;
    c.material = 1;
    c.t60 = 60;
    c.persist = true;
    c.couplingHz = 20;
    c.damping = 0;
    resonator::Engine e;
    require(e.prepare(rate, resonator::Architecture::Modal,
                      resonator::Memory::Shared, c),
            "Extreme modal prepare");
    std::array<float, 127> in{}, l{}, r{};
    for (int block = 0; block < 160; ++block) {
      for (std::size_t i = 0; i < in.size(); ++i)
        in[i] = static_cast<float>(power::noise(block * 127 + i));
      c.damping = block % 2 ? 0 : .8;
      c.inharmonic = block % 3 * .5;
      std::array<resonator::Event, 1> events{
          {{17,
            block == 0 ? resonator::EventType::Strike
                       : resonator::EventType::Retune,
            {double(24 + block % 72)}}}};
      e.process(in.data(), l.data(), r.data(), in.size(), c, events);
      for (std::size_t i = 0; i < in.size(); ++i)
        require(std::isfinite(l[i]) && std::isfinite(r[i]) &&
                    std::abs(l[i]) < 32 && std::abs(r[i]) < 32,
                "Material modal extremes remain finite and bounded");
    }
    require(e.snapshot().rejected == 0 && e.snapshot().sanitized == 0,
            "Valid extreme material controls require no sanitization");
  }
  using namespace sequence;
  auto performed = [](bool revision, bool expression) {
    Parameters p;
    p.materialBehavior = revision;
    p.sequenceEnabled = false;
    p.voiceMode = 1;
    p.cutoffHz = 700;
    p.pressure = .2F;
    Processor e;
    e.prepare(48000, p);
    Audio a(24000, 48000);
    std::array<MidiEvent, 2> events{{{0, MidiEventType::noteOn, 48, .8F},
                                     {4000, MidiEventType::expression, 74, 1}}};
    e.process(a.left.data(), a.right.data(), a.left.size(), p, {},
              std::span(events).first(expression ? 2 : 1));
    return a;
  };
  auto base = performed(true, false), gesture = performed(true, true);
  require(std::equal(base.left.begin(), base.left.begin() + 4000,
                     gesture.left.begin()),
          "Expression cannot affect preceding samples");
  require(difference(base, gesture) > .00001,
          "Brightness gesture affects sounding voice");
  require(performed(false, false).left == performed(false, true).left,
          "Old response ignores new expression routing");
  std::cout << "causal material behavior: passed\n";
}
