#pragma once
#include "core.hpp"
#include <cstdint>
namespace sketch {
struct PadInput {bool connected=false;uint16_t buttons=0;float lx=0,ly=0,rx=0,ry=0;uint8_t lt=0,rt=0;};
namespace padui {
constexpr uint16_t Up=1,Down=2,Left=4,Right=8,Menu=16,Back=32,LB=256,RB=512,A=4096,B=8192,X=16384,Y=32768;
inline float wheelRadius(float width,float height,float uiScale,float scale){
 float base=std::min({175.f*uiScale,height*.31f,width*.4f});
 return std::min({base*scale,height*.31f,width*.4f});
}
// Clockwise from twelve o'clock. Returning to center explicitly cancels.
inline int wheelSelection(float x,float y,int previous=-1){
 float radius=std::sqrt(x*x+y*y);if(!std::isfinite(radius)||radius<.35f)return -1;
 constexpr float pi=3.14159265358979323846f,sector=2*pi/7;
 float angle=std::atan2(x,y);if(angle<0)angle+=2*pi;
 if(previous>=0&&previous<7){float delta=std::remainder(angle-previous*sector,2*pi);if(std::abs(delta)<sector*.5f+.07f)return previous;}
 return int(std::floor((angle+sector*.5f)/sector))%7;
}
inline Point stick(float x,float y){float n=std::sqrt(x*x+y*y);if(!std::isfinite(n)||n<.22f)return {};float strength=std::pow(std::clamp((n-.22f)/.78f,0.f,1.f),1.5f);return {x/n*strength,-y/n*strength};}
inline Point move(Point p,const PadInput&in,float dt,float width,float height,float speed){
 auto coarse=stick(in.lx,in.ly),fine=stick(in.rx,in.ry);bool precise=length(fine)>0;auto d=precise?fine:coarse;
 dt=std::clamp(std::isfinite(dt)?dt:0.f,0.f,.05f);float scale=std::max(1.f,std::min(width,height));
 p=p+d*(dt*scale*(precise?.13f:.8f)*speed);return {std::clamp(p.x,0.f,std::max(0.f,width-1)),std::clamp(p.y,0.f,std::max(0.f,height-1))};
}
inline Point step(Point p,Point direction,const Grid*grid,float width,float height){
 if(grid&&grid->usable()){
  auto cell=grid->cell(p);if(std::isfinite(cell.x)&&std::isfinite(cell.y)&&std::abs(cell.x)<Grid::MaxCellCoordinate-2&&std::abs(cell.y)<Grid::MaxCellCoordinate-2){
   cell={std::round(cell.x),std::round(cell.y)};auto origin=grid->at(cell.x,cell.y);float score=.45f;Point best=p;
   for(auto d:{Point{1,0},Point{-1,0},Point{0,1},Point{0,-1}}){auto candidate=grid->at(cell.x+d.x,cell.y+d.y);auto delta=candidate-origin;float len=length(delta);if(!std::isfinite(len)||len<1||candidate.x<0||candidate.y<0||candidate.x>=width||candidate.y>=height)continue;float alignment=dot(delta,direction)/len;if(alignment>score){score=alignment;best=candidate;}}
   return best;
  }
 }
 return {std::clamp(p.x+direction.x*3,0.f,std::max(0.f,width-1)),std::clamp(p.y+direction.y*3,0.f,std::max(0.f,height-1))};
}
}
}
