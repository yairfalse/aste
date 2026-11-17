#include <AudioUnit/AudioUnit.h>
#include <CoreFoundation/CoreFoundation.h>
#include "research/quality/audio.hpp"
#include <cstring>
#include <iostream>
std::uint32_t code(const char* s) {
  if (std::strlen(s) != 4)
    throw std::runtime_error("four-character code required");
  return std::uint32_t(s[0]) << 24 | std::uint32_t(s[1]) << 16 |
         std::uint32_t(s[2]) << 8 | std::uint32_t(s[3]);
}
void check(OSStatus s, const char* what) {
  if (s) throw std::runtime_error(std::string(what) + ": " + std::to_string(s));
}
struct Context {
  power::Audio* source{};
  std::size_t at{};
};
OSStatus input(void* ctx, AudioUnitRenderActionFlags*, const AudioTimeStamp*,
               UInt32, UInt32 frames, AudioBufferList* data) {
  auto& c = *static_cast<Context*>(ctx);
  for (unsigned ch = 0; ch < data->mNumberBuffers; ++ch) {
    auto* p = static_cast<float*>(data->mBuffers[ch].mData);
    for (unsigned i = 0; i < frames; ++i)
      p[i] = c.at + i < c.source->left.size()
                 ? (ch ? c.source->right[c.at + i] : c.source->left[c.at + i])
                 : 0;
  }
  return noErr;
}
power::Audio read(const std::filesystem::path& path) {
  std::ifstream f(path, std::ios::binary);
  std::vector<char> b((std::istreambuf_iterator<char>(f)), {});
  std::size_t p = 12, start = 0, n = 0;
  while (p + 8 <= b.size()) {
    std::uint32_t k;
    std::memcpy(&k, b.data() + p + 4, 4);
    if (p + 8 + k > b.size()) throw std::runtime_error("extent");
    if (!std::memcmp(b.data() + p, "data", 4)) {
      start = p + 8;
      n = k;
    }
    p += 8 + k + (k % 2);
  }
  if (!start || n % 8) throw std::runtime_error("stereo float WAV required");
  power::Audio a(n / 8);
  for (std::size_t i = 0; i < n / 8; ++i) {
    std::memcpy(&a.left[i], b.data() + start + i * 8, 4);
    std::memcpy(&a.right[i], b.data() + start + i * 8 + 4, 4);
  }
  return a;
}
int main(int argc, char** argv) {
  AudioUnit unit = nullptr;
  try {
    if (argc != 4 && argc != 8)
      throw std::runtime_error(
          "usage: au_reference type subtype manufacturer [source-dir "
          "fresh-output mix-id mix-value]");
    AudioComponentDescription d{};
    d.componentType = code(argv[1]);
    d.componentSubType = code(argv[2]);
    d.componentManufacturer = code(argv[3]);
    auto component = AudioComponentFindNext(nullptr, &d);
    if (!component) throw std::runtime_error("component unavailable");
    check(AudioComponentInstanceNew(component, &unit), "instantiate");
    UInt32 version = 0;
    check(AudioComponentGetVersion(component, &version), "version");
    std::cout << "component_version " << version << "\n";
    UInt32 size = 0;
    check(AudioUnitGetPropertyInfo(unit, kAudioUnitProperty_ParameterList,
                                   kAudioUnitScope_Global, 0, &size, nullptr),
          "parameter list");
    std::vector<AudioUnitParameterID> ids(size / sizeof(AudioUnitParameterID));
    check(AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterList,
                               kAudioUnitScope_Global, 0, ids.data(), &size),
          "parameters");
    for (auto id : ids) {
      AudioUnitParameterInfo info{};
      size = sizeof(info);
      if (AudioUnitGetProperty(unit, kAudioUnitProperty_ParameterInfo,
                               kAudioUnitScope_Global, id, &info, &size))
        continue;
      char name[512]{};
      if (info.flags & kAudioUnitParameterFlag_HasCFNameString)
        CFStringGetCString(info.cfNameString, name, sizeof(name),
                           kCFStringEncodingUTF8);
      else
        std::strncpy(name, info.name, sizeof(name) - 1);
      float value = 0;
      AudioUnitGetParameter(unit, id, kAudioUnitScope_Global, 0, &value);
      std::cout << id << '\t' << name << '\t' << info.minValue << '\t'
                << info.maxValue << '\t' << value << '\n';
      if (info.flags & kAudioUnitParameterFlag_CFNameRelease)
        CFRelease(info.cfNameString);
    }
    if (argc == 8) {
      std::filesystem::path dest = argv[5];
      if (std::filesystem::exists(dest))
        throw std::runtime_error("fresh output required");
      std::filesystem::create_directories(dest);
      AudioStreamBasicDescription format{};
      format.mSampleRate = 48000;
      format.mFormatID = kAudioFormatLinearPCM;
      format.mFormatFlags = kAudioFormatFlagIsFloat | kAudioFormatFlagIsPacked |
                            kAudioFormatFlagIsNonInterleaved;
      format.mBytesPerPacket = format.mBytesPerFrame = 4;
      format.mFramesPerPacket = 1;
      format.mChannelsPerFrame = 2;
      format.mBitsPerChannel = 32;
      check(AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat,
                                 kAudioUnitScope_Input, 0, &format,
                                 sizeof(format)),
            "input format");
      check(AudioUnitSetProperty(unit, kAudioUnitProperty_StreamFormat,
                                 kAudioUnitScope_Output, 0, &format,
                                 sizeof(format)),
            "output format");
      UInt32 frames = 512;
      check(AudioUnitSetProperty(unit, kAudioUnitProperty_MaximumFramesPerSlice,
                                 kAudioUnitScope_Global, 0, &frames,
                                 sizeof(frames)),
            "frames");
      Context context;
      AURenderCallbackStruct cb{input, &context};
      check(AudioUnitSetProperty(unit, kAudioUnitProperty_SetRenderCallback,
                                 kAudioUnitScope_Input, 0, &cb, sizeof(cb)),
            "callback");
      check(AudioUnitSetParameter(unit, std::stoul(argv[6]),
                                  kAudioUnitScope_Global, 0, std::stof(argv[7]),
                                  0),
            "wet mix");
      check(AudioUnitInitialize(unit), "initialize");
      auto metrics = power::output(dest / "raw-levels.csv");
      metrics << "source,peak,rms,lufs\n";
      for (const char* name :
           {"impulse", "percussion", "chords", "tone", "speech"}) {
        auto src = read(std::filesystem::path(argv[4]) /
                        (std::string(name) + "-dry.wav"));
        context.source = &src;
        check(AudioUnitReset(unit, kAudioUnitScope_Global, 0), "reset");
        power::Audio out(src.left.size());
        struct StereoList {
          UInt32 count;
          AudioBuffer buffers[2];
        } buffer{2, {{1, 0, nullptr}, {1, 0, nullptr}}};
        power::Audio silence(24576);
        context.source = &silence;
        std::array<float, 512> warmL{}, warmR{};
        for (std::size_t at = 0; at < silence.left.size(); at += 512) {
          context.at = at;
          buffer.buffers[0].mData = warmL.data();
          buffer.buffers[1].mData = warmR.data();
          for (auto& b : buffer.buffers) b.mDataByteSize = 2048;
          AudioTimeStamp stamp{};
          stamp.mSampleTime = double(at);
          stamp.mFlags = kAudioTimeStampSampleTimeValid;
          AudioUnitRenderActionFlags flags = 0;
          check(AudioUnitRender(unit, &flags, &stamp, 0, 512,
                                reinterpret_cast<AudioBufferList*>(&buffer)),
                "warmup");
        }
        context.source = &src;
        for (std::size_t at = 0; at < src.left.size(); at += frames) {
          auto n = UInt32(std::min(std::size_t(frames), src.left.size() - at));
          context.at = at;
          buffer.buffers[0].mData = out.left.data() + at;
          buffer.buffers[1].mData = out.right.data() + at;
          for (auto& b : buffer.buffers) b.mDataByteSize = n * 4;
          AudioTimeStamp stamp{};
          stamp.mSampleTime = double(at + 24576);
          stamp.mFlags = kAudioTimeStampSampleTimeValid;
          AudioUnitRenderActionFlags flags = 0;
          check(AudioUnitRender(unit, &flags, &stamp, 0, n,
                                reinterpret_cast<AudioBufferList*>(&buffer)),
                "render");
        }
        auto m = power::measure(out);
        metrics << name << ',' << m.peak << ',' << m.rms << ','
                << m.lufs.value_or(-999) << '\n';
        power::wav(dest / (std::string(name) + "-reference-raw.wav"), out);
      }
      AudioUnitUninitialize(unit);
    }
    AudioComponentInstanceDispose(unit);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    if (unit) AudioComponentInstanceDispose(unit);
    return 1;
  }
}
