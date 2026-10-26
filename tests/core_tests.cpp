#include "core.hpp"
#include "grid.hpp"
#include "camera_grid.hpp"
#include "camera_picker.hpp"
#include "camera_fixture.hpp"
#include "level_watch_tests.hpp"
#include "camera_diagnostics_tests.hpp"
#include <iostream>
#include <cassert>
using namespace sketch;
namespace {
CameraFrame reflectedCamera(CameraFrame c,bool horizontal,bool vertical){
 for(int axis=0;axis<2;axis++)if(axis==0?horizontal:vertical){
  // Reflect clip-space rows and inverse columns together: these still pass
  // both inverse-matrix checks that the runtime previously relied on.
  for(int base:{0,192})for(int col=0;col<4;col++)c.f[base+axis*4+col]*=-1;
  for(int base:{16,176})for(int row=0;row<4;row++)c.f[base+row*4+axis]*=-1;
 }
 return c;
}
CameraFrame rotatedCamera(CameraFrame c,float radians){
 float cosine=std::cos(radians),sine=std::sin(radians);
 for(int base:{0,160})for(int row=0;row<4;row++){
  float x=c.f[base+row*4],y=c.f[base+row*4+1];
  c.f[base+row*4]=cosine*x-sine*y;c.f[base+row*4+1]=sine*x+cosine*y;
 }
 for(int col=0;col<4;col++){
  float x=c.f[16+col],y=c.f[20+col];c.f[16+col]=cosine*x-sine*y;c.f[20+col]=sine*x+cosine*y;
 }
 for(int base:{208,212,228,232,236}){
  float x=c.f[base],y=c.f[base+1];c.f[base]=cosine*x-sine*y;c.f[base+1]=sine*x+cosine*y;
 }
 return c;
}
CameraFrame resizedCamera(CameraFrame c,float width,float height){
 float aspectScale=(height/width)/(2160.f/3840.f);
 for(int base:{0,192})for(int col=0;col<4;col++)c.f[base+col]*=aspectScale;
 for(int base:{16,176})for(int row=0;row<4;row++)c.f[base+row*4]/=aspectScale;
 c.f[216]=1/width;c.f[217]=1/height;return c;
}
void checkCameraOrientation(const CameraFrame& captured){
 // World yaw changes individual grid-axis signs; it must not look like a flip.
 for(float degrees:{-180.f,-90.f,-45.f,0.f,45.f,90.f,180.f})for(Point viewport:std::vector<Point>{{3840,2160},{3200,1800},{1920,1080},{1280,720},{3440,1440}}){
  auto upright=resizedCamera(rotatedCamera(captured,degrees*3.14159265f/180),viewport.x,viewport.y);
  assert(validateCamera(upright,viewport.x,viewport.y)==CameraValidation::Valid);
  for(float layer:{0.f,1.f,1.75f})assert(gridFromCamera(upright,viewport.x,viewport.y,layer).usable());
  for(int axes=1;axes<=3;axes++){
   auto reflected=reflectedCamera(upright,(axes&1)!=0,(axes&2)!=0);
   assert(matrixInverseError(reflected.f.data(),reflected.f.data()+16)<.025f);
   assert(matrixInverseError(reflected.f.data()+176,reflected.f.data()+192)<.025f);
   assert(validateCamera(reflected,viewport.x,viewport.y)==CameraValidation::OrientationInvalid);
   assert(!validCamera(reflected,viewport.x,viewport.y));
   assert(!gridFromCamera(reflected,viewport.x,viewport.y,1).usable());
   auto preview=gridFromCamera(reflected,viewport.x,viewport.y,1,true);
   assert(preview.usable()&&cameraOrientation(reflected).category!=0);
   auto corrected=preview.reflected(viewport.x,viewport.y,(axes&1)!=0,(axes&2)!=0);
   auto expected=gridFromCamera(upright,viewport.x,viewport.y,1);
   auto twice=corrected.reflected(viewport.x,viewport.y,(axes&1)!=0,(axes&2)!=0);
   for(int x=-4;x<=4;++x)for(int y=-4;y<=4;++y){
    assert(distance(corrected.at(float(x),float(y)),expected.at(float(x),float(y)))<.02f);
    assert(distance(twice.at(float(x),float(y)),preview.at(float(x),float(y)))<.02f);
    assert(distance(corrected.snap(corrected.at(x+.15f,y-.1f)),expected.at(float(x),float(y)))<.02f);
   }
  }
 }
 // A broken inverse is invalid data, distinct from a consistent reflected frame.
 auto corrupt=captured;for(int col=0;col<4;col++)corrupt.f[4+col]*=-1;
 assert(validateCamera(corrupt,3840,2160)==CameraValidation::Invalid);
 assert(!validCamera(captured,NAN,2160)&&!validCamera(captured,3840,INFINITY));
 assert(!gridFromCamera(corrupt,3840,2160,1,true).usable());
 auto map=captured;map.f[235]=1;assert(!gridFromCamera(map,3840,2160,1,true).usable());
 auto viewport=captured;viewport.f[216]=1/1234.f;assert(!gridFromCamera(viewport,3840,2160,1,true).usable());
 auto malformed=captured;malformed.f[0]=NAN;assert(!gridFromCamera(malformed,3840,2160,1,true).usable());
 auto stopped=captured;stopped.f[225]=0;assert(!gridFromCamera(stopped,3840,2160,1,true).usable());
 assert(cameraCandidatePreferred(CameraValidation::Valid,100,CameraValidation::OrientationInvalid,200));
 assert(!cameraCandidatePreferred(CameraValidation::OrientationInvalid,200,CameraValidation::Valid,100));
 assert(cameraCandidatePreferred(CameraValidation::OrientationInvalid,200,CameraValidation::OrientationInvalid,100));
 assert(!cameraCandidatePreferred(CameraValidation::Invalid,300,CameraValidation::OrientationInvalid,100));
 assert(cameraCandidatePreferred(CameraValidation::Valid,1,CameraValidation::Valid,0xffffffff));
}
void checkCameraRenderScale(const CameraFrame& captured){
 for(Point output:std::vector<Point>{{3840,2160},{3200,1800},{2560,1440},{1920,1080},{1280,720},{640,360},{3440,1440},{1365,767}}){
  auto native=resizedCamera(captured,output.x,output.y);
  for(float internalHeight:{std::floor(output.y*.5f),720.f,1080.f,1440.f,2160.f}){
   float internalWidth=internalHeight==std::floor(output.y*.5f)?std::floor(output.x*.5f):std::floor(output.x*internalHeight/output.y);
   auto scaled=native;scaled.f[216]=1/internalWidth;scaled.f[217]=1/internalHeight;
   assert(validCamera(scaled,output.x,output.y));
   for(float z:{0.f,1.f,1.75f}){
    auto expected=gridFromCamera(native,output.x,output.y,z),actual=gridFromCamera(scaled,output.x,output.y,z);
    assert(expected.usable()&&actual.usable());
    for(int x=-5;x<=5;++x)for(int y=-5;y<=5;++y)assert(distance(expected.at(float(x),float(y)),actual.at(float(x),float(y)))<.002f);
   }
   for(int axes=1;axes<=3;++axes)assert(!validCamera(reflectedCamera(scaled,(axes&1)!=0,(axes&2)!=0),output.x,output.y));
   auto map=scaled;map.f[235]=1;assert(!validCamera(map,output.x,output.y));
  }
  for(float unknownHeight:{777.f,1499.f,4320.f}){
   auto unsupported=native;unsupported.f[216]=1/(output.x*unknownHeight/output.y);unsupported.f[217]=1/unknownHeight;
   assert(!validCamera(unsupported,output.x,output.y));
  }
  auto half=native;half.f[216]=2/output.x;half.f[217]=2/output.y;
  auto stretched=half;stretched.f[217]=native.f[217];assert(!validCamera(stretched,output.x,output.y));
 }
 // The reported output remained 3200x1800 while these internal settings changed.
 for(Point internal:std::vector<Point>{{3200,1800},{1600,900},{1280,720},{1920,1080},{2560,1440},{3840,2160}})
  assert(cameraViewportMatches(1/internal.x,1/internal.y,3200,1800));
 for(float bad:{0.f,-1.f,NAN,INFINITY})assert(!cameraViewportMatches(bad,1/2160.f,3840,2160));
 assert(!cameraViewportMatches(1/3200.f,1/1800.f,3840,2160));
 assert(!cameraViewportMatches(1/1920.f,1/1080.f,1920,1200)); // Different aspect.
 assert(!cameraViewportMatches(1/1920.f,1/1080.f,1e30f,1e30f));
}
}
int main(){std::cout.setf(std::ios::unitbuf);
 checkLevelWatch();
 checkCameraDiagnostics();
 Document d;Mark s;s.points={{0,0},{100,0}};d.add(s);auto before=d.snapshot();assert(d.eraseCircle({50,0},10));d.record(before);assert(d.marks.size()==2);assert(d.marks[0].points.back().x<40);assert(d.marks[1].points.front().x>60);assert(d.undo());assert(d.marks.size()==1&&d.marks[0].points.size()==2);assert(d.redo());assert(d.marks.size()==2);
 Document f;f.add(s);assert(f.eraseSweep({20,-100},{20,100},4));assert(f.marks.size()==2);
 Mark p;p.kind=Kind::Number;p.points={{200,200}};d.add(p);d.add(p);assert(d.marks.back().number==2);auto count=d.marks.size();auto save=d.snapshot();d.eraseCircle({200,200},2);d.record(save);assert(d.marks.size()==count-2);assert(d.nextNumber==3);d.undo();assert(d.marks.size()==count);d.clear();assert(d.nextNumber==1&&d.marks.empty());d.undo();assert(d.nextNumber==3&&d.marks.size()==count);
 // Stars never consume or display numbers; deleting an intermediate number leaves the sequence intact.
 Document separate;Mark star=p;star.kind=Kind::Pin;star.number=99;star.color=0xff332211;separate.add(star);
 assert(separate.nextNumber==1&&separate.marks.back().number==0&&separate.marks.back().color==star.color);
 p.points={{400,200}};separate.add(p);p.points={{600,200}};separate.add(p);separate.add(star);
 assert(separate.nextNumber==3&&separate.marks.back().number==0);
 auto separateBefore=separate.snapshot();assert(separate.eraseCircle({400,200},1));separate.record(separateBefore);
 assert(separate.nextNumber==3&&separate.marks[1].number==2);assert(separate.undo()&&separate.marks[1].number==1);
 assert(separate.redo()&&separate.nextNumber==3);p.points={{800,200}};separate.add(p);assert(separate.marks.back().number==3);
 separate.clear();assert(separate.nextNumber==1);assert(separate.undo()&&separate.nextNumber==4);p.points={{200,200}};
 // Restarting starts a second sequence without changing any existing number or other mark.
 separate.restartNumbers();assert(separate.nextNumber==1&&separate.marks.back().number==3);
 auto restartHistory=separate.undoStack.size();separate.restartNumbers();assert(separate.undoStack.size()==restartHistory);
 separate.add(p);assert(separate.marks.back().number==1&&separate.nextNumber==2);
 assert(separate.undo()&&separate.nextNumber==1&&separate.marks.back().number==3);
 assert(separate.undo()&&separate.nextNumber==4&&separate.marks.back().number==3);
 assert(separate.redo()&&separate.nextNumber==1);assert(separate.redo()&&separate.marks.back().number==1);
 Grid g;g.origin={30,50};g.u={80,12};g.v={-10,65};g.valid=true;assert(distance(g.snap(g.at(2.2f,-3.1f)),g.at(2,-3))<.01f);assert(std::abs(g.cell(g.at(-2,3)).x+2)<.001f);
 int x0,x1,y0,y1;assert(g.visibleCells(1920,1080,x0,x1,y0,y1)&&x1>x0&&y1>y0);
 assert(!g.visibleCells(0,1080,x0,x1,y0,y1)&&!g.visibleCells(NAN,1080,x0,x1,y0,y1)&&!g.visibleCells(1e8f,1080,x0,x1,y0,y1));
 Grid horizon;horizon.valid=true;horizon.u={20,0};horizon.v={0,20};horizon.px=std::nextafter(20.f/4096,0.f);
 Point edge{4096,0};assert(horizon.usable()&&std::isfinite(horizon.cell(edge).x)&&horizon.cell(edge).x>Grid::MaxCellCoordinate);
 assert(!horizon.visibleCells(4096,2160,x0,x1,y0,y1)&&distance(horizon.snap(edge),edge)<.001f);
 std::vector<Point> corners={{200,150},{900,160},{1000,800},{100,800}};auto projective=Grid::fromQuad(corners,8,7);assert(projective.usable());Point sourceCorners[]={{-.5f,-.5f},{7.5f,-.5f},{7.5f,6.5f},{-.5f,6.5f}};for(int i=0;i<4;i++)assert(distance(projective.at(sourceCorners[i].x,sourceCorners[i].y),corners[i])<.02f);for(auto c:std::vector<Point>{{0,0},{2.2f,3.4f},{7,6},{-2,-1}}){auto back=projective.cell(projective.at(c.x,c.y));assert(distance(back,c)<.005f);}assert(distance(projective.snap(projective.at(2.1f,4.2f)),projective.at(2,4))<.01f);assert(length(projective.at(2,6)-projective.at(2,5))>length(projective.at(2,1)-projective.at(2,0)));assert(!Grid::fromQuad({{0,0},{10,10},{0,10},{10,0}},8,7).usable());
 auto doubled=projective;doubled.scaleStep(2);assert(distance(doubled.at(1.3f,2.2f),projective.at(2.6f,4.4f))<.01f);doubled.scaleStep(.5f);assert(distance(doubled.at(2,3),projective.at(2,3))<.01f);Document emptyPins;emptyPins.add(p);emptyPins.eraseCircle({200,200},100);assert(emptyPins.marks.empty()&&emptyPins.nextNumber==2);emptyPins.clear();assert(emptyPins.nextNumber==1);emptyPins.undo();assert(emptyPins.marks.empty()&&emptyPins.nextNumber==2);
 CameraFrame camera=gameCameraFixture();assert(validCamera(camera,3840,2160));assert(validCamera(camera,1920,1080));assert(!validCamera(camera,1920,1200));
 assert(sameCameraView(camera,camera));auto homogeneous=camera;
 for(int i=0;i<16;++i){homogeneous.f[i]*=2;homogeneous.f[16+i]*=.5f;}assert(sameCameraView(camera,homogeneous));
 auto other=camera;other.f[208]+=1;assert(!sameCameraView(camera,other));
 assert(!sameCameraView(camera,reflectedCamera(camera,false,true)));other=camera;other.f[0]=NAN;assert(!sameCameraView(camera,other));
 auto dataGrid=gridFromCamera(camera,3840,2160,1);assert(dataGrid.usable());assert(!gridFromCamera(camera,3840,2160,1000).usable());
 auto direct=[&](float x,float y,float z){double q[4]{};double p[4]={x,y,z,1};for(int row=0;row<4;row++)for(int col=0;col<4;col++)q[row]+=camera.f[row*4+col]*p[col];return Point{float((1+q[0]/q[3])*1920),float((1-q[1]/q[3])*1080)};};
 for(int x=-1;x<11;x++)for(int y=2;y<11;y++)assert(distance(dataGrid.at(float(x-5),float(y-7)),direct(float(x),float(y),1))<.002f);
 assert(distance(dataGrid.snap(direct(3.1f,6.2f,1)),direct(3,6,1))<.002f);
 assert(distance(direct(5,3,1),direct(6,3,1))>distance(direct(5,9,1),direct(6,9,1)));
 auto layer0=gridFromCamera(camera,3840,2160,0);assert(layer0.usable()&&distance(layer0.origin,dataGrid.origin)>10);
 checkCameraOrientation(camera);
 checkCameraRenderScale(camera);
 auto badCamera=camera;badCamera.f[0]=NAN;assert(!validCamera(badCamera,3840,2160));badCamera=camera;badCamera.f[235]=1;assert(!validCamera(badCamera,3840,2160));badCamera=camera;badCamera.f[0]=1e30f;assert(!validCamera(badCamera,3840,2160));
 Image blank;blank.width=640;blank.height=400;blank.rgba.assign(640*400*4,100);assert(!detectGrid(blank).grid.valid);
 Image checker;checker.width=640;checker.height=400;checker.rgba.resize(640*400*4);for(int y=0;y<400;y++)for(int x=0;x<640;x++){int k=(y*640+x)*4;uint8_t c=(x%40<2||y%40<2)?35:180;checker.rgba[k]=checker.rgba[k+1]=checker.rgba[k+2]=c;checker.rgba[k+3]=255;}
 auto found=detectGrid(checker);std::cout<<"synthetic_grid "<<found.grid.valid<<" "<<found.diagnostic<<"\n";assert(found.grid.valid);assert(std::abs(length(found.grid.u)-40)<3&&std::abs(length(found.grid.v)-40)<3);
 std::cout<<"PASS: partial erase, sweep erase, undo/redo, independent numbering, clear, affine and projective snapping, trapezoid fitting, blank rejection, synthetic grid, live camera projection, reflected camera rejection, validated level reads and stable level transitions\n";
}
