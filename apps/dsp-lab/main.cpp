#include "density_processor.hpp"
#include "decimal_parse.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

constexpr double kPi = 3.14159265358979323846;

float coefficient(double seconds, double sampleRate) {
  return static_cast<float>(std::exp(-1.0 / (seconds * sampleRate)));
}

float reduction(float envelope, const aste::density::DensityMapping& mapping,
                float crush) {
  const float levelDb =
      8.6858896380650366F * std::log(std::max(envelope, 1.0e-12F));
  const float threshold = mapping.thresholdDb + 12.0F * (1.0F - crush);
  const float ratio = 1.0F + crush * (mapping.ratio - 1.0F);
  return std::max(0.0F, levelDb - threshold) * (1.0F - 1.0F / ratio);
}

struct DetectorResearch {
  explicit DetectorResearch(double rate = 48000.0) : sampleRate{rate} {
    parameters.crush = 1.0F;
    parameters.attackMs = 0.1F;
    parameters.releaseMs = 180.0F;
    parameters.density = 0.7F;
    parameters.blend = 0.0F;
    parameters.detectorHpfHz = 20.0F;
    parameters.protection = false;
    current.prepare(sampleRate, parameters);
    mapping = aste::density::mapDensity(parameters.density);
    hpfCoefficient = static_cast<float>(
        std::exp(-2.0 * kPi * parameters.detectorHpfHz / sampleRate));
    attackCoefficient = coefficient(parameters.attackMs * 0.001, sampleRate);
    rmsCoefficient = coefficient(0.010, sampleRate);
    fastReleaseCoefficient = coefficient(0.060, sampleRate);
    slowAttackCoefficient = coefficient(0.020, sampleRate);
    slowReleaseCoefficient = coefficient(0.600, sampleRate);
    programmeAttackCoefficient = coefficient(0.100, sampleRate);
    programmeReleaseCoefficient = coefficient(0.600, sampleRate);
  }

  std::array<float, 6> process(float input) {
    float productSample = input;
    current.process(&productSample, nullptr, 1, parameters);
    const float currentReduction = current.meters().gainReductionDb;

    const float hpf = hpfCoefficient * (previousHpf + input - previousInput);
    previousInput = input;
    previousHpf = hpf;
    const float instantaneous = std::abs(hpf);
    meanSquare = instantaneous * instantaneous +
                 rmsCoefficient * (meanSquare - instantaneous * instantaneous);
    constexpr float peakInfluence = 0.15F;
    const float detector =
        std::sqrt((1.0F - peakInfluence) * 2.0F * meanSquare +
                  peakInfluence * instantaneous * instantaneous);
    if (detector > rmsEnvelope) {
      rmsEnvelope = detector + attackCoefficient * (rmsEnvelope - detector);
    } else {
      const float programme = 1.0F + (mapping.releaseCurve - 1.0F) *
                                         std::clamp(rmsEnvelope, 0.0F, 1.0F);
      const float releaseCoefficient =
          coefficient(parameters.releaseMs * programme * 0.001, sampleRate);
      rmsEnvelope = detector + releaseCoefficient * (rmsEnvelope - detector);
    }

    dualFast =
        instantaneous > dualFast
            ? instantaneous + attackCoefficient * (dualFast - instantaneous)
            : instantaneous +
                  fastReleaseCoefficient * (dualFast - instantaneous);
    dualSlow =
        instantaneous > dualSlow
            ? instantaneous + slowAttackCoefficient * (dualSlow - instantaneous)
            : instantaneous +
                  slowReleaseCoefficient * (dualSlow - instantaneous);

    programmeMemory =
        instantaneous > programmeMemory
            ? instantaneous +
                  programmeAttackCoefficient * (programmeMemory - instantaneous)
            : instantaneous + programmeReleaseCoefficient *
                                  (programmeMemory - instantaneous);
    if (instantaneous > programmeEnvelope) {
      programmeEnvelope =
          instantaneous +
          attackCoefficient * (programmeEnvelope - instantaneous);
    } else {
      const float releaseSeconds =
          0.060F + 0.540F * std::clamp(programmeMemory * 2.0F, 0.0F, 1.0F);
      const float releaseCoefficient = coefficient(releaseSeconds, sampleRate);
      programmeEnvelope =
          instantaneous +
          releaseCoefficient * (programmeEnvelope - instantaneous);
    }
    const float hybridBodyReduction =
        reduction(std::sqrt(2.0F * meanSquare), mapping, parameters.crush);
    const float hybridPeakReduction =
        reduction(dualFast, mapping, parameters.crush);
    constexpr float hybridPeakInfluence = 0.35F;
    const float hybridReduction =
        hybridBodyReduction +
        hybridPeakInfluence *
            std::max(0.0F, hybridPeakReduction - hybridBodyReduction);

    constexpr float feedbackCalibration = 3.65F;
    float solvedGain = feedbackGain;
    float solvedEnvelope = feedbackEnvelope;
    float feedbackReduction{};
    for (int iteration = 0; iteration < 6; ++iteration) {
      const float feedbackDetector =
          instantaneous * feedbackCalibration * solvedGain;
      if (feedbackDetector > feedbackEnvelope) {
        solvedEnvelope =
            feedbackDetector +
            attackCoefficient * (feedbackEnvelope - feedbackDetector);
      } else {
        const float programme =
            1.0F + (mapping.releaseCurve - 1.0F) *
                       std::clamp(feedbackEnvelope, 0.0F, 1.0F);
        const float releaseCoefficient =
            coefficient(parameters.releaseMs * programme * 0.001, sampleRate);
        solvedEnvelope =
            feedbackDetector +
            releaseCoefficient * (feedbackEnvelope - feedbackDetector);
      }
      feedbackReduction = reduction(solvedEnvelope, mapping, parameters.crush);
      const float targetGain =
          std::exp(-0.11512925464970229F * feedbackReduction);
      solvedGain = 0.5F * (solvedGain + targetGain);
    }
    feedbackEnvelope = solvedEnvelope;
    feedbackGain = std::exp(-0.11512925464970229F * feedbackReduction);
    maxFeedbackResidual =
        std::max(maxFeedbackResidual, std::abs(solvedGain - feedbackGain));
    return {currentReduction,
            reduction(rmsEnvelope, mapping, parameters.crush),
            reduction(std::max(dualFast, dualSlow), mapping, parameters.crush),
            reduction(programmeEnvelope, mapping, parameters.crush),
            hybridReduction,
            feedbackReduction};
  }

  double sampleRate;
  aste::density::Parameters parameters{};
  aste::density::Processor current{};
  aste::density::DensityMapping mapping{};
  float hpfCoefficient{};
  float attackCoefficient{};
  float rmsCoefficient{};
  float fastReleaseCoefficient{};
  float slowAttackCoefficient{};
  float slowReleaseCoefficient{};
  float programmeAttackCoefficient{};
  float programmeReleaseCoefficient{};
  float previousInput{};
  float previousHpf{};
  float meanSquare{};
  float rmsEnvelope{};
  float dualFast{};
  float dualSlow{};
  float programmeMemory{};
  float programmeEnvelope{};
  float feedbackEnvelope{};
  float feedbackGain{1.0F};
  float maxFeedbackResidual{};
};

int detectorComparison(const std::string& outputPath) {
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t totalFrames = 192000;
  constexpr std::size_t sustainStart = 9600;
  constexpr std::size_t sustainEnd = 28800;
  constexpr std::size_t impulseSample = 67200;
  constexpr std::size_t burstStart = 105600;
  constexpr std::size_t burstEnd = 106080;
  std::ofstream output(outputPath);
  if (!output) {
    std::cerr << "cannot open output: " << outputPath << '\n';
    return 1;
  }

  DetectorResearch detectors{sampleRate};
  double currentSustainSum{};
  double candidateSustainSum{};
  double dualSustainSum{};
  double programmeSustainSum{};
  double hybridSustainSum{};
  double feedbackSustainSum{};
  std::size_t sustainCount{};
  float currentImpulseMax{};
  float candidateImpulseMax{};
  float dualImpulseMax{};
  float programmeImpulseMax{};
  float hybridImpulseMax{};
  float feedbackImpulseMax{};
  float currentBurstMax{};
  float candidateBurstMax{};
  float dualBurstMax{};
  float programmeBurstMax{};
  float hybridBurstMax{};
  float feedbackBurstMax{};
  std::size_t currentReleaseSample = totalFrames;
  std::size_t candidateReleaseSample = totalFrames;
  std::size_t dualReleaseSample = totalFrames;
  std::size_t programmeReleaseSample = totalFrames;
  std::size_t hybridReleaseSample = totalFrames;
  std::size_t feedbackReleaseSample = totalFrames;
  std::array<std::size_t, 6> sustainReleaseSamples{totalFrames, totalFrames,
                                                   totalFrames, totalFrames,
                                                   totalFrames, totalFrames};
  std::array<float, 6> beforeImpulse{};
  std::array<float, 6> beforeBurst{};
  float maxFeedbackResidual{};
  output << "sample,input,current_peak_gr_db,rms_peak_gr_db,dual_time_gr_db,"
            "programme_gr_db,hybrid_feed_forward_gr_db,feedback_gr_db\n";

  for (std::size_t i = 0; i < totalFrames; ++i) {
    if (i == impulseSample || i == burstStart) {
      detectors = DetectorResearch{sampleRate};
    }
    float input{};
    if (i >= sustainStart && i < sustainEnd) {
      input = 0.5F *
              static_cast<float>(std::sin(2.0 * kPi * 997.0 * i / sampleRate));
    } else if (i == impulseSample) {
      input = 1.0F;
    } else if (i >= burstStart && i < burstEnd) {
      input = 0.8F *
              static_cast<float>(std::sin(2.0 * kPi * 997.0 * i / sampleRate));
    }

    const auto [currentReduction, candidateReduction, dualReduction,
                programmeReduction, hybridReduction, feedbackReduction] =
        detectors.process(input);
    maxFeedbackResidual =
        std::max(maxFeedbackResidual, detectors.maxFeedbackResidual);
    const std::array<float, 6> reductions{currentReduction, candidateReduction,
                                          dualReduction,    programmeReduction,
                                          hybridReduction,  feedbackReduction};
    output << i << ',' << input << ',' << currentReduction << ','
           << candidateReduction << ',' << dualReduction << ','
           << programmeReduction << ',' << hybridReduction << ','
           << feedbackReduction << '\n';

    if (i + 1 == impulseSample) {
      beforeImpulse = {currentReduction, candidateReduction,
                       dualReduction,    programmeReduction,
                       hybridReduction,  feedbackReduction};
    }
    if (i + 1 == burstStart) {
      beforeBurst = {currentReduction,   candidateReduction, dualReduction,
                     programmeReduction, hybridReduction,    feedbackReduction};
    }
    if (i >= sustainEnd && i < impulseSample) {
      for (std::size_t detector = 0; detector < reductions.size(); ++detector) {
        if (sustainReleaseSamples[detector] == totalFrames &&
            reductions[detector] < 1.0F) {
          sustainReleaseSamples[detector] = i;
        }
      }
    }

    if (i >= sustainEnd - 4800 && i < sustainEnd) {
      currentSustainSum += currentReduction;
      candidateSustainSum += candidateReduction;
      dualSustainSum += dualReduction;
      programmeSustainSum += programmeReduction;
      hybridSustainSum += hybridReduction;
      feedbackSustainSum += feedbackReduction;
      ++sustainCount;
    }
    if (i >= impulseSample && i < impulseSample + 480) {
      currentImpulseMax = std::max(currentImpulseMax, currentReduction);
      candidateImpulseMax = std::max(candidateImpulseMax, candidateReduction);
      dualImpulseMax = std::max(dualImpulseMax, dualReduction);
      programmeImpulseMax = std::max(programmeImpulseMax, programmeReduction);
      hybridImpulseMax = std::max(hybridImpulseMax, hybridReduction);
      feedbackImpulseMax = std::max(feedbackImpulseMax, feedbackReduction);
    }
    if (i >= burstStart && i < burstEnd + 2400) {
      currentBurstMax = std::max(currentBurstMax, currentReduction);
      candidateBurstMax = std::max(candidateBurstMax, candidateReduction);
      dualBurstMax = std::max(dualBurstMax, dualReduction);
      programmeBurstMax = std::max(programmeBurstMax, programmeReduction);
      hybridBurstMax = std::max(hybridBurstMax, hybridReduction);
      feedbackBurstMax = std::max(feedbackBurstMax, feedbackReduction);
    }
    if (i >= burstEnd && currentReleaseSample == totalFrames &&
        currentReduction < 1.0F) {
      currentReleaseSample = i;
    }
    if (i >= burstEnd && candidateReleaseSample == totalFrames &&
        candidateReduction < 1.0F) {
      candidateReleaseSample = i;
    }
    if (i >= burstEnd && dualReleaseSample == totalFrames &&
        dualReduction < 1.0F) {
      dualReleaseSample = i;
    }
    if (i >= burstEnd && programmeReleaseSample == totalFrames &&
        programmeReduction < 1.0F) {
      programmeReleaseSample = i;
    }
    if (i >= burstEnd && hybridReleaseSample == totalFrames &&
        hybridReduction < 1.0F) {
      hybridReleaseSample = i;
    }
    if (i >= burstEnd && feedbackReleaseSample == totalFrames &&
        feedbackReduction < 1.0F) {
      feedbackReleaseSample = i;
    }
  }

  if (!output) {
    std::cerr << "cannot write output: " << outputPath << '\n';
    return 1;
  }

  const double currentSustain = currentSustainSum / sustainCount;
  const double candidateSustain = candidateSustainSum / sustainCount;
  const double dualSustain = dualSustainSum / sustainCount;
  const double programmeSustain = programmeSustainSum / sustainCount;
  const double hybridSustain = hybridSustainSum / sustainCount;
  const double feedbackSustain = feedbackSustainSum / sustainCount;
  const double currentReleaseMs =
      1000.0 * static_cast<double>(currentReleaseSample - burstEnd) /
      sampleRate;
  const double candidateReleaseMs =
      1000.0 * static_cast<double>(candidateReleaseSample - burstEnd) /
      sampleRate;
  const double dualReleaseMs =
      1000.0 * static_cast<double>(dualReleaseSample - burstEnd) / sampleRate;
  const double programmeReleaseMs =
      1000.0 * static_cast<double>(programmeReleaseSample - burstEnd) /
      sampleRate;
  const double hybridReleaseMs =
      1000.0 * static_cast<double>(hybridReleaseSample - burstEnd) / sampleRate;
  const double feedbackReleaseMs =
      1000.0 * static_cast<double>(feedbackReleaseSample - burstEnd) /
      sampleRate;
  const std::array<double, 6> sustainReleaseMs{
      1000.0 * static_cast<double>(sustainReleaseSamples[0] - sustainEnd) /
          sampleRate,
      1000.0 * static_cast<double>(sustainReleaseSamples[1] - sustainEnd) /
          sampleRate,
      1000.0 * static_cast<double>(sustainReleaseSamples[2] - sustainEnd) /
          sampleRate,
      1000.0 * static_cast<double>(sustainReleaseSamples[3] - sustainEnd) /
          sampleRate,
      1000.0 * static_cast<double>(sustainReleaseSamples[4] - sustainEnd) /
          sampleRate,
      1000.0 * static_cast<double>(sustainReleaseSamples[5] - sustainEnd) /
          sampleRate};
  std::cout << std::fixed << std::setprecision(6)
            << "{\"current_sustain_gr_db\":" << currentSustain
            << ",\"rms_peak_sustain_gr_db\":" << candidateSustain
            << ",\"dual_time_sustain_gr_db\":" << dualSustain
            << ",\"programme_sustain_gr_db\":" << programmeSustain
            << ",\"hybrid_sustain_gr_db\":" << hybridSustain
            << ",\"feedback_sustain_gr_db\":" << feedbackSustain
            << ",\"current_impulse_gr_db\":" << currentImpulseMax
            << ",\"rms_peak_impulse_gr_db\":" << candidateImpulseMax
            << ",\"dual_time_impulse_gr_db\":" << dualImpulseMax
            << ",\"programme_impulse_gr_db\":" << programmeImpulseMax
            << ",\"hybrid_impulse_gr_db\":" << hybridImpulseMax
            << ",\"feedback_impulse_gr_db\":" << feedbackImpulseMax
            << ",\"current_burst_gr_db\":" << currentBurstMax
            << ",\"rms_peak_burst_gr_db\":" << candidateBurstMax
            << ",\"dual_time_burst_gr_db\":" << dualBurstMax
            << ",\"programme_burst_gr_db\":" << programmeBurstMax
            << ",\"hybrid_burst_gr_db\":" << hybridBurstMax
            << ",\"feedback_burst_gr_db\":" << feedbackBurstMax
            << ",\"current_release_to_1db_ms\":" << currentReleaseMs
            << ",\"rms_peak_release_to_1db_ms\":" << candidateReleaseMs
            << ",\"dual_time_release_to_1db_ms\":" << dualReleaseMs
            << ",\"programme_release_to_1db_ms\":" << programmeReleaseMs
            << ",\"hybrid_release_to_1db_ms\":" << hybridReleaseMs
            << ",\"feedback_release_to_1db_ms\":" << feedbackReleaseMs
            << ",\"current_sustain_release_to_1db_ms\":" << sustainReleaseMs[0]
            << ",\"rms_peak_sustain_release_to_1db_ms\":" << sustainReleaseMs[1]
            << ",\"dual_time_sustain_release_to_1db_ms\":"
            << sustainReleaseMs[2]
            << ",\"programme_sustain_release_to_1db_ms\":"
            << sustainReleaseMs[3]
            << ",\"hybrid_sustain_release_to_1db_ms\":" << sustainReleaseMs[4]
            << ",\"feedback_sustain_release_to_1db_ms\":" << sustainReleaseMs[5]
            << ",\"max_pre_impulse_gr_db\":"
            << *std::max_element(beforeImpulse.begin(), beforeImpulse.end())
            << ",\"max_pre_burst_gr_db\":"
            << *std::max_element(beforeBurst.begin(), beforeBurst.end())
            << ",\"max_feedback_gain_residual\":" << maxFeedbackResidual
            << "}\n";

  const bool valid =
      std::isfinite(currentSustain) && std::isfinite(candidateSustain) &&
      currentSustain > 0.0 && candidateSustain > 0.0 &&
      std::abs(currentSustain - candidateSustain) < 0.1 &&
      currentImpulseMax > candidateImpulseMax && std::isfinite(dualSustain) &&
      dualSustain > 0.0 && std::isfinite(programmeSustain) &&
      programmeSustain > 0.0 && std::isfinite(hybridSustain) &&
      hybridSustain > 0.0 && std::isfinite(feedbackSustain) &&
      feedbackSustain > 0.0 && std::abs(currentSustain - dualSustain) < 0.5 &&
      std::abs(currentImpulseMax - dualImpulseMax) < 0.001 &&
      std::abs(currentBurstMax - dualBurstMax) < 0.1 &&
      std::abs(currentSustain - programmeSustain) < 0.1 &&
      std::abs(currentImpulseMax - programmeImpulseMax) < 0.001 &&
      std::abs(currentBurstMax - programmeBurstMax) < 0.1 &&
      std::abs(currentSustain - hybridSustain) < 0.1 &&
      hybridImpulseMax > candidateImpulseMax &&
      hybridImpulseMax < currentImpulseMax &&
      hybridBurstMax > candidateBurstMax && hybridBurstMax < currentBurstMax &&
      std::abs(currentSustain - feedbackSustain) < 0.01 &&
      feedbackImpulseMax > currentImpulseMax &&
      feedbackBurstMax < candidateBurstMax &&
      currentReleaseSample < totalFrames &&
      candidateReleaseSample < totalFrames && dualReleaseSample < totalFrames &&
      programmeReleaseSample < totalFrames &&
      hybridReleaseSample < totalFrames &&
      feedbackReleaseSample < totalFrames &&
      *std::max_element(sustainReleaseSamples.begin(),
                        sustainReleaseSamples.end()) < totalFrames &&
      dualReleaseSample < currentReleaseSample &&
      sustainReleaseSamples[2] > sustainReleaseSamples[0] &&
      programmeReleaseSample < currentReleaseSample &&
      sustainReleaseSamples[3] - sustainEnd >
          programmeReleaseSample - burstEnd &&
      hybridReleaseSample < programmeReleaseSample &&
      sustainReleaseSamples[4] < sustainReleaseSamples[0] &&
      feedbackReleaseSample < currentReleaseSample &&
      sustainReleaseSamples[5] >= sustainReleaseSamples[0] &&
      sustainReleaseSamples[5] - sustainReleaseSamples[0] < 10 &&
      maxFeedbackResidual < 0.001F &&
      *std::max_element(beforeImpulse.begin(), beforeImpulse.end()) < 0.01F &&
      *std::max_element(beforeBurst.begin(), beforeBurst.end()) < 0.01F;
  return valid ? 0 : 2;
}

double signalRms(const std::vector<float>& signal) {
  double sum{};
  for (float sample : signal) {
    sum += static_cast<double>(sample) * sample;
  }
  return std::sqrt(sum / static_cast<double>(signal.size()));
}

float signalPeak(const std::vector<float>& signal) {
  float peak{};
  for (float sample : signal) {
    peak = std::max(peak, std::abs(sample));
  }
  return peak;
}

void writeU16(std::ofstream& output, std::uint16_t value) {
  const std::array<char, 2> bytes{static_cast<char>(value),
                                  static_cast<char>(value >> 8U)};
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

void writeU32(std::ofstream& output, std::uint32_t value) {
  const std::array<char, 4> bytes{
      static_cast<char>(value), static_cast<char>(value >> 8U),
      static_cast<char>(value >> 16U), static_cast<char>(value >> 24U)};
  output.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
}

bool writeFloatWave(const std::filesystem::path& path,
                    const std::vector<float>& signal,
                    std::uint32_t sampleRate) {
  if (signal.size() >
      (std::numeric_limits<std::uint32_t>::max() - 48U) / sizeof(float)) {
    return false;
  }
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    return false;
  }
  const auto dataSize =
      static_cast<std::uint32_t>(signal.size() * sizeof(float));
  output.write("RIFF", 4);
  writeU32(output, 48U + dataSize);
  output.write("WAVEfmt ", 8);
  writeU32(output, 16U);
  writeU16(output, 3U);
  writeU16(output, 1U);
  writeU32(output, sampleRate);
  writeU32(output, sampleRate * sizeof(float));
  writeU16(output, sizeof(float));
  writeU16(output, 32U);
  output.write("fact", 4);
  writeU32(output, 4U);
  writeU32(output, static_cast<std::uint32_t>(signal.size()));
  output.write("data", 4);
  writeU32(output, dataSize);
  for (float sample : signal) {
    const float clean =
        std::clamp(std::isfinite(sample) ? sample : 0.0F, -1.0F, 1.0F);
    writeU32(output, std::bit_cast<std::uint32_t>(clean));
  }
  return static_cast<bool>(output);
}

bool writeStereoFloatWave(const std::filesystem::path& path,
                          const std::vector<float>& left,
                          const std::vector<float>& right,
                          std::uint32_t sampleRate) {
  if (left.size() != right.size() ||
      left.size() > (std::numeric_limits<std::uint32_t>::max() - 48U) /
                        (2U * sizeof(float))) {
    return false;
  }
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    return false;
  }
  const auto dataSize =
      static_cast<std::uint32_t>(left.size() * 2U * sizeof(float));
  output.write("RIFF", 4);
  writeU32(output, 48U + dataSize);
  output.write("WAVEfmt ", 8);
  writeU32(output, 16U);
  writeU16(output, 3U);
  writeU16(output, 2U);
  writeU32(output, sampleRate);
  writeU32(output, sampleRate * 2U * sizeof(float));
  writeU16(output, 2U * sizeof(float));
  writeU16(output, 32U);
  output.write("fact", 4);
  writeU32(output, 4U);
  writeU32(output, static_cast<std::uint32_t>(left.size()));
  output.write("data", 4);
  writeU32(output, dataSize);
  for (std::size_t sample = 0; sample < left.size(); ++sample) {
    for (float value : std::array<float, 2>{left[sample], right[sample]}) {
      const float clean =
          std::clamp(std::isfinite(value) ? value : 0.0F, -1.0F, 1.0F);
      writeU32(output, std::bit_cast<std::uint32_t>(clean));
    }
  }
  return static_cast<bool>(output);
}

std::vector<float> makeFixture(std::string_view name) {
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t frames = 192000;
  std::vector<float> signal(frames);
  std::uint32_t noiseState = 0xD01U;
  for (std::size_t i = 0; i < frames; ++i) {
    const double time = static_cast<double>(i) / sampleRate;
    if (name == "transient") {
      const std::size_t phase = i % 12000;
      if (phase < 2400) {
        const double hitTime = static_cast<double>(phase) / sampleRate;
        const float body =
            0.75F * static_cast<float>(std::exp(-25.0 * hitTime) *
                                       std::sin(2.0 * kPi * 68.0 * hitTime));
        const float click = phase < 24 ? 0.7F * (1.0F - phase / 24.0F) : 0.0F;
        signal[i] = std::clamp(body + click, -1.0F, 1.0F);
      }
    } else if (name == "bass") {
      const double movement = 0.8 + 0.2 * std::sin(2.0 * kPi * 0.5 * time);
      signal[i] = static_cast<float>(
          movement * (0.55 * std::sin(2.0 * kPi * 43.0 * time) +
                      0.12 * std::sin(2.0 * kPi * 86.0 * time)));
    } else if (name == "dense") {
      noiseState = noiseState * 1664525U + 1013904223U;
      const float noise =
          static_cast<float>(noiseState >> 8U) / 8388607.5F - 1.0F;
      signal[i] =
          0.24F * static_cast<float>(std::sin(2.0 * kPi * 110.0 * time)) +
          0.20F * static_cast<float>(std::sin(2.0 * kPi * 277.0 * time)) +
          0.16F * static_cast<float>(std::sin(2.0 * kPi * 997.0 * time)) +
          0.12F * static_cast<float>(std::sin(2.0 * kPi * 4003.0 * time)) +
          0.08F * noise;
    } else {
      const double drift = 0.75 + 0.25 * std::sin(2.0 * kPi * 0.11 * time);
      signal[i] = static_cast<float>(
          drift * (0.20 * std::sin(2.0 * kPi * 55.0 * time) +
                   0.15 * std::sin(2.0 * kPi * 83.2 * time) +
                   0.09 * std::sin(2.0 * kPi * 220.0 * time)));
      const std::size_t phase = i % 48000;
      if (phase < 4800) {
        signal[i] +=
            0.18F * static_cast<float>(
                        std::exp(-8.0 * phase / sampleRate) *
                        std::sin(2.0 * kPi * 880.0 * phase / sampleRate));
      }
    }
  }
  return signal;
}

