#include "core.hpp"
#include "grid.hpp"
#include "camera_grid.hpp"
#include "camera_fixture.hpp"
#include <iostream>
#include <cassert>
using namespace sketch;
int main(){std::cout.setf(std::ios::unitbuf);
 Document d;Mark s;s.points={{0,0},{100,0}};d.add(s);auto before=d.snapshot();assert(d.eraseCircle({50,0},10));d.record(before);assert(d.marks.size()==2);assert(d.marks[0].points.back().x<40);assert(d.marks[1].points.front().x>60);assert(d.undo());assert(d.marks.size()==1&&d.marks[0].points.size()==2);assert(d.redo());assert(d.marks.size()==2);
 Document f;f.add(s);assert(f.eraseSweep({20,-100},{20,100},4));assert(f.marks.size()==2);
 Mark p;p.kind=Kind::Pin;p.points={{200,200}};d.add(p);d.add(p);assert(d.marks.back().number==2);auto count=d.marks.size();auto save=d.snapshot();d.eraseCircle({200,200},2);d.record(save);assert(d.marks.size()==count-2);assert(d.nextPin==3);d.undo();assert(d.marks.size()==count);d.clear();assert(d.nextPin==1&&d.marks.empty());d.undo();assert(d.nextPin==3&&d.marks.size()==count);
 Grid g;g.origin={30,50};g.u={80,12};g.v={-10,65};g.valid=true;assert(distance(g.snap(g.at(2.2f,-3.1f)),g.at(2,-3))<.01f);assert(std::abs(g.cell(g.at(-2,3)).x+2)<.001f);
 std::vector<Point> corners={{200,150},{900,160},{1000,800},{100,800}};auto projective=Grid::fromQuad(corners,8,7);assert(projective.usable());Point sourceCorners[]={{-.5f,-.5f},{7.5f,-.5f},{7.5f,6.5f},{-.5f,6.5f}};for(int i=0;i<4;i++)assert(distance(projective.at(sourceCorners[i].x,sourceCorners[i].y),corners[i])<.02f);for(auto c:std::vector<Point>{{0,0},{2.2f,3.4f},{7,6},{-2,-1}}){auto back=projective.cell(projective.at(c.x,c.y));assert(distance(back,c)<.005f);}assert(distance(projective.snap(projective.at(2.1f,4.2f)),projective.at(2,4))<.01f);assert(length(projective.at(2,6)-projective.at(2,5))>length(projective.at(2,1)-projective.at(2,0)));assert(!Grid::fromQuad({{0,0},{10,10},{0,10},{10,0}},8,7).usable());
 auto doubled=projective;doubled.scaleStep(2);assert(distance(doubled.at(1.3f,2.2f),projective.at(2.6f,4.4f))<.01f);doubled.scaleStep(.5f);assert(distance(doubled.at(2,3),projective.at(2,3))<.01f);Document emptyPins;emptyPins.add(p);emptyPins.eraseCircle({200,200},100);assert(emptyPins.marks.empty()&&emptyPins.nextPin==2);emptyPins.clear();assert(emptyPins.nextPin==1);emptyPins.undo();assert(emptyPins.marks.empty()&&emptyPins.nextPin==2);
 CameraFrame camera=gameCameraFixture();assert(validCamera(camera,3840,2160));assert(!validCamera(camera,1920,1080));
 auto dataGrid=gridFromCamera(camera,3840,2160,1);assert(dataGrid.usable());assert(!gridFromCamera(camera,3840,2160,1000).usable());
 auto direct=[&](float x,float y,float z){double q[4]{};double p[4]={x,y,z,1};for(int row=0;row<4;row++)for(int col=0;col<4;col++)q[row]+=camera.f[row*4+col]*p[col];return Point{float((1+q[0]/q[3])*1920),float((1-q[1]/q[3])*1080)};};
 for(int x=-1;x<11;x++)for(int y=2;y<11;y++)assert(distance(dataGrid.at(float(x-5),float(y-7)),direct(float(x),float(y),1))<.002f);
 assert(distance(dataGrid.snap(direct(3.1f,6.2f,1)),direct(3,6,1))<.002f);
 assert(distance(direct(5,3,1),direct(6,3,1))>distance(direct(5,9,1),direct(6,9,1)));
 auto layer0=gridFromCamera(camera,3840,2160,0);assert(layer0.usable()&&distance(layer0.origin,dataGrid.origin)>10);
 auto badCamera=camera;badCamera.f[0]=NAN;assert(!validCamera(badCamera,3840,2160));badCamera=camera;badCamera.f[235]=1;assert(!validCamera(badCamera,3840,2160));badCamera=camera;badCamera.f[0]=1e30f;assert(!validCamera(badCamera,3840,2160));
 Image blank;blank.width=640;blank.height=400;blank.rgba.assign(640*400*4,100);assert(!detectGrid(blank).grid.valid);
 Image checker;checker.width=640;checker.height=400;checker.rgba.resize(640*400*4);for(int y=0;y<400;y++)for(int x=0;x<640;x++){int k=(y*640+x)*4;uint8_t c=(x%40<2||y%40<2)?35:180;checker.rgba[k]=checker.rgba[k+1]=checker.rgba[k+2]=c;checker.rgba[k+3]=255;}
 auto found=detectGrid(checker);std::cout<<"synthetic_grid "<<found.grid.valid<<" "<<found.diagnostic<<"\n";assert(found.grid.valid);assert(std::abs(length(found.grid.u)-40)<3&&std::abs(length(found.grid.v)-40)<3);
 std::cout<<"PASS: partial erase, sweep erase, undo/redo, pin numbering, clear, affine and projective snapping, trapezoid fitting, blank rejection, synthetic grid, live camera projection and invalid data rejection\n";
}