#pragma once
#include "camera_grid.hpp"
#include "scene_camera.hpp"
namespace sketch {
// Request-local previews only. These frames and tokens are never serialized.
struct CameraOption {
 uint64_t token=0;CameraFrame camera;float suggestedZ=1;bool actorLayer=false;
 uint8_t directionCategory=0;size_t copies=0;
 bool projectionUnverified=false,aspectUnverified=false;
 bool scenePose=false,sceneCorroborated=false;SceneCameraPose pose;
};
inline bool sameCameraView(const CameraFrame& a,const CameraFrame& b,bool compareViewport=true){
 uint32_t at=0,bt=0;std::memcpy(&at,a.f.data()+226,4);std::memcpy(&bt,b.f.data()+226,4);
 if(at!=bt||cameraOrientation(a).category!=cameraOrientation(b).category)return false;
 double as=0,bs=0;
 for(int i:{0,1,2,4,5,6,8,9,10,12,13,14}){as=std::max(as,std::abs(double(a.f[i])));bs=std::max(bs,std::abs(double(b.f[i])));}
 if(as<=0||bs<=0||!std::isfinite(as)||!std::isfinite(bs))return false;
 for(int i=0;i<16;++i){double x=a.f[i]/as,y=b.f[i]/bs;if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x-y)>1e-5*std::max({1.,std::abs(x),std::abs(y)}))return false;}
 for(int i:{208,209,210,212,213,214})if(!std::isfinite(a.f[i])||!std::isfinite(b.f[i])||std::abs(a.f[i]-b.f[i])>1e-4f)return false;
 if(compareViewport)for(int i:{216,217})if(a.f[i]<=0||b.f[i]<=0||std::abs(1/a.f[i]-1/b.f[i])>1)return false;
 return true;
}
// Session-only display choices. No content-rectangle detection is implied.
enum class CameraFit {Stretch,Contain,Cover};
inline constexpr float CameraAspects[]={0,16.f/9,16.f/10,4.f/3,5.f/4,64.f/27,43.f/18,32.f/9};
inline Grid mapCameraGrid(Grid grid,float width,float height,float aspect,CameraFit fit){
 if(!grid.usable()||!std::isfinite(width)||!std::isfinite(height)||width<=0||height<=0||
    !std::isfinite(aspect)||aspect<=0)return {};
 float sx=1,sy=1;
 if(fit==CameraFit::Contain){if(width/height>aspect)sx=aspect*height/width;else sy=width/aspect/height;}
 else if(fit==CameraFit::Cover){if(width/height>aspect)sy=width/aspect/height;else sx=aspect*height/width;}
 else if(fit!=CameraFit::Stretch)return {};
 Point offset{width*(1-sx)*.5f,height*(1-sy)*.5f};
 // Compose the affine viewport transform with the complete homography.
 grid.origin={sx*grid.origin.x+offset.x,sy*grid.origin.y+offset.y};
 grid.u={sx*grid.u.x+offset.x*grid.px,sy*grid.u.y+offset.y*grid.px};
 grid.v={sx*grid.v.x+offset.x*grid.py,sy*grid.v.y+offset.y*grid.py};
 if(!grid.usable())return {};return grid;
}
inline Grid cameraOptionGrid(const CameraOption& option,float width,float height,float z,int flip,int aspectChoice=0,CameraFit fit=CameraFit::Stretch){
 if(flip<0||flip>3||aspectChoice<0||size_t(aspectChoice)>=std::size(CameraAspects))return {};
 float aspect=option.scenePose?(aspectChoice?CameraAspects[aspectChoice]:width/height):option.camera.f[217]/option.camera.f[216];
 Grid grid;
 if(option.scenePose){
  std::array<float,16> matrix{};std::array<float,3> direction{};
  if(!option.pose.project(width,height,matrix,direction))return {};
  for(int col=0;col<4;++col)matrix[col]*=(width/height)/aspect;
  grid=gridFromProjection(matrix.data(),option.pose.position.data(),direction.data(),width,height,z);
 }else grid=gridFromCamera(option.camera,width,height,z,true,option.projectionUnverified,option.aspectUnverified);
 grid=mapCameraGrid(grid,width,height,aspect,fit);
 return grid.reflected(width,height,(flip&1)!=0,(flip&2)!=0);
}
}
