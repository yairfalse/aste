#include "density_processor.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
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
    const float detector = std::sqrt((1.0F - peakInfluence) * 2.0F * meanSquare +
                                     peakInfluence * instantaneous * instantaneous);
    if (detector > rmsEnvelope) {
      rmsEnvelope =
          detector + attackCoefficient * (rmsEnvelope - detector);
    } else {
      const float programme = 1.0F + (mapping.releaseCurve - 1.0F) *
                                         std::clamp(rmsEnvelope, 0.0F, 1.0F);
      const float releaseCoefficient =
          coefficient(parameters.releaseMs * programme * 0.001, sampleRate);
      rmsEnvelope =
          detector + releaseCoefficient * (rmsEnvelope - detector);
    }

    dualFast = instantaneous > dualFast
                   ? instantaneous + attackCoefficient * (dualFast - instantaneous)
                   : instantaneous +
                         fastReleaseCoefficient * (dualFast - instantaneous);
    dualSlow = instantaneous > dualSlow
                   ? instantaneous +
                         slowAttackCoefficient * (dualSlow - instantaneous)
                   : instantaneous +
                         slowReleaseCoefficient * (dualSlow - instantaneous);

    programmeMemory = instantaneous > programmeMemory
                          ? instantaneous + programmeAttackCoefficient *
                                                (programmeMemory - instantaneous)
                          : instantaneous + programmeReleaseCoefficient *
                                                (programmeMemory - instantaneous);
    if (instantaneous > programmeEnvelope) {
      programmeEnvelope = instantaneous +
                          attackCoefficient * (programmeEnvelope - instantaneous);
    } else {
      const float releaseSeconds =
          0.060F + 0.540F * std::clamp(programmeMemory * 2.0F, 0.0F, 1.0F);
      const float releaseCoefficient = coefficient(releaseSeconds, sampleRate);
      programmeEnvelope = instantaneous +
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
        solvedEnvelope = feedbackDetector +
                         attackCoefficient *
                             (feedbackEnvelope - feedbackDetector);
      } else {
        const float programme = 1.0F + (mapping.releaseCurve - 1.0F) *
                                           std::clamp(feedbackEnvelope, 0.0F, 1.0F);
        const float releaseCoefficient =
            coefficient(parameters.releaseMs * programme * 0.001, sampleRate);
        solvedEnvelope = feedbackDetector +
                         releaseCoefficient *
                             (feedbackEnvelope - feedbackDetector);
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
  std::array<std::size_t, 6> sustainReleaseSamples{
      totalFrames, totalFrames, totalFrames,
      totalFrames, totalFrames, totalFrames};
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
      input = 0.5F * static_cast<float>(
                         std::sin(2.0 * kPi * 997.0 * i / sampleRate));
    } else if (i == impulseSample) {
      input = 1.0F;
    } else if (i >= burstStart && i < burstEnd) {
      input = 0.8F * static_cast<float>(
                         std::sin(2.0 * kPi * 997.0 * i / sampleRate));
    }

    const auto [currentReduction, candidateReduction, dualReduction,
                programmeReduction, hybridReduction, feedbackReduction] =
        detectors.process(input);
    maxFeedbackResidual =
        std::max(maxFeedbackResidual, detectors.maxFeedbackResidual);
    const std::array<float, 6> reductions{
        currentReduction, candidateReduction, dualReduction,
        programmeReduction, hybridReduction, feedbackReduction};
    output << i << ',' << input << ',' << currentReduction << ','
           << candidateReduction << ',' << dualReduction << ','
           << programmeReduction << ',' << hybridReduction << ','
           << feedbackReduction << '\n';

    if (i + 1 == impulseSample) {
      beforeImpulse = {currentReduction, candidateReduction, dualReduction,
                       programmeReduction, hybridReduction, feedbackReduction};
    }
    if (i + 1 == burstStart) {
      beforeBurst = {currentReduction, candidateReduction, dualReduction,
                     programmeReduction, hybridReduction, feedbackReduction};
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
      programmeImpulseMax =
          std::max(programmeImpulseMax, programmeReduction);
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
    if (i >= burstEnd && dualReleaseSample == totalFrames && dualReduction < 1.0F) {
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
      1000.0 * static_cast<double>(currentReleaseSample - burstEnd) / sampleRate;
  const double candidateReleaseMs =
      1000.0 * static_cast<double>(candidateReleaseSample - burstEnd) / sampleRate;
  const double dualReleaseMs =
      1000.0 * static_cast<double>(dualReleaseSample - burstEnd) / sampleRate;
  const double programmeReleaseMs =
      1000.0 * static_cast<double>(programmeReleaseSample - burstEnd) / sampleRate;
  const double hybridReleaseMs =
      1000.0 * static_cast<double>(hybridReleaseSample - burstEnd) / sampleRate;
  const double feedbackReleaseMs =
      1000.0 * static_cast<double>(feedbackReleaseSample - burstEnd) / sampleRate;
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
            << ",\"dual_time_sustain_release_to_1db_ms\":" << sustainReleaseMs[2]
            << ",\"programme_sustain_release_to_1db_ms\":" << sustainReleaseMs[3]
            << ",\"hybrid_sustain_release_to_1db_ms\":" << sustainReleaseMs[4]
            << ",\"feedback_sustain_release_to_1db_ms\":" << sustainReleaseMs[5]
            << ",\"max_pre_impulse_gr_db\":"
            << *std::max_element(beforeImpulse.begin(), beforeImpulse.end())
            << ",\"max_pre_burst_gr_db\":"
            << *std::max_element(beforeBurst.begin(), beforeBurst.end())
            << ",\"max_feedback_gain_residual\":" << maxFeedbackResidual
            << "}\n";

  const bool valid = std::isfinite(currentSustain) &&
                     std::isfinite(candidateSustain) && currentSustain > 0.0 &&
                     candidateSustain > 0.0 &&
                     std::abs(currentSustain - candidateSustain) < 0.1 &&
                     currentImpulseMax > candidateImpulseMax &&
                     std::isfinite(dualSustain) && dualSustain > 0.0 &&
                     std::isfinite(programmeSustain) && programmeSustain > 0.0 &&
                     std::isfinite(hybridSustain) && hybridSustain > 0.0 &&
                     std::isfinite(feedbackSustain) && feedbackSustain > 0.0 &&
                     std::abs(currentSustain - dualSustain) < 0.5 &&
                     std::abs(currentImpulseMax - dualImpulseMax) < 0.001 &&
                     std::abs(currentBurstMax - dualBurstMax) < 0.1 &&
                     std::abs(currentSustain - programmeSustain) < 0.1 &&
                     std::abs(currentImpulseMax - programmeImpulseMax) < 0.001 &&
                     std::abs(currentBurstMax - programmeBurstMax) < 0.1 &&
                     std::abs(currentSustain - hybridSustain) < 0.1 &&
                     hybridImpulseMax > candidateImpulseMax &&
                     hybridImpulseMax < currentImpulseMax &&
                     hybridBurstMax > candidateBurstMax &&
                     hybridBurstMax < currentBurstMax &&
                     std::abs(currentSustain - feedbackSustain) < 0.01 &&
                     feedbackImpulseMax > currentImpulseMax &&
                     feedbackBurstMax < candidateBurstMax &&
                     currentReleaseSample < totalFrames &&
                     candidateReleaseSample < totalFrames &&
                     dualReleaseSample < totalFrames &&
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
                     *std::max_element(beforeImpulse.begin(), beforeImpulse.end()) <
                         0.01F &&
                     *std::max_element(beforeBurst.begin(), beforeBurst.end()) <
                         0.01F;
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
                    const std::vector<float>& signal, std::uint32_t sampleRate) {
  if (signal.size() >
      (std::numeric_limits<std::uint32_t>::max() - 48U) / sizeof(float)) {
    return false;
  }
  std::ofstream output(path, std::ios::binary);
  if (!output) {
    return false;
  }
  const auto dataSize = static_cast<std::uint32_t>(signal.size() * sizeof(float));
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
        const float body = 0.75F * static_cast<float>(
                                       std::exp(-25.0 * hitTime) *
                                       std::sin(2.0 * kPi * 68.0 * hitTime));
        const float click = phase < 24 ? 0.7F * (1.0F - phase / 24.0F) : 0.0F;
        signal[i] = std::clamp(body + click, -1.0F, 1.0F);
      }
    } else if (name == "bass") {
      const double movement = 0.8 + 0.2 * std::sin(2.0 * kPi * 0.5 * time);
      signal[i] = static_cast<float>(movement *
                                     (0.55 * std::sin(2.0 * kPi * 43.0 * time) +
                                      0.12 * std::sin(2.0 * kPi * 86.0 * time)));
    } else if (name == "dense") {
      noiseState = noiseState * 1664525U + 1013904223U;
      const float noise =
          static_cast<float>(noiseState >> 8U) / 8388607.5F - 1.0F;
      signal[i] = 0.24F * static_cast<float>(std::sin(2.0 * kPi * 110.0 * time)) +
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
        signal[i] += 0.18F * static_cast<float>(
                                 std::exp(-8.0 * phase / sampleRate) *
                                 std::sin(2.0 * kPi * 880.0 * phase / sampleRate));
      }
    }
  }
  return signal;
}

