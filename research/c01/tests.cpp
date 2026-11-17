#include "core.hpp"
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <vector>
#include <atomic>
#include <new>
namespace {std::atomic<bool> auditing{};std::atomic<int> allocations{};}
void* operator new(std::size_t n) {if(auditing.load())++allocations;if(auto* p=std::malloc(n))return p;throw std::bad_alloc{};}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
#ifdef C01_MAC_AUDIT
extern "C" void asteRealtimeAuditReset();
extern "C" void asteRealtimeAuditSetActive(int);
extern "C" std::size_t asteRealtimeAuditLockCalls();
extern "C" std::size_t asteRealtimeAuditFileOpenCalls();
extern "C" std::size_t asteRealtimeAuditWriteCalls();
#endif
using namespace aste::contamination;
void require(bool b,const char* message) {if(!b){std::cerr<<message<<'\n';std::exit(1);}}
std::vector<float> render(double fs,int factor,Controls c,Persistence memory=Persistence::Hybrid,std::size_t block=127) {
  Engine engine; require(engine.prepare(fs,factor,Topology::Coupled,memory),"prepare");
  const auto frames=static_cast<std::size_t>(fs*2);
  std::vector<Event> events;
  for(int hit=0;hit<4;++hit) for(int note:{48,51,55,62}) {
    events.push_back({static_cast<std::size_t>(hit*fs*.4),Kind::On,note,.8});
    events.push_back({static_cast<std::size_t>((hit*.4+.09)*fs),Kind::Off,note,0});
  }
  std::stable_sort(events.begin(),events.end(),[](auto a,auto b){return a.offset<b.offset;});
  std::vector<float> l(frames),r(frames); std::size_t event=0;
  for(std::size_t n=0;n<frames;n+=block) {
    std::vector<Event> current;
    const auto count=std::min(block,frames-n);
    while(event<events.size() && events[event].offset<n+count) {auto e=events[event++];e.offset-=n;current.push_back(e);}
    engine.process(l.data()+n,r.data()+n,count,c,current);
  }
  for(float v:l) require(std::isfinite(v)&&std::abs(v)<=2.01,"bounded finite");
  return l;
}
int main() {
  require(Engine::recommendedFactor(22050)==8 && Engine::recommendedFactor(48000)==4 && Engine::recommendedFactor(96000)==2 && Engine::recommendedFactor(192000)==1,"sample-rate factor policy");
  Controls c;c.memory=0;c.instability=0;c.release=.02;
  auto a=render(48000,4,c), b=render(48000,4,c,Persistence::Hybrid,1);
  require(a==b,"block partition invariance");
  require(a==render(48000,4,c),"deterministic restart");
  double repeat=0;for(int i=200;i<6000;++i) repeat=std::max(repeat,std::abs(double(a[i]-a[i+19200])));
  require(repeat<1e-6,"reset repetition equality");
  c.memory=1;c.pressure=.8;c.contamination=.7;
  b=render(48000,4,c);double difference=0;for(std::size_t i=0;i<a.size();++i)difference+=std::abs(a[i]-b[i]);
  require(difference>1,"memory/control effect");
  for(double fs:{22050.,44100.,48000.,96000.,192000.}) for(int factor:{1,2,4,8}) {
    Controls extreme;extreme.pressure=extreme.contamination=extreme.instability=extreme.memory=extreme.motion=1;
    extreme.colour=8000;extreme.release=.005;
    render(fs,factor,extreme,Persistence::Hybrid,509);
  }
  Engine engine;require(!engine.prepare(0),"invalid rate");require(!engine.prepare(48000,3),"invalid factor");
  require(engine.prepare(48000),"reprepare");
  for(int n=0;n<40;++n)engine.event({0,Kind::On,30+n,1});
  require(engine.observation().stolen==28,"bounded stealing");
  engine.event({0,Kind::Panic,0,0});
  std::array<float,1000> l{},r{};c.pressure=std::numeric_limits<double>::quiet_NaN();
  engine.process(l.data(),r.data(),l.size(),c);
  require(std::all_of(l.begin(),l.end(),[](float x){return x==0;}),"panic silence and NaN controls");
  engine.event({0,Kind::On,-1,1});engine.event({0,Kind::On,60,std::numeric_limits<double>::infinity()});
  require(engine.observation().rejected==2,"invalid events");
  engine.event({0,Kind::On,60,1});engine.event({0,Kind::Pedal,0,1});engine.event({0,Kind::Off,60,0});
  engine.process(l.data(),r.data(),l.size(),{});require(engine.observation().energy>0,"pedal hold");
  engine.event({0,Kind::Clear,0,0});require(engine.observation().stolen==0,"clear retains counters");
  // Warm runtime libraries, then audit the core callback with twelve held voices.
  for(int note=48;note<60;++note)engine.event({0,Kind::On,note,.9});
  engine.process(l.data(),r.data(),l.size(),{});
#ifdef C01_MAC_AUDIT
  asteRealtimeAuditReset();asteRealtimeAuditSetActive(1);
#endif
  auditing.store(true);engine.process(l.data(),r.data(),l.size(),{});auditing.store(false);
#ifdef C01_MAC_AUDIT
  asteRealtimeAuditSetActive(0);
  require(asteRealtimeAuditLockCalls()==0 && asteRealtimeAuditFileOpenCalls()==0 && asteRealtimeAuditWriteCalls()==0,"callback lock/file/log audit");
#endif
  require(allocations.load()==0,"callback allocation audit");
  // Same future event, different past excitation: memory must be causal.
  const auto causal=[&](double amount,bool clear) {
    Engine history,empty;history.prepare(48000);empty.prepare(48000);Controls memory;
    memory.memory=amount;memory.instability=memory.contamination=0;memory.release=.005;
    history.event({0,Kind::On,48,1});history.process(l.data(),r.data(),l.size(),memory);history.event({0,Kind::Off,48,0});
    for(int block=0;block<24;++block){history.process(l.data(),r.data(),l.size(),memory);empty.process(l.data(),r.data(),l.size(),memory);}
    if(clear)history.event({0,Kind::Clear,0,0});
    history.event({0,Kind::On,60,1});empty.event({0,Kind::On,60,1});
    std::array<float,1000> otherL{},otherR{};history.process(l.data(),r.data(),l.size(),memory);empty.process(otherL.data(),otherR.data(),otherL.size(),memory);
    double difference=0;for(std::size_t i=0;i<l.size();++i)difference=std::max(difference,std::abs(double(l[i]-otherL[i])));
    return difference;
  };
  require(causal(1,false)>1e-3,"previous excitation affects future note with memory");
  require(causal(0,false)<1e-6,"disabled memory removes history effect");
  require(causal(1,true)<1e-6,"clear removes memory effect");
  std::cout<<"C01 contracts passed; engine storage "<<sizeof(Engine)<<" bytes\n";
}
