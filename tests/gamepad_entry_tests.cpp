#include "gamepad_entry.hpp"
#include <cassert>
#include <cstdio>
using sketch::pad::TriggerEntry;
int main(){
 auto step=[](TriggerEntry&e,int l,int r,uint64_t t,bool open,int outL,int outR){uint8_t a=uint8_t(l),b=uint8_t(r);assert(e.filter(a,b,t)==open);assert(a==outL&&b==outR);};
 for(bool reverse:{false,true}){TriggerEntry e;step(e,0,0,0,false,0,0);step(e,reverse?0:200,reverse?200:0,10,false,0,0);step(e,220,230,80,true,0,0);step(e,255,255,1000,false,0,0);step(e,0,255,1100,false,0,0);step(e,0,0,1200,false,0,0);step(e,200,200,1201,true,0,0);}
 TriggerEntry tap;step(tap,0,0,0,false,0,0);step(tap,255,0,10,false,0,0);step(tap,0,0,20,false,0,0);step(tap,0,0,190,false,255,0);step(tap,0,0,191,false,0,0);step(tap,255,0,200,false,0,0);step(tap,255,0,380,false,255,0);step(tap,255,255,381,false,255,255);step(tap,0,0,400,false,0,0);
 TriggerEntry rearm;step(rearm,255,255,0,false,255,255);step(rearm,0,0,1,false,0,0);step(rearm,255,0,2,false,0,0);rearm.reset();step(rearm,255,255,100,false,255,255);step(rearm,0,0,101,false,0,0);step(rearm,160,160,102,true,0,0);
 TriggerEntry rapid;step(rapid,0,0,0,false,0,0);step(rapid,200,0,10,false,0,0);step(rapid,0,0,20,false,0,0);step(rapid,210,0,30,false,0,0);step(rapid,0,0,40,false,0,0);step(rapid,0,0,190,false,200,0);step(rapid,0,0,191,false,0,0);step(rapid,0,0,192,false,210,0);step(rapid,0,0,193,false,0,0);
 std::puts("trigger entry: either order, immediate pair, suppression, single tap/hold, late second trigger and neutral rearm passed");
}
