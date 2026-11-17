#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace aste::contamination {
namespace {
constexpr double tau = 2 * std::numbers::pi;
double bound(double x, double a, double b, double fallback = 0) noexcept {
  return std::isfinite(x) ? std::clamp(x, a, b) : fallback;
}
double random(std::uint64_t& r) noexcept {
  r ^= r << 13; r ^= r >> 7; r ^= r << 17;
  return static_cast<double>(r >> 11) * 0x1.0p-52 - 1;
}
double relax(double hz, double fs) noexcept { return -std::expm1(-tau * hz / fs); }
Controls clean(Controls c) noexcept {
  c.pressure = bound(c.pressure,0,1); c.contamination = bound(c.contamination,0,1);
  c.instability = bound(c.instability,0,1); c.memory = bound(c.memory,0,1);
  c.motion = bound(c.motion,0,1); c.colour = bound(c.colour,80,8000,1100);
  c.attack = bound(c.attack,.0005,4,.003); c.decay = bound(c.decay,.01,8,.25);
  c.sustain = bound(c.sustain,0,1); c.release = bound(c.release,.005,12,.2);
  c.detune = bound(c.detune,0,.01,.003);
  c.pairDepth = bound(c.pairDepth,0,1,1);
  return c;
}
}
bool Engine::prepare(double fs, int factor, Topology topology, Persistence memory,
                     Injection injection, Noise noise) noexcept {
  ready_ = false;
  if (!std::isfinite(fs) || fs < 22050 || fs > 192000 ||
      (factor != 1 && factor != 2 && factor != 4 && factor != 8)) return false;
  rate_ = fs; factor_ = factor; internal_ = fs * factor;
  topology_ = topology; persistence_ = memory; injection_ = injection; noise_ = noise;
  taps_ = static_cast<std::size_t>(32 * factor + 1);
  const double cutoff = .44 / factor;
  double sum = 0;
  for (std::size_t i=0; i<taps_; ++i) {
    const auto x = static_cast<double>(i) - (taps_-1)*.5;
    const double sinc = x == 0 ? 2*cutoff : std::sin(tau*cutoff*x)/(std::numbers::pi*x);
    const double window = .42-.5*std::cos(tau*i/(taps_-1))+.08*std::cos(2*tau*i/(taps_-1));
    sum += (fir_[i] = sinc*window);
  }
  for (std::size_t i=0;i<taps_;++i) fir_[i] /= sum;
  smoothing_ = -std::expm1(-1/(.015*rate_));
  reset(); ready_ = true; return true;
}
void Engine::reset() noexcept {
  voice_ = {}; historyL_ = {}; historyR_ = {}; cursor_=clock_=0;
  shared_=bias_=busLow_=busFast_=dcL_=dcR_=wheel_=touch_=bend_=0;
  pedal_=initialized_=false; observation_={};
}
void Engine::start(int note, double velocity) noexcept {
  auto* selected = &voice_[0];
  for (auto& v:voice_) if (v.stage==0) { selected=&v; break; }
  if (selected->stage != 0) {
    // Quietest release first, then oldest held voice. Fixed storage, bounded fade.
    for (auto& v:voice_) {
      const bool vr = v.stage==4, sr=selected->stage==4;
      if ((vr && !sr) || (vr==sr && (vr ? v.env<selected->env : v.age<selected->age))) selected=&v;
    }
    ++observation_.stolen;
  }
  auto& v=*selected;
  const double retained = v.memory, retainedBias=v.bias, last=v.last, drift=v.drift;
  const auto rng=v.rng;
  v={}; v.note=note; v.velocity=velocity; v.stage=1; v.down=true; v.age=++clock_;
  v.rng=static_cast<std::uint64_t>(note+1)*0x9e3779b97f4a7c15ULL;
  if(smooth_.instability>0 && rng!=1) {v.rng=rng;v.drift=drift;}
  if (persistence_==Persistence::Local || persistence_==Persistence::Hybrid) {
    v.memory=retained*smooth_.memory; v.bias=retainedBias*smooth_.memory;
  }
  v.tail=last; v.fade=static_cast<int>(.005*internal_);
}
void Engine::event(const Event& e) noexcept {
  if (!std::isfinite(e.value)) { ++observation_.rejected; return; }
  switch(e.kind) {
    case Kind::On:
      if(e.note<0 || e.note>127) { ++observation_.rejected; break; }
      if(e.value<=0) { event({0,Kind::Off,e.note,0}); break; }
      start(e.note,bound(e.value,0,1)); break;
    case Kind::Off:
      for(auto& v:voice_) if(v.note==e.note && v.down) {
        v.down=false; v.pedal=pedal_; if(!pedal_) v.stage=4;
      } break;
    case Kind::Pedal:
      pedal_=e.value>=.5;
      if(!pedal_) for(auto& v:voice_) if(v.pedal) {v.pedal=false; v.stage=4;}
      break;
    case Kind::Wheel: wheel_=bound(e.value,0,1); break;
    case Kind::Touch: touch_=bound(e.value,0,1); break;
    case Kind::Bend: bend_=bound(e.value,-1,1)*2; break;
    case Kind::Clear:
      shared_=bias_=busLow_=busFast_=0;
      for(auto& v:voice_) { v.memory=v.bias=v.drift=0; }
      break;
    case Kind::Panic: reset(); break;
  }
}
double Engine::tick() noexcept {
  const auto& c=smooth_;
  const double p=std::clamp(c.pressure+.2*touch_,0.,1.);
  const double contam=c.contamination*c.contamination;
  const bool shared=persistence_==Persistence::Shared || persistence_==Persistence::Hybrid;
  const double state=shared ? shared_*c.memory : 0, bias=shared ? bias_*c.memory : 0;
  double left=0,right=0, excitation=0, signedBus=0, interaction=0;
  const double release=std::exp(-6.907755/(c.release*internal_));
  for(auto& v:voice_) {
    const double retention=std::exp(-1/((.04+18*c.memory*c.memory)*internal_));
    v.memory*=retention; v.bias*=retention;
    if(v.stage==0) { v.last=0; continue; }
    if(v.stage==1) { v.env+=1/(c.attack*internal_); if(v.env>=1) {v.env=1;v.stage=2;} }
    else if(v.stage==2) { v.env=c.sustain+(v.env-c.sustain)*std::exp(-6.907755/(c.decay*internal_)); if(std::abs(v.env-c.sustain)<1e-5) v.stage=3; }
    else if(v.stage==3) v.env=c.sustain;
    else {v.env*=release; if(v.env<1e-6) {v.env=0;v.stage=0;} }
    const double white=random(v.rng);
    const double noiseRate=relax(180,internal_);
    v.noise+=noiseRate*(white-v.noise);
    const double n=noise_==Noise::White ? white : noise_==Noise::Highpass ? white-v.noise : v.noise*std::sqrt((2-noiseRate)/noiseRate);
    v.drift+=relax(.7,internal_)*(white*80-v.drift);
    const double local=(persistence_==Persistence::Reset || persistence_==Persistence::Shared) ? 0 : v.memory*c.memory;
    const double operating=std::clamp(state+local,0.,2.);
    const double frequency=std::min(.12*internal_,440*std::exp2((v.note-69+bend_+.055*c.instability*v.drift)/12));
    const double fm=injection_==Injection::Frequency ? contam*.018*n : 0;
    v.a=std::fmod(v.a+tau*frequency/internal_*(1+fm),tau);
    v.b=std::fmod(v.b+tau*frequency/internal_*(1+c.detune+.008*c.instability*v.drift),tau);
    const double noisePhase=injection_==Injection::Phase ? contam*.5*n : 0;
    const double biasNoise=injection_==Injection::Bias ? contam*.4*n : 0;
    const double feedbackNoise=injection_==Injection::Feedback ? contam*.8*n : 0;
    const double drive=1+5*p;
    const double index=.35+1.7*p+1.1*operating+c.motion*v.env+wheel_;
    const double returnSignal=topology_==Topology::FilterReturn ? v.bp : v.previous;
    const double coupled=topology_==Topology::External ? 0 : .55*std::tanh(drive*(returnSignal+feedbackNoise)+bias+biasNoise+.3*v.bias*c.memory);
    const double mod=std::sin(v.b+coupled);
    const double oscillator=.78*std::sin(v.a+index*mod*c.pairDepth+noisePhase)+.22*std::sin(v.a);
    v.previous+=relax(5000,internal_)*(oscillator-v.previous);
    // Bounded two-integrator nonlinear return, not an analogue SVF emulation.
    const double cutoff=std::min(.08*internal_,c.colour*(.55+1.4*v.env)*(1+.3*c.motion*operating));
    const double k=relax(cutoff,internal_);
    const double feed=topology_==Topology::External ? oscillator : std::tanh(drive*oscillator+.3*p*v.bp+biasNoise+.2*bias)/std::sqrt(drive);
    v.bp+=k*(feed-v.bp); v.lp+=k*(v.bp-v.lp);
    double sample=v.lp*v.env*v.velocity;
    if(injection_==Injection::Output) sample+=contam*.06*n*v.env;
    if(topology_==Topology::External) sample=std::tanh(drive*sample)/std::sqrt(drive);
    v.memory+=relax(3,internal_)*std::abs(sample);
    v.bias+=relax(1,internal_)*std::tanh(2*sample);
    if(v.fade>0) { sample+=v.tail*static_cast<double>(v.fade)/(.005*internal_); --v.fade; }
    v.last=sample;
    const double pan=static_cast<double>(v.note%7-3)*.045;
    left+=sample*(.707-pan); right+=sample*(.707+pan);
    excitation+=std::abs(sample); signedBus+=sample;
    interaction+=std::abs(coupled)*v.env;
  }
  busLow_+=relax(120,internal_)*(signedBus/voices-busLow_);
  busFast_+=relax(4000,internal_)*(signedBus/voices-busFast_);
  const double retain=std::exp(-1/((.04+18*c.memory*c.memory)*internal_));
  // Excitation-dependent reservoir. No free-running step sequence or random memory.
  shared_=std::min(2.,shared_*retain+relax(2,internal_)*(excitation/voices+2*std::abs(busFast_-busLow_)));
  bias_=std::clamp(bias_*retain+relax(.7,internal_)*busLow_,-1.,1.);
  left=std::tanh(left*.22); right=std::tanh(right*.22);
  dcL_+=relax(15,internal_)*(left-dcL_); dcR_+=relax(15,internal_)*(right-dcR_);
  observation_.energy=excitation/voices; observation_.memory=shared_; observation_.bias=bias_;
  observation_.interaction=interaction/voices;
  outR_=right-dcR_; return left-dcL_;
}
void Engine::process(float* l,float* r,std::size_t frames,const Controls& input,std::span<const Event> events) noexcept {
  const auto target=clean(input);
  if(!initialized_) {smooth_=target;initialized_=true;}
  std::size_t e=0; observation_.peak=0;
  for(std::size_t i=0;i<frames;++i) {
    while(e<events.size() && events[e].offset<=i) event(events[e++]);
    if(!ready_) {l[i]=r[i]=0;continue;}
    const auto approach=[&](double& x,double y){x+=smoothing_*(y-x);if(std::abs(x-y)<1e-9)x=y;};
    approach(smooth_.pressure,target.pressure); approach(smooth_.contamination,target.contamination);
    approach(smooth_.instability,target.instability); approach(smooth_.memory,target.memory);
    approach(smooth_.motion,target.motion); approach(smooth_.colour,target.colour);
    approach(smooth_.attack,target.attack); approach(smooth_.decay,target.decay);
    approach(smooth_.sustain,target.sustain); approach(smooth_.release,target.release);
    approach(smooth_.detune,target.detune);
    approach(smooth_.pairDepth,target.pairDepth);
    for(int k=0;k<factor_;++k) {historyL_[cursor_]=tick();historyR_[cursor_]=outR_;cursor_=(cursor_+1)%taps_;}
    double a=0,b=0;
    for(std::size_t j=0;j<taps_;++j) {auto index=(cursor_+taps_-1-j)%taps_;a+=fir_[j]*historyL_[index];b+=fir_[j]*historyR_[index];}
    l[i]=static_cast<float>(a);r[i]=static_cast<float>(b);
    observation_.peak=std::max({observation_.peak,std::abs(a),std::abs(b)});
  }
}
} // namespace aste::contamination
