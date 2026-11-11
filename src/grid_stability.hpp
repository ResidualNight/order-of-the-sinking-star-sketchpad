#pragma once
#include "core.hpp"
#include <cstdint>
namespace sketch {
// Request-local convergence in screen pixels. Compare with a fixed anchor,
// not the preceding sample, so slow cumulative camera drift cannot pass.
struct GridStability {
 static constexpr uint64_t MinimumMs=1200,StableMs=750,TimeoutMs=8000,MaxGapMs=250;
 // Live stable gameplay still drifts ~0.02..0.05 px per 50..63ms at 4K.
 // Permit subpixel motion across the window, while rejecting transition travel.
 static constexpr float TolerancePx=1.f;
 Grid anchor;uint64_t since=0,last=0;unsigned samples=0;int source=0;float lastDriftPx=0;
 void reset(){anchor={};since=last=0;samples=0;lastDriftPx=0;}
 bool observe(const Grid& grid,float width,float height,int kind,uint64_t elapsed){
  if(!grid.usable()||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0){reset();return false;}
  bool same=anchor.usable()&&kind==source&&elapsed>=last&&elapsed-last<=MaxGapMs;
  lastDriftPx=0;
  if(same)for(float x:{.1f,.5f,.9f})for(float y:{.1f,.5f,.9f}){
   Point screen{x*width,y*height},cell=anchor.cell(screen),projected=grid.at(cell.x,cell.y);
   float drift=distance(projected,screen);
   if(!std::isfinite(projected.x)||!std::isfinite(projected.y)||!std::isfinite(drift)){same=false;lastDriftPx=INFINITY;}
   else {lastDriftPx=std::max(lastDriftPx,drift);if(drift>TolerancePx)same=false;}
  }
  if(!same){anchor=grid;source=kind;since=elapsed;samples=0;}
  if(!samples||elapsed>last)++samples;last=elapsed;
  return samples>=3&&elapsed>=MinimumMs&&elapsed-since>=StableMs;
 }
};
}