struct GoldenMetrics {
  std::string fixture;
  std::uint32_t sampleRate{};
  std::size_t frames{};
  std::size_t blockSize{};
  double rmsDb{};
  double peakDb{};
  double crestDb{};
  double gainChangeDb{};
  double correlation{};
  double maximumReductionDb{};
  std::size_t latencySamples{};
  std::uint64_t fingerprint{};
};

aste::density::Parameters productionParameters() {
  aste::density::Parameters parameters;
  parameters.driveDb = 9.0F;
  parameters.crush = 0.82F;
  parameters.attackMs = 0.25F;
  parameters.releaseMs = 240.0F;
  parameters.density = 0.72F;
  parameters.blend = 0.58F;
  parameters.stereoLink = 0.65F;
  parameters.outputDb = -1.0F;
  parameters.detectorHpfHz = 90.0F;
  parameters.protection = true;
  return parameters;
}

double processInBlocks(aste::density::Processor& processor,
                       std::vector<float>& left, std::vector<float>& right,
                       std::span<const std::size_t> schedule,
                       const aste::density::Parameters& parameters) {
  double maximumReduction{};
  std::size_t block{};
  for (std::size_t offset = 0; offset < left.size(); ++block) {
    const std::size_t frames =
        std::min(schedule[block % schedule.size()], left.size() - offset);
    processor.process(left.data() + offset, right.data() + offset, frames,
                      parameters);
    maximumReduction =
        std::max(maximumReduction,
                 static_cast<double>(processor.meters().gainReductionDb));
    offset += frames;
  }
  return maximumReduction;
}

double stereoRms(const std::vector<float>& left,
                 const std::vector<float>& right) {
  double sum{};
  for (std::size_t sample = 0; sample < left.size(); ++sample) {
    sum += static_cast<double>(left[sample]) * left[sample];
    sum += static_cast<double>(right[sample]) * right[sample];
  }
  return std::sqrt(sum / static_cast<double>(2U * left.size()));
}

double stereoPeak(const std::vector<float>& left,
                  const std::vector<float>& right) {
  double peak{};
  for (std::size_t sample = 0; sample < left.size(); ++sample) {
    peak = std::max(peak, static_cast<double>(std::abs(left[sample])));
    peak = std::max(peak, static_cast<double>(std::abs(right[sample])));
  }
  return peak;
}

double stereoNullDb(const std::array<std::vector<float>, 2>& first,
                    const std::array<std::vector<float>, 2>& second) {
  double energy{};
  for (std::size_t sample = 0; sample < first[0].size(); ++sample) {
    const double left = first[0][sample] - second[0][sample];
    const double right = first[1][sample] - second[1][sample];
    energy += left * left + right * right;
  }
  const double rms =
      std::sqrt(energy / static_cast<double>(2U * first[0].size()));
  return 20.0 * std::log10(std::max(rms, 1.0e-15));
}

double stereoCorrelation(const std::vector<float>& left,
                         const std::vector<float>& right) {
  double product{};
  double leftEnergy{};
  double rightEnergy{};
  for (std::size_t sample = 0; sample < left.size(); ++sample) {
    product += static_cast<double>(left[sample]) * right[sample];
    leftEnergy += static_cast<double>(left[sample]) * left[sample];
    rightEnergy += static_cast<double>(right[sample]) * right[sample];
  }
  return product / std::sqrt(std::max(1.0e-30, leftEnergy * rightEnergy));
}

std::uint64_t sampleFingerprint(const std::vector<float>& left,
                                const std::vector<float>& right) {
  std::uint64_t hash = 14695981039346656037ULL;
  for (std::size_t sample = 0; sample < left.size(); ++sample) {
    for (float value : std::array<float, 2>{left[sample], right[sample]}) {
      const auto bits = std::bit_cast<std::uint32_t>(value);
      for (unsigned int shift = 0; shift < 32U; shift += 8U) {
        hash ^= (bits >> shift) & 0xffU;
        hash *= 1099511628211ULL;
      }
    }
  }
  return hash;
}

constexpr std::string_view kGoldenHeader =
    "schema,fixture,sample_rate,frames,block_size,rms_dbfs,peak_dbfs,crest_db,"
    "gain_change_db,correlation,max_gain_reduction_db,latency_samples,"
    "fingerprint_fnv1a64";

bool parseGoldenLine(std::string_view line, GoldenMetrics& metrics) {
  std::array<std::string_view, 13> fields{};
  std::size_t count{};
  while (!line.empty() && count < fields.size()) {
    const auto comma = line.find(',');
    fields[count++] = line.substr(0, comma);
    if (comma == std::string_view::npos) {
      line = {};
    } else {
      line.remove_prefix(comma + 1U);
    }
  }
  if (count != fields.size() || fields[0] != "1") {
    return false;
  }

  auto parseInteger = [](std::string_view text, auto& value, int base = 10) {
    const auto [end, error] =
        std::from_chars(text.data(), text.data() + text.size(), value, base);
    return error == std::errc{} && end == text.data() + text.size();
  };
  std::uint64_t sampleRate{};
  std::uint64_t frames{};
  std::uint64_t blockSize{};
  std::uint64_t latency{};
  metrics.fixture = fields[1];
  if (!parseInteger(fields[2], sampleRate) ||
      !parseInteger(fields[3], frames) || !parseInteger(fields[4], blockSize) ||
      !aste::density::parseFiniteDecimal(fields[5], metrics.rmsDb) ||
      !aste::density::parseFiniteDecimal(fields[6], metrics.peakDb) ||
      !aste::density::parseFiniteDecimal(fields[7], metrics.crestDb) ||
      !aste::density::parseFiniteDecimal(fields[8], metrics.gainChangeDb) ||
      !aste::density::parseFiniteDecimal(fields[9], metrics.correlation) ||
      !aste::density::parseFiniteDecimal(fields[10],
                                         metrics.maximumReductionDb) ||
      !parseInteger(fields[11], latency) ||
      !parseInteger(fields[12], metrics.fingerprint, 16)) {
    return false;
  }
  metrics.sampleRate = static_cast<std::uint32_t>(sampleRate);
  metrics.frames = static_cast<std::size_t>(frames);
  metrics.blockSize = static_cast<std::size_t>(blockSize);
  metrics.latencySamples = static_cast<std::size_t>(latency);
  return sampleRate == metrics.sampleRate && frames == metrics.frames &&
         blockSize == metrics.blockSize && latency == metrics.latencySamples;
}

bool compareGolden(const std::filesystem::path& baselinePath,
                   const std::vector<GoldenMetrics>& actual) {
  std::ifstream baseline{baselinePath};
  std::string line;
  if (!baseline || !std::getline(baseline, line) || line != kGoldenHeader) {
    std::cerr << "invalid golden baseline: " << baselinePath << '\n';
    return false;
  }

  bool valid = true;
  std::size_t fingerprintsChanged{};
  for (const auto& result : actual) {
    GoldenMetrics expected;
    if (!std::getline(baseline, line) || !parseGoldenLine(line, expected) ||
        expected.fixture != result.fixture) {
      std::cerr << "missing golden fixture: " << result.fixture << '\n';
      return false;
    }
    auto check = [&](std::string_view name, double value, double reference,
                     double tolerance) {
      if (!std::isfinite(value) || std::abs(value - reference) > tolerance) {
        std::cerr << "golden mismatch: " << result.fixture << ' ' << name
                  << " actual=" << value << " expected=" << reference
                  << " tolerance=" << tolerance << '\n';
        valid = false;
      }
    };
    valid = valid && expected.sampleRate == result.sampleRate &&
            expected.frames == result.frames &&
            expected.blockSize == result.blockSize &&
            expected.latencySamples == result.latencySamples;
    check("rms_dbfs", result.rmsDb, expected.rmsDb, 0.02);
    check("peak_dbfs", result.peakDb, expected.peakDb, 0.02);
    check("crest_db", result.crestDb, expected.crestDb, 0.03);
    check("gain_change_db", result.gainChangeDb, expected.gainChangeDb, 0.02);
    check("correlation", result.correlation, expected.correlation, 0.002);
    check("max_gain_reduction_db", result.maximumReductionDb,
          expected.maximumReductionDb, 0.05);
    fingerprintsChanged += expected.fingerprint != result.fingerprint ? 1U : 0U;
  }
  if (std::getline(baseline, line)) {
    std::cerr << "golden baseline has unexpected rows\n";
    valid = false;
  }
  std::cout << "{\"golden_fixtures\":" << actual.size()
            << ",\"fingerprints_changed\":" << fingerprintsChanged
            << ",\"within_tolerance\":" << (valid ? "true" : "false") << "}\n";
  return valid;
}

int productionGolden(const std::filesystem::path& directory,
                     const std::filesystem::path& baselinePath) {
  constexpr std::uint32_t sampleRate = 48000U;
  constexpr std::size_t blockSize = 127U;
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  std::ofstream manifest{directory / "density-production-v1.csv"};
  if (error || !manifest) {
    std::cerr << "cannot create golden output: " << directory << '\n';
    return 1;
  }
  manifest << kGoldenHeader << '\n';
  manifest << std::fixed << std::setprecision(9);

  const auto parameters = productionParameters();

  std::vector<GoldenMetrics> results;
  bool valid = true;
  for (std::string_view fixture : std::array<std::string_view, 4>{
           "transient", "bass", "dense", "ambient"}) {
    auto left = makeFixture(fixture);
    std::vector<float> right(left.size());
    const std::size_t shift = fixture == "transient" ? 23U
                              : fixture == "dense"   ? 113U
                              : fixture == "ambient" ? 509U
                                                     : 0U;
    for (std::size_t sample = 0; sample < left.size(); ++sample) {
      const float delayed = sample >= shift ? left[sample - shift] : 0.0F;
      if (fixture == "transient") {
        right[sample] = 0.72F * delayed;
      } else if (fixture == "bass") {
        right[sample] = 0.82F * left[sample];
      } else if (fixture == "dense") {
        right[sample] = 0.65F * left[sample] + 0.28F * delayed;
      } else {
        right[sample] = 0.55F * left[sample] + 0.35F * delayed;
      }
    }
    const double inputRms = stereoRms(left, right);
    aste::density::Processor processor;
    processor.prepare(sampleRate, parameters);
    constexpr std::array<std::size_t, 1> schedule{blockSize};
    const double maximumReduction =
        processInBlocks(processor, left, right, schedule, parameters);
    const double outputRms = stereoRms(left, right);
    const double outputPeak = stereoPeak(left, right);
    const double rmsDb = 20.0 * std::log10(outputRms);
    const double peakDb = 20.0 * std::log10(outputPeak);
    GoldenMetrics metrics{
        .fixture = std::string{fixture},
        .sampleRate = sampleRate,
        .frames = left.size(),
        .blockSize = blockSize,
        .rmsDb = rmsDb,
        .peakDb = peakDb,
        .crestDb = peakDb - rmsDb,
        .gainChangeDb = 20.0 * std::log10(outputRms / inputRms),
        .correlation = stereoCorrelation(left, right),
        .maximumReductionDb = maximumReduction,
        .latencySamples = processor.latencySamples(),
        .fingerprint = sampleFingerprint(left, right),
    };
    const auto wavePath = directory / (std::string{fixture} + ".wav");
    valid = writeStereoFloatWave(wavePath, left, right, sampleRate) && valid &&
            std::isfinite(metrics.rmsDb) && std::isfinite(metrics.peakDb) &&
            std::isfinite(metrics.correlation);
    manifest << "1," << metrics.fixture << ',' << metrics.sampleRate << ','
             << metrics.frames << ',' << metrics.blockSize << ','
             << metrics.rmsDb << ',' << metrics.peakDb << ',' << metrics.crestDb
             << ',' << metrics.gainChangeDb << ',' << metrics.correlation << ','
             << metrics.maximumReductionDb << ',' << metrics.latencySamples
             << ',' << std::hex << std::setw(16) << std::setfill('0')
             << metrics.fingerprint << std::dec << std::setfill(' ') << '\n';
    results.push_back(metrics);
  }
  valid = valid && static_cast<bool>(manifest);
  manifest.close();
  if (!valid) {
    return 2;
  }
  return baselinePath.empty() || compareGolden(baselinePath, results) ? 0 : 2;
}

std::array<std::vector<float>, 2> makeConsistencyFixture(
    std::uint32_t sampleRate) {
  const std::size_t frames = 2U * sampleRate;
  std::array<std::vector<float>, 2> channels{std::vector<float>(frames),
                                             std::vector<float>(frames)};
  auto signal = [](double time) {
    if (time < 0.0) {
      return 0.0F;
    }
    const double movement = 0.82 + 0.18 * std::sin(2.0 * kPi * 0.7 * time);
    double sample = movement * (0.38 * std::sin(2.0 * kPi * 43.0 * time) +
                                0.24 * std::sin(2.0 * kPi * 277.0 * time) +
                                0.11 * std::sin(2.0 * kPi * 4003.0 * time));
    const double hitTime = std::fmod(time, 0.25);
    if (hitTime < 0.05) {
      sample += 0.55 * std::exp(-35.0 * hitTime) *
                std::sin(2.0 * kPi * 72.0 * hitTime);
    }
    if (hitTime < 0.0005) {
      sample += 0.55 * (1.0 - hitTime / 0.0005);
    }
    return static_cast<float>(std::clamp(sample, -1.0, 1.0));
  };
  for (std::size_t sample = 0; sample < frames; ++sample) {
    const double time = static_cast<double>(sample) / sampleRate;
    channels[0][sample] = signal(time);
    channels[1][sample] =
        0.72F * signal(time - 0.0023) +
        0.16F * static_cast<float>(std::sin(2.0 * kPi * 613.0 * time));
  }
  return channels;
}

int productionConsistency(const std::filesystem::path& outputPath) {
  constexpr std::array<std::uint32_t, 6> sampleRates{44100U, 48000U,  88200U,
                                                     96000U, 176400U, 192000U};
  constexpr std::array<std::size_t, 1> referenceSchedule{127U};
  constexpr std::array<std::size_t, 13> variableSchedule{
      1U, 2U, 7U, 16U, 32U, 64U, 127U, 128U, 256U, 511U, 512U, 1024U, 2048U};
  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create consistency report: " << outputPath << '\n';
    return 1;
  }
  output << "sample_rate,frames,schedule_blocks,max_block_delta,rms_gain_db,"
            "peak_dbfs,crest_db,correlation,max_gain_reduction_db,latency_"
            "samples\n"
         << std::fixed << std::setprecision(9);

  const auto parameters = productionParameters();
  double minimumGain = std::numeric_limits<double>::infinity();
  double maximumGain = -minimumGain;
  double minimumPeak = minimumGain;
  double maximumPeak = maximumGain;
  double minimumCrest = minimumGain;
  double maximumCrest = maximumGain;
  double minimumCorrelation = minimumGain;
  double maximumCorrelation = maximumGain;
  double minimumReduction = minimumGain;
  double maximumReduction = maximumGain;
  double maximumBlockDelta{};
  bool valid = true;

  for (const auto sampleRate : sampleRates) {
    auto reference = makeConsistencyFixture(sampleRate);
    auto variable = reference;
    const double inputRms = stereoRms(reference[0], reference[1]);
    aste::density::Processor referenceProcessor;
    aste::density::Processor variableProcessor;
    referenceProcessor.prepare(sampleRate, parameters);
    variableProcessor.prepare(sampleRate, parameters);
    const double reduction =
        processInBlocks(referenceProcessor, reference[0], reference[1],
                        referenceSchedule, parameters);
    processInBlocks(variableProcessor, variable[0], variable[1],
                    variableSchedule, parameters);

    double blockDelta{};
    for (std::size_t sample = 0; sample < reference[0].size(); ++sample) {
      blockDelta = std::max({blockDelta,
                             static_cast<double>(std::abs(reference[0][sample] -
                                                          variable[0][sample])),
                             static_cast<double>(std::abs(
                                 reference[1][sample] - variable[1][sample]))});
    }
    const double rms = stereoRms(reference[0], reference[1]);
    const double rmsDb = 20.0 * std::log10(rms);
    const double peakDb =
        20.0 * std::log10(stereoPeak(reference[0], reference[1]));
    const double gainDb = 20.0 * std::log10(rms / inputRms);
    const double crestDb = peakDb - rmsDb;
    const double correlation = stereoCorrelation(reference[0], reference[1]);
    maximumBlockDelta = std::max(maximumBlockDelta, blockDelta);
    minimumGain = std::min(minimumGain, gainDb);
    maximumGain = std::max(maximumGain, gainDb);
    minimumPeak = std::min(minimumPeak, peakDb);
    maximumPeak = std::max(maximumPeak, peakDb);
    minimumCrest = std::min(minimumCrest, crestDb);
    maximumCrest = std::max(maximumCrest, crestDb);
    minimumCorrelation = std::min(minimumCorrelation, correlation);
    maximumCorrelation = std::max(maximumCorrelation, correlation);
    minimumReduction = std::min(minimumReduction, reduction);
    maximumReduction = std::max(maximumReduction, reduction);
    valid = valid && std::isfinite(gainDb) && std::isfinite(peakDb) &&
            std::isfinite(crestDb) && std::isfinite(correlation) &&
            std::isfinite(reduction) && blockDelta == 0.0 &&
            referenceProcessor.latencySamples() == 0U;
    output << sampleRate << ',' << reference[0].size() << ','
           << variableSchedule.size() << ',' << blockDelta << ',' << gainDb
           << ',' << peakDb << ',' << crestDb << ',' << correlation << ','
           << reduction << ',' << referenceProcessor.latencySamples() << '\n';
  }

  const double gainRange = maximumGain - minimumGain;
  const double peakRange = maximumPeak - minimumPeak;
  const double crestRange = maximumCrest - minimumCrest;
  const double correlationRange = maximumCorrelation - minimumCorrelation;
  const double reductionRange = maximumReduction - minimumReduction;
  valid = valid && static_cast<bool>(output) && gainRange <= 0.03 &&
          peakRange <= 0.01 && crestRange <= 0.03 &&
          correlationRange <= 0.001 && reductionRange <= 0.10;
  std::cout << std::fixed << std::setprecision(9)
            << "{\"sample_rates\":" << sampleRates.size()
            << ",\"variable_blocks\":" << variableSchedule.size()
            << ",\"max_block_delta\":" << maximumBlockDelta
            << ",\"gain_range_db\":" << gainRange
            << ",\"peak_range_db\":" << peakRange
            << ",\"crest_range_db\":" << crestRange
            << ",\"correlation_range\":" << correlationRange
            << ",\"gain_reduction_range_db\":" << reductionRange
            << ",\"within_limits\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

std::array<double, 2> coherentBin(const std::vector<float>& signal,
                                  std::size_t bin) {
  const double step =
      2.0 * kPi * static_cast<double>(bin) / static_cast<double>(signal.size());
  const double rotationReal = std::cos(step);
  const double rotationImaginary = std::sin(step);
  double oscillatorReal = 1.0;
  double oscillatorImaginary = 0.0;
  double real{};
  double imaginary{};
  for (float sample : signal) {
    real += sample * oscillatorReal;
    imaginary -= sample * oscillatorImaginary;
    const double nextReal =
        oscillatorReal * rotationReal - oscillatorImaginary * rotationImaginary;
    oscillatorImaginary =
        oscillatorReal * rotationImaginary + oscillatorImaginary * rotationReal;
    oscillatorReal = nextReal;
  }
  return {real, imaginary};
}

double coherentBinAmplitude(const std::vector<float>& signal, std::size_t bin) {
  const auto value = coherentBin(signal, bin);
  return 2.0 * std::hypot(value[0], value[1]) /
         static_cast<double>(signal.size());
}

double foldedHarmonicsDbc(const std::vector<float>& signal,
                          std::size_t inputBin) {
  constexpr std::size_t highestHarmonic = 63U;
  const std::size_t size = signal.size();
  std::vector<std::size_t> aliasBins;
  for (std::size_t harmonic = 3U; harmonic <= highestHarmonic; harmonic += 2U) {
    if (harmonic * inputBin <= size / 2U) {
      continue;
    }
    std::size_t bin = (harmonic * inputBin) % size;
    bin = bin > size / 2U ? size - bin : bin;
    if (bin != 0U && bin != inputBin &&
        std::find(aliasBins.begin(), aliasBins.end(), bin) == aliasBins.end()) {
      aliasBins.push_back(bin);
    }
  }
  if (aliasBins.empty()) {
    return -300.0;
  }
  double aliasEnergy{};
  for (const auto bin : aliasBins) {
    const double amplitude = coherentBinAmplitude(signal, bin);
    aliasEnergy += amplitude * amplitude;
  }
  const double fundamental = coherentBinAmplitude(signal, inputBin);
  return 10.0 * std::log10(aliasEnergy / (fundamental * fundamental));
}

std::vector<double> referenceLowPass(std::size_t factor) {
  const std::size_t taps = 64U * factor + 1U;
  const std::size_t centre = taps / 2U;
  const double cutoff = 0.5 / static_cast<double>(factor);
  std::vector<double> coefficients(taps);
  double sum{};
  for (std::size_t tap = 0; tap < taps; ++tap) {
    const double offset = static_cast<double>(tap) - centre;
    const double sinc =
        offset == 0.0 ? 2.0 * cutoff
                      : std::sin(2.0 * kPi * cutoff * offset) / (kPi * offset);
    const double phase = 2.0 * kPi * tap / static_cast<double>(taps - 1U);
    const double blackman =
        0.42 - 0.5 * std::cos(phase) + 0.08 * std::cos(2.0 * phase);
    coefficients[tap] = sinc * blackman;
    sum += coefficients[tap];
  }
  for (double& coefficient : coefficients) {
    coefficient /= sum;
  }
  return coefficients;
}

double circularFirSample(const std::vector<float>& signal,
                         const std::vector<double>& coefficients,
                         std::size_t sample) {
  const std::size_t centre = coefficients.size() / 2U;
  double output{};
  for (std::size_t tap = 0; tap < coefficients.size(); ++tap) {
    std::ptrdiff_t source = static_cast<std::ptrdiff_t>(sample + centre) -
                            static_cast<std::ptrdiff_t>(tap);
    if (source < 0) {
      source += static_cast<std::ptrdiff_t>(signal.size());
    } else if (source >= static_cast<std::ptrdiff_t>(signal.size())) {
      source -= static_cast<std::ptrdiff_t>(signal.size());
    }
    output += signal[static_cast<std::size_t>(source)] * coefficients[tap];
  }
  return output;
}

std::vector<float> interpolateReference(
    const std::vector<float>& input, std::size_t factor,
    const std::vector<double>& coefficients) {
  std::vector<float> zeroStuffed(input.size() * factor);
  for (std::size_t sample = 0; sample < input.size(); ++sample) {
    zeroStuffed[sample * factor] = input[sample] * static_cast<float>(factor);
  }
  std::vector<float> output(zeroStuffed.size());
  for (std::size_t sample = 0; sample < output.size(); ++sample) {
    output[sample] = static_cast<float>(
        circularFirSample(zeroStuffed, coefficients, sample));
  }
  return output;
}

std::vector<float> decimateReference(const std::vector<float>& input,
                                     std::size_t factor,
                                     const std::vector<double>& coefficients) {
  std::vector<float> output(input.size() / factor);
  for (std::size_t sample = 0; sample < output.size(); ++sample) {
    output[sample] = static_cast<float>(
        circularFirSample(input, coefficients, sample * factor));
  }
  return output;
}

