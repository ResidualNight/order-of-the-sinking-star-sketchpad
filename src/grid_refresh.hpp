#pragma once
#include <algorithm>
#include <chrono>
#include <cstdint>
namespace sketch {
// All requests are consumed on the render thread. Replacing one drops its pending result.
struct GridRefresh {
 static constexpr uint64_t ResizeSettleMs=250;
 bool requested=false,pending=false;
 uint64_t notBefore=0;
 static uint64_t now(){return uint64_t(std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now().time_since_epoch()).count());}
 void request(bool resized=false,uint64_t time=now()){
  requested=true;pending=false;
  notBefore=resized?time+ResizeSettleMs:std::max(notBefore,time);
 }
 bool ready(uint64_t time=now())const{return requested&&time>=notBefore;}
 void finish(){requested=false;pending=false;notBefore=0;}
 void cancel(){finish();}
};
}
