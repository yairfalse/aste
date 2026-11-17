#include "core.hpp"
#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <complex>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <string>
#include <vector>
using namespace aste::contamination;
namespace {
constexpr double fs=48000;
struct Study {
  std::string name; Controls c{}; Topology topology{Topology::Coupled};
  Persistence persistence{Persistence::Hybrid}; Injection injection{Injection::Feedback};
  Noise noise{Noise::Correlated}; int score{},factor{4};
};
struct Audio {std::vector<float> l,r;std::vector<Observation> state;};
void u32(std::ofstream& f,std::uint32_t x) {for(int i=0;i<4;++i)f.put(static_cast<char>((x>>(8*i))&255));}
void u16(std::ofstream& f,int x) {f.put(static_cast<char>(x&255));f.put(static_cast<char>((x>>8)&255));}
void wav(const std::filesystem::path& path,const Audio& a,double gain) {
  std::ofstream f(path,std::ios::binary);if(!f)throw std::runtime_error("WAV open");
  const auto bytes=static_cast<std::uint32_t>(a.l.size()*8);
  f.write("RIFF",4);u32(f,36+bytes);f.write("WAVEfmt ",8);u32(f,16);u16(f,3);u16(f,2);
  u32(f,48000);u32(f,384000);u16(f,8);u16(f,32);f.write("data",4);u32(f,bytes);
  for(std::size_t i=0;i<a.l.size();++i)for(float x:{a.l[i],a.r[i]})u32(f,std::bit_cast<std::uint32_t>(static_cast<float>(x*gain)));
}
std::vector<Event> score(int type) {
  std::vector<Event> out;
  const std::array<std::array<int,4>,4> chords{{{48,51,55,58},{43,48,50,55},{46,49,53,60},{48,49,54,62}}};
  const int count=type==2 ? 4 : type==3 ? 1 : 16;
  for(int hit=0;hit<count;++hit) {
    const double time=type==2 ? hit*3. : hit*.75;
    const auto& chord=chords[type==1 ? (hit/4)%4 : 0];
    for(int note:chord) {
      const double velocity=type==4 ? .25+.75*(hit%4)/3. : .8;
      out.push_back({static_cast<std::size_t>(time*fs),Kind::On,note,velocity});
      out.push_back({static_cast<std::size_t>((time+(type==3 ? 10 : type==4 ? .65 : .14))*fs),Kind::Off,note,0});
    }
  }
  if(type==4) {
    out.push_back({4*48000,Kind::Wheel,0,.8});out.push_back({6*48000,Kind::Touch,0,.7});
    out.push_back({8*48000,Kind::Bend,0,.4});out.push_back({10*48000,Kind::Bend,0,0});
  }
  std::stable_sort(out.begin(),out.end(),[](auto a,auto b){return a.offset<b.offset;});return out;
}
Audio render(const Study& s,std::ofstream* trace=nullptr) {
  Audio a;a.l.resize(14*48000);a.r.resize(a.l.size());
  Engine e;if(!e.prepare(fs,s.factor,s.topology,s.persistence,s.injection,s.noise))throw std::runtime_error("prepare");
  auto events=score(s.score);std::size_t event=0;
  for(std::size_t n=0;n<a.l.size();n+=128) {
    const auto count=std::min<std::size_t>(128,a.l.size()-n);std::array<Event,32> block{};std::size_t k=0;
    while(event<events.size() && events[event].offset<n+count) {
      auto ev=events[event++];if(trace)*trace<<s.name<<','<<ev.offset<<','<<int(ev.kind)<<','<<ev.note<<','<<ev.value<<'\n';
      ev.offset-=n;block[k++]=ev;
    }
    Controls c=s.c;
    if(s.score==5 && n>=8*48000) {c.memory=0;c.instability=0;c.contamination=0;}
    if(s.score==6 && n>=4*48000 && n<5*48000) c.pressure=1;
    e.process(a.l.data()+n,a.r.data()+n,count,c,{block.data(),k});
    if(n%48000<128)a.state.push_back(e.observation());
  }return a;
}
void fft(std::vector<std::complex<double>>& a) {
  const auto n=a.size();for(std::size_t i=1,j=0;i<n;++i) {auto b=n>>1;for(;j&b;b>>=1)j^=b;j^=b;if(i<j)std::swap(a[i],a[j]);}
  for(std::size_t len=2;len<=n;len*=2) {
    const auto w=std::polar(1.,-2*std::numbers::pi/len);
    for(std::size_t i=0;i<n;i+=len) {std::complex<double> p=1;for(std::size_t j=0;j<len/2;++j){auto x=a[i+j],y=p*a[i+j+len/2];a[i+j]=x+y;a[i+j+len/2]=x-y;p*=w;}}
  }
}
double rms(const Audio& a) {double sum=0;for(std::size_t i=0;i<a.l.size();++i)sum+=.5*(a.l[i]*a.l[i]+a.r[i]*a.r[i]);return std::sqrt(sum/a.l.size());}
void metrics(std::ostream& f,const Study& s,const Audio& a,double seconds) {
  double peak=0,delta=0,base=0;std::size_t finite=0;
  for(std::size_t i=0;i<a.l.size();++i) {peak=std::max({peak,std::abs(double(a.l[i])),std::abs(double(a.r[i]))});finite+=!std::isfinite(a.l[i])||!std::isfinite(a.r[i]);}
  for(int n=200;n<16000;++n){delta+=std::pow(a.l[n+15*36000]-a.l[n],2);base+=a.l[n]*a.l[n];}
  std::vector<std::complex<double>> spectrum(65536);
  for(std::size_t i=0;i<spectrum.size();++i)spectrum[i]=a.l[i]*(.5-.5*std::cos(2*std::numbers::pi*i/(spectrum.size()-1)));
  fft(spectrum);double total=0,weighted=0,high=0;
  for(std::size_t i=1;i<spectrum.size()/2;++i){double hz=i*fs/spectrum.size(),power=std::norm(spectrum[i]);total+=power;weighted+=hz*power;if(hz>8000)high+=power;}
  const auto level=rms(a);
  f<<s.name<<','<<level<<','<<peak<<','<<20*std::log10(peak/std::max(level,1e-20))<<','<<weighted/std::max(total,1e-20)<<','<<high/std::max(total,1e-20)<<','<<std::sqrt(delta/std::max(base,1e-20))<<','<<seconds/14<<','<<finite<<'\n';
}
std::vector<Study> studies() {
  std::vector<Study> out;
  Controls base;base.instability=0;base.memory=0;base.contamination=0;base.release=.12;
  for(auto [name,topology]:{std::pair{"external",Topology::External},{"coupled",Topology::Coupled},{"filter_return",Topology::FilterReturn}})out.push_back({std::string("C1_C2_")+name,base,topology,Persistence::Reset});
  auto foundation=base;foundation.pressure=0;foundation.pairDepth=0;
  out.push_back({"C1_single_sine",foundation,Topology::External,Persistence::Reset});
  foundation.pairDepth=1;out.push_back({"C1_feedforward_pair",foundation,Topology::External,Persistence::Reset});
  for(auto [name,injection]:{std::pair{"phase",Injection::Phase},{"frequency",Injection::Frequency},{"bias",Injection::Bias},{"feedback",Injection::Feedback},{"output",Injection::Output}})for(auto [colour,noise]:{std::pair{"white",Noise::White},{"correlated",Noise::Correlated},{"highpass",Noise::Highpass}}) {
    auto c=base;c.contamination=.8;out.push_back({std::string("C3_")+name+"_"+colour,c,Topology::Coupled,Persistence::Reset,injection,noise});
  }
  for(double instability:{0.,.4,1.}) {auto c=base;c.instability=instability;out.push_back({"C4_drift_"+std::to_string(instability),c});}
  for(auto [name,memory]:{std::pair{"reset",Persistence::Reset},{"local",Persistence::Local},{"shared",Persistence::Shared},{"hybrid",Persistence::Hybrid}}) {
    auto c=base;c.memory=1;c.pressure=.7;c.contamination=.3;
    out.push_back({std::string("C5_memory_")+name,c,Topology::Coupled,memory});
  }
  for(double pressure:{0.,.5,1.}){auto c=base;c.pressure=pressure;out.push_back({"C6_pressure_"+std::to_string(pressure),c});}
  auto c=base;c.memory=.9;c.pressure=.65;c.contamination=.4;c.instability=.4;
  out.push_back({"C7_extended_clusters",c,Topology::Coupled,Persistence::Hybrid,Injection::Feedback,Noise::Correlated,1});
  out.push_back({"C7_clusters_local",c,Topology::Coupled,Persistence::Local,Injection::Feedback,Noise::Correlated,1});
  out.push_back({"C7_clusters_shared",c,Topology::Coupled,Persistence::Shared,Injection::Feedback,Noise::Correlated,1});
  out.push_back({"C8_sparse",c,Topology::Coupled,Persistence::Hybrid,Injection::Feedback,Noise::Correlated,2});
  c.sustain=.8;c.release=1.5;out.push_back({"C8_sustained",c,Topology::Coupled,Persistence::Hybrid,Injection::Feedback,Noise::Correlated,3});
  out.push_back({"C8_performance",c,Topology::Coupled,Persistence::Hybrid,Injection::Feedback,Noise::Correlated,4});
  c.release=.12;out.push_back({"C5_return_to_clean",c,Topology::Coupled,Persistence::Hybrid,Injection::Feedback,Noise::Correlated,5});
  out.push_back({"C8_automation",c,Topology::Coupled,Persistence::Hybrid,Injection::Feedback,Noise::Correlated,6});
  return out;
}
void alias(const std::filesystem::path& path) {
  std::ofstream f(path);f<<"note,factor,pressure,nonharmonic_db,render_fraction\n";
  for(int note:{69,93,105})for(int factor:{1,2,4,8})for(double pressure:{.35,1.}) {
    Engine e;e.prepare(fs,factor);Controls c;c.memory=c.instability=c.contamination=c.motion=0;c.pressure=pressure;c.colour=8000;c.sustain=1;
    c.detune=0;
    e.event({0,Kind::On,note,1});std::vector<float> l(3*48000),r(l.size());
    auto start=std::chrono::steady_clock::now();e.process(l.data(),r.data(),l.size(),c);
    auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
    std::vector<std::complex<double>> spec(65536);
    for(std::size_t i=0;i<spec.size();++i)spec[i]=l[l.size()-spec.size()+i]*(.5-.5*std::cos(2*std::numbers::pi*i/(spec.size()-1)));
    fft(spec);double power=0,residual=0;double fundamental=440*std::exp2((note-69)/12.);
    for(std::size_t i=1;i<spec.size()/2;++i) {
      double hz=i*fs/spec.size(),bin=fs/spec.size(),v=std::norm(spec[i]);power+=v;
      // Zero detune, noise, drift and memory isolate harmonic synthesis.
      bool harmonic=false;for(int k=0;k<=int(24000/fundamental)+1;++k)if(std::abs(hz-k*fundamental)<5*bin)harmonic=true;
      if(!harmonic)residual+=v;
    }
    f<<note<<','<<factor<<','<<pressure<<','<<10*std::log10(std::max(residual,1e-30)/power)<<','<<seconds/3<<'\n';
  }
}
}
int main(int argc,char** argv) {
  try {
    if(argc!=2)throw std::runtime_error("usage: c01_lab NEW_PACK_DIRECTORY");
    std::filesystem::path root=argv[1];if(std::filesystem::exists(root))throw std::runtime_error("pack directory already exists");
    std::filesystem::create_directories(root/"source");std::filesystem::create_directories(root/"audio");
    std::ofstream table(root/"measurements.csv"),key(root/"key.csv"),events(root/"events.csv"),state(root/"state.csv"),responses(root/"responses.csv");
    table<<std::setprecision(10)<<"study,rms,peak,crest_db,centroid_hz,above_8k_fraction,first_last_difference,render_fraction,nonfinite\n";
    key<<"id,study,gain\n";events<<"study,sample,kind,note,value\n";state<<"study,second,energy,memory,bias,interaction\n";
    responses<<"id,chord_legibility,weight,noise_integration,memory_relationship,preference,confidence,notes\n";
    auto list=studies();
    // Obscured IDs; fixed permutation for reproducibility, no promise of double-blind administration.
    for(std::size_t n=0;n<list.size();++n) {
      const auto& s=list[n];auto start=std::chrono::steady_clock::now();auto a=render(s,&events);
      double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();metrics(table,s,a,seconds);
      wav(root/"source"/(s.name+".wav"),a,1);
      double peak=0;for(float x:a.l)peak=std::max(peak,std::abs(double(x)));for(float x:a.r)peak=std::max(peak,std::abs(double(x)));
      double gain=std::min(.1/std::max(rms(a),1e-20),.89/std::max(peak,1e-20));
      auto id="take_"+std::to_string(100+(n*37)%list.size());wav(root/"audio"/(id+".wav"),a,gain);
      key<<id<<','<<s.name<<','<<gain<<'\n';responses<<id<<",,,,,,,\n";
      for(std::size_t i=0;i<a.state.size();++i){const auto& o=a.state[i];state<<s.name<<','<<i<<','<<o.energy<<','<<o.memory<<','<<o.bias<<','<<o.interaction<<'\n';}
      std::cout<<s.name<<'\n';
    }
    alias(root/"alias.csv");std::ofstream meta(root/"metadata.txt");
    meta<<"C01 laboratory 0.1; 48000 Hz stereo IEEE float32; 14 seconds per take\nCompiler "<<__VERSION__<<"\nEngine bytes "<<sizeof(Engine)<<"\nMatched target -20 dBFS RMS, capped at -1 dBFS peak. Not LUFS matching.\nCPU render_fraction = wall seconds / audio seconds, serial lab, not deadline guarantee.\n";
  }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}
}