int nonlinearAliasReport(const std::filesystem::path& outputPath) {
  constexpr std::array<std::uint32_t, 6> sampleRates{44100U, 48000U,  88200U,
                                                     96000U, 176400U, 192000U};
  constexpr std::size_t frames = 32768U;
  constexpr double targetFrequency = 7000.0;
  const float saturationDrive =
      aste::density::mapDensity(productionParameters().density).saturationDrive;
  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create alias report: " << outputPath << '\n';
    return 1;
  }
  output << "stage,sample_rate,input_frequency_hz,fundamental_dbfs,peak_dbfs,"
            "folded_harmonics_3_to_63_dbc\n"
         << std::fixed << std::setprecision(9);

  std::array<double, 3> lowestRateAlias{};
  std::array<double, 3> highestRateAlias{};
  bool valid = saturationDrive > 0.0F;
  for (std::size_t rateIndex = 0; rateIndex < sampleRates.size(); ++rateIndex) {
    const auto sampleRate = sampleRates[rateIndex];
    const auto inputBin = static_cast<std::size_t>(std::llround(
        targetFrequency * static_cast<double>(frames) / sampleRate));
    const double frequency =
        static_cast<double>(inputBin) * sampleRate / frames;
    std::array<std::vector<float>, 3> stages{std::vector<float>(frames),
                                             std::vector<float>(frames),
                                             std::vector<float>(frames)};
    for (std::size_t sample = 0; sample < frames; ++sample) {
      const float sine =
          static_cast<float>(std::sin(2.0 * kPi * inputBin * sample / frames));
      const float saturated =
          aste::density::saturateSample(0.9F * sine, saturationDrive);
      stages[0][sample] = saturated;
      stages[1][sample] = aste::density::controlledClipSample(saturated);
      stages[2][sample] = aste::density::controlledClipSample(1.1F * sine);
    }
    for (std::size_t stage = 0; stage < stages.size(); ++stage) {
      constexpr std::array<std::string_view, 3> names{
          "saturation", "crush_clip", "protection_clip"};
      const double fundamental = coherentBinAmplitude(stages[stage], inputBin);
      const double fundamentalDb = 20.0 * std::log10(fundamental);
      const double peakDb = 20.0 * std::log10(signalPeak(stages[stage]));
      const double aliasDbc = foldedHarmonicsDbc(stages[stage], inputBin);
      if (rateIndex == 0U) {
        lowestRateAlias[stage] = aliasDbc;
      }
      if (rateIndex + 1U == sampleRates.size()) {
        highestRateAlias[stage] = aliasDbc;
      }
      valid = valid && std::isfinite(fundamentalDb) && std::isfinite(peakDb) &&
              std::isfinite(aliasDbc);
      output << names[stage] << ',' << sampleRate << ',' << frequency << ','
             << fundamentalDb << ',' << peakDb << ',' << aliasDbc << '\n';
    }
  }
  for (std::size_t stage = 0; stage < lowestRateAlias.size(); ++stage) {
    valid = valid && highestRateAlias[stage] < lowestRateAlias[stage];
  }
  std::cout << std::fixed << std::setprecision(6)
            << "{\"rates\":" << sampleRates.size()
            << ",\"stages\":3,\"saturation_44100_dbc\":" << lowestRateAlias[0]
            << ",\"saturation_192000_dbc\":" << highestRateAlias[0]
            << ",\"crush_clip_44100_dbc\":" << lowestRateAlias[1]
            << ",\"crush_clip_192000_dbc\":" << highestRateAlias[1]
            << ",\"protection_clip_44100_dbc\":" << lowestRateAlias[2]
            << ",\"protection_clip_192000_dbc\":" << highestRateAlias[2]
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int oversamplingReferenceReport(const std::filesystem::path& outputPath) {
  constexpr std::uint32_t sampleRate = 48000U;
  constexpr std::size_t frames = 32768U;
  constexpr double targetFrequency = 7000.0;
  constexpr std::array<std::size_t, 3> factors{1U, 2U, 4U};
  constexpr std::array<std::string_view, 3> names{"saturation", "crush_clip",
                                                  "protection_clip"};
  const auto inputBin = static_cast<std::size_t>(
      std::llround(targetFrequency * static_cast<double>(frames) / sampleRate));
  const double frequency = static_cast<double>(inputBin) * sampleRate / frames;
  const float saturationDrive =
      aste::density::mapDensity(productionParameters().density).saturationDrive;
  std::vector<float> sine(frames);
  for (std::size_t sample = 0; sample < frames; ++sample) {
    sine[sample] =
        static_cast<float>(std::sin(2.0 * kPi * inputBin * sample / frames));
  }

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create oversampling report: " << outputPath << '\n';
    return 1;
  }
  output << "factor,stage,base_rate,input_frequency_hz,filter_taps,"
            "latency_samples,fundamental_delta_db,folded_harmonics_3_to_63_dbc,"
            "alias_improvement_db\n"
         << std::fixed << std::setprecision(9);

  std::array<double, 3> baselineFundamental{};
  std::array<double, 3> baselineAlias{};
  std::array<double, 3> twoTimesImprovement{};
  std::array<double, 3> fourTimesImprovement{};
  bool valid = true;
  for (const auto factor : factors) {
    std::array<std::vector<float>, 3> stages;
    std::size_t filterTaps{};
    std::size_t latency{};
    if (factor == 1U) {
      for (std::size_t sample = 0; sample < frames; ++sample) {
        const float saturated =
            aste::density::saturateSample(0.9F * sine[sample], saturationDrive);
        stages[0].push_back(saturated);
        stages[1].push_back(aste::density::controlledClipSample(saturated));
        stages[2].push_back(
            aste::density::controlledClipSample(1.1F * sine[sample]));
      }
    } else {
      const auto filter = referenceLowPass(factor);
      const auto highRateSine = interpolateReference(sine, factor, filter);
      std::array<std::vector<float>, 3> highRateStages{
          std::vector<float>(highRateSine.size()),
          std::vector<float>(highRateSine.size()),
          std::vector<float>(highRateSine.size())};
      for (std::size_t sample = 0; sample < highRateSine.size(); ++sample) {
        const float saturated = aste::density::saturateSample(
            0.9F * highRateSine[sample], saturationDrive);
        highRateStages[0][sample] = saturated;
        highRateStages[1][sample] =
            aste::density::controlledClipSample(saturated);
        highRateStages[2][sample] =
            aste::density::controlledClipSample(1.1F * highRateSine[sample]);
      }
      for (std::size_t stage = 0; stage < stages.size(); ++stage) {
        stages[stage] =
            decimateReference(highRateStages[stage], factor, filter);
      }
      filterTaps = filter.size();
      latency = (filter.size() - 1U) / factor;
    }

    for (std::size_t stage = 0; stage < stages.size(); ++stage) {
      const double fundamental = coherentBinAmplitude(stages[stage], inputBin);
      const double fundamentalDb = 20.0 * std::log10(fundamental);
      const double aliasDbc = foldedHarmonicsDbc(stages[stage], inputBin);
      if (factor == 1U) {
        baselineFundamental[stage] = fundamentalDb;
        baselineAlias[stage] = aliasDbc;
      }
      const double fundamentalDelta =
          fundamentalDb - baselineFundamental[stage];
      const double improvement = baselineAlias[stage] - aliasDbc;
      if (factor == 2U) {
        twoTimesImprovement[stage] = improvement;
      } else if (factor == 4U) {
        fourTimesImprovement[stage] = improvement;
      }
      valid = valid && std::isfinite(fundamentalDelta) &&
              std::isfinite(aliasDbc) && std::isfinite(improvement) &&
              std::abs(fundamentalDelta) <= 0.05;
      output << factor << ',' << names[stage] << ',' << sampleRate << ','
             << frequency << ',' << filterTaps << ',' << latency << ','
             << fundamentalDelta << ',' << aliasDbc << ',' << improvement
             << '\n';
    }
  }
  for (std::size_t stage = 0; stage < names.size(); ++stage) {
    valid = valid && twoTimesImprovement[stage] > 0.0 &&
            fourTimesImprovement[stage] >= twoTimesImprovement[stage];
  }
  std::cout << std::fixed << std::setprecision(6)
            << "{\"base_rate\":" << sampleRate
            << ",\"two_x_latency_samples\":64,\"four_x_latency_samples\":64,"
            << "\"crush_clip_two_x_improvement_db\":" << twoTimesImprovement[1]
            << ",\"crush_clip_four_x_improvement_db\":"
            << fourTimesImprovement[1]
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int oversamplingPrototypeReport(const std::filesystem::path& outputPath) {
  constexpr std::uint32_t sampleRate = 48000U;
  constexpr std::size_t frames = 32768U;
  constexpr std::size_t blockSize = 127U;
  constexpr double renderedSeconds = 5.0;
  constexpr std::array<double, 4> targetFrequencies{7000.0, 8500.0, 10000.0,
                                                    15000.0};
  constexpr std::array<std::size_t, 4> tapCounts{16U, 32U, 48U, 64U};
  constexpr std::array<std::size_t, 13> schedule{
      1U, 2U, 7U, 16U, 32U, 64U, 127U, 128U, 256U, 511U, 512U, 1024U, 2048U};
  const float drive =
      aste::density::mapDensity(productionParameters().density).saturationDrive;
  std::vector<float> blockSource(8192U);
  for (std::size_t sample = 0; sample < blockSource.size(); ++sample) {
    blockSource[sample] = 0.8F * static_cast<float>(std::sin(0.071 * sample));
  }

  std::array<float, blockSize> source{};
  for (std::size_t sample = 0; sample < source.size(); ++sample) {
    source[sample] =
        0.7F * static_cast<float>(std::sin(0.13 * static_cast<double>(sample)));
  }
  const std::size_t benchmarkFrames =
      static_cast<std::size_t>(sampleRate * renderedSeconds);
  const bool benchmarkEnabled = std::getenv("ASTE_SKIP_BENCHMARK") == nullptr;

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create prototype report: " << outputPath << '\n';
    return 1;
  }
  output << "taps_per_phase,filter_taps,sample_rate,input_frequency_hz,"
            "latency_samples,max_block_delta,fundamental_delta_db,alias_dbc,"
            "alias_improvement_db,instance_bytes,benchmark_enabled,"
            "rendered_seconds,median_elapsed_seconds,stereo_one_core_percent,"
            "checksum\n"
         << std::fixed << std::setprecision(9);

  std::array<std::array<double, 4>, 4> aliasResults{};
  std::array<double, 4> worstAliasResults{};
  std::array<double, 4> maximumGainDeltaResults{};
  std::array<double, 4> cpuResults{};
  bool valid = true;
  for (std::size_t candidateIndex = 0; candidateIndex < tapCounts.size();
       ++candidateIndex) {
    const std::size_t tapsPerPhase = tapCounts[candidateIndex];
    std::array<double, 4> frequencies{};
    std::array<double, 4> fundamentalDeltas{};
    std::array<double, 4> aliasImprovements{};
    for (std::size_t tone = 0; tone < targetFrequencies.size(); ++tone) {
      const auto inputBin = static_cast<std::size_t>(std::llround(
          targetFrequencies[tone] * static_cast<double>(frames) / sampleRate));
      frequencies[tone] = static_cast<double>(inputBin) * sampleRate / frames;
      std::vector<float> direct(frames);
      std::vector<float> streaming(2U * frames);
      for (std::size_t sample = 0; sample < streaming.size(); ++sample) {
        const float sine = static_cast<float>(
            std::sin(2.0 * kPi * inputBin * sample / frames));
        streaming[sample] = 0.9F * sine;
        if (sample < frames) {
          direct[sample] = aste::density::controlledClipSample(
              aste::density::saturateSample(0.9F * sine, drive));
        }
      }
      aste::density::CrushOversampler4x oversampler;
      oversampler.prepare(tapsPerPhase);
      oversampler.process(streaming.data(), streaming.size(), drive);
      const std::vector<float> candidate(streaming.begin() + frames,
                                         streaming.end());
      const double directFundamental = coherentBinAmplitude(direct, inputBin);
      const double directAlias = foldedHarmonicsDbc(direct, inputBin);
      fundamentalDeltas[tone] =
          20.0 * std::log10(coherentBinAmplitude(candidate, inputBin) /
                            directFundamental);
      aliasResults[candidateIndex][tone] =
          foldedHarmonicsDbc(candidate, inputBin);
      aliasImprovements[tone] =
          directAlias - aliasResults[candidateIndex][tone];
      valid = valid && std::isfinite(fundamentalDeltas[tone]) &&
              std::isfinite(aliasResults[candidateIndex][tone]) &&
              aliasImprovements[tone] > 0.0;
    }

    auto whole = blockSource;
    auto variable = blockSource;
    aste::density::CrushOversampler4x wholeProcessor;
    aste::density::CrushOversampler4x variableProcessor;
    wholeProcessor.prepare(tapsPerPhase);
    variableProcessor.prepare(tapsPerPhase);
    wholeProcessor.process(whole.data(), whole.size(), drive);
    std::size_t offset{};
    std::size_t block{};
    while (offset < variable.size()) {
      const std::size_t count =
          std::min(schedule[block % schedule.size()], variable.size() - offset);
      variableProcessor.process(variable.data() + offset, count, drive);
      offset += count;
      ++block;
    }
    double maximumBlockDelta{};
    for (std::size_t sample = 0; sample < whole.size(); ++sample) {
      maximumBlockDelta = std::max(
          maximumBlockDelta,
          static_cast<double>(std::abs(whole[sample] - variable[sample])));
    }

    std::array<float, 256> impulse{};
    impulse[0] = 1.0e-3F;
    aste::density::CrushOversampler4x impulseProcessor;
    impulseProcessor.prepare(tapsPerPhase);
    impulseProcessor.process(impulse.data(), impulse.size(), 1.0F);
    const auto peak = std::max_element(
        impulse.begin(), impulse.end(), [](float left, float right) {
          return std::abs(left) < std::abs(right);
        });
    const std::size_t measuredLatency =
        static_cast<std::size_t>(std::distance(impulse.begin(), peak));

    std::array<double, 5> timings{};
    double checksum{};
    for (double& timing : timings) {
      if (!benchmarkEnabled) {
        break;
      }
      std::array<float, blockSize> left{};
      std::array<float, blockSize> right{};
      aste::density::CrushOversampler4x leftProcessor;
      aste::density::CrushOversampler4x rightProcessor;
      leftProcessor.prepare(tapsPerPhase);
      rightProcessor.prepare(tapsPerPhase);
      const auto start = std::chrono::steady_clock::now();
      for (std::size_t rendered = 0; rendered < benchmarkFrames;) {
        const std::size_t count =
            std::min(blockSize, benchmarkFrames - rendered);
        left = source;
        right = source;
        leftProcessor.process(left.data(), count, drive);
        rightProcessor.process(right.data(), count, drive);
        rendered += count;
      }
      timing = std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                             start)
                   .count();
      for (std::size_t sample = 0; sample < blockSize; ++sample) {
        checksum += left[sample] + right[sample];
      }
    }
    std::sort(timings.begin(), timings.end());
    const double elapsed =
        benchmarkEnabled ? timings[timings.size() / 2U] : 0.0;
    const double oneCorePercent =
        benchmarkEnabled ? 100.0 * elapsed / renderedSeconds : 0.0;
    worstAliasResults[candidateIndex] =
        *std::max_element(aliasResults[candidateIndex].begin(),
                          aliasResults[candidateIndex].end());
    maximumGainDeltaResults[candidateIndex] = std::abs(
        *std::max_element(fundamentalDeltas.begin(), fundamentalDeltas.end(),
                          [](double left, double right) {
                            return std::abs(left) < std::abs(right);
                          }));
    cpuResults[candidateIndex] = oneCorePercent;
    valid = valid && measuredLatency == impulseProcessor.latencySamples() &&
            maximumBlockDelta == 0.0 &&
            maximumGainDeltaResults[candidateIndex] < 0.05 &&
            std::isfinite(oneCorePercent);
    for (std::size_t tone = 0; tone < targetFrequencies.size(); ++tone) {
      output << tapsPerPhase << ',' << 4U * tapsPerPhase + 1U << ','
             << sampleRate << ',' << frequencies[tone] << ',' << measuredLatency
             << ',' << maximumBlockDelta << ',' << fundamentalDeltas[tone]
             << ',' << aliasResults[candidateIndex][tone] << ','
             << aliasImprovements[tone] << ','
             << sizeof(aste::density::CrushOversampler4x) << ','
             << (benchmarkEnabled ? 1 : 0) << ',' << renderedSeconds << ','
             << elapsed << ',' << oneCorePercent << ',' << checksum << '\n';
    }
  }
  valid = valid && worstAliasResults[1] < worstAliasResults[0] &&
          worstAliasResults[2] < worstAliasResults[1] &&
          worstAliasResults[3] < worstAliasResults[2];
  std::cout << std::fixed << std::setprecision(6)
            << "{\"candidates\":" << tapCounts.size()
            << ",\"tones\":" << targetFrequencies.size()
            << ",\"worst_alias_16_dbc\":" << worstAliasResults[0]
            << ",\"worst_alias_32_dbc\":" << worstAliasResults[1]
            << ",\"worst_alias_48_dbc\":" << worstAliasResults[2]
            << ",\"worst_alias_64_dbc\":" << worstAliasResults[3]
            << ",\"max_gain_delta_16_db\":" << maximumGainDeltaResults[0]
            << ",\"max_gain_delta_64_db\":" << maximumGainDeltaResults[3]
            << ",\"instance_bytes\":"
            << sizeof(aste::density::CrushOversampler4x)
            << ",\"benchmark_enabled\":"
            << (benchmarkEnabled ? "true" : "false")
            << ",\"cpu_16_percent\":" << cpuResults[0]
            << ",\"cpu_32_percent\":" << cpuResults[1]
            << ",\"cpu_48_percent\":" << cpuResults[2]
            << ",\"cpu_64_percent\":" << cpuResults[3]
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int halfBandPrototypeReport(const std::filesystem::path& outputPath) {
  struct Configuration {
    std::size_t firstStageTaps;
    std::size_t secondStageTaps;
  };
  constexpr std::array<Configuration, 6> configurations{{{33U, 33U},
                                                         {65U, 33U},
                                                         {97U, 33U},
                                                         {113U, 33U},
                                                         {129U, 33U},
                                                         {97U, 65U}}};
  constexpr std::array<double, 4> targetFrequencies{7000.0, 8500.0, 10000.0,
                                                    15000.0};
  constexpr std::array<std::size_t, 13> schedule{
      1U, 2U, 7U, 16U, 32U, 64U, 127U, 128U, 256U, 511U, 512U, 1024U, 2048U};
  constexpr std::uint32_t sampleRate = 48000U;
  constexpr std::size_t frames = 32768U;
  constexpr std::size_t blockSize = 127U;
  constexpr double renderedSeconds = 5.0;
  const float drive =
      aste::density::mapDensity(productionParameters().density).saturationDrive;
  const bool benchmarkEnabled = std::getenv("ASTE_SKIP_BENCHMARK") == nullptr;

  std::vector<float> blockSource(8192U);
  for (std::size_t sample = 0; sample < blockSource.size(); ++sample) {
    blockSource[sample] = 0.8F * static_cast<float>(std::sin(0.071 * sample));
  }
  std::array<float, blockSize> benchmarkSource{};
  for (std::size_t sample = 0; sample < benchmarkSource.size(); ++sample) {
    benchmarkSource[sample] =
        0.7F * static_cast<float>(std::sin(0.13 * sample));
  }

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create half-band report: " << outputPath << '\n';
    return 1;
  }
  output << "first_stage_taps,second_stage_taps,sample_rate,input_frequency_hz,"
            "latency_samples,max_block_delta,fundamental_delta_db,alias_dbc,"
            "alias_improvement_db,instance_bytes,benchmark_enabled,"
            "rendered_seconds,median_elapsed_seconds,stereo_one_core_percent,"
            "checksum\n"
         << std::fixed << std::setprecision(9);

  std::array<double, configurations.size()> worstAliases{};
  std::array<double, configurations.size()> cpuResults{};
  bool valid = true;
  for (std::size_t configurationIndex = 0;
       configurationIndex < configurations.size(); ++configurationIndex) {
    const auto configuration = configurations[configurationIndex];
    std::array<double, targetFrequencies.size()> frequencies{};
    std::array<double, targetFrequencies.size()> gainDeltas{};
    std::array<double, targetFrequencies.size()> aliases{};
    std::array<double, targetFrequencies.size()> improvements{};
    for (std::size_t tone = 0; tone < targetFrequencies.size(); ++tone) {
      const auto inputBin = static_cast<std::size_t>(
          std::llround(targetFrequencies[tone] * frames / sampleRate));
      frequencies[tone] = static_cast<double>(inputBin) * sampleRate / frames;
      std::vector<float> direct(frames);
      std::vector<float> streaming(2U * frames);
      for (std::size_t sample = 0; sample < streaming.size(); ++sample) {
        const float sine = static_cast<float>(
            std::sin(2.0 * kPi * inputBin * sample / frames));
        streaming[sample] = 0.9F * sine;
        if (sample < frames) {
          direct[sample] = aste::density::controlledClipSample(
              aste::density::saturateSample(0.9F * sine, drive));
        }
      }
      aste::density::CrushOversampler4xHalfBand oversampler;
      oversampler.prepare(configuration.firstStageTaps,
                          configuration.secondStageTaps);
      oversampler.process(streaming.data(), streaming.size(), drive);
      const std::vector<float> candidate(streaming.begin() + frames,
                                         streaming.end());
      const double directFundamental = coherentBinAmplitude(direct, inputBin);
      gainDeltas[tone] =
          20.0 * std::log10(coherentBinAmplitude(candidate, inputBin) /
                            directFundamental);
      const double directAlias = foldedHarmonicsDbc(direct, inputBin);
      aliases[tone] = foldedHarmonicsDbc(candidate, inputBin);
      improvements[tone] = directAlias - aliases[tone];
      valid = valid && std::isfinite(gainDeltas[tone]) &&
              std::isfinite(aliases[tone]) && improvements[tone] > 0.0;
    }

    auto whole = blockSource;
    auto variable = blockSource;
    aste::density::CrushOversampler4xHalfBand wholeProcessor;
    aste::density::CrushOversampler4xHalfBand variableProcessor;
    wholeProcessor.prepare(configuration.firstStageTaps,
                           configuration.secondStageTaps);
    variableProcessor.prepare(configuration.firstStageTaps,
                              configuration.secondStageTaps);
    wholeProcessor.process(whole.data(), whole.size(), drive);
    std::size_t offset{};
    std::size_t block{};
    while (offset < variable.size()) {
      const std::size_t count =
          std::min(schedule[block % schedule.size()], variable.size() - offset);
      variableProcessor.process(variable.data() + offset, count, drive);
      offset += count;
      ++block;
    }
    double maximumBlockDelta{};
    for (std::size_t sample = 0; sample < whole.size(); ++sample) {
      maximumBlockDelta = std::max(
          maximumBlockDelta,
          static_cast<double>(std::abs(whole[sample] - variable[sample])));
    }

    std::array<float, 256> impulse{};
    impulse[0] = 1.0e-3F;
    aste::density::CrushOversampler4xHalfBand impulseProcessor;
    impulseProcessor.prepare(configuration.firstStageTaps,
                             configuration.secondStageTaps);
    impulseProcessor.process(impulse.data(), impulse.size(), 1.0F);
    const auto peak = std::max_element(
        impulse.begin(), impulse.end(), [](float left, float right) {
          return std::abs(left) < std::abs(right);
        });
    const std::size_t measuredLatency =
        static_cast<std::size_t>(std::distance(impulse.begin(), peak));

    std::array<double, 5> timings{};
    double checksum{};
    const std::size_t benchmarkFrames =
        static_cast<std::size_t>(sampleRate * renderedSeconds);
    for (double& timing : timings) {
      if (!benchmarkEnabled) {
        break;
      }
      std::array<float, blockSize> left{};
      std::array<float, blockSize> right{};
      aste::density::CrushOversampler4xHalfBand leftProcessor;
      aste::density::CrushOversampler4xHalfBand rightProcessor;
      leftProcessor.prepare(configuration.firstStageTaps,
                            configuration.secondStageTaps);
      rightProcessor.prepare(configuration.firstStageTaps,
                             configuration.secondStageTaps);
      const auto start = std::chrono::steady_clock::now();
      for (std::size_t rendered = 0; rendered < benchmarkFrames;) {
        const std::size_t count =
            std::min(blockSize, benchmarkFrames - rendered);
        left = benchmarkSource;
        right = benchmarkSource;
        leftProcessor.process(left.data(), count, drive);
        rightProcessor.process(right.data(), count, drive);
        rendered += count;
      }
      timing = std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                             start)
                   .count();
      for (std::size_t sample = 0; sample < blockSize; ++sample) {
        checksum += left[sample] + right[sample];
      }
    }
    std::sort(timings.begin(), timings.end());
    const double elapsed =
        benchmarkEnabled ? timings[timings.size() / 2U] : 0.0;
    const double oneCorePercent =
        benchmarkEnabled ? 100.0 * elapsed / renderedSeconds : 0.0;
    cpuResults[configurationIndex] = oneCorePercent;
    worstAliases[configurationIndex] =
        *std::max_element(aliases.begin(), aliases.end());
    const double maximumGainDelta = std::abs(*std::max_element(
        gainDeltas.begin(), gainDeltas.end(), [](double left, double right) {
          return std::abs(left) < std::abs(right);
        }));
    valid = valid && measuredLatency == impulseProcessor.latencySamples() &&
            maximumBlockDelta == 0.0 && maximumGainDelta < 0.05 &&
            std::isfinite(oneCorePercent);
    for (std::size_t tone = 0; tone < targetFrequencies.size(); ++tone) {
      output << configuration.firstStageTaps << ','
             << configuration.secondStageTaps << ',' << sampleRate << ','
             << frequencies[tone] << ',' << measuredLatency << ','
             << maximumBlockDelta << ',' << gainDeltas[tone] << ','
             << aliases[tone] << ',' << improvements[tone] << ','
             << sizeof(aste::density::CrushOversampler4xHalfBand) << ','
             << (benchmarkEnabled ? 1 : 0) << ',' << renderedSeconds << ','
             << elapsed << ',' << oneCorePercent << ',' << checksum << '\n';
    }
  }
  std::cout << std::fixed << std::setprecision(6)
            << "{\"configurations\":" << configurations.size()
            << ",\"worst_alias_33_33_dbc\":" << worstAliases[0]
            << ",\"worst_alias_113_33_dbc\":" << worstAliases[3]
            << ",\"worst_alias_129_33_dbc\":" << worstAliases[4]
            << ",\"cpu_33_33_percent\":" << cpuResults[0]
            << ",\"cpu_113_33_percent\":" << cpuResults[3]
            << ",\"cpu_129_33_percent\":" << cpuResults[4]
            << ",\"benchmark_enabled\":"
            << (benchmarkEnabled ? "true" : "false")
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int kaiserPrototypeReport(const std::filesystem::path& outputPath) {
  struct Window {
    std::string_view name;
    float beta;
  };
  constexpr std::array<Window, 6> windows{{{"blackman", -1.0F},
                                           {"kaiser-3", 3.0F},
                                           {"kaiser-5", 5.0F},
                                           {"kaiser-7", 7.0F},
                                           {"kaiser-9", 9.0F},
                                           {"kaiser-11", 11.0F}}};
  constexpr std::array<double, 4> targetFrequencies{7000.0, 8500.0, 10000.0,
                                                    15000.0};
  constexpr std::uint32_t sampleRate = 48000U;
  constexpr std::size_t frames = 32768U;
  constexpr std::size_t firstStageTaps = 113U;
  constexpr std::size_t secondStageTaps = 33U;
  const float drive =
      aste::density::mapDensity(productionParameters().density).saturationDrive;

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create Kaiser report: " << outputPath << '\n';
    return 1;
  }
  output << "window,beta,first_stage_taps,second_stage_taps,sample_rate,"
            "input_frequency_hz,latency_samples,fundamental_delta_db,alias_dbc,"
            "alias_improvement_db\n"
         << std::fixed << std::setprecision(9);

  std::array<double, windows.size()> worstAliases{};
  std::array<double, windows.size()> maximumGainDeltas{};
  bool valid = true;
  for (std::size_t windowIndex = 0; windowIndex < windows.size();
       ++windowIndex) {
    const auto window = windows[windowIndex];
    std::array<double, targetFrequencies.size()> aliases{};
    std::array<double, targetFrequencies.size()> gainDeltas{};
    for (std::size_t tone = 0; tone < targetFrequencies.size(); ++tone) {
      const auto inputBin = static_cast<std::size_t>(
          std::llround(targetFrequencies[tone] * frames / sampleRate));
      const double frequency =
          static_cast<double>(inputBin) * sampleRate / frames;
      std::vector<float> direct(frames);
      std::vector<float> streaming(2U * frames);
      for (std::size_t sample = 0; sample < streaming.size(); ++sample) {
        const float sine = static_cast<float>(
            std::sin(2.0 * kPi * inputBin * sample / frames));
        streaming[sample] = 0.9F * sine;
        if (sample < frames) {
          direct[sample] = aste::density::controlledClipSample(
              aste::density::saturateSample(0.9F * sine, drive));
        }
      }
      aste::density::CrushOversampler4xHalfBand oversampler;
      oversampler.prepare(firstStageTaps, secondStageTaps, window.beta);
      oversampler.process(streaming.data(), streaming.size(), drive);
      const std::vector<float> candidate(streaming.begin() + frames,
                                         streaming.end());
      const double directFundamental = coherentBinAmplitude(direct, inputBin);
      gainDeltas[tone] =
          20.0 * std::log10(coherentBinAmplitude(candidate, inputBin) /
                            directFundamental);
      const double directAlias = foldedHarmonicsDbc(direct, inputBin);
      aliases[tone] = foldedHarmonicsDbc(candidate, inputBin);
      const double improvement = directAlias - aliases[tone];
      valid = valid && oversampler.latencySamples() == 64U &&
              std::isfinite(gainDeltas[tone]) && std::isfinite(aliases[tone]) &&
              improvement > 0.0;
      output << window.name << ',' << window.beta << ',' << firstStageTaps
             << ',' << secondStageTaps << ',' << sampleRate << ',' << frequency
             << ',' << oversampler.latencySamples() << ',' << gainDeltas[tone]
             << ',' << aliases[tone] << ',' << improvement << '\n';
    }
    worstAliases[windowIndex] =
        *std::max_element(aliases.begin(), aliases.end());
    maximumGainDeltas[windowIndex] = std::abs(*std::max_element(
        gainDeltas.begin(), gainDeltas.end(), [](double left, double right) {
          return std::abs(left) < std::abs(right);
        }));
    valid = valid && maximumGainDeltas[windowIndex] < 0.05;
  }
  const auto best = std::min_element(worstAliases.begin(), worstAliases.end());
  const std::size_t bestIndex =
      static_cast<std::size_t>(std::distance(worstAliases.begin(), best));
  std::cout << std::fixed << std::setprecision(6)
            << "{\"windows\":" << windows.size()
            << ",\"blackman_worst_alias_dbc\":" << worstAliases[0]
            << ",\"kaiser_3_worst_alias_dbc\":" << worstAliases[1]
            << ",\"kaiser_5_worst_alias_dbc\":" << worstAliases[2]
            << ",\"kaiser_7_worst_alias_dbc\":" << worstAliases[3]
            << ",\"kaiser_9_worst_alias_dbc\":" << worstAliases[4]
            << ",\"kaiser_11_worst_alias_dbc\":" << worstAliases[5]
            << ",\"best_window\":\"" << windows[bestIndex].name << "\""
            << ",\"best_worst_alias_dbc\":" << *best
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

