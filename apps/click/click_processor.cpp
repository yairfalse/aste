#include "click_processor.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>

namespace aste::click {
namespace {
constexpr double pi=std::numbers::pi,tau=2*pi;
constexpr std::array<double,6> divisions{.125,1./6,.25,1./3,.5,1};
double clamp(double x,double lo,double hi,double fallback=0) noexcept {return std::isfinite(x)?std::clamp(x,lo,hi):fallback;}
std::uint64_t hash(std::uint64_t n) noexcept {n^=n>>30;n*=0xbf58476d1ce4e5b9ULL;n^=n>>27;n*=0x94d049bb133111ebULL;return n^(n>>31);}
double unit(std::uint64_t n) noexcept {return (hash(n)>>11)*0x1.0p-53;}
double noise(std::uint64_t& n) noexcept {n^=n<<13;n^=n>>7;n^=n<<17;return (n>>11)*0x1.0p-52-1;}
int mod(long long x,int n) noexcept {return static_cast<int>((x%n+n)%n);}
Sound safe(Sound s) noexcept {
  s.velocityResponse=static_cast<float>(clamp(s.velocityResponse,0,1,1));s.source=std::clamp(s.source,0,2);s.interpolation=std::clamp(s.interpolation,0,2);
  s.pitch=static_cast<float>(clamp(s.pitch,18,16000,60));s.tune=static_cast<float>(clamp(s.tune,-48,48));
  s.decay=static_cast<float>(clamp(s.decay,.2,8000,120));s.cutoff=static_cast<float>(clamp(s.cutoff,30,20000,9000));
  s.excite=static_cast<float>(clamp(s.excite,0,1));s.body=static_cast<float>(clamp(s.body,0,1));s.resonance=static_cast<float>(clamp(s.resonance,0,.95));
  s.noise=static_cast<float>(clamp(s.noise,0,1));s.character=static_cast<float>(clamp(s.character,0,1));s.morph=static_cast<float>(clamp(s.morph,0,1));
  s.filterEnv=static_cast<float>(clamp(s.filterEnv,-1,1));s.level=static_cast<float>(clamp(s.level,0,1));s.pan=static_cast<float>(clamp(s.pan,-1,1));
  s.start=static_cast<float>(clamp(s.start,0,.999999));s.end=static_cast<float>(clamp(s.end,s.start,1,1));
  s.loopStart=static_cast<float>(clamp(s.loopStart,s.start,s.end));s.loopEnd=static_cast<float>(clamp(s.loopEnd,s.loopStart, s.end,s.end));
  s.loopMod=static_cast<float>(clamp(s.loopMod,-.5,.5));s.bits=static_cast<float>(clamp(s.bits,3,24,16));s.colour=static_cast<float>(clamp(s.colour,0,1));
  return s;
}
}
Engine::Engine() {
  for(auto& p:pending_)p.store(-1);
  for(auto& o:samples_)for(auto& bank:o)for(auto& ch:bank.data)ch.resize(sampleCapacity);
}
Engine::~Engine() {reset();for(int n=0;n<objects;++n){auto pending=pending_[n].exchange(-1);if(pending>=0)samples_[n][pending].users.fetch_sub(1);if(activeSample_[n]>=0)samples_[n][activeSample_[n]].users.fetch_sub(1);}}
bool Engine::prepare(double rate) {
  ready_=false;if(!std::isfinite(rate)||rate<22050||rate>192000)return false;
  rate_=rate;internal_=2*rate;voiceLimit_=rate>96000?12:rate>48000?24:48;capacity_=static_cast<std::size_t>(std::ceil(8*rate));
  for(auto* group:{&outer_.data,&inner_.data})for(auto& ch:*group)ch.resize(capacity_);
  double sum=0;for(int i=0;i<33;++i){double x=i-16;double win=.42-.5*std::cos(tau*i/32)+.08*std::cos(2*tau*i/32);sum+=(fir_[i]=(x==0?.44:std::sin(.44*pi*x)/(pi*x))*win);}
  for(auto& coefficient:fir_)coefficient/=sum;
  reset();ready_=true;return true;
}
void Engine::release(Voice& v) noexcept {if(v.bank>=0)samples_[v.object][v.bank].users.fetch_sub(1,std::memory_order_release);v.active=false;v.bank=-1;}
void Engine::reset() noexcept {
  for(auto& v:voice_)release(v);for(auto& h:hit_)h.active=false;
  outer_.active=inner_.active=false;outer_.length=inner_.length=0;
  outer_.written=inner_.written=outer_.available=inner_.available=0;
  written_=available_=firWrite_=0;known_=playing_=sequence_=wasRepeat_=wasNested_=false;
  previous_.fill(-1e-9);localRepeating_.fill(false);midiPerformance_.fill(0);levelsReady_=false;
  decimator_={};detectorLow_={};dc_={};outerLast_={};innerLast_={};outerMix_=innerMix_=0;envelope_=0;gain_=voidGain_=1;quarter_=audioClock_=0;age_=0;pressure_=dirt_=0;recovery_=180;meter_={};
}
int Engine::reserveSample(int object) noexcept {
  if(object<0||object>=objects)return -1;
  for(int b=0;b<sampleBanks;++b){int expected=0;if(samples_[object][b].users.compare_exchange_strong(expected,-1,std::memory_order_acq_rel))return b;}
  return -1;
}
float* Engine::sampleData(int o,int b,int ch) noexcept {return samples_[o][b].data[ch].data();}
void Engine::publishSample(int o,int b,std::size_t frames,double rate) noexcept {
  auto& sample=samples_[o][b];sample.frames=std::min(frames,sampleCapacity);sample.rate=clamp(rate,8000,192000,48000);
  sample.users.store(1,std::memory_order_release);
  auto previous=pending_[o].exchange(b,std::memory_order_acq_rel);if(previous>=0)samples_[o][previous].users.fetch_sub(1,std::memory_order_release);
}
void Engine::cancelSample(int o,int b) noexcept {samples_[o][b].users.store(0,std::memory_order_release);}
void Engine::unloadSample(int o) noexcept {auto previous=pending_[o].exchange(-2,std::memory_order_acq_rel);if(previous>=0)samples_[o][previous].users.fetch_sub(1,std::memory_order_release);}
void Engine::trigger(int object,const Sound& input,float velocity,int midi) noexcept {
  const auto candidates=std::span<Voice>{voice_.data(),static_cast<std::size_t>(voiceLimit_)};
  Voice* chosen=&voice_[0];for(auto& v:candidates)if(!v.active){chosen=&v;break;}
  if(chosen->active){for(auto& v:candidates)if(v.env<chosen->env)chosen=&v;++meter_.stolen;}
  const auto tail=chosen->active?static_cast<float>(chosen->last):0.F;release(*chosen);*chosen={};auto& v=*chosen;
  v.active=true;v.object=object;v.sound=safe(input);v.velocity=static_cast<float>(clamp(1-v.sound.velocityResponse+velocity*v.sound.velocityResponse,0,1));v.env=1;v.age=++age_;v.midi=midi;
  v.random=hash(v.age*101+static_cast<unsigned>(object)+1)|1;v.tail=tail;v.fade=1;
  v.decayCoeff=std::exp(-6.907755/(v.sound.decay*.001*internal_));v.edgeCoeff=std::pow(v.decayCoeff,1+20*(1-v.sound.body));
  v.baseFrequency=v.sound.pitch*std::exp2(v.sound.tune/12.);
  if(v.sound.source!=0 && activeSample_[object]>=0 && samples_[object][activeSample_[object]].frames>=2) {
    v.bank=activeSample_[object];auto& bank=samples_[object][v.bank];bank.users.fetch_add(1,std::memory_order_relaxed);
    v.cursor=(v.sound.reverse?v.sound.end:v.sound.start)*static_cast<double>(bank.frames-1);
    v.increment=bank.rate/internal_*std::exp2(v.sound.tune/12.)*(v.sound.reverse?-1:1);
  }
  ++meter_.hits;
}
void Engine::roll(int object,const Sound& sound,const Step& step,double when,bool musical,int midi,double division) noexcept {
  const int count=std::clamp(step.count,1,16);
  for(int k=0;k<count;++k) {
    Hit* slot=nullptr;for(auto& h:hit_)if(!h.active){slot=&h;break;}
    if(!slot){++meter_.dropped;break;}
    const double t=count==1?0:static_cast<double>(k)/(count-1);
    double position=std::pow(t,std::exp2(-2*clamp(step.curve,-1,1)));
    if(k>0 && k<count-1)position=clamp(position+(unit(age_+k*97)-.5)*clamp(step.irregular,0,1)/count,0,1);
    slot->active=true;slot->musical=musical;slot->when=when+position*clamp(step.span,.03125,8,1)*division;
    slot->object=object;slot->midi=midi;slot->sound=sound;
    slot->velocity=static_cast<float>(clamp(step.velocity+(step.endVelocity-step.velocity)*t,0,1));
    if(step.lock){slot->sound.tune+=step.pitch;slot->sound.start=step.start;slot->sound.decay=step.decay;slot->sound.cutoff=step.cutoff;slot->sound.morph=step.morph;}
    slot->sound.tune+=static_cast<float>(step.endPitch*t);
    slot->sound.morph=static_cast<float>(clamp(slot->sound.morph+step.mutation*t,0,1));
    slot->sound.cutoff*=static_cast<float>(std::exp2(step.mutation*t*2));
  }
}
void Engine::audition(int object,float velocity) noexcept {
  if(object<0||object>=objects)return;
  Step step;step.velocity=step.endVelocity=velocity;step.count=controls_.object[object].roll;step.span=controls_.rollSpan;
  roll(object,controls_.object[object],step,audioClock_,false,-1,divisions[std::clamp(controls_.object[object].division,0,5)]*60/bpm_*rate_);
}
void Engine::midi(const Midi& event) noexcept {
  if(event.cc>=0) {
    if(event.cc==120||event.cc==123){reset();return;}
    constexpr std::array<int,7> cc{20,21,22,23,24,25,26};for(std::size_t i=0;i<cc.size();++i)if(event.cc==cc[i])midiPerformance_[i]=std::clamp(event.note/127.F,0.F,1.F);
    return;
  }
  if(controls_.mode!=0)return; // Explicit exclusive MIDI/internal modes prevent unintentional double triggers.
  if(event.on)for(int o=0;o<objects;++o)if(controls_.object[o].midi==event.note)audition(o,event.velocity);
}
void Engine::schedule(const Controls& c,double q,double increment) noexcept {
  bool anySolo=false;for(const auto& o:c.object)anySolo|=o.solo;
  const double phrase=std::clamp(c.masterLength,1,32)*.25;
  for(int n=0;n<objects;++n) {
    const auto& o=c.object[n];const int length=std::clamp(o.length,1,steps);
    const double division=divisions[std::clamp(o.division,0,5)];
    double x=(o.phaseReset?std::fmod(std::fmod(q,phrase)+phrase,phrase):q)/division;
    // Global phrase reset is applied by process through previous_ at a master wrap.
    if(o.repeat&&!localRepeating_[n])repeatAnchor_[n]=std::floor(x)-std::clamp(o.repeatSteps,1,8)+1;
    if(o.repeat)x=repeatAnchor_[n]+std::fmod(std::max(0.,x-repeatAnchor_[n]),std::clamp(o.repeatSteps,1,8));
    if(o.repeat!=localRepeating_[n])previous_[n]=x-increment/division-1e-10;
    localRepeating_[n]=o.repeat;
    if(x<previous_[n])previous_[n]=std::floor(x)-1e-9;
    meter_.position[n]=static_cast<float>(std::fmod(std::fmod(x,length)+length,length));
    const auto integer=static_cast<long long>(std::floor(x));
    for(long long candidate=integer;candidate<=integer+1;++candidate) {
      const int index=mod(candidate,length);const auto& s=o.step[index];
      const double target=static_cast<double>(candidate)+clamp(s.timing,-.49,.49);
      if(target<=previous_[n]+1e-11 || target>x+1e-11 || !s.on || o.mute || (anySolo&&!o.solo))continue;
      const auto cycle=static_cast<long long>(std::floor(static_cast<double>(candidate)/length));
      const int condition=std::clamp(s.condition,0,7);
      const bool accept=condition==0 || (condition==1&&mod(cycle,2)==0) || (condition==2&&mod(cycle,2)==1) ||
        (condition==3&&mod(cycle,3)==0) || (condition==4&&mod(cycle,4)==0) || (condition==5&&cycle==0) ||
        (condition==6&&cycle>0) || (condition==7&&(c.fill||midiPerformance_[6]>=.5F));
      if(!accept || unit(static_cast<std::uint64_t>(candidate)*313+static_cast<unsigned>(n)*1117+c.seed)>=clamp(s.probability,0,1,1))continue;
      const int source=std::clamp(s.sound,0,3);
      Step rolled=s;rolled.count=std::max(s.count,o.roll);
      roll(source,c.object[source],rolled,quarter_-increment,true,-1,division);
    }
    previous_[n]=x;
  }
}
std::array<float,2> Engine::voice(Voice& v,const Controls& c) noexcept {
  const auto& s=v.sound;
  v.env*=v.decayCoeff;v.edgeEnv*=v.edgeCoeff;
  if(v.env<1e-6){release(v);return {};}
  const double f=std::min(internal_*.42,v.baseFrequency*shiftRatio_*(1+7*s.excite*v.env*v.env));
  v.a+=tau*f/internal_;if(v.a>=tau)v.a-=tau;v.b+=tau*std::min(internal_*.42,f*(1.4+3.3*s.body))/internal_;if(v.b>=tau)v.b-=tau;
  const double white=noise(v.random);v.noise+=.12*(white-v.noise);
  const double edge=(white-v.noise)*v.edgeEnv;
  const double tone=std::sin(v.a+(s.body*.8+s.character)*std::min(1.,internal_*.1/f)*std::sin(v.b)*v.env);
  const double impact=(.65*std::sin(v.a)+.35*tone)*(1-s.noise)+s.noise*(s.body*v.noise*2+edge);
  double synth=impact*v.env;
  std::array<double,2> sample{};
  if(v.bank>=0&&!v.sampleDone) {
    const auto& bank=samples_[v.object][v.bank];const double last=static_cast<double>(bank.frames-1);
    const double start=std::min(s.start*last,last-1),end=std::max(start+1,s.end*last);
    double loopStart=clamp((s.loopStart+s.loopMod*(1-v.env))*last,start,end-1,start);
    const double loopEnd=clamp((s.loopEnd+s.loopMod*(1-v.env))*last,loopStart+1,end,end);
    const bool wrapping=s.loop&&loopEnd-loopStart>=2;
    if(wrapping && ((v.increment>0&&v.cursor>=loopEnd)||(v.increment<0&&v.cursor<loopStart))) {
      const double span=loopEnd-loopStart;v.cursor=loopStart+std::fmod(std::fmod(v.cursor-loopStart,span)+span,span);
    }
    if(v.cursor<start || v.cursor>end)v.sampleDone=true;
    else for(int ch=0;ch<2;++ch) {
      const auto& data=bank.data[ch];const auto index=static_cast<long long>(std::floor(v.cursor));const double fraction=v.cursor-index;
      const auto at=[&](long long i){
        if(wrapping && v.cursor>=loopStart && v.cursor<loopEnd && (i<static_cast<long long>(loopStart)||i>=static_cast<long long>(loopEnd))) {
          const auto span=std::max(2LL,static_cast<long long>(loopEnd-loopStart));i=static_cast<long long>(loopStart)+(i-static_cast<long long>(loopStart))%span;if(i<loopStart)i+=span;
        }
        return static_cast<double>(data[static_cast<std::size_t>(std::clamp(i,0LL,static_cast<long long>(bank.frames-1)))]);
      };
      if(s.interpolation==0)sample[ch]=at(index);
      else if(s.interpolation==1)sample[ch]=at(index)+(at(index+1)-at(index))*fraction;
      else {double a=at(index-1),b=at(index),d=at(index+1),e=at(index+2);sample[ch]=b+.5*fraction*(d-a+fraction*(2*a-5*b+4*d-e+fraction*(3*(b-d)+e-a)));}
      // Short edge fades remove accidental trim/loop discontinuities without hiding intentional nearest-neighbour artifacts.
      const double distance=std::min(v.cursor-start,end-v.cursor),fade=std::min(s.body*16.,(end-start)*.125);sample[ch]*=(fade>0?std::min(1.,std::max(0.,distance)/fade):1)*v.env;
    }
    v.cursor+=v.increment*shiftRatio_;
  }
  std::array<float,2> output{};
  if(v.filterTick==0 || (s.filterEnv!=0 && (v.filterTick&15U)==0)) {
    const double cutoff=clamp(s.cutoff*std::exp2(s.filterEnv*4*v.env),30,internal_*.4,6000);
    v.g=std::tan(pi*cutoff/internal_);v.k=2-1.9*s.resonance;v.a1=1/(1+v.g*(v.g+v.k));
    v.g2=std::tan(pi*std::min(internal_*.4,cutoff*(1.25+2.75*s.morph))/internal_);v.a2=1/(1+v.g2*(v.g2+v.k));
  }
  ++v.filterTick;
  for(int ch=0;ch<2;++ch) {
    double x=s.source==0?synth:s.source==1?sample[ch]:.65*synth+.65*sample[ch];
    const double g=v.g,k=v.k,a1=v.a1;
    const double v1=a1*(v.ic1[ch]+g*(x-v.ic2[ch]));const double v2=v.ic2[ch]+g*v1;
    v.ic1[ch]=2*v1-v.ic1[ch];v.ic2[ch]=2*v2-v.ic2[ch];
    const double high=x-k*v1-v2;
    // Continuous original LP -> resonant band -> notch -> HP morph, with envelope motion.
    const double m=s.morph*3;
    const double g2=v.g2,a2=v.a2;
    const double band2=a2*(v.ic3[ch]+g2*(x-v.ic4[ch])),low2=v.ic4[ch]+g2*band2;
    v.ic3[ch]=2*band2-v.ic3[ch];v.ic4[ch]=2*low2-v.ic4[ch];
    const std::array modes{v2,(v1+.65*band2)*(1+.5*s.resonance),high+v2-.5*band2,high};
    const auto region=std::min(2,static_cast<int>(m));x=modes[region]+(modes[region+1]-modes[region])*(m-region);
    const double drive=1+8*s.character;const double bias=.18*s.character*v.env;
    if(s.character>0)x=(std::tanh(drive*x+bias)-std::tanh(bias))/std::sqrt(drive);
    if(s.colour>0){const double levels=std::exp2(s.bits-1);x+=(std::round(x*levels)/levels-x)*s.colour;}
    x*=v.velocity;
    const double pan=std::sqrt(clamp(ch==0?(1-s.pan)*.5:(1+s.pan)*.5,0,1));
    x=x*pan+v.tail*v.fade*.5;
    output[ch]=static_cast<float>(clamp(x,-8,8));
  }
  v.fade=std::max(0.F,v.fade-static_cast<float>(1/(.002*internal_)));v.last=(output[0]+output[1])*.5;
  meter_.energy[v.object]=std::max(meter_.energy[v.object],static_cast<float>(v.env*v.velocity));
  (void)c;return output;
}
void Engine::capture(Fragment& f,std::size_t length,std::size_t ago) noexcept {
  f.length=std::min({length,f.available,capacity_});f.cursor=0;
  ago=std::min(ago,f.available-f.length);
  f.start=(f.written+capacity_-f.length-ago)%capacity_;
  f.active=f.length>0;meter_.empty=!f.active;meter_.capturedSeconds=static_cast<float>(f.length/rate_);
}
std::array<float,2> Engine::read(Fragment& f,bool reverse) noexcept {
  if(!f.active||f.length==0)return {};
  const auto position=reverse?f.length-1-f.cursor:f.cursor;
  std::array<float,2> result{f.data[0][(f.start+position)%capacity_],f.data[1][(f.start+position)%capacity_]};
  // Crossfade the last 1 ms into the first 1 ms at the playback seam.
  const auto fade=std::min<std::size_t>(static_cast<std::size_t>(rate_*.001),f.length/4);
  if(fade>0&&f.cursor>=f.length-fade) {
    const auto j=f.cursor-(f.length-fade),other=reverse?f.length-1-j:j;const float mix=static_cast<float>(j)/static_cast<float>(fade);
    for(int ch=0;ch<2;++ch)result[ch]+=mix*(f.data[ch][(f.start+other)%capacity_]-result[ch]);
  }
  f.cursor=(f.cursor+1)%f.length;return result;
}
void Engine::process(float* left,float* right,std::size_t frames,const Controls& input,Transport transport) noexcept {
  controls_=input;auto& c=controls_;
  c.pressure=static_cast<float>(clamp(c.pressure,0,1));c.dirt=static_cast<float>(clamp(c.dirt,0,1));c.parallel=static_cast<float>(clamp(c.parallel,0,1));
  c.recovery=static_cast<float>(clamp(c.recovery,5,2000,180));c.masterLength=std::clamp(c.masterLength,1,32);
  c.regionStart=std::clamp(c.regionStart,0,c.masterLength-1);c.regionEnd=std::clamp(c.regionEnd,c.regionStart+1,c.masterLength);
  for(int n=0;n<objects;++n) {
    const int published=pending_[n].exchange(-1,std::memory_order_acq_rel);
    if(published!=-1){if(published==-2)for(auto& v:voice_)if(v.active&&v.object==n&&v.bank>=0){v.sound.decay=std::min(v.sound.decay,5.F);v.decayCoeff=std::exp(-6.907755/(v.sound.decay*.001*internal_));v.edgeCoeff=std::pow(v.decayCoeff,1+20*(1-v.sound.body));}if(activeSample_[n]>=0)samples_[n][activeSample_[n]].users.fetch_sub(1,std::memory_order_release);activeSample_[n]=published>=0?published:-1;}
  }
  if(!ready_){std::fill_n(left,frames,0);std::fill_n(right,frames,0);return;}
  const double bpm=clamp(transport.bpm,20,400,120),dq=bpm/(60*rate_);bpm_=bpm;
  const bool valid=transport.valid&&std::isfinite(transport.ppq)&&std::abs(transport.ppq)<=1e7&&std::abs(transport.ppq)<=1e7,sequence=valid&&transport.playing&&c.mode==1;
  const bool seek=known_&&valid&&std::abs(transport.ppq-expectedPpq_)>dq*.6;
  if((playing_&&!transport.playing) || (sequence&&(!sequence_||seek))) {
    reset();quarter_=valid?transport.ppq:0;
    previous_.fill(-1e-9);
  }
  known_=valid;playing_=transport.playing;sequence_=sequence;
  if(valid)expectedPpq_=transport.ppq+(transport.playing?frames*dq:0);
  const bool repeat=c.repeat||midiPerformance_[0]>=.5F,nested=c.nested||midiPerformance_[1]>=.5F;
  if(repeat&&(!wasRepeat_||!outer_.active)) {
    const double phrase=c.masterLength*.25,span=(c.regionEnd-c.regionStart)*.25;
    double position=std::fmod(quarter_,phrase);if(position<0)position+=phrase;
    double ago=std::fmod(position-c.regionEnd*.25+phrase,phrase);
    if(ago<0)ago+=phrase;
    const auto length=std::min(capacity_,static_cast<std::size_t>(std::min(8.,span*60/bpm)*rate_));
    const auto since=std::min(capacity_-length,static_cast<std::size_t>(std::min(8.,ago*60/bpm)*rate_));
    if((!wasRepeat_&&outer_.available>0)||outer_.available>=length+since)capture(outer_,length,since);else meter_.empty=true;
  }
  if(nested&&(!wasNested_||!inner_.active)) {
    const auto length=std::min(capacity_,static_cast<std::size_t>(clamp(c.innerSpan,.03125,4,.25)*60/bpm*rate_));
    if((!wasNested_&&inner_.available>0)||inner_.available>=length)capture(inner_,length,0);else meter_.empty=true;
  }
  if(!repeat&&wasRepeat_){outer_.active=false;outer_.available=0;outer_.written=0;}
  if(!nested&&wasNested_){inner_.active=false;inner_.available=0;inner_.written=0;}
  wasRepeat_=repeat;wasNested_=nested;meter_.peak=0;meter_.energy.fill(0);
  const bool hold=c.hold||midiPerformance_[5]>=.5F||(!c.continueTime&&(outer_.active||inner_.active));
  const bool shift=c.shift||midiPerformance_[4]>=.5F;
  if(shift!=previousShift_){previous_.fill(-1e9);previousShift_=shift;}
  bool anySolo=false;for(const auto& o:c.object)anySolo=anySolo||o.solo;
  const double smoothing=1-std::exp(-1/(.015*rate_));
  if(!levelsReady_){for(int n=0;n<objects;++n)levels_[n]=clamp(c.object[n].level,0,1);levelsReady_=true;}
  for(std::size_t i=0;i<frames;++i) {
    for(int n=0;n<objects;++n)levels_[n]+=(clamp(c.object[n].level,0,1)-levels_[n])*smoothing;
    const double previousQuarter=quarter_;
    if(sequence) {
      if(!hold)quarter_+=dq;
      const double q=previousQuarter+(shift?c.shiftSteps*.25:0);
      schedule(c,q,quarter_-previousQuarter);
    }
    for(auto& h:hit_)if(h.active && h.when <= (h.musical?previousQuarter:audioClock_)+1e-9) {
      if(!c.object[h.object].mute&&(!anySolo||c.object[h.object].solo))trigger(h.object,h.sound,h.velocity,h.midi);
      h.active=false;
    }
    pressure_+=(c.pressure-pressure_)*smoothing;dirt_+=(clamp(c.dirt+(c.crush||midiPerformance_[3]>=.5F?.65:0),0,1)-dirt_)*smoothing;
    recovery_+=(c.recovery-recovery_)*smoothing;
    std::array<double,2> audio{};
    for(int sub=0;sub<2;++sub) {
      std::array<double,2> sum{};
      for(auto& v:voice_)if(v.active) {
        const auto signal=voice(v,c);
        if(!c.object[v.object].mute&&(!anySolo||c.object[v.object].solo))for(int ch=0;ch<2;++ch)sum[ch]+=signal[ch]*levels_[v.object];
      }
      double detected=0;
      for(int ch=0;ch<2;++ch){detectorLow_[ch]+=(1-std::exp(-tau*85/internal_))*(sum[ch]-detectorLow_[ch]);detected=std::max(detected,std::abs(sum[ch]-.65*detectorLow_[ch]));}
      const double coefficient=detected>envelope_?std::exp(-1/(.0003*internal_)):std::exp(-1/(recovery_*.001*internal_));
      envelope_=detected+coefficient*(envelope_-detected);
      const double threshold=std::exp2((-10-30*pressure_)/6.020599913),ratio=1+19*pressure_;
      const double target=envelope_>threshold?std::pow(envelope_/threshold,1/ratio-1):1;
      gain_+=(target-gain_)*.05;
      meter_.reduction=static_cast<float>(-20*std::log10(std::max(gain_,1e-9)));
      for(int ch=0;ch<2;++ch) {
        double compressed=sum[ch]*gain_*std::exp2(pressure_*1.5);
        double drive=1+12*dirt_,bias=.22*dirt_;
        const double shaped=(std::tanh(compressed*drive+bias)-std::tanh(bias))/std::sqrt(drive);
        compressed+=(shaped-compressed)*dirt_;
        double x=(sum[ch]*(1-c.parallel)+compressed*c.parallel)*.5;
        dc_[ch]+=(1-std::exp(-tau*12/internal_))*(x-dc_[ch]);x-=dc_[ch];
        x=std::tanh(x*1.3)/1.3;decimator_[ch][firWrite_]=x;
      }
      firWrite_=(firWrite_+1)%33;
    }
    for(int ch=0;ch<2;++ch)for(std::size_t tap=0;tap<33;++tap)audio[ch]+=fir_[tap]*decimator_[ch][(firWrite_+32-tap)%33];
    const double fade=1-std::exp(-1/(.001*rate_));
    outerMix_+=((outer_.active?1:0)-outerMix_)*fade;innerMix_+=((inner_.active?1:0)-innerMix_)*fade;
    if(outer_.active){auto repeated=read(outer_,c.reverse);for(int ch=0;ch<2;++ch)outerLast_[ch]=repeated[ch];}
    for(int ch=0;ch<2;++ch)audio[ch]+=(outerLast_[ch]-audio[ch])*outerMix_;
    if(inner_.active){auto repeated=read(inner_,c.reverse);for(int ch=0;ch<2;++ch)innerLast_[ch]=repeated[ch];}
    for(int ch=0;ch<2;++ch)audio[ch]+=(innerLast_[ch]-audio[ch])*innerMix_;
    if((outer_.active||inner_.active)&&(c.crush||midiPerformance_[3]>=.5F))for(auto& channel:audio)channel=std::tanh(channel*8)/3;
    const bool voided=c.voided||midiPerformance_[2]>=.5F;
    voidGain_+=((voided?0:1)-voidGain_)*(1-std::exp(-1/(.002*rate_)));
    for(int ch=0;ch<2;++ch)audio[ch]*=voidGain_;
    // Inactive fragment banks continuously record the performed stereo output.
    // Capture freezes metadata only: no callback-sized memcpy and no allocation.
    for(auto* fragment:{&outer_,&inner_})if(!fragment->active){for(int ch=0;ch<2;++ch)fragment->data[ch][fragment->written]=static_cast<float>(audio[ch]);fragment->written=(fragment->written+1)%capacity_;fragment->available=std::min(fragment->available+1,capacity_);}
    left[i]=static_cast<float>(clamp(audio[0],-.98,.98));right[i]=static_cast<float>(clamp(audio[1],-.98,.98));
    meter_.peak=std::max({meter_.peak,std::abs(left[i]),std::abs(right[i])});++audioClock_;
  }
  meter_.masterPosition=static_cast<float>(std::fmod(std::fmod(std::fmod(quarter_/.25,c.masterLength)+c.masterLength,c.masterLength)+c.masterLength,c.masterLength));
  meter_.empty=(repeat&&!outer_.active)||(nested&&!inner_.active);meter_.capturedSeconds=static_cast<float>((inner_.active?inner_.length:outer_.active?outer_.length:0)/rate_);
  meter_.gestures=(repeat?1U:0U)|(nested?2U:0U)|((c.voided||midiPerformance_[2]>=.5F)?4U:0U)|((c.crush||midiPerformance_[3]>=.5F)?8U:0U)|(shift?16U:0U)|(hold?32U:0U);
  meter_.repeating=outer_.active;meter_.nested=inner_.active;meter_.holding=hold;
}
std::size_t Engine::heapBytes() const noexcept {
  std::size_t bytes=0;for(const auto& o:samples_)for(const auto& b:o)for(const auto& ch:b.data)bytes+=ch.capacity()*sizeof(float);
  for(const auto* f:{&outer_,&inner_})for(const auto& ch:f->data)bytes+=ch.capacity()*sizeof(float);
  return bytes;
}
Controls factory(int index) noexcept {
  Controls c;
  constexpr std::array pitches{46.F,240.F,3100.F,9800.F};constexpr std::array decays{420.F,120.F,18.F,5.F};constexpr std::array notes{36,38,42,46};
  for(int n=0;n<objects;++n) {
    auto& o=c.object[n];o.pitch=pitches[n];o.decay=decays[n];o.midi=notes[n];o.body=n==0?.7F:.35F;o.noise=n==0?.02F:n==1?.55F:.35F;
    o.excite=n==0?.65F:.3F;o.level=n==0?.85F:.55F;o.cutoff=n==0?1700.F:14000.F;
    for(int s=0;s<steps;++s){auto& step=o.step[s];step.sound=n;step.decay=o.decay;step.cutoff=o.cutoff;step.velocity=step.endVelocity=s%4==0?.9F:.65F;}
  }
  c.object[0].step[0].on=true;c.object[0].step[10].on=true;c.object[1].step[6].on=true;
  c.object[2].step[3].on=true;c.object[2].step[14].on=true;c.object[3].step[11].on=true;
  if(index==1){c.pressure=.15F;c.dirt=.1F;for(auto& o:c.object){o.decay*=.35F;o.pitch*=2;}c.object[0].step[0].on=false;c.object[2].length=15;c.object[3].length=11;}
  if(index==2){c.pressure=.8F;c.dirt=.5F;c.recovery=360;c.object[0].decay=1100;c.object[0].pitch=28;c.object[0].body=.9F;for(int n=1;n<4;++n)c.object[n].level=.3F;}
  if(index==3){c.object[0].length=15;c.object[1].length=13;c.object[2].length=23;c.object[3].length=11;for(int n=0;n<4;++n){auto& step=c.object[n].step[3+n*2];step.on=true;step.count=5+n;step.curve=n%2==0?.7F:-.6F;step.endVelocity=.2F;step.endPitch=12;step.irregular=.25F;}}
  if(index==4){c.dirt=.55F;c.pressure=.6F;for(int n=0;n<4;++n){c.object[n].source=2;c.object[n].loop=true;c.object[n].loopEnd=.1F;c.object[n].loopMod=.05F;c.object[n].decay=90+n*80.F;}c.object[2].roll=4;}
  if(index==5){for(auto& o:c.object){o.source=1;o.cutoff=9000;o.morph=.3F;o.decay=500;}c.object[1].reverse=true;c.object[2].tune=12;c.object[3].tune=-12;}
  return c;
}
} // namespace aste::click
