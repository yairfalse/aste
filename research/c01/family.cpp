#include "core.hpp"
#include "sequence_processor.hpp"
#include "field_processor.hpp"
#include "resonator_processor.hpp"
#include <algorithm>
#include <bit>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <vector>
namespace {
void word(std::ofstream& f,std::uint32_t x,int bytes=4){for(int i=0;i<bytes;++i)f.put(static_cast<char>((x>>(8*i))&255));}
void write(const std::filesystem::path& path,const std::vector<float>& l,const std::vector<float>& r) {
  double sum=0,peak=0;for(std::size_t i=0;i<l.size();++i){sum+=.5*(l[i]*l[i]+r[i]*r[i]);peak=std::max({peak,std::abs(double(l[i])),std::abs(double(r[i]))});}
  double gain=std::min(.1/std::sqrt(sum/l.size()),.89/std::max(peak,1e-20));
  std::ofstream f(path,std::ios::binary);f.write("RIFF",4);word(f,36+static_cast<std::uint32_t>(l.size()*8));f.write("WAVEfmt ",8);word(f,16);word(f,3,2);word(f,2,2);word(f,48000);word(f,384000);word(f,8,2);word(f,32,2);f.write("data",4);word(f,static_cast<std::uint32_t>(l.size()*8));
  for(std::size_t i=0;i<l.size();++i)for(float x:{l[i],r[i]})word(f,std::bit_cast<std::uint32_t>(static_cast<float>(x*gain)));
}
}
int main(int argc,char** argv){
  if(argc!=2 || std::filesystem::exists(argv[1])){std::cerr<<"usage: c01_family NEW_DIRECTORY\n";return 1;}
  std::filesystem::create_directories(argv[1]);
  for(int model=0;model<4;++model){
    std::vector<float> l(14*48000),r(l.size());
    aste::sequence::Processor seq;aste::sequence::Parameters sp;sp.voiceMode=1;sp.sequenceEnabled=false;sp.materialBehavior=true;
    sp.attackMs=3;sp.decayMs=250;sp.sustain=.55F;sp.releaseMs=120;seq.prepare(48000,sp);
    aste::field::Processor field;aste::field::Parameters fp;field.prepare(48000);
    aste::resonator::Engine resonator;aste::resonator::Controls rc;rc.t60=.5;rc.couplingHz=3;
    resonator.prepare(48000,aste::resonator::Architecture::Modal,aste::resonator::Memory::Shared,rc);
    aste::resonator::Exciter exciter;exciter.prepare(48000);
    aste::contamination::Engine c01;c01.prepare(48000);aste::contamination::Controls c;c.memory=.9;c.pressure=.65;c.contamination=.4;c.instability=.4;c.release=.12;
    for(std::size_t n=0;n<l.size();++n){
      const bool on=n%36000==0 && n<16*36000,off=n%36000==6720 && n<16*36000;
      std::array<aste::sequence::MidiEvent,4> midi{};std::size_t k=0;
      if(on||off)for(int note:{48,51,55,58}){
        midi[k++]={0,on?aste::sequence::MidiEventType::noteOn:aste::sequence::MidiEventType::noteOff,note,.8F};
        if(model==0)c01.event({0,on?aste::contamination::Kind::On:aste::contamination::Kind::Off,note,.8});
      }
      if(model==0)c01.process(l.data()+n,r.data()+n,1,c);
      else if(model<=2){
        seq.process(l.data()+n,r.data()+n,1,sp,{}, {midi.data(),k});
        if(model==2)field.process(l.data()+n,r.data()+n,1,fp);
      }else{
        std::array<aste::resonator::Event,1> ev{};
        if(on){exciter.trigger(aste::resonator::Excitation::Shaped,101,1680);ev[0].type=aste::resonator::EventType::Strike;}
        const float excitation=.8F*exciter.next();resonator.process(&excitation,l.data()+n,r.data()+n,1,rc,{ev.data(),on?1U:0U});
      }
    }
    const std::array names{"C01_hybrid","Sequence_poly","Sequence_into_Field","Resonator_modal"};
    write(std::filesystem::path(argv[1])/(std::string(names[static_cast<std::size_t>(model)])+".wav"),l,r);
  }
  std::ofstream notes(std::filesystem::path(argv[1])/"conditions.txt");
  notes<<"Same C minor-seven notes, sixteen repetitions, 750ms spacing, 140ms gate, velocity .8.\nR01 uses its shaped burst and shared modal bank; key release is its decay rather than a synth ADSR.\nCurrent dirty-workspace family processors; source hashes recorded separately. RMS match -20 dBFS, peak capped -1 dBFS.\nField uses default processor controls. This tests a family reference, not best possible existing-family patches.\n";
}
