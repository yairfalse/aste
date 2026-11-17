#include "research/quality/audio.hpp"
#include "apps/density/density_processor.hpp"
#include "apps/harmonic/harmonic_processor.hpp"
#include "apps/sequence/sequence_processor.hpp"
#include "apps/loop/loop_processor.hpp"
#include "apps/impulse/impulse_processor.hpp"
#include "apps/field/field_processor.hpp"
#include "apps/non-machine/non_machine_processor.hpp"
#include "apps/resonator/resonator_processor.hpp"
#include <reference/density/density_processor.hpp>
#include <reference/harmonic/harmonic_processor.hpp>
#include <reference/sequence/sequence_processor.hpp>
#include <reference/loop/loop_processor.hpp>
#include <reference/impulse/impulse_processor.hpp>
#include <reference/field/field_processor.hpp>
#include <reference/non-machine/non_machine_processor.hpp>
#include <reference/resonator/resonator_processor.hpp>
#include <chrono>
#include <iostream>
#include <memory>
using namespace power;
void require(bool ok, const char* why) {
  if (!ok) throw std::runtime_error(why);
}
template <class P>
void intent(P& p, bool active) {
  if constexpr (requires { p.materialBehavior; }) p.materialBehavior = active;
}
template <class E, class P>
Audio density(bool active, int kind, double rate, std::size_t block = 127) {
  auto a = fixture(kind, rate);
  P p;
  p.driveDb = 6;
  p.crush = .9F;
  p.character = .8F;
  p.density = .7F;
  p.attackMs = 9;
  p.releaseMs = 75;
  p.blend = .85F;
  p.outputDb = -6;
  intent(p, active);
  E e;
  e.prepare(rate, p);
  for (std::size_t at = 0; at < a.left.size(); at += block)
    e.process(a.left.data() + at, a.right.data() + at,
              std::min(block, a.left.size() - at), p);
  return a;
}
template <class E, class P>
Audio harmonic(bool active, int kind, double rate, std::size_t block = 127) {
  auto a = fixture(kind, rate);
  P p;
  p.harmonic = .8F;
  p.inputDb = 6;
  p.outputDb = -6;
  intent(p, active);
  E e;
  e.prepare(rate, p);
  for (std::size_t at = 0; at < a.left.size(); at += block)
    e.process(a.left.data() + at, a.right.data() + at,
              std::min(block, a.left.size() - at), p);
  return a;
}
template <class E, class P, class Event>
Audio sequence(bool active, int, double rate, std::size_t block = 127) {
  Audio a(static_cast<std::size_t>(rate * 8), rate);
  P p;
  p.sequenceEnabled = false;
  p.voiceMode = 1;
  p.pressure = .2F;
  p.cutoffHz = 700;
  p.outputDb = -9;
  intent(p, active);
  E e;
  e.prepare(rate, p);
  using Type = decltype(Event{}.type);
  std::vector<Event> events;
  for (int i = 0; i < 8; ++i) {
    const auto at = static_cast<std::size_t>(rate * .5 * i);
    events.push_back({at, Type::noteOn, 36 + (i % 3) * 3, .8F});
    events.push_back({at + static_cast<std::size_t>(rate * .3), Type::noteOff,
                      36 + (i % 3) * 3, 0});
  }
  if constexpr (requires { Type::expression; })
    if (active) {
      events.push_back(
          {static_cast<std::size_t>(rate), Type::expression, 1, 1});
      events.push_back(
          {static_cast<std::size_t>(rate * 2), Type::expression, 74, .8F});
      events.push_back(
          {static_cast<std::size_t>(rate * 3), Type::expression, 1, 0});
    }
  std::stable_sort(events.begin(), events.end(), [](auto a, auto b) {
    return a.sampleOffset < b.sampleOffset;
  });
  std::size_t next = 0;
  for (std::size_t at = 0; at < a.left.size(); at += block) {
    const auto n = std::min(block, a.left.size() - at);
    std::array<Event, 32> local;
    std::size_t count = 0;
    while (next < events.size() && events[next].sampleOffset < at + n) {
      local[count] = events[next++];
      local[count++].sampleOffset -= at;
    }
    e.process(a.left.data() + at, a.right.data() + at, n, p, {},
              std::span(local).first(count));
  }
  return a;
}
template <class E, class P, class Event>
Audio impulse(bool active, int kind, double rate, std::size_t block = 127) {
  Audio a(static_cast<std::size_t>(rate * 8), rate);
  P p;
  p.sequenceEnabled = false;
  p.variation = 0;
  p.energy = .65F;
  p.outputDb = -6;
  intent(p, active);
  E e;
  e.prepare(rate, p);
  for (std::size_t at = 0; at < a.left.size(); at += block) {
    const auto n = std::min(block, a.left.size() - at);
    std::array<Event, 8> events;
    std::size_t count = 0;
    for (int hit = 0; hit < 12; ++hit) {
      auto when = static_cast<std::size_t>(rate * (hit * .28 + .05));
      if (when >= at && when < at + n)
        events[count++] = {when - at,
                           36 + (kind == 0   ? 4
                                 : kind == 4 ? 3
                                             : 6),
                           .8F, true};
    }
    e.process(a.left.data() + at, a.right.data() + at, n, p, {},
              std::span(events).first(count));
  }
  return a;
}
template <class E, class P>
Audio field(bool active, int kind, double rate, std::size_t block = 127) {
  auto a = fixture(kind, rate);
  for (std::size_t i = static_cast<std::size_t>(rate * .3); i < a.left.size();
       ++i)
    a.left[i] = a.right[i] = 0;
  P p;
  p.mass = .88F;
  p.grain = .6F;
  p.pitch = .4F;
  p.motion = .55F;
  p.distance = .2F;
  p.blend = 1;
  p.outputDb = -3;
  p.texture = .85F;
  p.focus = .45F;
  intent(p, active);
  auto e = std::make_unique<E>();
  e->prepare(rate);
  for (std::size_t at = 0; at < a.left.size();) {
    auto n = std::min(
        {block, a.left.size() - at,
         static_cast<std::size_t>(rate) - at % static_cast<std::size_t>(rate)});
    p.forever = true;
    e->process(a.left.data() + at, a.right.data() + at, n, p);
    at += n;
  }
  return a;
}
template <class E, class P>
Audio loop(bool active, int kind, double rate, std::size_t block = 127) {
  auto a = fixture(kind, rate);
  P p;
  p.capture = true;
  p.overdub = 0;
  p.loopLengthSeconds = 1;
  p.mix = 1;
  p.degradation = active ? .28F : .08F;
  p.amplifier = active ? .48F : .25F;
  p.outputDb = -6;
  auto e = std::make_unique<E>();
  e->prepare(rate, 2);
  for (std::size_t at = 0; at < a.left.size();) {
    auto n = std::min(
        {block, a.left.size() - at,
         static_cast<std::size_t>(rate) - at % static_cast<std::size_t>(rate)});
    p.capture = at < static_cast<std::size_t>(rate);
    p.reloop = at == static_cast<std::size_t>(rate * 2) ||
               at == static_cast<std::size_t>(rate * 4);
    if (!p.capture)
      for (std::size_t i = at; i < at + n; ++i) a.left[i] = a.right[i] = 0;
    e->process(a.left.data() + at, a.right.data() + at, n, p);
    at += n;
  }
  return a;
}
template <class E, class P, class Hit>
Audio nonmachine(bool active, int, double rate, std::size_t block = 127) {
  Audio a(static_cast<std::size_t>(rate * 8), rate);
  P p;
  intent(p, active);
  p.voidStart = p.voidEnd = 8;
  for (auto& t : p.tracks)
    for (auto& event : t.events) event.enabled = false;
  auto& t = p.tracks[0];
  t.material = 2;
  t.structure = .55F;
  t.strike = .7F;
  t.roughness = .12F;
  t.events[0].pitch = 160;
  t.events[0].decay = .22F;
  E e;
  require(e.prepare(rate, p), "N prepare");
  for (std::size_t at = 0; at < a.left.size(); at += block) {
    auto n = std::min(block, a.left.size() - at);
    std::array<Hit, 8> hits;
    std::size_t count = 0;
    for (int h = 0; h < 12; ++h) {
      auto when = static_cast<std::size_t>(rate * (.05 + h * .28));
      if (at <= when && when < at + n) hits[count++] = {when - at, 0, 0, 1};
    }
    e.process(a.left.data() + at, a.right.data() + at, n,
              {false, true, false, static_cast<std::int64_t>(at),
               at * 2. / rate, 120, true},
              nullptr, {}, std::span(hits).first(count));
  }
  return a;
}
template <class E, class C, class Exciter, class Event>
Audio resonator(bool active, int, double rate, std::size_t block = 127) {
  Audio a(static_cast<std::size_t>(rate * 8), rate);
  C c;
  c.t60 = 6;
  c.damping = .18;
  c.couplingHz = 7;
  c.stretch = .04;
  c.inharmonic = .12;
  if constexpr (requires { c.material; }) c.material = active ? .7 : 0;
  E e;
  Exciter source;
  using Type = decltype(Event{}.type);
  using Architecture = decltype(aste::resonator::Architecture::Modal);
  if constexpr (std::is_same_v<E, aste::resonator::Engine>)
    require(e.prepare(rate, aste::resonator::Architecture::Modal,
                      aste::resonator::Memory::Shared, c),
            "R prepare");
  else
    require(e.prepare(rate, aste::before::resonator::Architecture::Modal,
                      aste::before::resonator::Memory::Shared, c),
            "Reference R prepare");
  source.prepare(rate);
  std::vector<float> input(block);
  for (std::size_t at = 0; at < a.left.size();) {
    auto n = std::min(
        {block, a.left.size() - at,
         static_cast<std::size_t>(rate) - at % static_cast<std::size_t>(rate)});
    std::array<Event, 1> event;
    std::size_t count = 0;
    if (at % static_cast<std::size_t>(rate) == 0 &&
        at < static_cast<std::size_t>(rate * 4)) {
      event[count++] = {0, Type::Strike, {36}};
      if constexpr (std::is_same_v<Exciter, aste::resonator::Exciter>)
        source.trigger(aste::resonator::Excitation::Pink, 101,
                       static_cast<std::size_t>(rate * .018));
      else
        source.trigger(aste::before::resonator::Excitation::Pink, 101,
                       static_cast<std::size_t>(rate * .018));
    }
    for (std::size_t i = 0; i < n; ++i) input[i] = source.next() * .2F;
    e.process(input.data(), a.left.data() + at, a.right.data() + at, n, c,
              std::span(event).first(count));
    at += n;
  }
  return a;
}
Audio render(int product, bool old, bool active, int source, double rate,
             std::size_t block = 127) {
#define EFFECT(P) case P:
  switch (product) {
    case 0:
      return old ? density<aste::before::density::Processor,
                           aste::before::density::Parameters>(false, source,
                                                              rate, block)
                 : density<aste::density::Processor, aste::density::Parameters>(
                       active, source, rate, block);
    case 1:
      return old ? harmonic<aste::before::harmonic::Processor,
                            aste::before::harmonic::Parameters>(false, source,
                                                                rate, block)
                 : harmonic<aste::harmonic::Processor,
                            aste::harmonic::Parameters>(active, source, rate,
                                                        block);
    case 2:
      return old ? sequence<aste::before::sequence::Processor,
                            aste::before::sequence::Parameters,
                            aste::before::sequence::MidiEvent>(false, source,
                                                               rate, block)
                 : sequence<aste::sequence::Processor,
                            aste::sequence::Parameters,
                            aste::sequence::MidiEvent>(active, source, rate,
                                                       block);
    case 3:
      return old ? loop<aste::before::loop::Processor,
                        aste::before::loop::Parameters>(active, source, rate,
                                                        block)
                 : loop<aste::loop::Processor, aste::loop::Parameters>(
                       active, source, rate, block);
    case 4:
      return old ? impulse<aste::before::impulse::Processor,
                           aste::before::impulse::Parameters,
                           aste::before::impulse::MidiEvent>(false, source,
                                                             rate, block)
                 : impulse<aste::impulse::Processor, aste::impulse::Parameters,
                           aste::impulse::MidiEvent>(active, source, rate,
                                                     block);
    case 5:
      return old ? field<aste::before::field::Processor,
                         aste::before::field::Parameters>(false, source, rate,
                                                          block)
                 : field<aste::field::Processor, aste::field::Parameters>(
                       active, source, rate, block);
    case 6:
      return old ? nonmachine<aste::before::nonmachine::Engine,
                              aste::before::nonmachine::Pattern,
                              aste::before::nonmachine::ManualHit>(
                       false, source, rate, block)
                 : nonmachine<aste::nonmachine::Engine,
                              aste::nonmachine::Pattern,
                              aste::nonmachine::ManualHit>(active, source, rate,
                                                           block);
    default:
      return old ? resonator<aste::before::resonator::Engine,
                             aste::before::resonator::Controls,
                             aste::before::resonator::Exciter,
                             aste::before::resonator::Event>(false, source,
                                                             rate, block)
                 : resonator<aste::resonator::Engine, aste::resonator::Controls,
                             aste::resonator::Exciter, aste::resonator::Event>(
                       active, source, rate, block);
  }
}
#include "behavior_checks.hpp"
int main(int argc, char** argv) {
  try {
    behaviorChecks();
    const std::array<const char*, 8> names{
        "density", "harmonic", "sequence",    "loop",
        "impulse", "field",    "non-machine", "resonator"};
    unsigned comparisons = 0;
    for (double rate : {44100., 48000., 88200., 96000., 176400., 192000.})
      for (int p = 0; p < 8; ++p) {
        auto old = render(p, true, false, 5, rate),
             current = render(p, false, false, 5, rate);
        require(old.left == current.left && old.right == current.right,
                "Disabled revision differs from frozen pre-edit audio");
        ++comparisons;
      }
    std::cout << "Exact pre-edit comparisons: " << comparisons << '\n';
    for (int p = 0; p < 8; ++p) {
      const auto started = std::chrono::steady_clock::now();
      auto a = render(p, false, true, 5, 48000);
      const auto elapsed = std::chrono::duration<double>(
                               std::chrono::steady_clock::now() - started)
                               .count();
      std::cout << names[p] << " offline realtime fraction " << elapsed / 8
                << '\n';
      auto b = render(p, false, true, 5, 48000, 511);
      validate(a);
      validate(b);
      require(a.left == b.left && a.right == b.right,
              "New material behavior depends on audio block partition");
      std::cout << names[p] << " new peak " << measure(a).peak << " delta "
                << difference(a, render(p, false, false, 5, 48000)) << '\n';
    }
    if (argc == 2) {
      const std::filesystem::path folder{argv[1]};
      require(!std::filesystem::exists(folder),
              "Choose a fresh listening folder");
      std::filesystem::create_directories(folder);
      auto csv = output(folder / "measurements.csv");
      csv << "product,source,variant,raw_peak,raw_rms,matched_lufs\n";
      for (int p = 0; p < 8; ++p)
        for (int source : {0, 4, 5}) {
          if ((p == 2 || p == 6 || p == 7) && source != 5) continue;
          std::array<Audio, 2> raw{render(p, false, false, source, 48000),
                                   render(p, false, true, source, 48000)},
              matched = raw;
          double target = -23;
          for (int headroom = 0; headroom < 8; ++headroom) {
            double next = target;
            for (std::size_t v = 0; v < 2; ++v) {
              auto level = loudness(raw[v]);
              require(level.has_value(), "Silent comparison source");
              double scale = gain(target - *level);
              for (int iteration = 0; iteration < 8; ++iteration) {
                matched[v] = raw[v];
                for (auto* channel : {&matched[v].left, &matched[v].right})
                  for (auto& sample : *channel) sample *= scale;
                const auto m = measure(matched[v]);
                if (m.lufs && std::abs(*m.lufs - target) < .0001) break;
                scale *= gain(target - *m.lufs);
              }
              const auto m = measure(matched[v]);
              if (m.peak > .8)
                next = std::min(next,
                                target - 20 * std::log10(m.peak / .8) - .00001);
            }
            if (next == target) break;
            target = next;
          }
          for (std::size_t v = 0; v < 2; ++v) {
            const auto before = measure(raw[v]), m = measure(matched[v]);
            require(
                m.peak <= .8001 && m.lufs && std::abs(*m.lufs - target) < .05,
                "Listening match/headroom failed");
            wav(folder / (std::string(names[p]) + "-" + std::to_string(source) +
                          "-" + (v ? "material" : "previous") + ".wav"),
                matched[v]);
            csv << names[p] << ',' << source << ',' << v << ',' << before.peak
                << ',' << before.rms << ',' << *m.lufs << '\n';
          }
        }
    }
    std::cout << "material review: passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
