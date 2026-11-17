#include "apps/non-machine/non_machine_processor.hpp"
#include "apps/resonator/resonator_processor.hpp"
#include "research/quality/audio.hpp"
#include <iostream>
using namespace power;
void require(bool ok, const char* why) {
  if (!ok) throw std::runtime_error(why);
}
struct Percussion {
  Audio audio;
  std::vector<aste::nonmachine::Hit> trace;
};
Percussion drum(bool authored, bool midi, bool capture) {
  constexpr double rate = 48000;
  constexpr std::size_t frames = 384000;
  auto p = aste::nonmachine::studyPattern();
  p.voidStart = p.voidEnd = 8;
  for (auto& track : p.tracks)
    for (auto& e : track.events) e.enabled = false;
  auto& t = p.tracks[0];
  t.length = 4;
  t.material = 2;
  t.structure = .55F;
  t.strike = .7F;
  t.roughness = .12F;
  auto& e = t.events[0];
  e.enabled = authored;
  e.beat = .5;
  e.repeats = 1;
  e.pitch = 160;
  e.decay = .22F;
  e.color = .35F;
  aste::nonmachine::Engine engine;
  require(engine.prepare(rate, p), "N prepares");
  Percussion result{Audio(frames), {}};
  std::array<aste::nonmachine::Hit, 512> hits;
  aste::nonmachine::Trace trace{hits};
  for (std::size_t at = 0; at < frames; at += 127) {
    const auto n = std::min(std::size_t{127}, frames - at);
    std::array<aste::nonmachine::ManualHit, 1> manual{};
    std::size_t manualCount = 0;
    if (midi && at <= 48000 && 48000 < at + n)
      manual[manualCount++] = {48000 - at, 0, 0, 1};
    std::array<aste::nonmachine::Command, 4> commands{};
    std::size_t count = 0;
    if (capture) {
      const std::array<std::size_t, 4> times{19200, 84000, 120000, 204000};
      for (std::size_t i = 0; i < 4; ++i)
        if (at <= times[i] && times[i] < at + n) {
          auto& c = commands[count++];
          c.offset = times[i] - at;
          c.target = i < 2 ? 0 : 3;
          c.action = i % 2 ? aste::nonmachine::Action::Release
                           : aste::nonmachine::Action::Capture;
          c.length = i < 2 ? 5760 : 12000;
          c.mode = aste::nonmachine::Mode::Continue;
        }
    }
    engine.process(result.audio.left.data() + at,
                   result.audio.right.data() + at, n,
                   {true, true, false, static_cast<std::int64_t>(at),
                    at / 24000., 120, !authored},
                   &trace, std::span(commands).first(count),
                   std::span(manual).first(manualCount));
  }
  result.trace.assign(hits.begin(), hits.begin() + trace.size);
  validate(result.audio);
  return result;
}
struct Resonance {
  Audio audio;
  std::size_t interventions{};
};
Resonance body(bool first, bool second, aste::resonator::Memory memory,
               bool retune = false) {
  constexpr std::size_t frames = 384000;
  Audio a(frames);
  aste::resonator::Engine engine;
  aste::resonator::Exciter exciter;
  aste::resonator::Controls controls;
  controls.t60 = 6;
  controls.damping = .25;
  controls.couplingHz = 3;
  controls.mutationCents = 0;
  require(engine.prepare(48000, aste::resonator::Architecture::Modal, memory,
                         controls),
          "R prepares");
  exciter.prepare(48000);
  for (std::size_t at = 0; at < frames; ++at) {
    std::array<aste::resonator::Event, 1> events;
    std::size_t count = 0;
    if ((first && at == 0) || (second && at == 96000)) {
      events[count++] = {0, aste::resonator::EventType::Strike, {48}};
      exciter.trigger(aste::resonator::Excitation::Shaped, 101, 1680);
    }
    if (retune && at == 96000) {
      events[count++] = {0, aste::resonator::EventType::Retune, {53}};
    }
    float input = exciter.next() * .15F;
    engine.process(&input, &a.left[at], &a.right[at], 1, controls,
                   std::span(events).first(count));
  }
  validate(a);
  return {std::move(a), engine.snapshot().safetyInterventions};
}
int main(int argc, char** argv) {
  try {
    const auto silent = drum(false, false, false),
               manual = drum(false, true, false),
               score = drum(true, false, false),
               captured = drum(true, false, true);
    require(measure(silent.audio).peak == 0,
            "Empty score without inputs must be silent");
    require(manual.trace.empty() && measure(manual.audio).peak > .001,
            "Manual trigger outside the visible score was not demonstrated");
    require(score.trace == captured.trace,
            "Captures changed the underlying authored event schedule");
    require(difference(score.audio, captured.audio) > .001,
            "Capture did not change audible result");
    const auto a = body(true, false, aste::resonator::Memory::Shared),
               b = body(false, true, aste::resonator::Memory::Shared),
               ab = body(true, true, aste::resonator::Memory::Shared);
    const auto reset = body(true, true, aste::resonator::Memory::Reset),
               retuned =
                   body(true, false, aste::resonator::Memory::Shared, true);
    require(
        a.interventions == 0 && b.interventions == 0 && ab.interventions == 0,
        "Safety scaling invalidates the linear comparison");
    double error = 0, energy = 0, maxError = 0;
    for (std::size_t i = 0; i < ab.audio.left.size(); ++i)
      for (bool right : {false, true}) {
        const double actual = right ? ab.audio.right[i] : ab.audio.left[i];
        const double expected =
            (right ? a.audio.right[i] : a.audio.left[i]) +
            static_cast<double>(right ? b.audio.right[i] : b.audio.left[i]);
        error += (actual - expected) * (actual - expected);
        energy += actual * actual;
        maxError = std::max(maxError, std::abs(actual - expected));
      }
    const double residual = 10 * std::log10(std::max(1.e-30, error / energy));
    require(residual < -110,
            "Fixed-tuning shared modal memory did not demonstrate linear "
            "superposition");
    std::cout << "N empty score peak " << measure(silent.audio).peak
              << "; hidden manual peak " << measure(manual.audio).peak
              << " / visible traced events " << manual.trace.size() << '\n';
    std::cout << "N authored events " << score.trace.size()
              << " / capture same trace " << (score.trace == captured.trace)
              << " / audio RMS difference "
              << difference(score.audio, captured.audio) << '\n';
    std::cout << "R shared-memory additivity residual " << residual
              << " dB; maximum error " << maxError << "; safety interventions "
              << ab.interventions << '\n';
    if (argc == 2) {
      const std::filesystem::path folder{argv[1]};
      require(!std::filesystem::exists(folder), "Use a new export folder");
      std::filesystem::create_directories(folder);
      const std::array<std::pair<const char*, const Audio*>, 9> examples{
          {{"n-empty-no-input.wav", &silent.audio},
           {"n-empty-plus-manual.wav", &manual.audio},
           {"n-authored-hits.wav", &score.audio},
           {"n-same-hits-plus-captures.wav", &captured.audio},
           {"r-first-excitation.wav", &a.audio},
           {"r-second-excitation.wav", &b.audio},
           {"r-shared-two-excitations.wav", &ab.audio},
           {"r-reset-two-excitations.wav", &reset.audio},
           {"r-retune-existing-energy.wav", &retuned.audio}}};
      // One scale per product retains real silence and actual level relations.
      double peakN = 0, peakR = 0;
      for (std::size_t i = 0; i < examples.size(); ++i) {
        auto& peak = i < 4 ? peakN : peakR;
        peak = std::max(peak, measure(*examples[i].second).peak);
      }
      auto measurements = output(folder / "measurements.csv");
      measurements << "file,raw_peak,raw_rms,shared_scale\n";
      for (std::size_t i = 0; i < examples.size(); ++i) {
        const auto& [name, audio] = examples[i];
        const auto m = measure(*audio);
        const double scale = .7 / (i < 4 ? peakN : peakR);
        wav(folder / name, *audio, scale);
        measurements << name << ',' << m.peak << ',' << m.rms << ',' << scale
                     << '\n';
      }
      auto causes = output(folder / "n-causes.csv");
      causes << "sample,seconds,cause,object_or_target\n";
      for (const auto& hit : score.trace)
        causes << hit.sample << ',' << hit.sample / 48000. << ",authored_hit,"
               << hit.voice << '\n';
      causes << "19200,0.4,local_capture,0\n84000,1.75,local_release,0\n120000,"
                "2.5,master_capture,3\n204000,4.25,master_release,3\n";
    }
    std::cout << "intent audit: passed\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
