#pragma once
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <span>
#include <vector>

namespace aste::click {
constexpr int objects=4, steps=32, voices=48, sampleBanks=3;
constexpr std::size_t sampleCapacity=1U<<20;
struct Step {
  bool on{},lock{};
  float velocity{.8F},pitch{},start{},cutoff{6000},morph{},decay{120},timing{},
      probability{1},span{1},curve{},endVelocity{.8F},endPitch{},mutation{},irregular{};
  int sound{},condition{},count{1};
};
struct Sound {
  int source{},interpolation{1};
  float velocityResponse{1};
  float pitch{60},tune{},excite{.5F},body{.5F},decay{160},resonance{.25F},noise{.2F},
      character{.2F},cutoff{9000},morph{},filterEnv{},level{.7F},pan{},
      start{},end{1},loopStart{},loopEnd{1},loopMod{},bits{16},colour{};
  bool reverse{},loop{};
};
struct Object : Sound {
  int length{16},division{2},midi{36},roll{1},repeatSteps{4};
  bool mute{},solo{},phaseReset{},repeat{};
  std::array<Step,steps> step{};
};
struct Controls {
  std::array<Object,objects> object{};
  int mode{1},masterLength{16},regionStart{},regionEnd{16},shiftSteps{1};
  float innerSpan{.25F},rollSpan{1};
  bool repeat{},nested{},reverse{},hold{},continueTime{true},voided{},crush{},shift{},fill{};
  float pressure{.3F},recovery{180},dirt{.2F},parallel{.7F};
  std::uint32_t seed{1701};
};
struct Transport {bool valid{},playing{};double ppq{},bpm{120};};
struct Midi {std::size_t offset{};int note{};float velocity{};bool on{true};int cc{-1};};
struct Meter {
  std::array<float,objects> position{},energy{};
  float masterPosition{},peak{},reduction{},capturedSeconds{};
  std::uint64_t hits{},dropped{},stolen{};std::uint32_t gestures{};
  bool repeating{},nested{},holding{},empty{};
};
class Engine {
 public:
  Engine();
  ~Engine();
  Engine(const Engine&)=delete;
  Engine& operator=(const Engine&)=delete;
  bool prepare(double rate);
  void reset() noexcept;
  // Loader owns a reserved bank until publish/cancel; callback never reads a writing bank.
  int reserveSample(int object) noexcept;
  float* sampleData(int object,int bank,int channel) noexcept;
  void publishSample(int object,int bank,std::size_t frames,double rate) noexcept;
  void cancelSample(int object,int bank) noexcept;
  void unloadSample(int object) noexcept;
  void audition(int object,float velocity=1) noexcept;
  void midi(const Midi&) noexcept;
  void process(float*,float*,std::size_t,const Controls&,Transport={}) noexcept;
  int polyphony() const noexcept {return voiceLimit_;}
  Meter meter() const noexcept {return meter_;}
  std::size_t heapBytes() const noexcept;
  static constexpr int latencySamples=8;
 private:
  struct Sample {
    std::array<std::vector<float>,2> data;
    std::atomic<int> users{};std::size_t frames{};double rate{48000};
  };
  struct Voice {
    bool active{},sampleDone{};int object{},bank{-1},midi{-1};
    Sound sound{};double a{},b{},cursor{},increment{},env{},noise{},hp{},last{};
    std::array<double,2> ic1{},ic2{},ic3{},ic4{};
    double decayCoeff{},edgeCoeff{},edgeEnv{1},baseFrequency{},g{},g2{},k{},a1{},a2{};
    unsigned filterTick{};
    std::uint64_t random{1},age{};float velocity{},fade{},tail{};
  };
  struct Hit {
    bool active{},musical{};double when{};int object{},midi{-1};
    float velocity{};Sound sound{};
  };
  struct Fragment {std::array<std::vector<float>,2> data;std::size_t length{},cursor{},start{},written{},available{};bool active{};};
  void release(Voice&) noexcept;
  void trigger(int,const Sound&,float,int) noexcept;
  void roll(int,const Sound&,const Step&,double,bool,int,double) noexcept;
  void schedule(const Controls&,double,double) noexcept;
  std::array<float,2> voice(Voice&,const Controls&) noexcept;
  void capture(Fragment&,std::size_t,std::size_t) noexcept;
  std::array<float,2> read(Fragment&,bool) noexcept;
  double bpm_{120};
  double rate_{48000},internal_{96000},quarter_{},expectedPpq_{},audioClock_{};
  int voiceLimit_{48};
  bool ready_{},known_{},playing_{},sequence_{},wasRepeat_{},wasNested_{},previousShift_{};
  std::array<std::array<Sample,sampleBanks>,objects> samples_;
  std::array<std::atomic<int>,objects> pending_{};
  std::array<int,objects> activeSample_{{-1,-1,-1,-1}};
  std::array<Voice,voices> voice_{};
  std::array<Hit,512> hit_{};
  Fragment outer_,inner_;
  std::size_t written_{},available_{},capacity_{};
  std::array<double,objects> previous_{{-1e-9,-1e-9,-1e-9,-1e-9}},repeatAnchor_{};
  std::array<bool,objects> localRepeating_{};
  std::array<float,7> midiPerformance_{};
  std::array<double,objects> levels_{};bool levelsReady_{};
  std::array<std::array<double,33>,2> decimator_{};
  std::array<double,33> fir_{};std::size_t firWrite_{};
  std::array<double,2> detectorLow_{},dc_{};
  std::array<double,2> outerLast_{},innerLast_{};
  double outerMix_{},innerMix_{};
  double envelope_{},gain_{1},voidGain_{1},shiftRatio_{1},dirt_{},pressure_{},recovery_{180};
  Controls controls_{};std::uint64_t age_{};Meter meter_{};
};
Controls factory(int index) noexcept;
} // namespace aste::click
