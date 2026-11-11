#pragma once
#include "grid_stability.hpp"
#include <cassert>
inline void checkGridStability(){
 using namespace sketch;
 for(Point output:std::vector<Point>{{640,360},{1920,1080},{3840,2160},{2560,1600},{3440,1440}}){
  Grid g=Grid::fromQuad({{output.x*.2f,output.y*.2f},{output.x*.8f,output.y*.25f},{output.x*.9f,output.y*.8f},{output.x*.1f,output.y*.85f}},8,7);
  assert(g.usable());GridStability stable;
  for(uint64_t t=0;t<1200;t+=100)assert(!stable.observe(g,output.x,output.y,100,t));
  assert(stable.observe(g,output.x,output.y,100,1200));
  // A camera can drift by less than tolerance each frame while moving many
  // pixels overall. Every sample is measured from a fixed window anchor.
  stable.reset();
  for(uint64_t t=0;t<3000;t+=100){auto moving=g;moving.origin.x+=float(t)*.006f;assert(!stable.observe(moving,output.x,output.y,100,t));}
  auto final=g;final.origin.x+=10;
  for(uint64_t t=3000;t<=3700;t+=100)assert(!stable.observe(final,output.x,output.y,100,t));
  assert(stable.observe(final,output.x,output.y,100,3800));
  assert(!stable.observe(final,output.x,output.y,200,3900)); // Source change.
  assert(!stable.observe(final,output.x,output.y,200,4300)); // Sampling gap.
  assert(!stable.observe({},output.x,output.y,200,4400)&&!stable.anchor.usable());
  for(uint64_t t=4500;t<=5200;t+=100)assert(!stable.observe(final,output.x,output.y,200,t));
  assert(stable.observe(final,output.x,output.y,200,5300));
  auto zoom=final;zoom.scaleStep(1.01f);assert(!stable.observe(zoom,output.x,output.y,200,5400));
 }
 // Reproduce live stable-gameplay drift (up to ~0.05px per 63ms): an
 // apparently stationary game camera need not become bitwise stationary.
 Grid live;live.valid=true;live.origin={900,600};GridStability stable;
 for(uint64_t t=0;t<=1200;t+=60){auto moving=live;moving.origin.x+=.7f*float(t)/1000;
  assert(stable.observe(moving,3840,2160,100,t)==(t==1200));
 }
}
