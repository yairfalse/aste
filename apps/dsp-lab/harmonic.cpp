#include "harmonic_processor.hpp"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string_view>
#include <vector>

namespace {

constexpr double kCentreHz = 1000.0;
constexpr double kInputPeak = 0.5;

struct PeakFilter {
  double b0{1.0};
  double b1{};
  double b2{};
  double a1{};
  double a2{};
  double z1{};
  double z2{};
  bool bypass{true};

  static PeakFilter make(double sampleRate, double frequency, double q,
                         double gainDb) {
    if (gainDb == 0.0) {
      return {};
    }
    const double amplitude = std::pow(10.0, gainDb / 40.0);
    const double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    const double alpha = std::sin(omega) / (2.0 * q);
    const double cosine = std::cos(omega);
    const double a0 = 1.0 + alpha / amplitude;
    return {
        .b0 = (1.0 + alpha * amplitude) / a0,
        .b1 = (-2.0 * cosine) / a0,
        .b2 = (1.0 - alpha * amplitude) / a0,
        .a1 = (-2.0 * cosine) / a0,
        .a2 = (1.0 - alpha / amplitude) / a0,
        .bypass = false,
    };
  }

  double process(double input) {
    if (bypass) {
      return input;
    }
    const double output = b0 * input + z1;
    z1 = b1 * input - a1 * output + z2;
    z2 = b2 * input - a2 * output;
    return output;
  }

  std::complex<double> response(double sampleRate, double frequency) const {
    if (bypass) {
      return {1.0, 0.0};
    }
    const double omega = 2.0 * std::numbers::pi * frequency / sampleRate;
    const std::complex<double> delay =
        std::exp(std::complex<double>{0.0, -omega});
    return (b0 + b1 * delay + b2 * delay * delay) /
           (1.0 + a1 * delay + a2 * delay * delay);
  }
};

double proportionalQ(double gainDb) {
  return 0.7 * (1.0 + 0.7 * std::abs(gainDb) / 12.0);
}

struct ResidualExciter {
  PeakFilter filter;
  double gainDb{};
  double harmonic{};

  double process(double input) {
    const double linear = filter.process(input);
    if (gainDb <= 0.0 || harmonic <= 0.0) {
      return linear;
    }
    const double residual = linear - input;
    const double drive = 1.0 + 8.0 * harmonic;
    const double shaped = std::tanh(drive * residual) / drive;
    return linear + harmonic * (shaped - residual);
  }
};

struct PreEmphasisExciter {
  PeakFilter linearContour;
  PeakFilter colouredContour;
  PeakFilter preEmphasis;
  PeakFilter deEmphasis;
  double gainDb{};
  double harmonic{};

  static PreEmphasisExciter make(double sampleRate, double frequency,
                                 double gain, double amount) {
    const double q = proportionalQ(gain);
    return {
        .linearContour = PeakFilter::make(sampleRate, frequency, q, gain),
        .colouredContour = PeakFilter::make(sampleRate, frequency, q, gain),
        .preEmphasis = PeakFilter::make(sampleRate, frequency, 0.9, 6.0),
        .deEmphasis = PeakFilter::make(sampleRate, frequency, 0.9, -6.0),
        .gainDb = gain,
        .harmonic = amount,
    };
  }

  double process(double input) {
    const double linear = linearContour.process(input);
    if (gainDb <= 0.0 || harmonic <= 0.0) {
      return linear;
    }
    const double emphasized = preEmphasis.process(input);
    const double drive = 1.0 + 2.0 * harmonic;
    const double nonlinear = std::tanh(drive * emphasized) / drive;
    const double restored = deEmphasis.process(nonlinear);
    const double coloured = colouredContour.process(restored);
    return linear + 0.05 * harmonic * (coloured - linear);
  }
};

struct StateVariableBand {
  double g{};
  double a1{};
  double a2{};
  double a3{};
  double state1{};
  double state2{};

  static StateVariableBand make(double sampleRate, double frequency, double q) {
    const double g = std::tan(std::numbers::pi * frequency / sampleRate);
    const double k = 1.0 / q;
    const double a1 = 1.0 / (1.0 + g * (g + k));
    return {
        .g = g,
        .a1 = a1,
        .a2 = g * a1,
        .a3 = g * g * a1,
    };
  }

  double process(double input, double amount) {
    const double v3 = input - state2;
    const double linearBand = a1 * state1 + a2 * v3;
    const double drive = 1.0 + 2.0 * amount;
    const double boundedBand = std::tanh(drive * linearBand) / drive;
    const double band = linearBand + amount * (boundedBand - linearBand);
    const double low = state2 + g * band;
    state1 = 2.0 * band - state1;
    state2 = 2.0 * low - state2;
    return band;
  }
};

struct StatefulExciter {
  PeakFilter linearContour;
  StateVariableBand linearBand;
  StateVariableBand nonlinearBand;
  double gainDb{};
  double harmonic{};