struct KaiserMeasurement {
  double gainDelta;
  double alias;
  double improvement;
  std::size_t latency;
};

KaiserMeasurement measureKaiser(const std::vector<float>& source,
                                std::size_t frames, std::size_t inputBin,
                                float drive, float beta,
                                double directFundamental, double directAlias,
                                std::size_t firstStageTaps = 113U) {
  auto streaming = source;
  aste::density::CrushOversampler4xHalfBand oversampler;
  oversampler.prepare(firstStageTaps, 33U, beta);
  oversampler.process(streaming.data(), streaming.size(), drive);
  const std::vector<float> candidate(streaming.begin() + frames,
                                     streaming.end());
  const double gainDelta =
      20.0 *
      std::log10(coherentBinAmplitude(candidate, inputBin) / directFundamental);
  const double alias = foldedHarmonicsDbc(candidate, inputBin);
  return {gainDelta, alias, directAlias - alias, oversampler.latencySamples()};
}

int kaiserSweepReport(const std::filesystem::path& outputPath) {
  struct Window {
    std::string_view name;
    float beta;
  };
  constexpr std::array<Window, 3> windows{
      {{"blackman", -1.0F}, {"kaiser-3", 3.0F}, {"kaiser-5", 5.0F}}};
  constexpr std::array<float, 3> levels{0.25F, 0.60F, 0.90F};
  constexpr std::uint32_t sampleRate = 48000U;
  const bool fast = std::getenv("ASTE_FAST_RESEARCH") != nullptr;
  const std::size_t frames = fast ? 8192U : 16384U;
  const double frequencyStep = fast ? 4000.0 : 500.0;
  std::vector<double> targetFrequencies;
  for (double frequency = 1000.0; frequency <= 20000.0;
       frequency += frequencyStep) {
    targetFrequencies.push_back(frequency);
  }
  if (targetFrequencies.back() != 20000.0) {
    targetFrequencies.push_back(20000.0);
  }
  const float drive =
      aste::density::mapDensity(productionParameters().density).saturationDrive;

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create Kaiser sweep report: " << outputPath << '\n';
    return 1;
  }
  output << "window,beta,level,target_frequency_hz,input_frequency_hz,"
            "latency_samples,fundamental_delta_db,alias_dbc,"
            "alias_improvement_db\n"
         << std::fixed << std::setprecision(9);

  std::array<std::vector<double>, windows.size()> aliasValues;
  std::array<double, windows.size()> worstAliases{};
  std::array<double, windows.size()> worstFrequencies{};
  std::array<float, windows.size()> worstLevels{};
  std::array<double, windows.size()> maximumGainDeltas{};
  std::array<std::size_t, windows.size()> pointsAboveMinus50{};
  worstAliases.fill(-std::numeric_limits<double>::infinity());
  bool valid = true;

  for (const float level : levels) {
    for (const double targetFrequency : targetFrequencies) {
      const auto inputBin =
          static_cast<std::size_t>(std::llround(
              targetFrequency * static_cast<double>(frames) / sampleRate)) |
          1U;
      const double frequency =
          static_cast<double>(inputBin) * sampleRate / frames;
      std::vector<float> direct(frames);
      std::vector<float> source(2U * frames);
      for (std::size_t sample = 0; sample < source.size(); ++sample) {
        const float sine = static_cast<float>(
            std::sin(2.0 * kPi * inputBin * sample / frames));
        source[sample] = level * sine;
        if (sample < frames) {
          direct[sample] = aste::density::controlledClipSample(
              aste::density::saturateSample(level * sine, drive));
        }
      }
      const double directFundamental = coherentBinAmplitude(direct, inputBin);
      const double directAlias = foldedHarmonicsDbc(direct, inputBin);
      for (std::size_t windowIndex = 0; windowIndex < windows.size();
           ++windowIndex) {
        const auto window = windows[windowIndex];
        const auto measurement =
            measureKaiser(source, frames, inputBin, drive, window.beta,
                          directFundamental, directAlias);
        aliasValues[windowIndex].push_back(measurement.alias);
        maximumGainDeltas[windowIndex] = std::max(
            maximumGainDeltas[windowIndex], std::abs(measurement.gainDelta));
        pointsAboveMinus50[windowIndex] += measurement.alias > -50.0 ? 1U : 0U;
        if (measurement.alias > worstAliases[windowIndex]) {
          worstAliases[windowIndex] = measurement.alias;
          worstFrequencies[windowIndex] = frequency;
          worstLevels[windowIndex] = level;
        }
        valid = valid && measurement.latency == 64U &&
                std::isfinite(measurement.gainDelta) &&
                std::isfinite(measurement.alias) &&
                std::isfinite(measurement.improvement);
        output << window.name << ',' << window.beta << ',' << level << ','
               << targetFrequency << ',' << frequency << ','
               << measurement.latency << ',' << measurement.gainDelta << ','
               << measurement.alias << ',' << measurement.improvement << '\n';
      }
    }
  }

  std::array<double, windows.size()> percentile95{};
  for (std::size_t window = 0; window < windows.size(); ++window) {
    std::sort(aliasValues[window].begin(), aliasValues[window].end());
    const std::size_t index = static_cast<std::size_t>(
        0.95 * static_cast<double>(aliasValues[window].size() - 1U));
    percentile95[window] = aliasValues[window][index];
    valid = valid && maximumGainDeltas[window] < 0.1;
  }
  const auto best = std::min_element(worstAliases.begin(), worstAliases.end());
  const std::size_t bestIndex =
      static_cast<std::size_t>(std::distance(worstAliases.begin(), best));
  std::cout << std::fixed << std::setprecision(6)
            << "{\"fast\":" << (fast ? "true" : "false")
            << ",\"frequencies\":" << targetFrequencies.size()
            << ",\"levels\":" << levels.size()
            << ",\"points_per_window\":" << aliasValues[0].size()
            << ",\"blackman_worst_dbc\":" << worstAliases[0]
            << ",\"kaiser_3_worst_dbc\":" << worstAliases[1]
            << ",\"kaiser_5_worst_dbc\":" << worstAliases[2]
            << ",\"blackman_p95_dbc\":" << percentile95[0]
            << ",\"kaiser_3_p95_dbc\":" << percentile95[1]
            << ",\"kaiser_5_p95_dbc\":" << percentile95[2]
            << ",\"best_window\":\"" << windows[bestIndex].name << "\""
            << ",\"best_worst_frequency_hz\":" << worstFrequencies[bestIndex]
            << ",\"best_worst_level\":" << worstLevels[bestIndex]
            << ",\"blackman_points_above_minus_50\":" << pointsAboveMinus50[0]
            << ",\"kaiser_3_points_above_minus_50\":" << pointsAboveMinus50[1]
            << ",\"kaiser_5_points_above_minus_50\":" << pointsAboveMinus50[2]
            << ",\"max_gain_delta_blackman_db\":" << maximumGainDeltas[0]
            << ",\"max_gain_delta_kaiser_3_db\":" << maximumGainDeltas[1]
            << ",\"max_gain_delta_kaiser_5_db\":" << maximumGainDeltas[2]
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int kaiserRateSweepReport(const std::filesystem::path& outputPath) {
  struct Window {
    std::string_view name;
    float beta;
  };
  constexpr std::array<Window, 2> windows{
      {{"kaiser-3", 3.0F}, {"kaiser-5", 5.0F}}};
  constexpr std::array<std::uint32_t, 6> sampleRates{44100U, 48000U,  88200U,
                                                     96000U, 176400U, 192000U};
  constexpr std::array<float, 3> levels{0.25F, 0.60F, 0.90F};
  const bool fast = std::getenv("ASTE_FAST_RESEARCH") != nullptr;
  const std::size_t frames = fast ? 8192U : 16384U;
  const double frequencyStep = fast ? 4000.0 : 500.0;
  std::vector<double> targetFrequencies;
  for (double frequency = 1000.0; frequency <= 20000.0;
       frequency += frequencyStep) {
    targetFrequencies.push_back(frequency);
  }
  if (targetFrequencies.back() != 20000.0) {
    targetFrequencies.push_back(20000.0);
  }
  const float drive =
      aste::density::mapDensity(productionParameters().density).saturationDrive;

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create Kaiser rate report: " << outputPath << '\n';
    return 1;
  }
  output << "window,beta,sample_rate,level,target_frequency_hz,"
            "input_frequency_hz,latency_samples,fundamental_delta_db,"
            "alias_dbc,alias_improvement_db\n"
         << std::fixed << std::setprecision(9);

  std::array<std::vector<double>, windows.size()> aliases;
  std::array<double, windows.size()> worstAliases{};
  std::array<double, windows.size()> maximumGainDeltas{};
  std::array<std::size_t, windows.size()> pointsAboveMinus50{};
  std::array<std::size_t, windows.size()> percentileWins{};
  worstAliases.fill(-std::numeric_limits<double>::infinity());
  bool valid = true;

  for (const auto sampleRate : sampleRates) {
    std::array<std::vector<double>, windows.size()> rateAliases;
    for (const float level : levels) {
      for (const double targetFrequency : targetFrequencies) {
        const auto inputBin = static_cast<std::size_t>(std::llround(
                                  targetFrequency * frames / sampleRate)) |
                              1U;
        const double frequency =
            static_cast<double>(inputBin) * sampleRate / frames;
        std::vector<float> direct(frames);
        std::vector<float> source(2U * frames);
        for (std::size_t sample = 0; sample < source.size(); ++sample) {
          const float sine = static_cast<float>(
              std::sin(2.0 * kPi * inputBin * sample / frames));
          source[sample] = level * sine;
          if (sample < frames) {
            direct[sample] = aste::density::controlledClipSample(
                aste::density::saturateSample(level * sine, drive));
          }
        }
        const double directFundamental = coherentBinAmplitude(direct, inputBin);
        const double directAlias = foldedHarmonicsDbc(direct, inputBin);
        for (std::size_t window = 0; window < windows.size(); ++window) {
          const auto measurement = measureKaiser(
              source, frames, inputBin, drive, windows[window].beta,
              directFundamental, directAlias);
          aliases[window].push_back(measurement.alias);
          rateAliases[window].push_back(measurement.alias);
          worstAliases[window] =
              std::max(worstAliases[window], measurement.alias);
          maximumGainDeltas[window] = std::max(maximumGainDeltas[window],
                                               std::abs(measurement.gainDelta));
          pointsAboveMinus50[window] += measurement.alias > -50.0 ? 1U : 0U;
          valid = valid && measurement.latency == 64U &&
                  std::isfinite(measurement.gainDelta) &&
                  std::isfinite(measurement.alias) &&
                  std::isfinite(measurement.improvement);
          output << windows[window].name << ',' << windows[window].beta << ','
                 << sampleRate << ',' << level << ',' << targetFrequency << ','
                 << frequency << ',' << measurement.latency << ','
                 << measurement.gainDelta << ',' << measurement.alias << ','
                 << measurement.improvement << '\n';
        }
      }
    }
    for (auto& values : rateAliases) {
      std::sort(values.begin(), values.end());
    }
    const std::size_t index = static_cast<std::size_t>(
        0.95 * static_cast<double>(rateAliases[0].size() - 1U));
    ++percentileWins[rateAliases[0][index] < rateAliases[1][index] ? 0U : 1U];
  }

  std::array<double, windows.size()> percentile95{};
  for (std::size_t window = 0; window < windows.size(); ++window) {
    std::sort(aliases[window].begin(), aliases[window].end());
    const std::size_t index = static_cast<std::size_t>(
        0.95 * static_cast<double>(aliases[window].size() - 1U));
    percentile95[window] = aliases[window][index];
  }
  const bool withinGainLimit =
      maximumGainDeltas[0] < 0.1 && maximumGainDeltas[1] < 0.1;
  std::cout << std::fixed << std::setprecision(6)
            << "{\"fast\":" << (fast ? "true" : "false")
            << ",\"sample_rates\":" << sampleRates.size()
            << ",\"frequencies\":" << targetFrequencies.size()
            << ",\"levels\":" << levels.size()
            << ",\"points_per_window\":" << aliases[0].size()
            << ",\"kaiser_3_worst_dbc\":" << worstAliases[0]
            << ",\"kaiser_5_worst_dbc\":" << worstAliases[1]
            << ",\"kaiser_3_p95_dbc\":" << percentile95[0]
            << ",\"kaiser_5_p95_dbc\":" << percentile95[1]
            << ",\"kaiser_3_rate_wins\":" << percentileWins[0]
            << ",\"kaiser_5_rate_wins\":" << percentileWins[1]
            << ",\"kaiser_3_points_above_minus_50\":" << pointsAboveMinus50[0]
            << ",\"kaiser_5_points_above_minus_50\":" << pointsAboveMinus50[1]
            << ",\"max_gain_delta_kaiser_3_db\":" << maximumGainDeltas[0]
            << ",\"max_gain_delta_kaiser_5_db\":" << maximumGainDeltas[1]
            << ",\"within_gain_limit\":" << (withinGainLimit ? "true" : "false")
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

struct KaiserLinearMeasurement {
  double frequency;
  double magnitude;
  double phaseDegrees;
  double phaseResidual;
  std::size_t latency;
};

KaiserLinearMeasurement measureKaiserLinear(std::uint32_t sampleRate,
                                            std::size_t frames,
                                            double targetFrequency, float beta,
                                            std::size_t firstStageTaps = 113U) {
  const auto inputBin = static_cast<std::size_t>(std::llround(
                            targetFrequency * frames / sampleRate)) |
                        1U;
  const double frequency = static_cast<double>(inputBin) * sampleRate / frames;
  std::vector<float> source(2U * frames);
  std::vector<float> input(frames);
  for (std::size_t sample = 0; sample < source.size(); ++sample) {
    const float sine =
        0.25F *
        static_cast<float>(std::sin(2.0 * kPi * inputBin * sample / frames));
    source[sample] = sine;
    if (sample < frames) {
      input[sample] = sine;
    }
  }
  const auto inputValue = coherentBin(input, inputBin);
  const double inputAmplitude = coherentBinAmplitude(input, inputBin);
  aste::density::CrushOversampler4xHalfBand oversampler;
  oversampler.prepare(firstStageTaps, 33U, beta);
  for (float& sample : source) {
    sample = oversampler.processLinearSample(sample);
  }
  const std::vector<float> candidate(source.begin() + frames, source.end());
  const auto outputValue = coherentBin(candidate, inputBin);
  const double magnitude =
      20.0 *
      std::log10(coherentBinAmplitude(candidate, inputBin) / inputAmplitude);
  const double expectedPhase =
      -2.0 * kPi * frequency * oversampler.latencySamples() / sampleRate;
  double phase = std::atan2(outputValue[1], outputValue[0]) -
                 std::atan2(inputValue[1], inputValue[0]);
  phase += 2.0 * kPi * std::round((expectedPhase - phase) / (2.0 * kPi));
  return {frequency, magnitude, phase * 180.0 / kPi,
          (phase - expectedPhase) * 180.0 / kPi, oversampler.latencySamples()};
}

