#include "core.hpp"
#include <algorithm>
#include <chrono>
#include <iostream>
#include <vector>
using namespace aste::contamination;
int main(){
  std::cout<<"rate,factor,voices,engine_bytes,mean_fraction,p99_fraction,max_fraction\n";
  for(double rate:{22050.,44100.,48000.,96000.,192000.})for(int factor:{1,2,4,8}){
    Engine e;e.prepare(rate,factor);Controls c;c.pressure=c.contamination=c.memory=c.instability=1;c.colour=8000;
    for(int note=48;note<60;++note)e.event({0,Kind::On,note,1});
    std::array<float,128> l{},r{};std::vector<double> times;times.reserve(500);
    for(int block=0;block<550;++block){
      auto start=std::chrono::steady_clock::now();e.process(l.data(),r.data(),l.size(),c);
      double elapsed=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()/(128/rate);
      if(block>=50)times.push_back(elapsed);
    }
    double mean=0;for(double x:times)mean+=x/times.size();std::sort(times.begin(),times.end());
    std::cout<<rate<<','<<factor<<",12,"<<sizeof(Engine)<<','<<mean<<','<<times[495]<<','<<times.back()<<'\n';
  }
}