  static StatefulExciter make(double sampleRate, double frequency, double gain,
                              double amount) {
    return {
        .linearContour =
            PeakFilter::make(sampleRate, frequency, proportionalQ(gain), gain),
        .linearBand = StateVariableBand::make(sampleRate, frequency, 0.9),
        .nonlinearBand = StateVariableBand::make(sampleRate, frequency, 0.9),
        .gainDb = gain,
        .harmonic = amount,
    };
  }

  double process(double input) {
    const double linear = linearContour.process(input);
    if (gainDb <= 0.0) {
      return linear;
    }
    const double reference = linearBand.process(input, 0.0);
    const double coloured = nonlinearBand.process(input, harmonic);
    return linear + 0.08 * harmonic * (coloured - reference);
  }
};

double decibels(double value) {
  return 20.0 * std::log10(std::max(value, 1.0e-15));
}

double bandwidthOctaves(double sampleRate, double gainDb) {
  if (gainDb == 0.0) {
    return 0.0;
  }
  const auto filter =
      PeakFilter::make(sampleRate, kCentreHz, proportionalQ(gainDb), gainDb);
  const double target = gainDb * 0.5;
  double lower = kCentreHz;
  double upper = kCentreHz;
  constexpr int points = 8192;
  for (int index = 0; index < points; ++index) {
    const double fraction = static_cast<double>(index) / (points - 1);
    const double frequency = 20.0 * std::pow(1000.0, fraction);
    const double magnitude =
        decibels(std::abs(filter.response(sampleRate, frequency)));
    const bool inside =
        gainDb > 0.0 ? magnitude >= target : magnitude <= target;
    if (inside && frequency <= kCentreHz) {
      lower = frequency;
    }
    if (inside && frequency >= kCentreHz) {
      upper = frequency;
    }
  }
  return std::log2(upper / lower);
}

struct Spectrum {
  double fundamental{};
  double phaseDegrees{};
  double secondDbc{};
  double thirdDbc{};
};

struct ExtendedSpectrum {
  double fundamental{};
  double phaseDegrees{};
  double secondDbc{};
  double thirdDbc{};
  double foldedOddProxyDbc{};
  bool thirdObservable{true};
  bool finite{true};
};

double foldedFrequency(double frequency, double sampleRate) {
  double folded = std::fmod(frequency, sampleRate);
  if (folded > sampleRate * 0.5) {
    folded = sampleRate - folded;
  }
  return folded;
}

template <typename Processor>
ExtendedSpectrum measureExtended(Processor processor, double sampleRate,
                                 double frequency, double inputPeak) {
  constexpr std::size_t harmonics = 15;
  const std::size_t frames = static_cast<std::size_t>(sampleRate / 10.0);
  const std::size_t warmup = frames / 2;
  std::array<std::complex<double>, harmonics> bins{};
  std::array<std::complex<double>, harmonics> oscillators{};
  std::array<std::complex<double>, harmonics> rotations{};
  oscillators.fill(std::complex<double>{1.0, 0.0});
  for (std::size_t index = 0; index < harmonics; ++index) {
    const double angle = -2.0 * std::numbers::pi * frequency *
                         static_cast<double>(index + 1) / sampleRate;
    rotations[index] = std::exp(std::complex<double>{0.0, angle});
  }

  bool finite = true;
  for (std::size_t index = 0; index < warmup + frames; ++index) {
    const double phase = 2.0 * std::numbers::pi * frequency *
                         static_cast<double>(index) / sampleRate;
    const double processed = processor.process(inputPeak * std::sin(phase));
    finite = finite && std::isfinite(processed);
    if (index < warmup) {
      for (std::size_t harmonic = 0; harmonic < harmonics; ++harmonic) {
        oscillators[harmonic] *= rotations[harmonic];
      }
      continue;
    }
    for (std::size_t harmonic = 0; harmonic < harmonics; ++harmonic) {
      bins[harmonic] += processed * oscillators[harmonic];
      oscillators[harmonic] *= rotations[harmonic];
    }
  }

  const double scale = 2.0 / static_cast<double>(frames);
  const std::complex<double> fundamentalPhasor = bins[0] * scale;
  const double fundamental = std::abs(fundamentalPhasor);
  double foldedOddPower = 0.0;
  std::array<double, harmonics> countedFrequencies{};
  std::size_t counted = 0;
  const double thirdFolded = foldedFrequency(3.0 * frequency, sampleRate);
  const bool thirdObservable = std::abs(thirdFolded - frequency) > 1.0e-6;
  for (std::size_t harmonic = 3; harmonic <= harmonics; harmonic += 2) {
    const double original = frequency * static_cast<double>(harmonic);
    if (original <= sampleRate * 0.5) {
      continue;
    }
    const double folded = foldedFrequency(original, sampleRate);
    bool duplicate = false;
    for (std::size_t index = 0; index < counted; ++index) {
      duplicate =
          duplicate || std::abs(countedFrequencies[index] - folded) < 1.0e-6;
    }
    bool collidesWithInBandHarmonic = false;
    for (std::size_t inBand = 1; inBand <= harmonics; ++inBand) {
      const double inBandFrequency = frequency * static_cast<double>(inBand);
      if (inBandFrequency > sampleRate * 0.5) {
        break;
      }
      collidesWithInBandHarmonic = collidesWithInBandHarmonic ||
                                   std::abs(inBandFrequency - folded) < 1.0e-6;
    }
    if (!duplicate && !collidesWithInBandHarmonic) {
      countedFrequencies[counted++] = folded;
      const double amplitude = std::abs(bins[harmonic - 1]) * scale;
      foldedOddPower += amplitude * amplitude;
    }
  }

  return {
      .fundamental = fundamental,
      .phaseDegrees =
          std::arg(fundamentalPhasor / std::complex<double>{0.0, -inputPeak}) *
          180.0 / std::numbers::pi,
      .secondDbc = decibels(std::abs(bins[1]) * scale / fundamental),
      .thirdDbc = decibels(std::abs(bins[2]) * scale / fundamental),
      .foldedOddProxyDbc = decibels(std::sqrt(foldedOddPower) / fundamental),
      .thirdObservable = thirdObservable,
      .finite = finite && std::isfinite(fundamental) && fundamental > 0.0,
  };
}

struct ImdSpectrum {
  double productDbc{};
  bool finite{true};
};

template <typename Processor>
ImdSpectrum measureImd(Processor processor, double sampleRate,
                       double firstFrequency, double firstPeak,
                       double secondFrequency, double secondPeak,
                       std::array<double, 2> productFrequencies) {
  constexpr std::size_t binCount = 4;
  const std::array<double, binCount> frequencies{
      firstFrequency, secondFrequency, productFrequencies[0],
      productFrequencies[1]};
  const std::size_t frames = static_cast<std::size_t>(sampleRate / 10.0);
  const std::size_t warmup = frames / 2;
  std::array<std::complex<double>, binCount> bins{};
  std::array<std::complex<double>, binCount> oscillators{};
  std::array<std::complex<double>, binCount> rotations{};
  oscillators.fill(std::complex<double>{1.0, 0.0});
  for (std::size_t index = 0; index < binCount; ++index) {
    const double angle =
        -2.0 * std::numbers::pi * frequencies[index] / sampleRate;
    rotations[index] = std::exp(std::complex<double>{0.0, angle});
  }

  bool finite = true;
  for (std::size_t index = 0; index < warmup + frames; ++index) {
    const double time = static_cast<double>(index) / sampleRate;
    const double input =
        firstPeak * std::sin(2.0 * std::numbers::pi * firstFrequency * time) +
        secondPeak * std::sin(2.0 * std::numbers::pi * secondFrequency * time);
    const double processed = processor.process(input);
    finite = finite && std::isfinite(processed);
    if (index < warmup) {
      for (std::size_t bin = 0; bin < binCount; ++bin) {
        oscillators[bin] *= rotations[bin];
      }
      continue;
    }
    for (std::size_t bin = 0; bin < binCount; ++bin) {
      bins[bin] += processed * oscillators[bin];
      oscillators[bin] *= rotations[bin];
    }
  }

  const double scale = 2.0 / static_cast<double>(frames);
  const double reference =
      std::max(std::abs(bins[0]), std::abs(bins[1])) * scale;
  const double productPower =
      std::norm(bins[2] * scale) + std::norm(bins[3] * scale);
  return {
      .productDbc = decibels(std::sqrt(productPower) / reference),
      .finite = finite && std::isfinite(reference) && reference > 0.0,
  };
}

Spectrum measure(double sampleRate, double gainDb, double harmonic) {
  ResidualExciter processor{
      .filter = PeakFilter::make(sampleRate, kCentreHz, proportionalQ(gainDb),
                                 gainDb),
      .gainDb = gainDb,
      .harmonic = harmonic,
  };
  const std::size_t frames = static_cast<std::size_t>(sampleRate);
  const std::size_t warmup = frames / 4;
  std::array<std::complex<double>, 3> bins{};
  for (std::size_t index = 0; index < warmup + frames; ++index) {
    const double phase = 2.0 * std::numbers::pi * kCentreHz *
                         static_cast<double>(index) / sampleRate;
    const double output = processor.process(kInputPeak * std::sin(phase));
    if (index < warmup) {
      continue;
    }
    const double measuredIndex = static_cast<double>(index - warmup);
    for (std::size_t harmonicIndex = 0; harmonicIndex < bins.size();
         ++harmonicIndex) {
      const double angle = -2.0 * std::numbers::pi * kCentreHz *
                           static_cast<double>(harmonicIndex + 1) *
                           measuredIndex / sampleRate;
      bins[harmonicIndex] +=
          output * std::exp(std::complex<double>{0.0, angle});
    }
  }
  const double scale = 2.0 / static_cast<double>(frames);
  const std::complex<double> fundamentalPhasor = bins[0] * scale;
  const double fundamental = std::abs(fundamentalPhasor);
  return {
      .fundamental = fundamental,
      .phaseDegrees =
          std::arg(fundamentalPhasor / std::complex<double>{0.0, -kInputPeak}) *
          180.0 / std::numbers::pi,
      .secondDbc = decibels(std::abs(bins[1]) * scale / fundamental),
      .thirdDbc = decibels(std::abs(bins[2]) * scale / fundamental),
  };
}

bool renderComparison(const char* outputPath) {
  constexpr std::array<double, 6> rates{44100.0, 48000.0,  88200.0,
                                        96000.0, 176400.0, 192000.0};
  constexpr std::array<double, 4> amounts{0.0, 0.25, 0.5, 1.0};
  std::ofstream output{outputPath};
  if (!output) {
    return false;
  }
  output << "sample_rate,model,gain_db,harmonic,measured_centre_db,phase_deg,"
            "linear_bandwidth_octaves,h2_dbc,h3_dbc,neutral_null\n";

  bool valid = true;
  for (double sampleRate : rates) {
    ResidualExciter neutral{
        .filter = PeakFilter::make(sampleRate, kCentreHz, 0.7, 0.0),
        .gainDb = 0.0,
        .harmonic = 1.0,
    };
    double neutralNull = 0.0;
    for (int sample = 0; sample < 1024; ++sample) {
      const double input = sample == 0 ? 1.0 : 0.0;
      neutralNull =
          std::max(neutralNull, std::abs(neutral.process(input) - input));
    }
    valid = valid && neutralNull == 0.0;

    for (double gainDb : {-12.0, 12.0}) {
      const auto filter = PeakFilter::make(sampleRate, kCentreHz,
                                           proportionalQ(gainDb), gainDb);
      const auto response = filter.response(sampleRate, kCentreHz);
      const double centreDb = decibels(std::abs(response));
      const double phaseDegrees = std::arg(response) * 180.0 / std::numbers::pi;
      const double bandwidth = bandwidthOctaves(sampleRate, gainDb);
      const auto spectrum = measure(sampleRate, gainDb, 0.0);
      output << sampleRate << ",linear," << gainDb << ",0," << centreDb << ','
             << phaseDegrees << ',' << bandwidth << ',' << spectrum.secondDbc
             << ',' << spectrum.thirdDbc << ',' << neutralNull << '\n';
      valid = valid && std::isfinite(centreDb) && std::isfinite(bandwidth) &&
              std::abs(centreDb - gainDb) < 1.0e-9;
    }

    double previousThird = -301.0;
    for (double amount : amounts) {
      const auto spectrum = measure(sampleRate, 12.0, amount);
      output << sampleRate << ",residual,12," << amount << ','
             << decibels(spectrum.fundamental / kInputPeak) << ','
             << spectrum.phaseDegrees << ','
             << bandwidthOctaves(sampleRate, 12.0) << ',' << spectrum.secondDbc
             << ',' << spectrum.thirdDbc << ',' << neutralNull << '\n';
      valid = valid && std::isfinite(spectrum.fundamental) &&
              std::isfinite(spectrum.secondDbc) &&
              std::isfinite(spectrum.thirdDbc) &&
              spectrum.thirdDbc >= previousThird - 0.01;
      previousThird = spectrum.thirdDbc;
    }
    const auto cleanCut = measure(sampleRate, -12.0, 1.0);
    output << sampleRate << ",residual,-12,1,"
           << decibels(cleanCut.fundamental / kInputPeak) << ','
           << cleanCut.phaseDegrees << ','
           << bandwidthOctaves(sampleRate, -12.0) << ',' << cleanCut.secondDbc
           << ',' << cleanCut.thirdDbc << ',' << neutralNull << '\n';
    const auto drivenBoost = measure(sampleRate, 12.0, 1.0);
    valid = valid && cleanCut.thirdDbc < -180.0 && drivenBoost.thirdDbc > -80.0;
  }
  return valid && output.good();
}

struct CandidateResult {
  bool valid{true};
  bool advances{true};
  double maximumGainErrorDb{};
  double maximumPhaseErrorDegrees{};
  double canonicalThirdDbc{-300.0};
  double worstFoldedOddProxyDbc{-300.0};
  double worstImdDbc{-300.0};
  double maximumImpulsePeak{};
  double maximumOverloadPeak{};
  double maximumStepSteadyError{};
  double worstRecoveryDbfs{-300.0};
};

template <typename Factory>
CandidateResult renderCandidate(const char* tonePath, const char* imdPath,
                                Factory makeProcessor) {
  constexpr std::array<double, 6> rates{44100.0, 48000.0,  88200.0,
                                        96000.0, 176400.0, 192000.0};
  constexpr std::array<double, 5> centres{80.0, 400.0, 1000.0, 4000.0, 12000.0};
  constexpr std::array<double, 4> levels{0.1, 0.25, 0.5, 0.9};
  constexpr std::array<double, 4> amounts{0.0, 0.25, 0.5, 1.0};
  std::ofstream tones{tonePath};
  std::ofstream imd{imdPath};
  CandidateResult result;
  if (!tones || !imd) {
    result.valid = false;
    result.advances = false;
    return result;
  }
  tones << "sample_rate,sweep,centre_hz,input_peak,gain_db,harmonic,"
           "measured_gain_db,gain_error_db,phase_error_deg,h2_dbc,h3_dbc,"
           "h3_observable,folded_odd_proxy_dbc,finite,advance_gate\n";
  imd << "sample_rate,fixture,harmonic,product_dbc,finite\n";

  for (double sampleRate : rates) {
    for (double centre : centres) {
      auto neutral = makeProcessor(sampleRate, centre, 0.0, 1.0);
      double neutralNull = 0.0;
      for (int sample = 0; sample < 1024; ++sample) {
        const double input = sample == 0 ? 1.0 : 0.0;
        neutralNull =
            std::max(neutralNull, std::abs(neutral.process(input) - input));
      }
      result.valid = result.valid && neutralNull == 0.0;

      for (double level : levels) {
        const auto spectrum =
            measureExtended(makeProcessor(sampleRate, centre, 12.0, 1.0),
                            sampleRate, centre, level);
        const double measuredGainDb = decibels(spectrum.fundamental / level);
        const double gainErrorDb = measuredGainDb - 12.0;
        const double phaseErrorDegrees = spectrum.phaseDegrees;
        const bool gatedLevel = level <= 0.5;
        const bool rowAdvances = spectrum.finite &&
                                 std::abs(phaseErrorDegrees) <= 1.0 &&
                                 (!gatedLevel || std::abs(gainErrorDb) <= 0.5);
        tones << sampleRate << ",level," << centre << ',' << level << ",12,1,"
              << measuredGainDb << ',' << gainErrorDb << ','
              << phaseErrorDegrees << ',' << spectrum.secondDbc << ','
              << spectrum.thirdDbc << ',' << spectrum.thirdObservable << ','
              << spectrum.foldedOddProxyDbc << ',' << spectrum.finite << ','
              << rowAdvances << '\n';
        result.valid = result.valid && spectrum.finite;
        result.advances = result.advances && rowAdvances;
        if (gatedLevel) {
          result.maximumGainErrorDb =
              std::max(result.maximumGainErrorDb, std::abs(gainErrorDb));
        }
        result.maximumPhaseErrorDegrees = std::max(
            result.maximumPhaseErrorDegrees, std::abs(phaseErrorDegrees));
        result.worstFoldedOddProxyDbc =
            std::max(result.worstFoldedOddProxyDbc, spectrum.foldedOddProxyDbc);
      }

      const auto cut =
          measureExtended(makeProcessor(sampleRate, centre, -12.0, 1.0),
                          sampleRate, centre, 0.5);
      const double cutGainDb = decibels(cut.fundamental / 0.5);
      const bool cutAdvances =
          cut.finite && (!cut.thirdObservable || cut.thirdDbc < -140.0);
      tones << sampleRate << ",cut," << centre << ",0.5,-12,1," << cutGainDb
            << ',' << cutGainDb + 12.0 << ',' << cut.phaseDegrees << ','
            << cut.secondDbc << ',' << cut.thirdDbc << ','
            << cut.thirdObservable << ',' << cut.foldedOddProxyDbc << ','
            << cut.finite << ',' << cutAdvances << '\n';
      result.valid = result.valid && cut.finite;
      result.advances = result.advances && cutAdvances;
    }

    double previousThirdDbc = -301.0;
    for (double amount : amounts) {
      const auto spectrum =
          measureExtended(makeProcessor(sampleRate, kCentreHz, 12.0, amount),
                          sampleRate, kCentreHz, kInputPeak);
      const double measuredGainDb = decibels(spectrum.fundamental / kInputPeak);
      const bool monotonic = spectrum.thirdDbc >= previousThirdDbc - 0.01;
      const bool canonicalRange =
          amount != 1.0 ||
          (spectrum.thirdDbc >= -80.0 && spectrum.thirdDbc <= -20.0);
      const bool rowAdvances = spectrum.finite && monotonic && canonicalRange;
      tones << sampleRate << ",macro," << kCentreHz << ',' << kInputPeak
            << ",12," << amount << ',' << measuredGainDb << ','
            << measuredGainDb - 12.0 << ',' << spectrum.phaseDegrees << ','
            << spectrum.secondDbc << ',' << spectrum.thirdDbc << ','
            << spectrum.thirdObservable << ',' << spectrum.foldedOddProxyDbc
            << ',' << spectrum.finite << ',' << rowAdvances << '\n';
      result.valid = result.valid && spectrum.finite;
      result.advances = result.advances && rowAdvances;
      previousThirdDbc = spectrum.thirdDbc;
      if (amount == 1.0) {
        result.canonicalThirdDbc =
            std::max(result.canonicalThirdDbc, spectrum.thirdDbc);
      }
    }

    for (double amount : {0.0, 1.0}) {
      const auto smpte =
          measureImd(makeProcessor(sampleRate, 7000.0, 12.0, amount),
                     sampleRate, 60.0, 0.32, 7000.0, 0.08, {6880.0, 7120.0});
      imd << sampleRate << ",smpte," << amount << ',' << smpte.productDbc << ','
          << smpte.finite << '\n';
      const auto ccif = measureImd(
          makeProcessor(sampleRate, 19500.0, 12.0, amount), sampleRate, 19000.0,
          0.2, 20000.0, 0.2, {18000.0, 21000.0});
      imd << sampleRate << ",ccif," << amount << ',' << ccif.productDbc << ','
          << ccif.finite << '\n';
      result.valid = result.valid && smpte.finite && ccif.finite;
      if (amount == 1.0) {
        result.worstImdDbc =
            std::max({result.worstImdDbc, smpte.productDbc, ccif.productDbc});
      }
    }
  }

  result.valid = result.valid && tones.good() && imd.good();
  result.advances = result.advances && result.valid;
  return result;
}

CandidateResult renderPreEmphasis(const char* tonePath, const char* imdPath) {
  return renderCandidate(
      tonePath, imdPath,
      [](double sampleRate, double frequency, double gain, double amount) {
        return PreEmphasisExciter::make(sampleRate, frequency, gain, amount);
      });
}

CandidateResult renderStateful(const char* tonePath, const char* imdPath,
                               const char* dynamicsPath) {
  CandidateResult result = renderCandidate(
      tonePath, imdPath,
      [](double sampleRate, double frequency, double gain, double amount) {
        return StatefulExciter::make(sampleRate, frequency, gain, amount);
      });
  constexpr std::array<double, 6> rates{44100.0, 48000.0,  88200.0,
                                        96000.0, 176400.0, 192000.0};
  constexpr std::array<double, 5> centres{80.0, 400.0, 1000.0, 4000.0, 12000.0};
  std::ofstream dynamics{dynamicsPath};
  if (!dynamics) {
    result.valid = false;
    result.advances = false;
    return result;
  }
  dynamics << "sample_rate,test,centre_hz,peak,steady_error,tail_dbfs,finite,"
              "advance_gate\n";

  for (double sampleRate : rates) {
    for (double centre : centres) {
      auto neutral = StatefulExciter::make(sampleRate, centre, 0.0, 1.0);
      double neutralPeak = 0.0;
      for (int sample = 0; sample < 1024; ++sample) {
        neutralPeak = std::max(neutralPeak, std::abs(neutral.process(0.0)));
      }
      const bool neutralAdvances = neutralPeak == 0.0;
      dynamics << sampleRate << ",neutral," << centre << ',' << neutralPeak
               << ",0,-300," << neutralAdvances << ',' << neutralAdvances
               << '\n';

      auto impulse = StatefulExciter::make(sampleRate, centre, 12.0, 1.0);
      const std::size_t recoveryFrames =
          static_cast<std::size_t>(sampleRate * 2.0);
      const std::size_t tailFrames = static_cast<std::size_t>(sampleRate * 0.1);
      double impulsePeak = 0.0;
      double impulseTail = 0.0;
      bool impulseFinite = true;
      for (std::size_t sample = 0; sample < recoveryFrames; ++sample) {
        const double output = impulse.process(sample == 0 ? 1.0 : 0.0);
        impulseFinite = impulseFinite && std::isfinite(output);
        impulsePeak = std::max(impulsePeak, std::abs(output));
        if (sample >= recoveryFrames - tailFrames) {
          impulseTail = std::max(impulseTail, std::abs(output));
        }
      }
      const bool impulseAdvances = impulseFinite && impulseTail < 1.0e-6;
      dynamics << sampleRate << ",impulse," << centre << ',' << impulsePeak
               << ",0," << decibels(impulseTail) << ',' << impulseFinite << ','
               << impulseAdvances << '\n';

      auto step = StatefulExciter::make(sampleRate, centre, 12.0, 1.0);
      const std::size_t stepFrames = static_cast<std::size_t>(sampleRate * 0.5);
      double stepPeak = 0.0;
      double stepSteadyError = 0.0;
      double stepTail = 0.0;
      bool stepFinite = true;
      for (std::size_t sample = 0; sample < stepFrames + recoveryFrames;
           ++sample) {
        const double input = sample < stepFrames ? 0.5 : 0.0;
        const double output = step.process(input);
        stepFinite = stepFinite && std::isfinite(output);
        stepPeak = std::max(stepPeak, std::abs(output));
        if (sample >= stepFrames - tailFrames && sample < stepFrames) {
          stepSteadyError = std::max(stepSteadyError, std::abs(output - input));
        }
        if (sample >= stepFrames + recoveryFrames - tailFrames) {
          stepTail = std::max(stepTail, std::abs(output));
        }
      }
      dynamics << sampleRate << ",step," << centre << ',' << stepPeak << ','
               << stepSteadyError << ',' << decibels(stepTail) << ','
               << stepFinite << ',' << stepFinite << '\n';

      auto overload = StatefulExciter::make(sampleRate, centre, 12.0, 1.0);
      const std::size_t drivenFrames =
          static_cast<std::size_t>(sampleRate * 0.1);
      double overloadPeak = 0.0;
      double overloadTail = 0.0;
      bool overloadFinite = true;
      for (std::size_t sample = 0; sample < drivenFrames + recoveryFrames;
           ++sample) {
        const double input =
            sample < drivenFrames
                ? 0.9 * std::sin(2.0 * std::numbers::pi * centre *
                                 static_cast<double>(sample) / sampleRate)
                : 0.0;
        const double output = overload.process(input);
        overloadFinite = overloadFinite && std::isfinite(output);
        overloadPeak = std::max(overloadPeak, std::abs(output));
        if (sample >= drivenFrames + recoveryFrames - tailFrames) {
          overloadTail = std::max(overloadTail, std::abs(output));
        }
      }
      const bool overloadAdvances = overloadFinite && overloadTail < 1.0e-6;
      dynamics << sampleRate << ",overload," << centre << ',' << overloadPeak
               << ",0," << decibels(overloadTail) << ',' << overloadFinite
               << ',' << overloadAdvances << '\n';

      result.valid = result.valid && neutralAdvances && impulseFinite &&
                     stepFinite && overloadFinite;
      result.advances = result.advances && neutralAdvances && impulseAdvances &&
                        stepFinite && overloadAdvances;
      result.maximumImpulsePeak =
          std::max(result.maximumImpulsePeak, impulsePeak);
      result.maximumOverloadPeak =
          std::max(result.maximumOverloadPeak, overloadPeak);
      result.maximumStepSteadyError =
          std::max(result.maximumStepSteadyError, stepSteadyError);
      result.worstRecoveryDbfs =
          std::max({result.worstRecoveryDbfs, decibels(impulseTail),
                    decibels(stepTail), decibels(overloadTail)});
    }
  }

  result.valid = result.valid && dynamics.good();
  result.advances = result.advances && result.valid;
  return result;
}

bool renderProductReport(const char* outputPath) {
  constexpr std::array<double, 6> rates{44100.0, 48000.0,  88200.0,
                                        96000.0, 176400.0, 192000.0};
  std::ofstream output{outputPath};
  if (!output) {
    return false;
  }
  output << "sample_rate,fixture,rms_gain_db,peak,harmonic_activity,latency,"
            "finite\n";
  bool valid = true;
  for (double sampleRate : rates) {
    for (int fixture = 0; fixture < 2; ++fixture) {
      aste::harmonic::Parameters parameters;
      parameters.foundationGainDb = fixture == 0 ? 4.0F : 6.0F;
      parameters.bodyGainDb = fixture == 0 ? 3.0F : -4.0F;
      parameters.presenceGainDb = fixture == 0 ? 2.0F : 3.0F;
      parameters.airGainDb = fixture == 0 ? 1.0F : -2.0F;
      parameters.harmonic = fixture == 0 ? 0.75F : 1.0F;
      parameters.outputDb = fixture == 0 ? -1.0F : -2.0F;
      aste::harmonic::Processor processor;
      processor.prepare(sampleRate, parameters);
      constexpr std::size_t blockSize = 127;
      std::array<float, blockSize> left{};
      std::array<float, blockSize> right{};
      const std::size_t frames = static_cast<std::size_t>(sampleRate);
      double inputPower = 0.0;
      double outputPower = 0.0;
      float peak = 0.0F;
      float activity = 0.0F;
      bool finite = true;
      for (std::size_t offset = 0; offset < frames; offset += blockSize) {
        const std::size_t count = std::min(blockSize, frames - offset);
        for (std::size_t sample = 0; sample < count; ++sample) {
          const double time = static_cast<double>(offset + sample) / sampleRate;
          const float signal = static_cast<float>(
              0.22 * std::sin(2.0 * std::numbers::pi * 70.0 * time) +
              0.16 * std::sin(2.0 * std::numbers::pi * 430.0 * time) +
              0.11 * std::sin(2.0 * std::numbers::pi * 2700.0 * time) +
              0.07 * std::sin(2.0 * std::numbers::pi * 11000.0 * time));
          left[sample] = signal;
          right[sample] = signal * 0.93F;
          inputPower += static_cast<double>(left[sample]) * left[sample] +
                        static_cast<double>(right[sample]) * right[sample];
        }
        processor.process(left.data(), right.data(), count, parameters);
        const auto meters = processor.meters();
        activity = std::max(activity, meters.harmonicActivity);
        for (std::size_t sample = 0; sample < count; ++sample) {
          finite = finite && std::isfinite(left[sample]) &&
                   std::isfinite(right[sample]);
          peak =
              std::max({peak, std::abs(left[sample]), std::abs(right[sample])});
          outputPower += static_cast<double>(left[sample]) * left[sample] +
                         static_cast<double>(right[sample]) * right[sample];
        }
      }
      const double gainDb = 10.0 * std::log10(std::max(outputPower, 1.0e-30) /
                                              std::max(inputPower, 1.0e-30));
      output << sampleRate << ',' << (fixture == 0 ? "broad" : "sculpt") << ','
             << gainDb << ',' << peak << ',' << activity << ','
             << processor.latencySamples() << ',' << finite << '\n';
      valid = valid && finite && activity > 0.0F &&
              processor.latencySamples() == 0U && peak <= 16.0F;
    }
  }
  return valid && output.good();
}

bool benchmarkProduct() {
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t blockSize = 128;
  constexpr std::size_t seconds = 30;
  constexpr std::size_t runs = 5;
  constexpr std::size_t blocks =
      static_cast<std::size_t>(sampleRate) * seconds / blockSize;
  std::array<double, runs> percentages{};
  double checksum = 0.0;
  for (std::size_t run = 0; run < runs; ++run) {
    aste::harmonic::Parameters parameters;
    parameters.foundationGainDb = 6.0F;
    parameters.bodyGainDb = 4.0F;
    parameters.presenceGainDb = 5.0F;
    parameters.airGainDb = 3.0F;
    parameters.harmonic = 1.0F;
    aste::harmonic::Processor processor;
    processor.prepare(sampleRate, parameters);
    std::array<float, blockSize> left{};
    std::array<float, blockSize> right{};
    std::size_t offset = 0;
    const auto start = std::chrono::steady_clock::now();
    for (std::size_t block = 0; block < blocks; ++block) {
      parameters.foundationGainDb = block % 2U == 0U ? 6.0F : -4.0F;
      parameters.bodyFrequencyHz = block % 2U == 0U ? 250.0F : 700.0F;
      parameters.presenceGainDb = block % 2U == 0U ? 5.0F : -3.0F;
      parameters.airFrequencyHz = block % 2U == 0U ? 9000.0F : 17000.0F;
      parameters.harmonic = block % 2U == 0U ? 1.0F : 0.1F;
      for (std::size_t sample = 0; sample < blockSize; ++sample) {
        left[sample] =
            0.5F * std::sin(static_cast<float>(offset + sample) * 0.071F);
        right[sample] =
            0.5F * std::sin(static_cast<float>(offset + sample) * 0.083F);
      }
      processor.process(left.data(), right.data(), blockSize, parameters);
      checksum += std::abs(left[block % blockSize]) +
                  std::abs(right[block % blockSize]);
      offset += blockSize;
    }
    const double elapsed =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
            .count();
    percentages[run] = 100.0 * elapsed / static_cast<double>(seconds);
  }
  std::sort(percentages.begin(), percentages.end());
  const double median = percentages[runs / 2U];
  std::cout << "{\"sample_rate\":48000,\"block_size\":128,\"seconds\":30,"
               "\"runs\":5,\"median_core_percent\":"
            << median << ",\"minimum_core_percent\":" << percentages.front()
            << ",\"maximum_core_percent\":" << percentages.back()
            << ",\"checksum\":" << checksum << "}\n";
  return std::isfinite(checksum) && checksum > 0.0 && median < 1.0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 3 && std::string_view{argv[1]} == "--compare") {
    if (!renderComparison(argv[2])) {
      std::cerr << "harmonic_lab: comparison failed\n";
      return 1;
    }
    std::cout << "harmonic_lab: comparison ok\n";
    return 0;
  }
  if (argc == 4 && std::string_view{argv[1]} == "--preemphasis") {
    const CandidateResult result = renderPreEmphasis(argv[2], argv[3]);
    std::cout << "{\"candidate\":\"preemphasis\",\"valid\":" << std::boolalpha
              << result.valid << ",\"advances\":" << result.advances
              << ",\"max_gain_error_db\":" << result.maximumGainErrorDb
              << ",\"max_phase_error_deg\":" << result.maximumPhaseErrorDegrees
              << ",\"canonical_h3_dbc\":" << result.canonicalThirdDbc
              << ",\"worst_folded_odd_proxy_dbc\":"
              << result.worstFoldedOddProxyDbc
              << ",\"worst_imd_dbc\":" << result.worstImdDbc << "}\n";
    return result.valid ? 0 : 1;
  }
  if (argc == 5 && std::string_view{argv[1]} == "--stateful") {
    const CandidateResult result = renderStateful(argv[2], argv[3], argv[4]);
    std::cout << "{\"candidate\":\"stateful\",\"valid\":" << std::boolalpha
              << result.valid << ",\"advances\":" << result.advances
              << ",\"max_gain_error_db\":" << result.maximumGainErrorDb
              << ",\"max_phase_error_deg\":" << result.maximumPhaseErrorDegrees
              << ",\"canonical_h3_dbc\":" << result.canonicalThirdDbc
              << ",\"worst_folded_odd_proxy_dbc\":"
              << result.worstFoldedOddProxyDbc
              << ",\"worst_imd_dbc\":" << result.worstImdDbc
              << ",\"max_impulse_peak\":" << result.maximumImpulsePeak
              << ",\"max_overload_peak\":" << result.maximumOverloadPeak
              << ",\"max_step_steady_error\":" << result.maximumStepSteadyError
              << ",\"worst_recovery_dbfs\":" << result.worstRecoveryDbfs
              << "}\n";
    return result.valid ? 0 : 1;
  }
  if (argc == 3 && std::string_view{argv[1]} == "--product-report") {
    if (!renderProductReport(argv[2])) {
      std::cerr << "harmonic_lab: product report failed\n";
      return 1;
    }
    std::cout << "harmonic_lab: product report ok\n";
    return 0;
  }
  if (argc == 2 && std::string_view{argv[1]} == "--product-benchmark") {
    return benchmarkProduct() ? 0 : 1;
  }
  std::cerr << "usage: harmonic_lab --compare OUTPUT.csv\n"
               "       harmonic_lab --preemphasis TONE.csv IMD.csv\n"
               "       harmonic_lab --stateful TONE.csv IMD.csv DYNAMICS.csv\n"
               "       harmonic_lab --product-report OUTPUT.csv\n"
               "       harmonic_lab --product-benchmark\n";
  return 2;
}
