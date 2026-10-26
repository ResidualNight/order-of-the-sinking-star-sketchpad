#pragma once
#include "camera_grid.hpp"
namespace sketch {
// Request-local previews only. These frames and tokens are never serialized.
struct CameraOption {
 uint64_t token=0;CameraFrame camera;float suggestedZ=1;bool actorLayer=false;
 uint8_t directionCategory=0;size_t copies=0;
};
inline bool sameCameraView(const CameraFrame& a,const CameraFrame& b){
 uint32_t at=0,bt=0;std::memcpy(&at,a.f.data()+226,4);std::memcpy(&bt,b.f.data()+226,4);
 if(at!=bt||cameraOrientation(a).category!=cameraOrientation(b).category)return false;
 double as=0,bs=0;
 for(int i:{0,1,2,4,5,6,8,9,10,12,13,14}){as=std::max(as,std::abs(double(a.f[i])));bs=std::max(bs,std::abs(double(b.f[i])));}
 if(as<=0||bs<=0||!std::isfinite(as)||!std::isfinite(bs))return false;
 for(int i=0;i<16;++i){double x=a.f[i]/as,y=b.f[i]/bs;if(!std::isfinite(x)||!std::isfinite(y)||std::abs(x-y)>1e-5*std::max({1.,std::abs(x),std::abs(y)}))return false;}
 for(int i:{208,209,210,212,213,214})if(!std::isfinite(a.f[i])||!std::isfinite(b.f[i])||std::abs(a.f[i]-b.f[i])>1e-4f)return false;
 for(int i:{216,217})if(a.f[i]<=0||b.f[i]<=0||std::abs(1/a.f[i]-1/b.f[i])>1)return false;
 return true;
}
inline Grid cameraOptionGrid(const CameraOption& option,float width,float height,float z,int flip){
 if(flip<0||flip>3)return {};
 return gridFromCamera(option.camera,width,height,z,true).reflected(width,height,(flip&1)!=0,(flip&2)!=0);
}
}
