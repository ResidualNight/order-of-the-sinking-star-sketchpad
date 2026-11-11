#pragma once
#include <cstdint>
#include <array>
namespace sketch::pad {
// Single owner: only fresh actual game queries advance buffered events.
class TriggerEntry {
 enum class Phase { Neutral, Idle, Pair, Drain, Forward, Consume };
 struct Sample {uint8_t left=0,right=0;};
 Phase phase=Phase::Neutral;uint64_t started=0;
 std::array<Sample,256> queue{};unsigned head=0,count=0;Sample last{};
 bool append(Sample sample){
  if(count&&sample.left==last.left&&sample.right==last.right)return true;
  if(count==queue.size())return false;
  queue[(head+count)%queue.size()]=last=sample;++count;return true;
 }
public:
 static constexpr uint8_t Release=30,Press=160;
 static constexpr uint64_t PairMs=180;
 void reset(){phase=Phase::Neutral;head=count=0;}
 bool filter(uint8_t&left,uint8_t&right,uint64_t now){
  bool neutral=left<=Release&&right<=Release;
  if(phase==Phase::Neutral){if(neutral)phase=Phase::Idle;return false;}
  if(phase==Phase::Consume){left=right=0;if(neutral)phase=Phase::Idle;return false;}
  if(phase==Phase::Forward){if(neutral)phase=Phase::Idle;return false;}
  if(phase==Phase::Idle){if(neutral)return false;phase=Phase::Pair;started=now;head=count=0;}
  if(!append({left,right})){phase=Phase::Forward;head=count=0;return false;} // bounded failure: pass through
  if(phase==Phase::Pair){
   if(now-started<=PairMs&&left>=Press&&right>=Press){phase=Phase::Consume;head=count=0;left=right=0;return true;}
   if(now-started<PairMs){left=right=0;return false;}
   phase=Phase::Drain;
  }
  auto sample=queue[head];head=(head+1)%queue.size();--count;left=sample.left;right=sample.right;
  if(!count)phase=neutral?Phase::Idle:Phase::Forward;
  return false;
 }
};
}