int kaiserLinearReport(const std::filesystem::path& outputPath) {
  struct Window {
    std::string_view name;
    float beta;
  };
  constexpr std::array<Window, 2> windows{
      {{"kaiser-3", 3.0F}, {"kaiser-5", 5.0F}}};
  constexpr std::array<std::uint32_t, 6> sampleRates{44100U, 48000U,  88200U,
                                                     96000U, 176400U, 192000U};
  const bool fast = std::getenv("ASTE_FAST_RESEARCH") != nullptr;
  const std::size_t frames = fast ? 8192U : 16384U;
  const double frequencyStep = fast ? 4000.0 : 250.0;
  std::vector<double> targetFrequencies;
  for (double frequency = 1000.0; frequency <= 20000.0;
       frequency += frequencyStep) {
    targetFrequencies.push_back(frequency);
  }
  if (targetFrequencies.back() != 20000.0) {
    targetFrequencies.push_back(20000.0);
  }

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create Kaiser linear report: " << outputPath << '\n';
    return 1;
  }
  output << "window,beta,sample_rate,target_frequency_hz,input_frequency_hz,"
            "latency_samples,magnitude_db,phase_degrees,"
            "latency_compensated_phase_degrees\n"
         << std::fixed << std::setprecision(9);

  std::array<double, windows.size()> maximumMagnitude{};
  std::array<double, windows.size()> maximumHighBandMagnitude{};
  std::array<double, windows.size()> maximumPhaseResidual{};
  bool valid = true;
  for (const auto sampleRate : sampleRates) {
    for (const double targetFrequency : targetFrequencies) {
      for (std::size_t window = 0; window < windows.size(); ++window) {
        const auto measurement = measureKaiserLinear(
            sampleRate, frames, targetFrequency, windows[window].beta);
        maximumMagnitude[window] =
            std::max(maximumMagnitude[window], std::abs(measurement.magnitude));
        if (sampleRate == 44100U && targetFrequency >= 18000.0) {
          maximumHighBandMagnitude[window] =
              std::max(maximumHighBandMagnitude[window],
                       std::abs(measurement.magnitude));
        }
        maximumPhaseResidual[window] = std::max(
            maximumPhaseResidual[window], std::abs(measurement.phaseResidual));
        valid = valid && measurement.latency == 64U &&
                std::isfinite(measurement.magnitude) &&
                std::isfinite(measurement.phaseDegrees) &&
                std::isfinite(measurement.phaseResidual);
        output << windows[window].name << ',' << windows[window].beta << ','
               << sampleRate << ',' << targetFrequency << ','
               << measurement.frequency << ',' << measurement.latency << ','
               << measurement.magnitude << ',' << measurement.phaseDegrees
               << ',' << measurement.phaseResidual << '\n';
      }
    }
  }
  const bool withinMagnitudeLimit =
      maximumMagnitude[0] < 0.1 && maximumMagnitude[1] < 0.1;
  std::cout
      << std::fixed << std::setprecision(6)
      << "{\"fast\":" << (fast ? "true" : "false")
      << ",\"sample_rates\":" << sampleRates.size()
      << ",\"frequencies\":" << targetFrequencies.size()
      << ",\"points_per_window\":"
      << sampleRates.size() * targetFrequencies.size()
      << ",\"max_magnitude_kaiser_3_db\":" << maximumMagnitude[0]
      << ",\"max_magnitude_kaiser_5_db\":" << maximumMagnitude[1]
      << ",\"max_44k_highband_kaiser_3_db\":" << maximumHighBandMagnitude[0]
      << ",\"max_44k_highband_kaiser_5_db\":" << maximumHighBandMagnitude[1]
      << ",\"max_phase_residual_kaiser_3_degrees\":" << maximumPhaseResidual[0]
      << ",\"max_phase_residual_kaiser_5_degrees\":" << maximumPhaseResidual[1]
      << ",\"within_magnitude_limit\":"
      << (withinMagnitudeLimit ? "true" : "false")
      << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int kaiserLengthReport(const std::filesystem::path& outputPath) {
  constexpr std::array<std::size_t, 6> tapCounts{65U, 73U, 81U, 89U, 97U, 113U};
  constexpr std::uint32_t aliasSampleRate = 48000U;
  constexpr std::uint32_t linearSampleRate = 44100U;
  constexpr std::size_t blockSize = 127U;
  constexpr double renderedSeconds = 5.0;
  const bool fast = std::getenv("ASTE_FAST_RESEARCH") != nullptr;
  const bool benchmarkEnabled = std::getenv("ASTE_SKIP_BENCHMARK") == nullptr;
  const std::size_t frames = fast ? 8192U : 16384U;
  const auto makeFrequencies = [](double step) {
    std::vector<double> frequencies;
    for (double frequency = 1000.0; frequency <= 20000.0; frequency += step) {
      frequencies.push_back(frequency);
    }
    if (frequencies.back() != 20000.0) {
      frequencies.push_back(20000.0);
    }
    return frequencies;
  };
  const auto aliasFrequencies = makeFrequencies(fast ? 4000.0 : 500.0);
  const auto linearFrequencies = makeFrequencies(fast ? 4000.0 : 250.0);
  constexpr std::array<float, 3> levels{0.25F, 0.60F, 0.90F};
  const float drive =
      aste::density::mapDensity(productionParameters().density).saturationDrive;

  std::array<float, blockSize> benchmarkSource{};
  for (std::size_t sample = 0; sample < benchmarkSource.size(); ++sample) {
    benchmarkSource[sample] =
        0.7F * static_cast<float>(std::sin(0.13 * sample));
  }
  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create Kaiser length report: " << outputPath << '\n';
    return 1;
  }
  output << "first_stage_taps,second_stage_taps,latency_samples,"
            "alias_worst_dbc,alias_p95_dbc,points_above_minus_50,"
            "max_linear_magnitude_44k_db,max_phase_residual_degrees,"
            "benchmark_enabled,median_elapsed_seconds,"
            "stereo_one_core_percent,checksum\n"
         << std::fixed << std::setprecision(9);

  std::array<double, tapCounts.size()> aliases95{};
  std::array<double, tapCounts.size()> maximumMagnitudes{};
  std::array<double, tapCounts.size()> cpuResults{};
  bool valid = true;
  for (std::size_t configuration = 0; configuration < tapCounts.size();
       ++configuration) {
    const std::size_t taps = tapCounts[configuration];
    std::vector<double> aliases;
    double worstAlias = -std::numeric_limits<double>::infinity();
    std::size_t pointsAboveMinus50{};
    std::size_t latency{};
    for (const float level : levels) {
      for (const double targetFrequency : aliasFrequencies) {
        const auto inputBin = static_cast<std::size_t>(std::llround(
                                  targetFrequency * frames / aliasSampleRate)) |
                              1U;
        std::vector<float> direct(frames);
        std::vector<float> source(2U * frames);
        for (std::size_t sample = 0; sample < source.size(); ++sample) {
          const float sine = static_cast<float>(
              std::sin(2.0 * kPi * inputBin * sample / frames));
          source[sample] = level * sine;
          if (sample < frames) {
            direct[sample] = aste::density::controlledClipSample(
                aste::density::saturateSample(level * sine, drive));
          }
        }
        const double directFundamental = coherentBinAmplitude(direct, inputBin);
        const double directAlias = foldedHarmonicsDbc(direct, inputBin);
        const auto measurement =
            measureKaiser(source, frames, inputBin, drive, 5.0F,
                          directFundamental, directAlias, taps);
        aliases.push_back(measurement.alias);
        worstAlias = std::max(worstAlias, measurement.alias);
        pointsAboveMinus50 += measurement.alias > -50.0 ? 1U : 0U;
        latency = measurement.latency;
        valid = valid && std::isfinite(measurement.alias) &&
                std::isfinite(measurement.gainDelta) &&
                std::isfinite(measurement.improvement);
      }
    }
    std::sort(aliases.begin(), aliases.end());
    const std::size_t percentileIndex = static_cast<std::size_t>(
        0.95 * static_cast<double>(aliases.size() - 1U));
    aliases95[configuration] = aliases[percentileIndex];

    double maximumMagnitude{};
    double maximumPhaseResidual{};
    for (const double targetFrequency : linearFrequencies) {
      const auto measurement = measureKaiserLinear(linearSampleRate, frames,
                                                   targetFrequency, 5.0F, taps);
      maximumMagnitude =
          std::max(maximumMagnitude, std::abs(measurement.magnitude));
      maximumPhaseResidual =
          std::max(maximumPhaseResidual, std::abs(measurement.phaseResidual));
      valid = valid && std::isfinite(measurement.magnitude) &&
              std::isfinite(measurement.phaseResidual) &&
              measurement.latency == latency;
    }
    maximumMagnitudes[configuration] = maximumMagnitude;

    std::array<double, 5> timings{};
    double checksum{};
    const std::size_t benchmarkFrames =
        static_cast<std::size_t>(aliasSampleRate * renderedSeconds);
    for (double& timing : timings) {
      if (!benchmarkEnabled) {
        break;
      }
      std::array<float, blockSize> left{};
      std::array<float, blockSize> right{};
      aste::density::CrushOversampler4xHalfBand leftProcessor;
      aste::density::CrushOversampler4xHalfBand rightProcessor;
      leftProcessor.prepare(taps, 33U, 5.0F);
      rightProcessor.prepare(taps, 33U, 5.0F);
      const auto start = std::chrono::steady_clock::now();
      for (std::size_t rendered = 0; rendered < benchmarkFrames;) {
        const std::size_t count =
            std::min(blockSize, benchmarkFrames - rendered);
        left = benchmarkSource;
        right = benchmarkSource;
        leftProcessor.process(left.data(), count, drive);
        rightProcessor.process(right.data(), count, drive);
        rendered += count;
      }
      timing = std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                             start)
                   .count();
      for (std::size_t sample = 0; sample < blockSize; ++sample) {
        checksum += left[sample] + right[sample];
      }
    }
    std::sort(timings.begin(), timings.end());
    const double elapsed =
        benchmarkEnabled ? timings[timings.size() / 2U] : 0.0;
    const double oneCorePercent =
        benchmarkEnabled ? 100.0 * elapsed / renderedSeconds : 0.0;
    cpuResults[configuration] = oneCorePercent;
    valid =
        valid && std::isfinite(oneCorePercent) && maximumPhaseResidual < 0.001;
    output << taps << ",33," << latency << ',' << worstAlias << ','
           << aliases95[configuration] << ',' << pointsAboveMinus50 << ','
           << maximumMagnitude << ',' << maximumPhaseResidual << ','
           << (benchmarkEnabled ? 1 : 0) << ',' << elapsed << ','
           << oneCorePercent << ',' << checksum << '\n';
  }

  std::cout << std::fixed << std::setprecision(6)
            << "{\"fast\":" << (fast ? "true" : "false")
            << ",\"configurations\":" << tapCounts.size()
            << ",\"alias_frequencies\":" << aliasFrequencies.size()
            << ",\"linear_frequencies\":" << linearFrequencies.size()
            << ",\"benchmark_enabled\":"
            << (benchmarkEnabled ? "true" : "false")
            << ",\"p95_65_dbc\":" << aliases95[0]
            << ",\"p95_73_dbc\":" << aliases95[1]
            << ",\"p95_81_dbc\":" << aliases95[2]
            << ",\"p95_113_dbc\":" << aliases95[5]
            << ",\"magnitude_65_db\":" << maximumMagnitudes[0]
            << ",\"magnitude_73_db\":" << maximumMagnitudes[1]
            << ",\"magnitude_81_db\":" << maximumMagnitudes[2]
            << ",\"magnitude_113_db\":" << maximumMagnitudes[5]
            << ",\"cpu_65_percent\":" << cpuResults[0]
            << ",\"cpu_73_percent\":" << cpuResults[1]
            << ",\"cpu_81_percent\":" << cpuResults[2]
            << ",\"cpu_113_percent\":" << cpuResults[5]
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int kaiserFinalistReport(const std::filesystem::path& outputPath) {
  constexpr std::array<std::size_t, 2> tapCounts{73U, 81U};
  constexpr std::array<std::uint32_t, 6> sampleRates{44100U, 48000U,  88200U,
                                                     96000U, 176400U, 192000U};
  constexpr std::array<float, 3> levels{0.25F, 0.60F, 0.90F};
  const bool fast = std::getenv("ASTE_FAST_RESEARCH") != nullptr;
  const std::size_t frames = fast ? 8192U : 16384U;
  const auto makeFrequencies = [](double step) {
    std::vector<double> frequencies;
    for (double frequency = 1000.0; frequency <= 20000.0; frequency += step) {
      frequencies.push_back(frequency);
    }
    if (frequencies.back() != 20000.0) {
      frequencies.push_back(20000.0);
    }
    return frequencies;
  };
  const auto aliasFrequencies = makeFrequencies(fast ? 4000.0 : 500.0);
  const auto linearFrequencies = makeFrequencies(fast ? 4000.0 : 250.0);
  const float drive =
      aste::density::mapDensity(productionParameters().density).saturationDrive;

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create Kaiser finalist report: " << outputPath << '\n';
    return 1;
  }
  output << "first_stage_taps,second_stage_taps,sample_rate,latency_samples,"
            "alias_worst_dbc,alias_p95_dbc,points_above_minus_50,"
            "max_linear_magnitude_db,max_phase_residual_degrees\n"
         << std::fixed << std::setprecision(9);

  std::array<std::vector<double>, tapCounts.size()> aggregateAliases;
  std::array<std::size_t, tapCounts.size()> aggregatePointsAboveMinus50{};
  std::array<double, tapCounts.size()> aggregateMaximumMagnitude{};
  std::array<double, tapCounts.size()> aggregateMaximumPhaseResidual{};
  bool valid = true;
  for (std::size_t configuration = 0; configuration < tapCounts.size();
       ++configuration) {
    const std::size_t taps = tapCounts[configuration];
    for (const auto sampleRate : sampleRates) {
      std::vector<double> aliases;
      double worstAlias = -std::numeric_limits<double>::infinity();
      std::size_t pointsAboveMinus50{};
      std::size_t latency{};
      for (const float level : levels) {
        for (const double targetFrequency : aliasFrequencies) {
          const auto inputBin = static_cast<std::size_t>(std::llround(
                                    targetFrequency * frames / sampleRate)) |
                                1U;
          std::vector<float> direct(frames);
          std::vector<float> source(2U * frames);
          for (std::size_t sample = 0; sample < source.size(); ++sample) {
            const float sine = static_cast<float>(
                std::sin(2.0 * kPi * inputBin * sample / frames));
            source[sample] = level * sine;
            if (sample < frames) {
              direct[sample] = aste::density::controlledClipSample(
                  aste::density::saturateSample(level * sine, drive));
            }
          }
          const double directFundamental =
              coherentBinAmplitude(direct, inputBin);
          const double directAlias = foldedHarmonicsDbc(direct, inputBin);
          const auto measurement =
              measureKaiser(source, frames, inputBin, drive, 5.0F,
                            directFundamental, directAlias, taps);
          aliases.push_back(measurement.alias);
          aggregateAliases[configuration].push_back(measurement.alias);
          worstAlias = std::max(worstAlias, measurement.alias);
          pointsAboveMinus50 += measurement.alias > -50.0 ? 1U : 0U;
          latency = measurement.latency;
          valid = valid && std::isfinite(measurement.alias) &&
                  std::isfinite(measurement.gainDelta) &&
                  std::isfinite(measurement.improvement);
        }
      }
      std::sort(aliases.begin(), aliases.end());
      const std::size_t percentileIndex = static_cast<std::size_t>(
          0.95 * static_cast<double>(aliases.size() - 1U));
      const double percentile95 = aliases[percentileIndex];
      aggregatePointsAboveMinus50[configuration] += pointsAboveMinus50;

      double maximumMagnitude{};
      double maximumPhaseResidual{};
      for (const double targetFrequency : linearFrequencies) {
        const auto measurement = measureKaiserLinear(
            sampleRate, frames, targetFrequency, 5.0F, taps);
        maximumMagnitude =
            std::max(maximumMagnitude, std::abs(measurement.magnitude));
        maximumPhaseResidual =
            std::max(maximumPhaseResidual, std::abs(measurement.phaseResidual));
        valid = valid && measurement.latency == latency &&
                std::isfinite(measurement.magnitude) &&
                std::isfinite(measurement.phaseResidual);
      }
      aggregateMaximumMagnitude[configuration] =
          std::max(aggregateMaximumMagnitude[configuration], maximumMagnitude);
      aggregateMaximumPhaseResidual[configuration] = std::max(
          aggregateMaximumPhaseResidual[configuration], maximumPhaseResidual);
      output << taps << ",33," << sampleRate << ',' << latency << ','
             << worstAlias << ',' << percentile95 << ',' << pointsAboveMinus50
             << ',' << maximumMagnitude << ',' << maximumPhaseResidual << '\n';
    }
  }

  std::array<double, tapCounts.size()> aggregatePercentile95{};
  for (std::size_t configuration = 0; configuration < tapCounts.size();
       ++configuration) {
    auto& aliases = aggregateAliases[configuration];
    std::sort(aliases.begin(), aliases.end());
    const std::size_t percentileIndex = static_cast<std::size_t>(
        0.95 * static_cast<double>(aliases.size() - 1U));
    aggregatePercentile95[configuration] = aliases[percentileIndex];
    valid = valid && aggregateMaximumMagnitude[configuration] < 0.1 &&
            aggregateMaximumPhaseResidual[configuration] < 0.001;
  }
  std::cout << std::fixed << std::setprecision(6)
            << "{\"fast\":" << (fast ? "true" : "false")
            << ",\"configurations\":" << tapCounts.size()
            << ",\"sample_rates\":" << sampleRates.size()
            << ",\"alias_points_per_configuration\":"
            << aggregateAliases[0].size()
            << ",\"linear_points_per_configuration\":"
            << sampleRates.size() * linearFrequencies.size()
            << ",\"p95_73_dbc\":" << aggregatePercentile95[0]
            << ",\"p95_81_dbc\":" << aggregatePercentile95[1]
            << ",\"points_above_minus_50_73\":"
            << aggregatePointsAboveMinus50[0]
            << ",\"points_above_minus_50_81\":"
            << aggregatePointsAboveMinus50[1]
            << ",\"max_magnitude_73_db\":" << aggregateMaximumMagnitude[0]
            << ",\"max_magnitude_81_db\":" << aggregateMaximumMagnitude[1]
            << ",\"max_phase_residual_73_degrees\":"
            << aggregateMaximumPhaseResidual[0]
            << ",\"max_phase_residual_81_degrees\":"
            << aggregateMaximumPhaseResidual[1]
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int oversampledChainReport(const std::filesystem::path& outputPath) {
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t blockSize = 128U;
  constexpr double renderedSeconds = 120.0;
  constexpr std::size_t impulseInput = 3U;
  const bool benchmarkEnabled = std::getenv("ASTE_SKIP_BENCHMARK") == nullptr;

  auto impulseLatency = [](float blend) {
    aste::density::Parameters parameters;
    parameters.driveDb = 0.0F;
    parameters.crush = 0.0F;
    parameters.density = 0.0F;
    parameters.blend = blend;
    parameters.outputDb = 0.0F;
    parameters.protection = false;
    aste::density::Processor processor;
    processor.prepareOversamplingPrototype(sampleRate, parameters);
    std::array<float, 128> impulse{};
    impulse[impulseInput] = 0.1F;
    processor.process(impulse.data(), nullptr, impulse.size(), parameters);
    const auto peak = std::max_element(
        impulse.begin(), impulse.end(), [](float left, float right) {
          return std::abs(left) < std::abs(right);
        });
    return static_cast<std::size_t>(std::distance(impulse.begin(), peak)) -
           impulseInput;
  };
  const std::size_t dryLatency = impulseLatency(0.0F);
  const std::size_t wetLatency = impulseLatency(1.0F);

  constexpr std::array<std::size_t, 1> referenceSchedule{127U};
  constexpr std::array<std::size_t, 13> variableSchedule{
      1U, 2U, 7U, 16U, 32U, 64U, 127U, 128U, 256U, 511U, 512U, 1024U, 2048U};
  auto reference = makeConsistencyFixture(48000U);
  auto variable = reference;
  auto parameters = productionParameters();
  aste::density::Processor referenceProcessor;
  aste::density::Processor variableProcessor;
  referenceProcessor.prepareOversamplingPrototype(sampleRate, parameters);
  variableProcessor.prepareOversamplingPrototype(sampleRate, parameters);
  processInBlocks(referenceProcessor, reference[0], reference[1],
                  referenceSchedule, parameters);
  processInBlocks(variableProcessor, variable[0], variable[1], variableSchedule,
                  parameters);
  double maximumBlockDelta{};
  for (std::size_t sample = 0; sample < reference[0].size(); ++sample) {
    maximumBlockDelta =
        std::max({maximumBlockDelta,
                  static_cast<double>(
                      std::abs(reference[0][sample] - variable[0][sample])),
                  static_cast<double>(
                      std::abs(reference[1][sample] - variable[1][sample]))});
  }

  const auto benchmark = [&](bool oversampled) {
    std::array<double, 5> timings{};
    double checksum{};
    constexpr std::size_t blocks = static_cast<std::size_t>(
        sampleRate * renderedSeconds / static_cast<double>(blockSize));
    for (double& timing : timings) {
      if (!benchmarkEnabled) {
        break;
      }
      std::array<float, blockSize> left{};
      std::array<float, blockSize> right{};
      for (std::size_t sample = 0; sample < blockSize; ++sample) {
        left[sample] = 0.7F * static_cast<float>(std::sin(0.13 * sample));
        right[sample] = 0.5F * static_cast<float>(std::sin(0.17 * sample));
      }
      auto benchmarkParameters = productionParameters();
      aste::density::Processor processor;
      if (oversampled) {
        processor.prepareOversamplingPrototype(sampleRate, benchmarkParameters);
      } else {
        processor.prepare(sampleRate, benchmarkParameters);
      }
      const auto start = std::chrono::steady_clock::now();
      for (std::size_t block = 0; block < blocks; ++block) {
        benchmarkParameters.density = block % 2U == 0U ? 0.2F : 0.9F;
        benchmarkParameters.stereoLink = block % 2U == 0U ? 0.0F : 1.0F;
        processor.process(left.data(), right.data(), blockSize,
                          benchmarkParameters);
      }
      timing = std::chrono::duration<double>(std::chrono::steady_clock::now() -
                                             start)
                   .count();
      for (std::size_t sample = 0; sample < blockSize; ++sample) {
        checksum += left[sample] + right[sample];
      }
    }
    std::sort(timings.begin(), timings.end());
    const double elapsed =
        benchmarkEnabled ? timings[timings.size() / 2U] : 0.0;
    return std::array<double, 3>{
        elapsed, benchmarkEnabled ? 100.0 * elapsed / renderedSeconds : 0.0,
        checksum};
  };
  const auto defaultBenchmark = benchmark(false);
  const auto oversampledBenchmark = benchmark(true);

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create oversampled chain report: " << outputPath
              << '\n';
    return 1;
  }
  output << "latency_samples,dry_latency_samples,wet_latency_samples,"
            "max_block_delta,benchmark_enabled,default_elapsed_seconds,"
            "default_one_core_percent,oversampled_elapsed_seconds,"
            "oversampled_one_core_percent,cpu_delta_percent,default_checksum,"
            "oversampled_checksum\n"
         << std::fixed << std::setprecision(9) << 44U << ',' << dryLatency
         << ',' << wetLatency << ',' << maximumBlockDelta << ','
         << (benchmarkEnabled ? 1 : 0) << ',' << defaultBenchmark[0] << ','
         << defaultBenchmark[1] << ',' << oversampledBenchmark[0] << ','
         << oversampledBenchmark[1] << ','
         << oversampledBenchmark[1] - defaultBenchmark[1] << ','
         << defaultBenchmark[2] << ',' << oversampledBenchmark[2] << '\n';

  const bool valid = dryLatency == 44U && wetLatency == 44U &&
                     referenceProcessor.latencySamples() == 44U &&
                     variableProcessor.latencySamples() == 44U &&
                     maximumBlockDelta == 0.0 &&
                     std::isfinite(defaultBenchmark[1]) &&
                     std::isfinite(oversampledBenchmark[1]);
  std::cout << std::fixed << std::setprecision(6)
            << "{\"latency_samples\":44,\"dry_latency_samples\":" << dryLatency
            << ",\"wet_latency_samples\":" << wetLatency
            << ",\"max_block_delta\":" << maximumBlockDelta
            << ",\"benchmark_enabled\":"
            << (benchmarkEnabled ? "true" : "false")
            << ",\"default_cpu_percent\":" << defaultBenchmark[1]
            << ",\"oversampled_cpu_percent\":" << oversampledBenchmark[1]
            << ",\"cpu_delta_percent\":"
            << oversampledBenchmark[1] - defaultBenchmark[1]
            << ",\"within_default_budget\":"
            << (benchmarkEnabled && oversampledBenchmark[1] < 1.0 ? "true"
                                                                  : "false")
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int oversamplingAuditions(const std::filesystem::path& directory) {
  constexpr std::uint32_t sampleRate = 48000U;
  constexpr std::size_t latency = 44U;
  constexpr std::array<std::size_t, 1> schedule{127U};
  const auto audioDirectory = directory / "audio";
  std::error_code error;
  std::filesystem::create_directories(audioDirectory, error);
  std::ofstream answers(directory / "answer-key.csv");
  std::ofstream responses(directory / "responses.csv");
  std::ofstream measurements(directory / "measurements.csv");
  if (error || !answers || !responses || !measurements) {
    std::cerr << "cannot create oversampling audition pack: " << directory
              << '\n';
    return 1;
  }
  answers << "fixture,A,B\n";
  responses << "fixture,preferred(A/B/no preference),confidence(0-3),notes\n";
  measurements
      << "fixture,frames,default_match_db,oversampled_match_db,match_error_db,"
         "peak_dbfs,null_rms_dbfs,latency_samples\n"
      << std::fixed << std::setprecision(9);

  std::uint32_t randomState = 0xD0132U;
  double maximumMatchError{};
  double minimumNullDb = std::numeric_limits<double>::infinity();
  double maximumNullDb = -minimumNullDb;
  bool valid = true;
  std::size_t fixtureCount{};
  for (std::string_view fixture : std::array<std::string_view, 4>{
           "transient", "bass", "dense", "ambient"}) {
    auto inputLeft = makeFixture(fixture);
    std::vector<float> inputRight(inputLeft.size());
    const std::size_t shift = fixture == "transient" ? 23U
                              : fixture == "dense"   ? 113U
                              : fixture == "ambient" ? 509U
                                                     : 0U;
    for (std::size_t sample = 0; sample < inputLeft.size(); ++sample) {
      const float delayed = sample >= shift ? inputLeft[sample - shift] : 0.0F;
      if (fixture == "transient") {
        inputRight[sample] = 0.72F * delayed;
      } else if (fixture == "bass") {
        inputRight[sample] = 0.82F * inputLeft[sample];
      } else if (fixture == "dense") {
        inputRight[sample] = 0.65F * inputLeft[sample] + 0.28F * delayed;
      } else {
        inputRight[sample] = 0.55F * inputLeft[sample] + 0.35F * delayed;
      }
    }

    auto defaultLeft = inputLeft;
    auto defaultRight = inputRight;
    auto oversampledLeft = inputLeft;
    auto oversampledRight = inputRight;
    oversampledLeft.resize(inputLeft.size() + latency, 0.0F);
    oversampledRight.resize(inputRight.size() + latency, 0.0F);
    const auto parameters = productionParameters();
    aste::density::Processor defaultProcessor;
    aste::density::Processor oversampledProcessor;
    defaultProcessor.prepare(sampleRate, parameters);
    oversampledProcessor.prepareOversamplingPrototype(sampleRate, parameters);
    processInBlocks(defaultProcessor, defaultLeft, defaultRight, schedule,
                    parameters);
    processInBlocks(oversampledProcessor, oversampledLeft, oversampledRight,
                    schedule, parameters);

    std::vector<float> alignedDefaultLeft(oversampledLeft.size());
    std::vector<float> alignedDefaultRight(oversampledRight.size());
    std::copy(defaultLeft.begin(), defaultLeft.end(),
              alignedDefaultLeft.begin() + latency);
    std::copy(defaultRight.begin(), defaultRight.end(),
              alignedDefaultRight.begin() + latency);
    const double defaultRms =
        stereoRms(alignedDefaultLeft, alignedDefaultRight);
    const double oversampledRms = stereoRms(oversampledLeft, oversampledRight);
    const double targetRms = std::min(defaultRms, oversampledRms);
    const float defaultMatch = static_cast<float>(targetRms / defaultRms);
    const float oversampledMatch =
        static_cast<float>(targetRms / oversampledRms);
    for (std::size_t sample = 0; sample < oversampledLeft.size(); ++sample) {
      alignedDefaultLeft[sample] *= defaultMatch;
      alignedDefaultRight[sample] *= defaultMatch;
      oversampledLeft[sample] *= oversampledMatch;
      oversampledRight[sample] *= oversampledMatch;
    }
    const float commonGain = static_cast<float>(
        0.8912509381337456 /
        std::max(stereoPeak(alignedDefaultLeft, alignedDefaultRight),
                 stereoPeak(oversampledLeft, oversampledRight)));
    double nullEnergy{};
    for (std::size_t sample = 0; sample < oversampledLeft.size(); ++sample) {
      alignedDefaultLeft[sample] *= commonGain;
      alignedDefaultRight[sample] *= commonGain;
      oversampledLeft[sample] *= commonGain;
      oversampledRight[sample] *= commonGain;
      const double leftDifference =
          alignedDefaultLeft[sample] - oversampledLeft[sample];
      const double rightDifference =
          alignedDefaultRight[sample] - oversampledRight[sample];
      nullEnergy +=
          leftDifference * leftDifference + rightDifference * rightDifference;
    }
    const double matchedDefaultRms =
        stereoRms(alignedDefaultLeft, alignedDefaultRight);
    const double matchedOversampledRms =
        stereoRms(oversampledLeft, oversampledRight);
    const double matchErrorDb =
        20.0 * std::log10(matchedDefaultRms / matchedOversampledRms);
    const double peakDb =
        20.0 *
        std::log10(std::max(stereoPeak(alignedDefaultLeft, alignedDefaultRight),
                            stereoPeak(oversampledLeft, oversampledRight)));
    const double nullRms = std::sqrt(
        nullEnergy / static_cast<double>(2U * oversampledLeft.size()));
    const double nullDb = 20.0 * std::log10(nullRms);
    maximumMatchError = std::max(maximumMatchError, std::abs(matchErrorDb));
    minimumNullDb = std::min(minimumNullDb, nullDb);
    maximumNullDb = std::max(maximumNullDb, nullDb);

    randomState = randomState * 1664525U + 1013904223U;
    const bool oversampledIsA = (randomState & 1U) != 0U;
    const auto pathA = audioDirectory / (std::string{fixture} + "-A.wav");
    const auto pathB = audioDirectory / (std::string{fixture} + "-B.wav");
    const bool wroteA = writeStereoFloatWave(
        pathA, oversampledIsA ? oversampledLeft : alignedDefaultLeft,
        oversampledIsA ? oversampledRight : alignedDefaultRight, sampleRate);
    const bool wroteB = writeStereoFloatWave(
        pathB, oversampledIsA ? alignedDefaultLeft : oversampledLeft,
        oversampledIsA ? alignedDefaultRight : oversampledRight, sampleRate);
    answers << fixture << ',' << (oversampledIsA ? "73/33" : "1x") << ','
            << (oversampledIsA ? "1x" : "73/33") << '\n';
    responses << fixture << ",,,\n";
    measurements << fixture << ',' << oversampledLeft.size() << ','
                 << 20.0 * std::log10(defaultMatch) << ','
                 << 20.0 * std::log10(oversampledMatch) << ',' << matchErrorDb
                 << ',' << peakDb << ',' << nullDb << ',' << latency << '\n';
    valid = valid && wroteA && wroteB && std::isfinite(matchErrorDb) &&
            std::abs(matchErrorDb) < 0.001 && std::isfinite(nullDb) &&
            std::abs(peakDb + 1.0) < 0.001;
    ++fixtureCount;
  }
  valid = valid && static_cast<bool>(answers) && static_cast<bool>(responses) &&
          static_cast<bool>(measurements) && fixtureCount == 4U;
  std::cout << std::fixed << std::setprecision(6)
            << "{\"fixtures\":" << fixtureCount << ",\"pairs\":" << fixtureCount
            << ",\"seed\":852274,\"max_match_error_db\":" << maximumMatchError
            << ",\"minimum_null_rms_dbfs\":" << minimumNullDb
            << ",\"maximum_null_rms_dbfs\":" << maximumNullDb
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

std::array<std::vector<float>, 2> makeAutomationSignal(std::size_t frames,
                                                       double sampleRate) {
  std::array<std::vector<float>, 2> signal{std::vector<float>(frames),
                                           std::vector<float>(frames)};
  for (std::size_t sample = 0; sample < frames; ++sample) {
    const double time = static_cast<double>(sample) / sampleRate;
    const float amplitude =
        0.35F + 0.20F * static_cast<float>(std::sin(2.0 * kPi * 1.7 * time));
    signal[0][sample] =
        amplitude *
        (0.65F * static_cast<float>(std::sin(2.0 * kPi * 173.0 * time)) +
         0.25F * static_cast<float>(std::sin(2.0 * kPi * 997.0 * time)));
    signal[1][sample] =
        amplitude *
        (0.57F * static_cast<float>(std::sin(2.0 * kPi * 211.0 * time)) +
         0.22F * static_cast<float>(std::sin(2.0 * kPi * 1231.0 * time)));
  }
  return signal;
}

struct BoundaryCurvature {
  double maximumBoundary{};
  double maximumLocal{};
  double maximumExcess{};
  std::size_t transitions{};
  bool finite{true};
};

BoundaryCurvature measureBoundaryCurvature(const std::vector<float>& left,
                                           const std::vector<float>& right,
                                           std::size_t blockSize,
                                           std::size_t localRadius) {
  BoundaryCurvature result;
  for (std::size_t sample = 0; sample < left.size(); ++sample) {
    result.finite = result.finite && std::isfinite(left[sample]) &&
                    std::isfinite(right[sample]);
  }
  for (std::size_t boundary = blockSize; boundary < left.size();
       boundary += blockSize) {
    const double boundaryCurvature = std::max(
        std::abs(static_cast<double>(
            left[boundary] - 2.0F * left[boundary - 1U] + left[boundary - 2U])),
        std::abs(static_cast<double>(right[boundary] -
                                     2.0F * right[boundary - 1U] +
                                     right[boundary - 2U])));
    double localCurvature{};
    const std::size_t first =
        boundary > localRadius ? boundary - localRadius : 2U;
    const std::size_t last = std::min(left.size() - 1U, boundary + localRadius);
    for (std::size_t sample = first; sample <= last; ++sample) {
      if (sample == boundary) {
        continue;
      }
      localCurvature = std::max(
          {localCurvature,
           std::abs(static_cast<double>(
               left[sample] - 2.0F * left[sample - 1U] + left[sample - 2U])),
           std::abs(static_cast<double>(right[sample] -
                                        2.0F * right[sample - 1U] +
                                        right[sample - 2U]))});
    }
    result.maximumBoundary =
        std::max(result.maximumBoundary, boundaryCurvature);
    result.maximumLocal = std::max(result.maximumLocal, localCurvature);
    result.maximumExcess =
        std::max(result.maximumExcess,
                 std::max(0.0, boundaryCurvature - localCurvature));
    ++result.transitions;
  }
  return result;
}

int automationReport(const std::filesystem::path& outputPath) {
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t frames = 48000U;
  constexpr std::size_t blockSize = 127U;
  constexpr std::size_t localRadius = 16U;
  constexpr double excessCeiling = 0.001;
  struct Parameter {
    const char* name;
    float aste::density::Parameters::* value;
    float minimum;
    float maximum;
  };
  constexpr std::array<Parameter, 9> parameters{{
      {"drive", &aste::density::Parameters::driveDb, -12.0F, 24.0F},
      {"crush", &aste::density::Parameters::crush, 0.0F, 1.0F},
      {"attack", &aste::density::Parameters::attackMs, 0.02F, 30.0F},
      {"release", &aste::density::Parameters::releaseMs, 20.0F, 1200.0F},
      {"density", &aste::density::Parameters::density, 0.0F, 1.0F},
      {"blend", &aste::density::Parameters::blend, 0.0F, 1.0F},
      {"stereo", &aste::density::Parameters::stereoLink, 0.0F, 1.0F},
      {"output", &aste::density::Parameters::outputDb, -24.0F, 12.0F},
      {"detector_hpf", &aste::density::Parameters::detectorHpfHz, 20.0F,
       300.0F},
  }};

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create automation report: " << outputPath << '\n';
    return 1;
  }
  output << "parameter,transitions,max_boundary_curvature,max_local_curvature,"
            "max_excess,excess_dbfs,finite,pass\n"
         << std::fixed << std::setprecision(9);

  bool valid = true;
  bool withinCeiling = true;
  double worstExcess{};
  std::string_view worstParameter;
  for (std::size_t test = 0; test <= parameters.size(); ++test) {
    auto signal = makeAutomationSignal(frames, sampleRate);

    auto current = productionParameters();
    for (std::size_t index = 0; index < parameters.size(); ++index) {
      if (test == index || test == parameters.size()) {
        current.*parameters[index].value = parameters[index].minimum;
      }
    }
    aste::density::Processor processor;
    processor.prepare(sampleRate, current);
    std::size_t rendered{};
    std::size_t block{};
    while (rendered < frames) {
      const bool high = (block & 1U) != 0U;
      for (std::size_t index = 0; index < parameters.size(); ++index) {
        if (test == index || test == parameters.size()) {
          current.*parameters[index].value =
              high ? parameters[index].maximum : parameters[index].minimum;
        }
      }
      const std::size_t count = std::min(blockSize, frames - rendered);
      processor.process(signal[0].data() + rendered,
                        signal[1].data() + rendered, count, current);
      rendered += count;
      ++block;
    }

    const auto curvature =
        measureBoundaryCurvature(signal[0], signal[1], blockSize, localRadius);
    const bool passed =
        curvature.finite && curvature.maximumExcess <= excessCeiling;
    const std::string_view name =
        test == parameters.size() ? "all" : parameters[test].name;
    const double excessDb =
        20.0 * std::log10(std::max(curvature.maximumExcess, 1.0e-15));
    output << name << ',' << curvature.transitions << ','
           << curvature.maximumBoundary << ',' << curvature.maximumLocal << ','
           << curvature.maximumExcess << ',' << excessDb << ','
           << (curvature.finite ? 1 : 0) << ',' << (passed ? 1 : 0) << '\n';
    if (curvature.maximumExcess > worstExcess) {
      worstExcess = curvature.maximumExcess;
      worstParameter = name;
    }
    valid = valid && curvature.finite;
    withinCeiling = withinCeiling && passed;
  }
  valid = valid && static_cast<bool>(output);
  std::cout << std::fixed << std::setprecision(6)
            << "{\"parameters\":9,\"simultaneous_case\":true,"
               "\"block_size\":127,\"excess_ceiling_dbfs\":-60.000000,"
               "\"worst_parameter\":\""
            << worstParameter << "\",\"worst_excess_dbfs\":"
            << 20.0 * std::log10(std::max(worstExcess, 1.0e-15))
            << ",\"within_ceiling\":" << (withinCeiling ? "true" : "false")
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int outputSmoothingReport(const std::filesystem::path& outputPath) {
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t frames = 48000U;
  constexpr std::size_t blockSize = 127U;
  constexpr std::size_t localRadius = 16U;
  constexpr float minimumDb = -24.0F;
  constexpr float maximumDb = 12.0F;
  constexpr double excessCeiling = 0.001;
  struct Profile {
    const char* name;
    double seconds;
    int stages;
  };
  constexpr std::array<Profile, 4> profiles{{
      {"exp_5ms_reference", 0.005, 1},
      {"exp_10ms", 0.010, 1},
      {"exp_20ms", 0.020, 1},
      {"cascade_3ms_3ms_current", 0.003, 2},
  }};

  auto unscaled = makeAutomationSignal(frames, sampleRate);
  auto baseParameters = productionParameters();
  baseParameters.outputDb = 0.0F;
  baseParameters.protection = false;
  aste::density::Processor baseProcessor;
  baseProcessor.prepare(sampleRate, baseParameters);
  constexpr std::array<std::size_t, 1> schedule{blockSize};
  processInBlocks(baseProcessor, unscaled[0], unscaled[1], schedule,
                  baseParameters);

  auto production = makeAutomationSignal(frames, sampleRate);
  auto productionParametersValue = productionParameters();
  productionParametersValue.outputDb = minimumDb;
  aste::density::Processor productionProcessor;
  productionProcessor.prepare(sampleRate, productionParametersValue);
  for (std::size_t offset = 0, block = 0; offset < frames; ++block) {
    productionParametersValue.outputDb =
        (block & 1U) != 0U ? maximumDb : minimumDb;
    const std::size_t count = std::min(blockSize, frames - offset);
    productionProcessor.process(production[0].data() + offset,
                                production[1].data() + offset, count,
                                productionParametersValue);
    offset += count;
  }

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create output smoothing report: " << outputPath
              << '\n';
    return 1;
  }
  output << "profile,stages,time_constant_ms,transitions,max_excess,"
            "excess_dbfs,first_sample_motion_db,response_5ms_percent,"
            "settle_within_1db_ms,max_delta_vs_production,finite,pass\n"
         << std::fixed << std::setprecision(9);

  bool valid = true;
  double bestExcess = std::numeric_limits<double>::infinity();
  std::string_view bestProfile;
  for (const auto& profile : profiles) {
    auto rendered = unscaled;
    const float smoothingCoefficient =
        static_cast<float>(std::exp(-1.0 / (profile.seconds * sampleRate)));
    float first = minimumDb;
    float second = minimumDb;
    for (std::size_t sample = 0; sample < frames; ++sample) {
      const float target =
          ((sample / blockSize) & 1U) != 0U ? maximumDb : minimumDb;
      first = target + smoothingCoefficient * (first - target);
      if (profile.stages == 2) {
        second = first + smoothingCoefficient * (second - first);
      }
      const float gain = std::exp((profile.stages == 2 ? second : first) *
                                  0.11512925464970229F);
      rendered[0][sample] =
          aste::density::controlledClipSample(rendered[0][sample] * gain);
      rendered[1][sample] =
          aste::density::controlledClipSample(rendered[1][sample] * gain);
    }

    const auto curvature = measureBoundaryCurvature(rendered[0], rendered[1],
                                                    blockSize, localRadius);
    double maximumProductionDelta{};
    for (std::size_t sample = 0; sample < frames; ++sample) {
      maximumProductionDelta =
          std::max({maximumProductionDelta,
                    std::abs(static_cast<double>(rendered[0][sample] -
                                                 production[0][sample])),
                    std::abs(static_cast<double>(rendered[1][sample] -
                                                 production[1][sample]))});
    }

    first = minimumDb;
    second = minimumDb;
    float firstSampleMotion{};
    float responseAtFiveMs{};
    std::size_t settleSample = frames;
    for (std::size_t sample = 0; sample < frames; ++sample) {
      first = maximumDb + smoothingCoefficient * (first - maximumDb);
      if (profile.stages == 2) {
        second = first + smoothingCoefficient * (second - first);
      }
      const float response = profile.stages == 2 ? second : first;
      if (sample == 0U) {
        firstSampleMotion = response - minimumDb;
      }
      if (sample == 239U) {
        responseAtFiveMs =
            100.0F * (response - minimumDb) / (maximumDb - minimumDb);
      }
      if (settleSample == frames && maximumDb - response <= 1.0F) {
        settleSample = sample;
      }
    }
    const double settleMs =
        1000.0 * static_cast<double>(settleSample + 1U) / sampleRate;
    const double excessDb =
        20.0 * std::log10(std::max(curvature.maximumExcess, 1.0e-15));
    const bool passed =
        curvature.finite && curvature.maximumExcess <= excessCeiling;
    output << profile.name << ',' << profile.stages << ','
           << 1000.0 * profile.seconds << ',' << curvature.transitions << ','
           << curvature.maximumExcess << ',' << excessDb << ','
           << firstSampleMotion << ',' << responseAtFiveMs << ',' << settleMs
           << ',' << maximumProductionDelta << ',' << (curvature.finite ? 1 : 0)
           << ',' << (passed ? 1 : 0) << '\n';
    if (curvature.maximumExcess < bestExcess) {
      bestExcess = curvature.maximumExcess;
      bestProfile = profile.name;
    }
    valid = valid && curvature.finite && std::isfinite(settleMs) &&
            (std::string_view{profile.name} != "cascade_3ms_3ms_current" ||
             maximumProductionDelta == 0.0);
  }
  valid = valid && static_cast<bool>(output);
  std::cout << std::fixed << std::setprecision(6)
            << "{\"profiles\":" << profiles.size()
            << ",\"production_model_delta\":0.000000,\"best_profile\":\""
            << bestProfile << "\",\"best_excess_dbfs\":"
            << 20.0 * std::log10(std::max(bestExcess, 1.0e-15))
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int driveSmoothingReport(const std::filesystem::path& outputPath) {
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t frames = 48000U;
  constexpr std::size_t blockSize = 127U;
  constexpr std::size_t localRadius = 16U;
  constexpr float minimumDb = -12.0F;
  constexpr float maximumDb = 24.0F;
  constexpr double excessCeiling = 0.001;
  struct Profile {
    const char* name;
    double seconds;
    bool cascade;
  };
  constexpr std::array<Profile, 4> profiles{{
      {"exp_5ms_reference", 0.005, false},
      {"exp_10ms", 0.010, false},
      {"exp_20ms", 0.020, false},
      {"cascade_3ms_3ms_current", 0.003, true},
  }};

  auto production = makeAutomationSignal(frames, sampleRate);
  auto productionParametersValue = productionParameters();
  productionParametersValue.driveDb = minimumDb;
  aste::density::Processor productionProcessor;
  productionProcessor.prepare(sampleRate, productionParametersValue);
  for (std::size_t offset = 0, block = 0; offset < frames; ++block) {
    productionParametersValue.driveDb =
        (block & 1U) != 0U ? maximumDb : minimumDb;
    const std::size_t count = std::min(blockSize, frames - offset);
    productionProcessor.process(production[0].data() + offset,
                                production[1].data() + offset, count,
                                productionParametersValue);
    offset += count;
  }

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create drive smoothing report: " << outputPath << '\n';
    return 1;
  }
  output << "profile,stages,time_constant_ms,transitions,max_excess,"
            "excess_dbfs,first_sample_motion_db,response_5ms_percent,"
            "settle_within_1db_ms,max_delta_vs_production,finite,pass\n"
         << std::fixed << std::setprecision(9);

  bool valid = true;
  double bestExcess = std::numeric_limits<double>::infinity();
  std::string_view bestProfile;
  for (const auto& profile : profiles) {
    auto rendered = makeAutomationSignal(frames, sampleRate);
    auto parameters = productionParameters();
    parameters.driveDb = minimumDb;
    aste::density::Processor processor;
    processor.prepareDriveSmoothingPrototype(sampleRate, profile.seconds,
                                             profile.cascade, parameters);
    for (std::size_t offset = 0, block = 0; offset < frames; ++block) {
      parameters.driveDb = (block & 1U) != 0U ? maximumDb : minimumDb;
      const std::size_t count = std::min(blockSize, frames - offset);
      processor.process(rendered[0].data() + offset,
                        rendered[1].data() + offset, count, parameters);
      offset += count;
    }

    const auto curvature = measureBoundaryCurvature(rendered[0], rendered[1],
                                                    blockSize, localRadius);
    double maximumProductionDelta{};
    for (std::size_t sample = 0; sample < frames; ++sample) {
      maximumProductionDelta =
          std::max({maximumProductionDelta,
                    std::abs(static_cast<double>(rendered[0][sample] -
                                                 production[0][sample])),
                    std::abs(static_cast<double>(rendered[1][sample] -
                                                 production[1][sample]))});
    }

    const float smoothingCoefficient =
        static_cast<float>(std::exp(-1.0 / (profile.seconds * sampleRate)));
    float first = minimumDb;
    float second = minimumDb;
    float firstSampleMotion{};
    float responseAtFiveMs{};
    std::size_t settleSample = frames;
    for (std::size_t sample = 0; sample < frames; ++sample) {
      first = maximumDb + smoothingCoefficient * (first - maximumDb);
      if (profile.cascade) {
        second = first + smoothingCoefficient * (second - first);
      }
      const float response = profile.cascade ? second : first;
      if (sample == 0U) {
        firstSampleMotion = response - minimumDb;
      }
      if (sample == 239U) {
        responseAtFiveMs =
            100.0F * (response - minimumDb) / (maximumDb - minimumDb);
      }
      if (settleSample == frames && maximumDb - response <= 1.0F) {
        settleSample = sample;
      }
    }
    const double settleMs =
        1000.0 * static_cast<double>(settleSample + 1U) / sampleRate;
    const double excessDb =
        20.0 * std::log10(std::max(curvature.maximumExcess, 1.0e-15));
    const bool passed =
        curvature.finite && curvature.maximumExcess <= excessCeiling;
    output << profile.name << ',' << (profile.cascade ? 2 : 1) << ','
           << 1000.0 * profile.seconds << ',' << curvature.transitions << ','
           << curvature.maximumExcess << ',' << excessDb << ','
           << firstSampleMotion << ',' << responseAtFiveMs << ',' << settleMs
           << ',' << maximumProductionDelta << ',' << (curvature.finite ? 1 : 0)
           << ',' << (passed ? 1 : 0) << '\n';
    if (curvature.maximumExcess < bestExcess) {
      bestExcess = curvature.maximumExcess;
      bestProfile = profile.name;
    }
    valid = valid && curvature.finite && std::isfinite(settleMs) &&
            (std::string_view{profile.name} != "cascade_3ms_3ms_current" ||
             maximumProductionDelta == 0.0);
  }
  valid = valid && static_cast<bool>(output);
  std::cout << std::fixed << std::setprecision(6)
            << "{\"profiles\":" << profiles.size()
            << ",\"production_model_delta\":0.000000,\"best_profile\":\""
            << bestProfile << "\",\"best_excess_dbfs\":"
            << 20.0 * std::log10(std::max(bestExcess, 1.0e-15))
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int attackSmoothingReport(const std::filesystem::path& outputPath) {
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t frames = 48000U;
  constexpr std::size_t blockSize = 127U;
  constexpr std::size_t localRadius = 16U;
  constexpr float minimumMs = 0.02F;
  constexpr float maximumMs = 30.0F;
  constexpr double excessCeiling = 0.001;
  struct Profile {
    const char* name;
    double seconds;
    bool cascade;
    bool smoothed;
    bool production;
  };
  constexpr std::array<Profile, 4> profiles{{
      {"unsmoothed_reference", 0.0, false, false, false},
      {"log_exp_5ms_current", 0.005, false, true, true},
      {"log_exp_10ms", 0.010, false, true, false},
      {"log_cascade_3ms_3ms", 0.003, true, true, false},
  }};

  auto production = makeAutomationSignal(frames, sampleRate);
  auto productionParametersValue = productionParameters();
  productionParametersValue.attackMs = minimumMs;
  aste::density::Processor productionProcessor;
  productionProcessor.prepare(sampleRate, productionParametersValue);
  for (std::size_t offset = 0, block = 0; offset < frames; ++block) {
    productionParametersValue.attackMs =
        (block & 1U) != 0U ? maximumMs : minimumMs;
    const std::size_t count = std::min(blockSize, frames - offset);
    productionProcessor.process(production[0].data() + offset,
                                production[1].data() + offset, count,
                                productionParametersValue);
    offset += count;
  }

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create attack smoothing report: " << outputPath
              << '\n';
    return 1;
  }
  output << "profile,stages,time_constant_ms,transitions,max_excess,"
            "excess_dbfs,first_sample_ms,response_5ms_percent_log,"
            "settle_within_1pct_log_ms,max_delta_vs_production,finite,pass\n"
         << std::fixed << std::setprecision(9);

  const float minimumLog = std::log(minimumMs);
  const float maximumLog = std::log(maximumMs);
  bool valid = true;
  double bestExcess = std::numeric_limits<double>::infinity();
  std::string_view bestProfile;
  for (const auto& profile : profiles) {
    auto rendered = makeAutomationSignal(frames, sampleRate);
    auto parameters = productionParameters();
    parameters.attackMs = minimumMs;
    aste::density::Processor processor;
    if (profile.production) {
      processor.prepare(sampleRate, parameters);
    } else {
      processor.prepareAttackSmoothingPrototype(sampleRate, profile.seconds,
                                                profile.cascade, parameters);
    }
    for (std::size_t offset = 0, block = 0; offset < frames; ++block) {
      parameters.attackMs = (block & 1U) != 0U ? maximumMs : minimumMs;
      const std::size_t count = std::min(blockSize, frames - offset);
      processor.process(rendered[0].data() + offset,
                        rendered[1].data() + offset, count, parameters);
      offset += count;
    }

    const auto curvature = measureBoundaryCurvature(rendered[0], rendered[1],
                                                    blockSize, localRadius);
    double maximumProductionDelta{};
    for (std::size_t sample = 0; sample < frames; ++sample) {
      maximumProductionDelta =
          std::max({maximumProductionDelta,
                    std::abs(static_cast<double>(rendered[0][sample] -
                                                 production[0][sample])),
                    std::abs(static_cast<double>(rendered[1][sample] -
                                                 production[1][sample]))});
    }

    const float smoothingCoefficient =
        profile.smoothed ? static_cast<float>(
                               std::exp(-1.0 / (profile.seconds * sampleRate)))
                         : 0.0F;
    float first = minimumLog;
    float second = minimumLog;
    float firstSampleMs{};
    float responseAtFiveMs{};
    std::size_t settleSample = frames;
    for (std::size_t sample = 0; sample < frames; ++sample) {
      first = profile.smoothed
                  ? maximumLog + smoothingCoefficient * (first - maximumLog)
                  : maximumLog;
      if (profile.cascade) {
        second = first + smoothingCoefficient * (second - first);
      }
      const float response = profile.cascade ? second : first;
      if (sample == 0U) {
        firstSampleMs = std::exp(response);
      }
      if (sample == 239U) {
        responseAtFiveMs =
            100.0F * (response - minimumLog) / (maximumLog - minimumLog);
      }
      if (settleSample == frames &&
          maximumLog - response <= 0.01F * (maximumLog - minimumLog)) {
        settleSample = sample;
      }
    }
    const double settleMs =
        1000.0 * static_cast<double>(settleSample + 1U) / sampleRate;
    const double excessDb =
        20.0 * std::log10(std::max(curvature.maximumExcess, 1.0e-15));
    const bool passed =
        curvature.finite && curvature.maximumExcess <= excessCeiling;
    output << profile.name << ','
           << (profile.smoothed ? (profile.cascade ? 2 : 1) : 0) << ','
           << 1000.0 * profile.seconds << ',' << curvature.transitions << ','
           << curvature.maximumExcess << ',' << excessDb << ',' << firstSampleMs
           << ',' << responseAtFiveMs << ',' << settleMs << ','
           << maximumProductionDelta << ',' << (curvature.finite ? 1 : 0) << ','
           << (passed ? 1 : 0) << '\n';
    if (curvature.maximumExcess < bestExcess) {
      bestExcess = curvature.maximumExcess;
      bestProfile = profile.name;
    }
    valid = valid && curvature.finite && std::isfinite(settleMs) &&
            (!profile.production || maximumProductionDelta == 0.0);
  }
  valid = valid && static_cast<bool>(output);
  std::cout << std::fixed << std::setprecision(6)
            << "{\"profiles\":" << profiles.size()
            << ",\"production_model_delta\":0.000000,\"best_profile\":\""
            << bestProfile << "\",\"best_excess_dbfs\":"
            << 20.0 * std::log10(std::max(bestExcess, 1.0e-15))
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int blendSmoothingReport(const std::filesystem::path& outputPath) {
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t frames = 48000U;
  constexpr std::size_t blockSize = 127U;
  constexpr std::size_t localRadius = 16U;
  constexpr double excessCeiling = 0.001;
  struct Profile {
    const char* name;
    double seconds;
    bool cascade;
  };
  constexpr std::array<Profile, 4> profiles{{
      {"exp_5ms_reference", 0.005, false},
      {"exp_10ms", 0.010, false},
      {"exp_20ms", 0.020, false},
      {"cascade_3ms_3ms_current", 0.003, true},
  }};

  auto production = makeAutomationSignal(frames, sampleRate);
  auto productionParametersValue = productionParameters();
  productionParametersValue.blend = 0.0F;
  aste::density::Processor productionProcessor;
  productionProcessor.prepare(sampleRate, productionParametersValue);
  for (std::size_t offset = 0, block = 0; offset < frames; ++block) {
    productionParametersValue.blend = (block & 1U) != 0U ? 1.0F : 0.0F;
    const std::size_t count = std::min(blockSize, frames - offset);
    productionProcessor.process(production[0].data() + offset,
                                production[1].data() + offset, count,
                                productionParametersValue);
    offset += count;
  }

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create blend smoothing report: " << outputPath << '\n';
    return 1;
  }
  output << "profile,stages,time_constant_ms,transitions,max_excess,"
            "excess_dbfs,first_sample_motion_percent,response_5ms_percent,"
            "settle_within_1pct_ms,max_delta_vs_production,finite,pass\n"
         << std::fixed << std::setprecision(9);

  bool valid = true;
  double bestExcess = std::numeric_limits<double>::infinity();
  std::string_view bestProfile;
  for (const auto& profile : profiles) {
    auto rendered = makeAutomationSignal(frames, sampleRate);
    auto parameters = productionParameters();
    parameters.blend = 0.0F;
    aste::density::Processor processor;
    processor.prepareBlendSmoothingPrototype(sampleRate, profile.seconds,
                                             profile.cascade, parameters);
    for (std::size_t offset = 0, block = 0; offset < frames; ++block) {
      parameters.blend = (block & 1U) != 0U ? 1.0F : 0.0F;
      const std::size_t count = std::min(blockSize, frames - offset);
      processor.process(rendered[0].data() + offset,
                        rendered[1].data() + offset, count, parameters);
      offset += count;
    }

    const auto curvature = measureBoundaryCurvature(rendered[0], rendered[1],
                                                    blockSize, localRadius);
    double maximumProductionDelta{};
    for (std::size_t sample = 0; sample < frames; ++sample) {
      maximumProductionDelta =
          std::max({maximumProductionDelta,
                    std::abs(static_cast<double>(rendered[0][sample] -
                                                 production[0][sample])),
                    std::abs(static_cast<double>(rendered[1][sample] -
                                                 production[1][sample]))});
    }

    const float smoothingCoefficient =
        static_cast<float>(std::exp(-1.0 / (profile.seconds * sampleRate)));
    float first{};
    float second{};
    float firstSampleMotion{};
    float responseAtFiveMs{};
    std::size_t settleSample = frames;
    for (std::size_t sample = 0; sample < frames; ++sample) {
      first = 1.0F + smoothingCoefficient * (first - 1.0F);
      if (profile.cascade) {
        second = first + smoothingCoefficient * (second - first);
      }
      const float response = profile.cascade ? second : first;
      if (sample == 0U) {
        firstSampleMotion = 100.0F * response;
      }
      if (sample == 239U) {
        responseAtFiveMs = 100.0F * response;
      }
      if (settleSample == frames && 1.0F - response <= 0.01F) {
        settleSample = sample;
      }
    }
    const double settleMs =
        1000.0 * static_cast<double>(settleSample + 1U) / sampleRate;
    const double excessDb =
        20.0 * std::log10(std::max(curvature.maximumExcess, 1.0e-15));
    const bool passed =
        curvature.finite && curvature.maximumExcess <= excessCeiling;
    output << profile.name << ',' << (profile.cascade ? 2 : 1) << ','
           << 1000.0 * profile.seconds << ',' << curvature.transitions << ','
           << curvature.maximumExcess << ',' << excessDb << ','
           << firstSampleMotion << ',' << responseAtFiveMs << ',' << settleMs
           << ',' << maximumProductionDelta << ',' << (curvature.finite ? 1 : 0)
           << ',' << (passed ? 1 : 0) << '\n';
    if (curvature.maximumExcess < bestExcess) {
      bestExcess = curvature.maximumExcess;
      bestProfile = profile.name;
    }
    valid = valid && curvature.finite && std::isfinite(settleMs) &&
            (std::string_view{profile.name} != "cascade_3ms_3ms_current" ||
             maximumProductionDelta == 0.0);
  }
  valid = valid && static_cast<bool>(output);
  std::cout << std::fixed << std::setprecision(6)
            << "{\"profiles\":" << profiles.size()
            << ",\"production_model_delta\":0.000000,\"best_profile\":\""
            << bestProfile << "\",\"best_excess_dbfs\":"
            << 20.0 * std::log10(std::max(bestExcess, 1.0e-15))
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int automationAuditions(const std::filesystem::path& directory) {
  constexpr std::uint32_t sampleRate = 48000U;
  constexpr std::size_t frames = 192000U;
  constexpr std::size_t blockSize = 127U;
  constexpr std::size_t localRadius = 16U;
  constexpr double excessCeiling = 0.001;
  constexpr std::uint32_t seed = 0xD0143U;
  const auto audioDirectory = directory / "audio";
  std::error_code error;
  std::filesystem::create_directories(audioDirectory, error);
  std::ofstream answers(directory / "answer-key.csv");
  std::ofstream responses(directory / "responses.csv");
  std::ofstream measurements(directory / "measurements.csv");
  std::ofstream notes(directory / "listening-notes.md");
  if (error || !answers || !responses || !measurements || !notes) {
    std::cerr << "cannot create automation audition pack: " << directory
              << '\n';
    return 1;
  }
  answers << "case,A,B\n";
  responses << "case,preferred(A/B/no preference),"
               "artifacts(A/B/both/none),confidence(0-3),notes\n";
  measurements
      << "case,frames,production_match_db,reference_match_db,match_error_db,"
         "peak_dbfs,null_rms_dbfs,production_excess_dbfs,"
         "reference_excess_dbfs,improvement_db\n"
      << std::fixed << std::setprecision(9);
  notes
      << "# Density automation audition\n\n"
         "Do not open `answer-key.csv` until `responses.csv` is complete.\n\n"
         "Each pair applies full-range endpoint automation every 127 samples "
         "to a deterministic stereo signal. A and B are RMS matched and "
         "share -1 dBFS peak normalization. Listen on monitors and headphones "
         "at normal and quiet levels. Record unintended ticks, buzz, or rough "
         "edges; prefer neither file when the difference is not reliable.\n";

  const auto setCase = [](aste::density::Parameters& parameters,
                          std::string_view name, bool high) {
    if (name == "drive" || name == "all") {
      parameters.driveDb = high ? 24.0F : -12.0F;
    }
    if (name == "attack" || name == "all") {
      parameters.attackMs = high ? 30.0F : 0.02F;
    }
    if (name == "blend" || name == "all") {
      parameters.blend = high ? 1.0F : 0.0F;
    }
    if (name == "all") {
      parameters.crush = high ? 1.0F : 0.0F;
      parameters.releaseMs = high ? 1200.0F : 20.0F;
      parameters.density = high ? 1.0F : 0.0F;
      parameters.stereoLink = high ? 1.0F : 0.0F;
      parameters.outputDb = high ? 12.0F : -24.0F;
      parameters.detectorHpfHz = high ? 300.0F : 20.0F;
    }
  };

  std::uint32_t randomState = seed;
  double maximumMatchError{};
  double minimumImprovement = std::numeric_limits<double>::infinity();
  bool valid = true;
  std::size_t caseCount{};
  for (std::string_view name :
       std::array<std::string_view, 4>{"drive", "attack", "blend", "all"}) {
    auto production = makeAutomationSignal(frames, sampleRate);
    auto reference = production;
    auto parameters = productionParameters();
    setCase(parameters, name, false);
    aste::density::Processor productionProcessor;
    aste::density::Processor referenceProcessor;
    productionProcessor.prepare(sampleRate, parameters);
    referenceProcessor.prepareAutomationReferencePrototype(sampleRate,
                                                           parameters);
    for (std::size_t offset = 0, block = 0; offset < frames; ++block) {
      setCase(parameters, name, (block & 1U) != 0U);
      const std::size_t count = std::min(blockSize, frames - offset);
      productionProcessor.process(production[0].data() + offset,
                                  production[1].data() + offset, count,
                                  parameters);
      referenceProcessor.process(reference[0].data() + offset,
                                 reference[1].data() + offset, count,
                                 parameters);
      offset += count;
    }

    const auto productionCurvature = measureBoundaryCurvature(
        production[0], production[1], blockSize, localRadius);
    const auto referenceCurvature = measureBoundaryCurvature(
        reference[0], reference[1], blockSize, localRadius);
    const double productionExcessDb =
        20.0 * std::log10(std::max(productionCurvature.maximumExcess, 1.0e-15));
    const double referenceExcessDb =
        20.0 * std::log10(std::max(referenceCurvature.maximumExcess, 1.0e-15));
    const double improvementDb = referenceExcessDb - productionExcessDb;
    minimumImprovement = std::min(minimumImprovement, improvementDb);

    const double productionRms = stereoRms(production[0], production[1]);
    const double referenceRms = stereoRms(reference[0], reference[1]);
    const double targetRms = std::min(productionRms, referenceRms);
    const float productionMatch = static_cast<float>(targetRms / productionRms);
    const float referenceMatch = static_cast<float>(targetRms / referenceRms);
    for (std::size_t sample = 0; sample < frames; ++sample) {
      production[0][sample] *= productionMatch;
      production[1][sample] *= productionMatch;
      reference[0][sample] *= referenceMatch;
      reference[1][sample] *= referenceMatch;
    }
    const float commonGain = static_cast<float>(
        0.8912509381337456 / std::max(stereoPeak(production[0], production[1]),
                                      stereoPeak(reference[0], reference[1])));
    double nullEnergy{};
    for (std::size_t sample = 0; sample < frames; ++sample) {
      production[0][sample] *= commonGain;
      production[1][sample] *= commonGain;
      reference[0][sample] *= commonGain;
      reference[1][sample] *= commonGain;
      const double leftDifference =
          production[0][sample] - reference[0][sample];
      const double rightDifference =
          production[1][sample] - reference[1][sample];
      nullEnergy +=
          leftDifference * leftDifference + rightDifference * rightDifference;
    }
    const double matchedProductionRms = stereoRms(production[0], production[1]);
    const double matchedReferenceRms = stereoRms(reference[0], reference[1]);
    const double matchErrorDb =
        20.0 * std::log10(matchedProductionRms / matchedReferenceRms);
    const double peakDb =
        20.0 * std::log10(std::max(stereoPeak(production[0], production[1]),
                                   stereoPeak(reference[0], reference[1])));
    const double nullDb =
        20.0 *
        std::log10(std::sqrt(nullEnergy / static_cast<double>(2U * frames)));
    maximumMatchError = std::max(maximumMatchError, std::abs(matchErrorDb));

    randomState = randomState * 1664525U + 1013904223U;
    const bool productionIsA = (randomState & 1U) != 0U;
    const auto pathA = audioDirectory / (std::string{name} + "-A.wav");
    const auto pathB = audioDirectory / (std::string{name} + "-B.wav");
    const bool wroteA = writeStereoFloatWave(
        pathA, productionIsA ? production[0] : reference[0],
        productionIsA ? production[1] : reference[1], sampleRate);
    const bool wroteB = writeStereoFloatWave(
        pathB, productionIsA ? reference[0] : production[0],
        productionIsA ? reference[1] : production[1], sampleRate);
    answers << name << ',' << (productionIsA ? "production" : "reference")
            << ',' << (productionIsA ? "reference" : "production") << '\n';
    responses << name << ",,,,\n";
    measurements << name << ',' << frames << ','
                 << 20.0 * std::log10(productionMatch) << ','
                 << 20.0 * std::log10(referenceMatch) << ',' << matchErrorDb
                 << ',' << peakDb << ',' << nullDb << ',' << productionExcessDb
                 << ',' << referenceExcessDb << ',' << improvementDb << '\n';
    valid = valid && wroteA && wroteB && productionCurvature.finite &&
            referenceCurvature.finite &&
            productionCurvature.maximumExcess <= excessCeiling &&
            improvementDb > 0.0 && std::isfinite(nullDb) &&
            std::abs(matchErrorDb) < 0.001 && std::abs(peakDb + 1.0) < 0.001;
    ++caseCount;
  }
  valid = valid && static_cast<bool>(answers) && static_cast<bool>(responses) &&
          static_cast<bool>(measurements) && static_cast<bool>(notes) &&
          caseCount == 4U;
  std::cout << std::fixed << std::setprecision(6) << "{\"cases\":" << caseCount
            << ",\"pairs\":" << caseCount << ",\"seed\":" << seed
            << ",\"max_match_error_db\":" << maximumMatchError
            << ",\"minimum_improvement_db\":" << minimumImprovement
            << ",\"all_production_within_ceiling\":true,"
               "\"measurement_valid\":"
            << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

std::array<std::vector<float>, 2> makeStereoStabilityFixture(
    std::string_view fixture) {
  std::array<std::vector<float>, 2> input;
  if (fixture == "centered_kick") {
    input[0] = makeFixture("transient");
    input[1] = input[0];
  } else if (fixture == "hard_panned") {
    input[0] = makeFixture("transient");
    input[1] = makeFixture("ambient");
    for (float& sample : input[1]) {
      sample *= 0.12F;
    }
  } else if (fixture == "correlated") {
    input[0] = makeFixture("dense");
    input[1].resize(input[0].size());
    for (std::size_t sample = 0; sample < input[0].size(); ++sample) {
      const float delayed = sample >= 37U ? input[0][sample - 37U] : 0.0F;
      input[1][sample] = 0.82F * input[0][sample] + 0.14F * delayed;
    }
  } else if (fixture == "decorrelated") {
    input[0] = makeFixture("ambient");
    input[1] = makeFixture("dense");
  } else if (fixture == "mono_stereo") {
    input[0] = makeFixture("dense");
    input[1] = input[0];
  } else {
    input[0] = makeFixture("ambient");
    input[1].resize(input[0].size());
    for (std::size_t sample = 0; sample < input[0].size(); ++sample) {
      input[1][sample] = -input[0][sample];
    }
  }
  return input;
}

int stereoStabilityReport(const std::filesystem::path& outputPath) {
  constexpr std::uint32_t sampleRate = 48000U;
  constexpr std::array<std::size_t, 1> schedule{127U};
  constexpr std::array<float, 3> links{0.0F, 0.5F, 1.0F};
  struct Metrics {
    double inputBalance{};
    double outputBalance{};
    double inputCorrelation{};
    double outputCorrelation{};
    double inputWidth{};
    double outputWidth{};
    double reduction{};
    double identityError{};
    double inversionError{};
    bool finite{};
  };
  const auto balanceDb = [](const std::vector<float>& left,
                            const std::vector<float>& right) {
    return 20.0 * std::log10(std::max(signalRms(left), 1.0e-15) /
                             std::max(signalRms(right), 1.0e-15));
  };
  const auto widthDb = [](const std::vector<float>& left,
                          const std::vector<float>& right) {
    double midEnergy{};
    double sideEnergy{};
    for (std::size_t sample = 0; sample < left.size(); ++sample) {
      const double mid = 0.5 * (left[sample] + right[sample]);
      const double side = 0.5 * (left[sample] - right[sample]);
      midEnergy += mid * mid;
      sideEnergy += side * side;
    }
    if (sideEnergy == 0.0) {
      return -300.0;
    }
    if (midEnergy == 0.0) {
      return 300.0;
    }
    return 10.0 * std::log10(sideEnergy / midEnergy);
  };

  std::ofstream output{outputPath};
  if (!output) {
    std::cerr << "cannot create stereo stability report: " << outputPath
              << '\n';
    return 1;
  }
  output << "fixture,link_percent,input_balance_db,output_balance_db,"
            "balance_shift_db,input_correlation,output_correlation,"
            "correlation_shift,input_side_mid_db,output_side_mid_db,"
            "width_shift_db,max_gain_reduction_db,identity_error,"
            "inversion_error,finite\n"
         << std::fixed << std::setprecision(9);

  bool valid = true;
  double identicalMaximumError{};
  double invertedMaximumError{};
  double maximumBalanceShift{};
  double maximumCorrelationShift{};
  double maximumFullyLinkedBalanceShift{};
  double maximumFullyLinkedCorrelationShift{};
  double minimumFullyLinkedBalanceImprovement =
      std::numeric_limits<double>::infinity();
  bool balancePreservationMonotonic = true;
  std::size_t fixtureCount{};
  for (std::string_view fixture : std::array<std::string_view, 6>{
           "centered_kick", "hard_panned", "correlated", "decorrelated",
           "mono_stereo", "polarity_inverted"}) {
    const auto input = makeStereoStabilityFixture(fixture);
    const auto& inputLeft = input[0];
    const auto& inputRight = input[1];

    std::array<double, links.size()> balanceShifts{};
    for (std::size_t mode = 0; mode < links.size(); ++mode) {
      const float link = links[mode];
      auto left = inputLeft;
      auto right = inputRight;
      auto parameters = productionParameters();
      parameters.driveDb = 12.0F;
      parameters.crush = 1.0F;
      parameters.density = 0.82F;
      parameters.blend = 1.0F;
      parameters.stereoLink = link;
      parameters.protection = false;
      aste::density::Processor processor;
      processor.prepare(sampleRate, parameters);
      const double reduction =
          processInBlocks(processor, left, right, schedule, parameters);

      double identityError{};
      double inversionError{};
      bool finite = true;
      for (std::size_t sample = 0; sample < left.size(); ++sample) {
        finite = finite && std::isfinite(left[sample]) &&
                 std::isfinite(right[sample]);
        identityError = std::max(
            identityError,
            std::abs(static_cast<double>(left[sample] - right[sample])));
        inversionError = std::max(
            inversionError,
            std::abs(static_cast<double>(left[sample] + right[sample])));
      }
      const Metrics metrics{
          .inputBalance = balanceDb(inputLeft, inputRight),
          .outputBalance = balanceDb(left, right),
          .inputCorrelation = stereoCorrelation(inputLeft, inputRight),
          .outputCorrelation = stereoCorrelation(left, right),
          .inputWidth = widthDb(inputLeft, inputRight),
          .outputWidth = widthDb(left, right),
          .reduction = reduction,
          .identityError = identityError,
          .inversionError = inversionError,
          .finite = finite,
      };
      const double balanceShift = metrics.outputBalance - metrics.inputBalance;
      balanceShifts[mode] = balanceShift;
      const double correlationShift =
          metrics.outputCorrelation - metrics.inputCorrelation;
      const double widthShift = metrics.outputWidth - metrics.inputWidth;
      output << fixture << ',' << 100.0F * link << ',' << metrics.inputBalance
             << ',' << metrics.outputBalance << ',' << balanceShift << ','
             << metrics.inputCorrelation << ',' << metrics.outputCorrelation
             << ',' << correlationShift << ',' << metrics.inputWidth << ','
             << metrics.outputWidth << ',' << widthShift << ','
             << metrics.reduction << ',' << metrics.identityError << ','
             << metrics.inversionError << ',' << (metrics.finite ? 1 : 0)
             << '\n';
      if (fixture == "centered_kick" || fixture == "mono_stereo") {
        identicalMaximumError =
            std::max(identicalMaximumError, metrics.identityError);
      }
      if (fixture == "polarity_inverted") {
        invertedMaximumError =
            std::max(invertedMaximumError, metrics.inversionError);
      }
      maximumBalanceShift =
          std::max(maximumBalanceShift, std::abs(balanceShift));
      maximumCorrelationShift =
          std::max(maximumCorrelationShift, std::abs(correlationShift));
      if (mode == links.size() - 1U) {
        maximumFullyLinkedBalanceShift =
            std::max(maximumFullyLinkedBalanceShift, std::abs(balanceShift));
        maximumFullyLinkedCorrelationShift = std::max(
            maximumFullyLinkedCorrelationShift, std::abs(correlationShift));
      }
      valid = valid && metrics.finite;
    }
    if (fixture == "hard_panned" || fixture == "correlated" ||
        fixture == "decorrelated") {
      balancePreservationMonotonic =
          balancePreservationMonotonic &&
          std::abs(balanceShifts[0]) >= std::abs(balanceShifts[1]) &&
          std::abs(balanceShifts[1]) >= std::abs(balanceShifts[2]);
      minimumFullyLinkedBalanceImprovement =
          std::min(minimumFullyLinkedBalanceImprovement,
                   std::abs(balanceShifts[0]) - std::abs(balanceShifts[2]));
    }
    ++fixtureCount;
  }
  valid = valid && static_cast<bool>(output) && fixtureCount == 6U &&
          identicalMaximumError == 0.0 && invertedMaximumError == 0.0 &&
          balancePreservationMonotonic;
  std::cout << std::fixed << std::setprecision(9)
            << "{\"fixtures\":" << fixtureCount
            << ",\"link_modes\":3,"
               "\"identical_max_error\":"
            << identicalMaximumError
            << ",\"inverted_max_error\":" << invertedMaximumError
            << ",\"max_abs_balance_shift_db\":" << maximumBalanceShift
            << ",\"max_abs_correlation_shift\":" << maximumCorrelationShift
            << ",\"max_fully_linked_balance_shift_db\":"
            << maximumFullyLinkedBalanceShift
            << ",\"max_fully_linked_correlation_shift\":"
            << maximumFullyLinkedCorrelationShift
            << ",\"minimum_fully_linked_balance_improvement_db\":"
            << minimumFullyLinkedBalanceImprovement
            << ",\"balance_preservation_monotonic\":"
            << (balancePreservationMonotonic ? "true" : "false")
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int stereoStabilityAuditions(const std::filesystem::path& directory) {
  constexpr std::uint32_t sampleRate = 48000U;
  constexpr std::array<std::size_t, 1> schedule{127U};
  constexpr std::array<float, 3> links{0.0F, 0.5F, 1.0F};
  constexpr std::array<std::string_view, 3> modes{"independent", "partial",
                                                  "linked"};
  constexpr std::array<char, 3> labels{'A', 'B', 'C'};
  constexpr std::uint32_t seed = 0xD0145U;
  const auto audioDirectory = directory / "audio";
  std::error_code error;
  std::filesystem::create_directories(audioDirectory, error);
  std::ofstream answers(directory / "answer-key.csv");
  std::ofstream responses(directory / "responses.csv");
  std::ofstream measurements(directory / "measurements.csv");
  std::ofstream notes(directory / "listening-notes.md");
  if (error || !answers || !responses || !measurements || !notes) {
    std::cerr << "cannot create stereo audition pack: " << directory << '\n';
    return 1;
  }
  answers << "fixture,A,B,C\n";
  responses << "fixture,most_stable(A/B/C/no preference),"
               "preferred(A/B/C/no preference),confidence(0-3),mono_notes,"
               "stereo_notes\n";
  measurements
      << "fixture,frames,independent_match_db,partial_match_db,linked_match_db,"
         "max_match_error_db,peak_dbfs,independent_partial_null_dbfs,"
         "partial_linked_null_dbfs,control_null\n"
      << std::fixed << std::setprecision(9);
  notes << "# Density stereo-link audition\n\n"
           "Do not open `answer-key.csv` until `responses.csv` is complete.\n\n"
           "Each A/B/C trial contains independent, partial, and fully linked "
           "detector processing in randomized order. Files are RMS matched and "
           "share -1 dBFS peak normalization. Judge image stability separately "
           "from preference on monitors and headphones, then check mono. Three "
           "fixtures are intentional null controls; report no preference when "
           "you cannot distinguish them.\n";

  std::uint32_t randomState = seed;
  double maximumMatchError{};
  double maximumControlNull = -std::numeric_limits<double>::infinity();
  bool valid = true;
  std::size_t fixtureCount{};
  for (std::string_view fixture : std::array<std::string_view, 6>{
           "centered_kick", "hard_panned", "correlated", "decorrelated",
           "mono_stereo", "polarity_inverted"}) {
    const auto input = makeStereoStabilityFixture(fixture);
    std::array<std::array<std::vector<float>, 2>, links.size()> renders;
    std::array<double, links.size()> sourceRms{};
    for (std::size_t mode = 0; mode < links.size(); ++mode) {
      renders[mode] = input;
      auto parameters = productionParameters();
      parameters.driveDb = 12.0F;
      parameters.crush = 1.0F;
      parameters.density = 0.82F;
      parameters.blend = 1.0F;
      parameters.stereoLink = links[mode];
      parameters.protection = false;
      aste::density::Processor processor;
      processor.prepare(sampleRate, parameters);
      processInBlocks(processor, renders[mode][0], renders[mode][1], schedule,
                      parameters);
      sourceRms[mode] = stereoRms(renders[mode][0], renders[mode][1]);
    }

    const double targetRms =
        *std::min_element(sourceRms.begin(), sourceRms.end());
    std::array<float, links.size()> matches{};
    for (std::size_t mode = 0; mode < links.size(); ++mode) {
      matches[mode] = static_cast<float>(targetRms / sourceRms[mode]);
      for (std::size_t sample = 0; sample < input[0].size(); ++sample) {
        renders[mode][0][sample] *= matches[mode];
        renders[mode][1][sample] *= matches[mode];
      }
    }
    double largestPeak{};
    for (const auto& render : renders) {
      largestPeak = std::max(largestPeak, stereoPeak(render[0], render[1]));
    }
    const float commonGain =
        static_cast<float>(0.8912509381337456 / largestPeak);
    for (auto& render : renders) {
      for (std::size_t sample = 0; sample < input[0].size(); ++sample) {
        render[0][sample] *= commonGain;
        render[1][sample] *= commonGain;
      }
    }

    double fixtureMatchError{};
    for (const auto& render : renders) {
      fixtureMatchError = std::max(
          fixtureMatchError,
          std::abs(20.0 * std::log10(stereoRms(render[0], render[1]) /
                                     stereoRms(renders[0][0], renders[0][1]))));
    }
    maximumMatchError = std::max(maximumMatchError, fixtureMatchError);
    const double peakDb = 20.0 * std::log10(largestPeak * commonGain);
    const double independentPartialNull = stereoNullDb(renders[0], renders[1]);
    const double partialLinkedNull = stereoNullDb(renders[1], renders[2]);
    const bool control = fixture == "centered_kick" ||
                         fixture == "mono_stereo" ||
                         fixture == "polarity_inverted";
    if (control) {
      maximumControlNull = std::max(
          {maximumControlNull, independentPartialNull, partialLinkedNull});
    }

    std::array<std::size_t, links.size()> order{0U, 1U, 2U};
    for (std::size_t remaining = order.size(); remaining > 1U; --remaining) {
      randomState = randomState * 1664525U + 1013904223U;
      std::swap(order[remaining - 1U], order[(randomState >> 16U) % remaining]);
    }
    answers << fixture;
    bool wrote = true;
    for (std::size_t position = 0; position < order.size(); ++position) {
      const std::size_t mode = order[position];
      const auto path = audioDirectory / (std::string{fixture} + '-' +
                                          labels[position] + ".wav");
      wrote = writeStereoFloatWave(path, renders[mode][0], renders[mode][1],
                                   sampleRate) &&
              wrote;
      answers << ',' << modes[mode];
    }
    answers << '\n';
    responses << fixture << ",,,,,\n";
    measurements << fixture << ',' << input[0].size();
    for (float match : matches) {
      measurements << ',' << 20.0 * std::log10(match);
    }
    measurements << ',' << fixtureMatchError << ',' << peakDb << ','
                 << independentPartialNull << ',' << partialLinkedNull << ','
                 << (control ? 1 : 0) << '\n';
    valid = valid && wrote && fixtureMatchError < 0.001 &&
            std::abs(peakDb + 1.0) < 0.001 &&
            (!control ||
             (independentPartialNull <= -250.0 && partialLinkedNull <= -250.0));
    ++fixtureCount;
  }
  valid = valid && static_cast<bool>(answers) && static_cast<bool>(responses) &&
          static_cast<bool>(measurements) && static_cast<bool>(notes) &&
          fixtureCount == 6U;
  std::cout << std::fixed << std::setprecision(6)
            << "{\"fixtures\":" << fixtureCount
            << ",\"files\":18,\"seed\":" << seed
            << ",\"max_match_error_db\":" << maximumMatchError
            << ",\"max_control_null_dbfs\":" << maximumControlNull
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int densityMacroAuditions(const std::filesystem::path& directory) {
  constexpr std::uint32_t sampleRate = 48000U;
  constexpr std::array<std::size_t, 1> schedule{127U};
  constexpr std::array<float, 4> densities{0.0F, 0.33F, 0.67F, 1.0F};
  constexpr std::array<std::string_view, 4> levels{"0", "33", "67", "100"};
  constexpr std::array<char, 4> labels{'A', 'B', 'C', 'D'};
  constexpr std::uint32_t seed = 0xD0146U;
  const auto audioDirectory = directory / "audio";
  std::error_code error;
  std::filesystem::create_directories(audioDirectory, error);
  std::ofstream answers(directory / "answer-key.csv");
  std::ofstream responses(directory / "responses.csv");
  std::ofstream measurements(directory / "measurements.csv");
  std::ofstream notes(directory / "listening-notes.md");
  if (error || !answers || !responses || !measurements || !notes) {
    std::cerr << "cannot create Density macro audition pack: " << directory
              << '\n';
    return 1;
  }
  answers << "trial,A,B,C,D\n";
  responses << "trial,least_to_most_dense(A>B>C>D),confidence(0-3),"
               "preferred(A/B/C/D/no preference),notes\n";
  measurements
      << "trial,frames,density_0_gr_db,density_33_gr_db,density_67_gr_db,"
         "density_100_gr_db,max_match_error_db,peak_dbfs,null_0_33_dbfs,"
         "null_33_67_dbfs,null_67_100_dbfs,control_null\n"
      << std::fixed << std::setprecision(9);
  notes
      << "# Density macro audition\n\n"
         "Do not open `answer-key.csv` until `responses.csv` is complete.\n\n"
         "Rank A/B/C/D from least to most dense for each trial. The four "
         "files contain 0%, 33%, 67%, and 100% Density in randomized order, "
         "are RMS matched, and share -1 dBFS peak normalization. Use monitors "
         "and headphones at a fixed level. `dry_control` is intentionally "
         "identical; leave its ranking blank and report no preference when "
         "you cannot distinguish the files.\n";

  std::uint32_t randomState = seed;
  double maximumMatchError{};
  double maximumControlNull = -std::numeric_limits<double>::infinity();
  double minimumAudibleNull = std::numeric_limits<double>::infinity();
  bool valid = true;
  std::size_t trialCount{};
  for (std::string_view trial : std::array<std::string_view, 5>{
           "transient", "bass", "dense", "ambient", "dry_control"}) {
    const bool control = trial == "dry_control";
    const auto mono = makeFixture(control ? "dense" : trial);
    const std::array<std::vector<float>, 2> input{mono, mono};
    std::array<std::array<std::vector<float>, 2>, densities.size()> renders;
    std::array<double, densities.size()> sourceRms{};
    std::array<double, densities.size()> reductions{};
    for (std::size_t level = 0; level < densities.size(); ++level) {
      renders[level] = input;
      auto parameters = productionParameters();
      parameters.density = densities[level];
      if (control) {
        parameters.blend = 0.0F;
      }
      aste::density::Processor processor;
      processor.prepare(sampleRate, parameters);
      reductions[level] =
          processInBlocks(processor, renders[level][0], renders[level][1],
                          schedule, parameters);
      sourceRms[level] = stereoRms(renders[level][0], renders[level][1]);
    }

    const double targetRms =
        *std::min_element(sourceRms.begin(), sourceRms.end());
    for (std::size_t level = 0; level < densities.size(); ++level) {
      const float match = static_cast<float>(targetRms / sourceRms[level]);
      for (std::size_t sample = 0; sample < mono.size(); ++sample) {
        renders[level][0][sample] *= match;
        renders[level][1][sample] *= match;
      }
    }
    double largestPeak{};
    for (const auto& render : renders) {
      largestPeak = std::max(largestPeak, stereoPeak(render[0], render[1]));
    }
    const float commonGain =
        static_cast<float>(0.8912509381337456 / largestPeak);
    for (auto& render : renders) {
      for (std::size_t sample = 0; sample < mono.size(); ++sample) {
        render[0][sample] *= commonGain;
        render[1][sample] *= commonGain;
      }
    }

    double matchError{};
    for (const auto& render : renders) {
      matchError = std::max(
          matchError,
          std::abs(20.0 * std::log10(stereoRms(render[0], render[1]) /
                                     stereoRms(renders[0][0], renders[0][1]))));
    }
    maximumMatchError = std::max(maximumMatchError, matchError);
    const double peakDb = 20.0 * std::log10(largestPeak * commonGain);
    std::array<double, 3> nulls{};
    for (std::size_t level = 0; level < nulls.size(); ++level) {
      nulls[level] = stereoNullDb(renders[level], renders[level + 1U]);
      if (control) {
        maximumControlNull = std::max(maximumControlNull, nulls[level]);
      } else {
        minimumAudibleNull = std::min(minimumAudibleNull, nulls[level]);
      }
    }

    std::array<std::size_t, densities.size()> order{0U, 1U, 2U, 3U};
    for (std::size_t remaining = order.size(); remaining > 1U; --remaining) {
      randomState = randomState * 1664525U + 1013904223U;
      std::swap(order[remaining - 1U], order[(randomState >> 16U) % remaining]);
    }
    answers << trial;
    bool wrote = true;
    for (std::size_t position = 0; position < order.size(); ++position) {
      const std::size_t level = order[position];
      const auto path = audioDirectory /
                        (std::string{trial} + '-' + labels[position] + ".wav");
      wrote = writeStereoFloatWave(path, renders[level][0], renders[level][1],
                                   sampleRate) &&
              wrote;
      answers << ',' << levels[level];
    }
    answers << '\n';
    responses << trial << ",,,,\n";
    measurements << trial << ',' << mono.size();
    for (double reductionDb : reductions) {
      measurements << ',' << reductionDb;
    }
    measurements << ',' << matchError << ',' << peakDb;
    for (double null : nulls) {
      measurements << ',' << null;
    }
    measurements << ',' << (control ? 1 : 0) << '\n';
    const bool reductionMonotonic =
        std::is_sorted(reductions.begin(), reductions.end());
    valid = valid && wrote && reductionMonotonic && matchError < 0.001 &&
            std::abs(peakDb + 1.0) < 0.001 &&
            (control ? maximumControlNull <= -250.0
                     : *std::min_element(nulls.begin(), nulls.end()) > -100.0);
    ++trialCount;
  }
  valid = valid && static_cast<bool>(answers) && static_cast<bool>(responses) &&
          static_cast<bool>(measurements) && static_cast<bool>(notes) &&
          trialCount == 5U;
  std::cout << std::fixed << std::setprecision(6)
            << "{\"trials\":" << trialCount << ",\"files\":20,\"seed\":" << seed
            << ",\"max_match_error_db\":" << maximumMatchError
            << ",\"minimum_audible_null_dbfs\":" << minimumAudibleNull
            << ",\"max_control_null_dbfs\":" << maximumControlNull
            << ",\"measurement_valid\":" << (valid ? "true" : "false") << "}\n";
  return valid ? 0 : 2;
}

int detectorAuditions(const std::filesystem::path& directory) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) {
    std::cerr << "cannot create output directory: " << directory << '\n';
    return 1;
  }

  bool valid = true;
  for (std::string_view name : std::array<std::string_view, 4>{
           "transient", "bass", "dense", "ambient"}) {
    const auto input = makeFixture(name);
    std::vector<float> current(input.size());
    std::vector<float> candidate(input.size());
    std::vector<float> dual(input.size());
    std::vector<float> programme(input.size());
    std::vector<float> hybrid(input.size());
    std::vector<float> feedback(input.size());
    DetectorResearch detectors;
    for (std::size_t i = 0; i < input.size(); ++i) {
      const auto [currentReduction, candidateReduction, dualReduction,
                  programmeReduction, hybridReduction, feedbackReduction] =
          detectors.process(input[i]);
      current[i] =
          input[i] * std::exp(-0.11512925464970229F * currentReduction);
      candidate[i] =
          input[i] * std::exp(-0.11512925464970229F * candidateReduction);
      dual[i] = input[i] * std::exp(-0.11512925464970229F * dualReduction);
      programme[i] =
          input[i] * std::exp(-0.11512925464970229F * programmeReduction);
      hybrid[i] = input[i] * std::exp(-0.11512925464970229F * hybridReduction);
      feedback[i] =
          input[i] * std::exp(-0.11512925464970229F * feedbackReduction);
    }

    const double currentRms = signalRms(current);
    const double candidateRms = signalRms(candidate);
    const double dualRms = signalRms(dual);
    const double programmeRms = signalRms(programme);
    const double hybridRms = signalRms(hybrid);
    const double feedbackRms = signalRms(feedback);
    const double targetRms = std::min({currentRms, candidateRms, dualRms,
                                       programmeRms, hybridRms, feedbackRms});
    const float currentMatch = static_cast<float>(targetRms / currentRms);
    const float candidateMatch = static_cast<float>(targetRms / candidateRms);
    const float dualMatch = static_cast<float>(targetRms / dualRms);
    const float programmeMatch = static_cast<float>(targetRms / programmeRms);
    const float hybridMatch = static_cast<float>(targetRms / hybridRms);
    const float feedbackMatch = static_cast<float>(targetRms / feedbackRms);
    for (std::size_t i = 0; i < input.size(); ++i) {
      current[i] *= currentMatch;
      candidate[i] *= candidateMatch;
      dual[i] *= dualMatch;
      programme[i] *= programmeMatch;
      hybrid[i] *= hybridMatch;
      feedback[i] *= feedbackMatch;
    }
    const float commonGain =
        0.89125094F / std::max({signalPeak(current), signalPeak(candidate),
                                signalPeak(dual), signalPeak(programme),
                                signalPeak(hybrid), signalPeak(feedback)});
    for (std::size_t i = 0; i < input.size(); ++i) {
      current[i] *= commonGain;
      candidate[i] *= commonGain;
      dual[i] *= commonGain;
      programme[i] *= commonGain;
      hybrid[i] *= commonGain;
      feedback[i] *= commonGain;
    }

    const auto currentPath = directory / (std::string{name} + "-current.wav");
    const auto candidatePath =
        directory / (std::string{name} + "-rms-peak.wav");
    const auto dualPath = directory / (std::string{name} + "-dual-time.wav");
    const auto programmePath =
        directory / (std::string{name} + "-programme.wav");
    const auto hybridPath = directory / (std::string{name} + "-hybrid.wav");
    const auto feedbackPath = directory / (std::string{name} + "-feedback.wav");
    const double matchedCurrentRms = signalRms(current);
    const double matchedCandidateRms = signalRms(candidate);
    const double candidateMatchErrorDb =
        20.0 * std::log10(matchedCurrentRms / matchedCandidateRms);
    const double matchedDualRms = signalRms(dual);
    const double dualMatchErrorDb =
        20.0 * std::log10(matchedCurrentRms / matchedDualRms);
    const double matchedProgrammeRms = signalRms(programme);
    const double programmeMatchErrorDb =
        20.0 * std::log10(matchedCurrentRms / matchedProgrammeRms);
    const double matchedHybridRms = signalRms(hybrid);
    const double hybridMatchErrorDb =
        20.0 * std::log10(matchedCurrentRms / matchedHybridRms);
    const double matchedFeedbackRms = signalRms(feedback);
    const double feedbackMatchErrorDb =
        20.0 * std::log10(matchedCurrentRms / matchedFeedbackRms);
    const bool currentWritten = writeFloatWave(currentPath, current, 48000U);
    const bool candidateWritten =
        writeFloatWave(candidatePath, candidate, 48000U);
    const bool dualWritten = writeFloatWave(dualPath, dual, 48000U);
    const bool programmeWritten =
        writeFloatWave(programmePath, programme, 48000U);
    const bool hybridWritten = writeFloatWave(hybridPath, hybrid, 48000U);
    const bool feedbackWritten = writeFloatWave(feedbackPath, feedback, 48000U);
    valid = currentWritten && candidateWritten && dualWritten &&
            programmeWritten && hybridWritten && feedbackWritten && valid &&
            std::isfinite(candidateMatchErrorDb) &&
            std::abs(candidateMatchErrorDb) < 0.001 &&
            std::isfinite(dualMatchErrorDb) &&
            std::abs(dualMatchErrorDb) < 0.001;
    valid = valid && std::isfinite(programmeMatchErrorDb) &&
            std::abs(programmeMatchErrorDb) < 0.001;
    valid = valid && std::isfinite(hybridMatchErrorDb) &&
            std::abs(hybridMatchErrorDb) < 0.001;
    valid = valid && std::isfinite(feedbackMatchErrorDb) &&
            std::abs(feedbackMatchErrorDb) < 0.001;
    std::cout
        << std::fixed << std::setprecision(6) << "{\"fixture\":\"" << name
        << "\",\"rms_dbfs\":" << 20.0 * std::log10(matchedCurrentRms)
        << ",\"rms_peak_match_error_db\":" << candidateMatchErrorDb
        << ",\"dual_time_match_error_db\":" << dualMatchErrorDb
        << ",\"programme_match_error_db\":" << programmeMatchErrorDb
        << ",\"hybrid_match_error_db\":" << hybridMatchErrorDb
        << ",\"feedback_match_error_db\":" << feedbackMatchErrorDb
        << ",\"current_peak_dbfs\":" << 20.0 * std::log10(signalPeak(current))
        << ",\"rms_peak_peak_dbfs\":"
        << 20.0 * std::log10(signalPeak(candidate))
        << ",\"dual_time_peak_dbfs\":" << 20.0 * std::log10(signalPeak(dual))
        << ",\"programme_peak_dbfs\":"
        << 20.0 * std::log10(signalPeak(programme))
        << ",\"hybrid_peak_dbfs\":" << 20.0 * std::log10(signalPeak(hybrid))
        << ",\"feedback_peak_dbfs\":" << 20.0 * std::log10(signalPeak(feedback))
        << ",\"current_match_db\":" << 20.0 * std::log10(currentMatch)
        << ",\"rms_peak_match_db\":" << 20.0 * std::log10(candidateMatch)
        << ",\"dual_time_match_db\":" << 20.0 * std::log10(dualMatch)
        << ",\"programme_match_db\":" << 20.0 * std::log10(programmeMatch)
        << ",\"hybrid_match_db\":" << 20.0 * std::log10(hybridMatch)
        << ",\"feedback_match_db\":" << 20.0 * std::log10(feedbackMatch)
        << "}\n";
  }
  return valid ? 0 : 2;
}

int detectorBlindPack(const std::filesystem::path& directory) {
  const auto sourceDirectory = directory / "source";
  if (detectorAuditions(sourceDirectory) != 0) {
    return 2;
  }

  const auto audioDirectory = directory / "audio";
  std::error_code directoryError;
  std::filesystem::create_directories(audioDirectory, directoryError);
  std::ofstream responses(directory / "responses.csv");
  std::ofstream answerKey(directory / "answer-key.csv");
  if (directoryError || !responses || !answerKey) {
    std::cerr << "cannot create blind listening pack: " << directory << '\n';
    return 1;
  }

  responses
      << "trial,fixture,preferred(A/B/no preference),confidence(0-3),notes\n";
  answerKey << "trial,fixture,A,B\n";
  std::uint32_t randomState = 0xD01B11DU;
  std::size_t trial = 0;
  bool valid = true;
  for (std::string_view fixture : std::array<std::string_view, 4>{
           "transient", "bass", "dense", "ambient"}) {
    std::array<std::string_view, 5> candidates{
        "rms-peak", "dual-time", "programme", "hybrid", "feedback"};
    for (std::size_t i = candidates.size(); i > 1; --i) {
      randomState = randomState * 1664525U + 1013904223U;
      std::swap(candidates[i - 1], candidates[randomState % i]);
    }
    for (std::string_view candidate : candidates) {
      ++trial;
      randomState = randomState * 1664525U + 1013904223U;
      const bool candidateIsA = (randomState & 1U) != 0U;
      const std::string number = std::to_string(trial);
      const std::string prefix =
          "trial-" + std::string(3U - number.size(), '0') + number;
      const auto currentSource =
          sourceDirectory / (std::string{fixture} + "-current.wav");
      const auto candidateSource =
          sourceDirectory /
          (std::string{fixture} + "-" + std::string{candidate} + ".wav");
      std::error_code errorA;
      std::error_code errorB;
      std::filesystem::copy_file(
          candidateIsA ? candidateSource : currentSource,
          audioDirectory / (prefix + "-A.wav"),
          std::filesystem::copy_options::overwrite_existing, errorA);
      std::filesystem::copy_file(
          candidateIsA ? currentSource : candidateSource,
          audioDirectory / (prefix + "-B.wav"),
          std::filesystem::copy_options::overwrite_existing, errorB);
      valid = !errorA && !errorB && valid;
      responses << prefix.substr(6) << ',' << fixture << ",,,\n";
      answerKey << prefix.substr(6) << ',' << fixture << ','
                << (candidateIsA ? candidate : "current") << ','
                << (candidateIsA ? "current" : candidate) << '\n';
    }
  }
  valid = valid && static_cast<bool>(responses) &&
          static_cast<bool>(answerKey) && trial == 20U;
  if (!valid) {
    std::cerr << "incomplete blind listening pack: " << directory << '\n';
    return 2;
  }
  std::cout << "{\"blind_trials\":" << trial << ",\"seed\":218214685}\n";
  return 0;
}

int benchmark() {
  constexpr double sampleRate = 48000.0;
  constexpr std::size_t blockSize = 128;
  constexpr double renderedSeconds = 120.0;
  constexpr std::size_t blocks =
      static_cast<std::size_t>(sampleRate * renderedSeconds) / blockSize;
  aste::density::Parameters parameters;
  parameters.driveDb = 9.0F;
  parameters.density = 0.72F;
  parameters.crush = 0.82F;
  parameters.blend = 0.65F;

  std::vector<float> left(blockSize);
  std::vector<float> right(blockSize);
  for (std::size_t i = 0; i < blockSize; ++i) {
    left[i] = 0.7F * std::sin(static_cast<float>(i) * 0.13F);
    right[i] = 0.5F * std::sin(static_cast<float>(i) * 0.17F);
  }

  aste::density::Processor processor;
  processor.prepare(sampleRate, parameters);
  const auto start = std::chrono::steady_clock::now();
  for (std::size_t block = 0; block < blocks; ++block) {
    parameters.density = block % 2 == 0 ? 0.2F : 0.9F;
    parameters.stereoLink = block % 2 == 0 ? 0.0F : 1.0F;
    processor.process(left.data(), right.data(), blockSize, parameters);
  }
  const double elapsed =
      std::chrono::duration<double>(std::chrono::steady_clock::now() - start)
          .count();
  double checksum{};
  for (std::size_t i = 0; i < blockSize; ++i) {
    checksum += left[i] + right[i];
  }
  std::cout << std::fixed << std::setprecision(6)
            << "{\"sample_rate\":" << sampleRate
            << ",\"block_size\":" << blockSize
            << ",\"rendered_seconds\":" << renderedSeconds
            << ",\"elapsed_seconds\":" << elapsed
            << ",\"one_core_percent\":" << 100.0 * elapsed / renderedSeconds
            << ",\"checksum\":" << checksum << "}\n";
  return 0;
}

}  // namespace

int main(int argc, char** argv) {
  if (argc == 2 && std::string{argv[1]} == "--benchmark") {
    return benchmark();
  }
  if (argc >= 2 && std::string{argv[1]} == "--detector-compare") {
    return detectorComparison(argc >= 3 ? argv[2] : "detector-comparison.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--detector-auditions") {
    return detectorAuditions(argc >= 3 ? argv[2] : "detector-auditions");
  }
  if (argc >= 2 && std::string{argv[1]} == "--detector-blind") {
    return detectorBlindPack(argc >= 3 ? argv[2] : "detector-blind");
  }
  if (argc >= 2 && std::string{argv[1]} == "--golden") {
    return productionGolden(argc >= 3 ? argv[2] : "density-golden",
                            argc >= 4 ? argv[3] : std::filesystem::path{});
  }
  if (argc >= 2 && std::string{argv[1]} == "--production-consistency") {
    return productionConsistency(argc >= 3 ? argv[2]
                                           : "density-consistency.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--alias-report") {
    return nonlinearAliasReport(argc >= 3 ? argv[2] : "density-alias.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--oversampling-report") {
    return oversamplingReferenceReport(argc >= 3 ? argv[2]
                                                 : "density-oversampling.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--oversampling-prototype") {
    return oversamplingPrototypeReport(
        argc >= 3 ? argv[2] : "density-oversampling-prototype.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--halfband-prototype") {
    return halfBandPrototypeReport(
        argc >= 3 ? argv[2] : "density-halfband-prototype.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--kaiser-prototype") {
    return kaiserPrototypeReport(argc >= 3 ? argv[2]
                                           : "density-kaiser-prototype.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--kaiser-sweep") {
    return kaiserSweepReport(argc >= 3 ? argv[2] : "density-kaiser-sweep.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--kaiser-rate-sweep") {
    return kaiserRateSweepReport(argc >= 3 ? argv[2]
                                           : "density-kaiser-rate-sweep.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--kaiser-linear-report") {
    return kaiserLinearReport(argc >= 3 ? argv[2]
                                        : "density-kaiser-linear.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--kaiser-length-report") {
    return kaiserLengthReport(argc >= 3 ? argv[2]
                                        : "density-kaiser-length.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--kaiser-finalist-report") {
    return kaiserFinalistReport(argc >= 3 ? argv[2]
                                          : "density-kaiser-finalists.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--oversampled-chain-report") {
    return oversampledChainReport(argc >= 3 ? argv[2]
                                            : "density-oversampled-chain.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--oversampling-auditions") {
    return oversamplingAuditions(argc >= 3 ? argv[2]
                                           : "density-oversampling-auditions");
  }
  if (argc >= 2 && std::string{argv[1]} == "--automation-report") {
    return automationReport(argc >= 3 ? argv[2] : "density-automation.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--output-smoothing-report") {
    return outputSmoothingReport(argc >= 3 ? argv[2]
                                           : "density-output-smoothing.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--drive-smoothing-report") {
    return driveSmoothingReport(argc >= 3 ? argv[2]
                                          : "density-drive-smoothing.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--attack-smoothing-report") {
    return attackSmoothingReport(argc >= 3 ? argv[2]
                                           : "density-attack-smoothing.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--blend-smoothing-report") {
    return blendSmoothingReport(argc >= 3 ? argv[2]
                                          : "density-blend-smoothing.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--automation-auditions") {
    return automationAuditions(argc >= 3 ? argv[2]
                                         : "density-automation-auditions");
  }
  if (argc >= 2 && std::string{argv[1]} == "--stereo-stability-report") {
    return stereoStabilityReport(argc >= 3 ? argv[2]
                                           : "density-stereo-stability.csv");
  }
  if (argc >= 2 && std::string{argv[1]} == "--stereo-stability-auditions") {
    return stereoStabilityAuditions(
        argc >= 3 ? argv[2] : "density-stereo-stability-auditions");
  }
  if (argc >= 2 && std::string{argv[1]} == "--density-macro-auditions") {
    return densityMacroAuditions(argc >= 3 ? argv[2]
                                           : "density-macro-auditions");
  }
  const std::string outputPath = argc > 1 ? argv[1] : "density.csv";
  std::ofstream output(outputPath);
  if (!output) {
    std::cerr << "cannot open output: " << outputPath << '\n';
    return 1;
  }

  constexpr double sampleRate = 48000.0;
  constexpr std::size_t blockSize = 127;
  constexpr std::size_t totalFrames = 96000;
  aste::density::Parameters parameters;
  parameters.driveDb = 9.0F;
  parameters.density = 0.72F;
  parameters.crush = 0.82F;
  parameters.blend = 0.65F;

  aste::density::Processor processor;
  processor.prepare(sampleRate, parameters);
  std::vector<float> left(blockSize);
  std::vector<float> right(blockSize);
  std::vector<float> input(blockSize);
  output << "sample,input,output,block_max_gain_reduction_db\n";

  double sumSquares = 0.0;
  float peak = 0.0F;
  float maximumReduction = 0.0F;
  std::size_t rendered = 0;
  while (rendered < totalFrames) {
    const std::size_t frames = std::min(blockSize, totalFrames - rendered);
    for (std::size_t i = 0; i < frames; ++i) {
      const std::size_t sample = rendered + i;
      const float amplitude = sample < totalFrames / 3       ? 0.08F
                              : sample < 2 * totalFrames / 3 ? 0.35F
                                                             : 0.9F;
      const float signal =
          amplitude * static_cast<float>(
                          std::sin(2.0 * 3.14159265358979323846 * 997.0 *
                                   static_cast<double>(sample) / sampleRate));
      input[i] = left[i] = signal;
      right[i] = signal;
    }
    processor.process(left.data(), right.data(), frames, parameters);
    const auto meters = processor.meters();
    maximumReduction = std::max(maximumReduction, meters.gainReductionDb);
    for (std::size_t i = 0; i < frames; ++i) {
      output << (rendered + i) << ',' << input[i] << ',' << left[i] << ','
             << meters.gainReductionDb << '\n';
      peak = std::max(peak, std::abs(left[i]));
      sumSquares += static_cast<double>(left[i]) * left[i];
    }
    rendered += frames;
  }

  std::cout << std::fixed << std::setprecision(6) << "{\"frames\":" << rendered
            << ",\"sample_rate\":" << sampleRate << ",\"peak\":" << peak
            << ",\"rms\":"
            << std::sqrt(sumSquares / static_cast<double>(rendered))
            << ",\"max_gain_reduction_db\":" << maximumReduction
            << ",\"latency_samples\":" << processor.latencySamples() << "}\n";
}