int detectorAuditions(const std::filesystem::path& directory) {
  std::error_code error;
  std::filesystem::create_directories(directory, error);
  if (error) {
    std::cerr << "cannot create output directory: " << directory << '\n';
    return 1;
  }

  bool valid = true;
  for (std::string_view name :
       std::array<std::string_view, 4>{"transient", "bass", "dense", "ambient"}) {
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
      current[i] = input[i] * std::exp(-0.11512925464970229F * currentReduction);
      candidate[i] = input[i] * std::exp(-0.11512925464970229F * candidateReduction);
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
        0.89125094F /
        std::max({signalPeak(current), signalPeak(candidate), signalPeak(dual),
                  signalPeak(programme), signalPeak(hybrid),
                  signalPeak(feedback)});
    for (std::size_t i = 0; i < input.size(); ++i) {
      current[i] *= commonGain;
      candidate[i] *= commonGain;
      dual[i] *= commonGain;
      programme[i] *= commonGain;
      hybrid[i] *= commonGain;
      feedback[i] *= commonGain;
    }

    const auto currentPath = directory / (std::string{name} + "-current.wav");
    const auto candidatePath = directory / (std::string{name} + "-rms-peak.wav");
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
    const bool candidateWritten = writeFloatWave(candidatePath, candidate, 48000U);
    const bool dualWritten = writeFloatWave(dualPath, dual, 48000U);
    const bool programmeWritten =
        writeFloatWave(programmePath, programme, 48000U);
    const bool hybridWritten = writeFloatWave(hybridPath, hybrid, 48000U);
    const bool feedbackWritten = writeFloatWave(feedbackPath, feedback, 48000U);
    valid = currentWritten && candidateWritten && dualWritten &&
            programmeWritten && hybridWritten && feedbackWritten && valid &&
            std::isfinite(candidateMatchErrorDb) &&
            std::abs(candidateMatchErrorDb) < 0.001 &&
            std::isfinite(dualMatchErrorDb) && std::abs(dualMatchErrorDb) < 0.001;
    valid = valid && std::isfinite(programmeMatchErrorDb) &&
            std::abs(programmeMatchErrorDb) < 0.001;
    valid = valid && std::isfinite(hybridMatchErrorDb) &&
            std::abs(hybridMatchErrorDb) < 0.001;
    valid = valid && std::isfinite(feedbackMatchErrorDb) &&
            std::abs(feedbackMatchErrorDb) < 0.001;
    std::cout << std::fixed << std::setprecision(6)
              << "{\"fixture\":\"" << name << "\",\"rms_dbfs\":"
              << 20.0 * std::log10(matchedCurrentRms)
              << ",\"rms_peak_match_error_db\":" << candidateMatchErrorDb
              << ",\"dual_time_match_error_db\":" << dualMatchErrorDb
              << ",\"programme_match_error_db\":" << programmeMatchErrorDb
              << ",\"hybrid_match_error_db\":" << hybridMatchErrorDb
              << ",\"feedback_match_error_db\":" << feedbackMatchErrorDb
              << ",\"current_peak_dbfs\":"
              << 20.0 * std::log10(signalPeak(current))
              << ",\"rms_peak_peak_dbfs\":"
              << 20.0 * std::log10(signalPeak(candidate))
              << ",\"dual_time_peak_dbfs\":"
              << 20.0 * std::log10(signalPeak(dual))
              << ",\"programme_peak_dbfs\":"
              << 20.0 * std::log10(signalPeak(programme))
              << ",\"hybrid_peak_dbfs\":"
              << 20.0 * std::log10(signalPeak(hybrid))
              << ",\"feedback_peak_dbfs\":"
              << 20.0 * std::log10(signalPeak(feedback))
              << ",\"current_match_db\":"
              << 20.0 * std::log10(currentMatch)
              << ",\"rms_peak_match_db\":"
              << 20.0 * std::log10(candidateMatch)
              << ",\"dual_time_match_db\":"
              << 20.0 * std::log10(dualMatch)
              << ",\"programme_match_db\":"
              << 20.0 * std::log10(programmeMatch)
              << ",\"hybrid_match_db\":"
              << 20.0 * std::log10(hybridMatch)
              << ",\"feedback_match_db\":"
              << 20.0 * std::log10(feedbackMatch) << "}\n";
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

  responses << "trial,fixture,preferred(A/B/no preference),confidence(0-3),notes\n";
  answerKey << "trial,fixture,A,B\n";
  std::uint32_t randomState = 0xD01B11DU;
  std::size_t trial = 0;
  bool valid = true;
  for (std::string_view fixture :
       std::array<std::string_view, 4>{"transient", "bass", "dense", "ambient"}) {
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
      std::filesystem::copy_file(candidateIsA ? candidateSource : currentSource,
                                 audioDirectory / (prefix + "-A.wav"),
                                 std::filesystem::copy_options::overwrite_existing,
                                 errorA);
      std::filesystem::copy_file(candidateIsA ? currentSource : candidateSource,
                                 audioDirectory / (prefix + "-B.wav"),
                                 std::filesystem::copy_options::overwrite_existing,
                                 errorB);
      valid = !errorA && !errorB && valid;
      responses << prefix.substr(6) << ',' << fixture << ",,,\n";
      answerKey << prefix.substr(6) << ',' << fixture << ','
                << (candidateIsA ? candidate : "current") << ','
                << (candidateIsA ? "current" : candidate) << '\n';
    }
  }
  valid = valid && static_cast<bool>(responses) && static_cast<bool>(answerKey) &&
          trial == 20U;
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
            << ",\"one_core_percent\":"
            << 100.0 * elapsed / renderedSeconds << ",\"checksum\":"
            << checksum << "}\n";
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
      const float amplitude = sample < totalFrames / 3 ? 0.08F
                              : sample < 2 * totalFrames / 3 ? 0.35F
                                                              : 0.9F;
      const float signal = amplitude * static_cast<float>(
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

  std::cout << std::fixed << std::setprecision(6)
            << "{\"frames\":" << rendered << ",\"sample_rate\":" << sampleRate
            << ",\"peak\":" << peak << ",\"rms\":"
            << std::sqrt(sumSquares / static_cast<double>(rendered))
            << ",\"max_gain_reduction_db\":" << maximumReduction
            << ",\"latency_samples\":" << processor.latencySamples() << "}\n";
}
