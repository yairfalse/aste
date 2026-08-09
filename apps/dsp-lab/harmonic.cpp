#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <numbers>
#include <string_view>

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

}  // namespace

int main(int argc, char** argv) {
  if (argc != 3 || std::string_view{argv[1]} != "--compare") {
    std::cerr << "usage: harmonic_lab --compare OUTPUT.csv\n";
    return 2;
  }
  if (!renderComparison(argv[2])) {
    std::cerr << "harmonic_lab: comparison failed\n";
    return 1;
  }
  std::cout << "harmonic_lab: comparison ok\n";
  return 0;
}
