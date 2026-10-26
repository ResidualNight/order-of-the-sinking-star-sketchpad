#pragma once
#include "core.hpp"
#include <array>
#include <cstring>
namespace sketch {
struct CameraFrame {
 std::array<float,240> f{};
 uint32_t frame() const {uint32_t n;std::memcpy(&n,f.data()+225,4);return n;}
 uint32_t sourceType() const {uint32_t n;std::memcpy(&n,f.data()+226,4);return n;}
};
// r67271 View_Kind values, corroborated by controlled main/reflection captures.
// Unknown/new values retain validation; this hint never admits invalid data.
inline bool auxiliaryCamera(uint32_t type){return (type>=1&&type<=7)||type==9||type==10;}
inline const char* cameraSourceName(uint32_t type){return type==8?"main":type==5?"reflection":auxiliaryCamera(type)?"auxiliary":"unknown";}
inline int cameraSourceRank(uint32_t type){return type==8?2:auxiliaryCamera(type)?0:1;}
enum class CameraValidation {Invalid,Valid,OrientationInvalid};
enum class CameraReject {
 None,TransformValues,CameraValues,Viewport,DirectionNorm,FrameIndex,CameraTime,MapAmount,
 TransformInverse,ProjectionInverse,ForwardDepth,Orientation,ReadFailure,FrameStatic,FrameJump,TimeReversed,TimeJump,Count
};
inline constexpr const char* CameraRejectNames[]={"none","transform_values","camera_values","viewport","direction_norm","frame_index","camera_time","map_amount","transform_inverse","projection_inverse","forward_depth","orientation","read_failure","frame_static","frame_jump","time_reversed","time_jump"};
static_assert(sizeof(CameraRejectNames)/sizeof(CameraRejectNames[0])==size_t(CameraReject::Count));
struct CameraCheck {
 CameraValidation result=CameraValidation::Invalid;CameraReject reason=CameraReject::None;
 int badIndex=-1;float badValue=0;
};
// r67271 has native/half-display modes plus fixed 720p/1080p/1440p/4K modes.
// Affected-machine diagnostics also show consistent quarter-output views
// (960x540 at 3840x2160). Admit that exact size relation through the same
// numeric, inverse, orientation and freshness checks as every other view.
// Internal rendering can be smaller OR larger than the output. The projection
// is already in NDC; only gridFromCamera maps it onto the complete swapchain.
// Keep the known mode sizes and aspect check instead of accepting arbitrary data.
inline bool cameraViewportMatches(float inverseWidth,float inverseHeight,float w,float h,bool reflection=false){
 if(!std::isfinite(w)||!std::isfinite(h)||w<640||h<360||w>16384||h>16384||
    !std::isfinite(inverseWidth)||!std::isfinite(inverseHeight)||inverseWidth<=0||inverseHeight<=0)return false;
 float cw=1/inverseWidth,ch=1/inverseHeight;
 if(!std::isfinite(cw)||!std::isfinite(ch)||cw<160||ch<90||cw>16385||ch>16385)return false;
 for(float scale:{1.f,.5f,.25f})if(std::abs(cw-w*scale)<=1&&std::abs(ch-h*scale)<=1)return true;
 for(float renderHeight:{720.f,1080.f,1440.f,2160.f})
  if(std::abs(ch-renderHeight)<=1&&std::abs(cw-renderHeight*w/h)<=1)return true;
 // Controlled r67271 captures: reflection size is 1/2 or 1/4 of a
 // supported scene viewport. Only source type 5 may use this extra relation;
 // the reader never automatically adopts known auxiliary views.
 if(reflection)for(float scale:{.5f,.25f}){
  for(float parent:{1.f,.5f})if(std::abs(cw-w*parent*scale)<=1&&std::abs(ch-h*parent*scale)<=1)return true;
  for(float parentHeight:{720.f,1080.f,1440.f,2160.f})
   if(std::abs(ch-parentHeight*scale)<=1&&std::abs(cw-parentHeight*w/h*scale)<=1)return true;
 }
 return false;
}
inline float matrixInverseError(const float*a,const float*b){float error=0;for(int row=0;row<4;row++)for(int col=0;col<4;col++){float value=0;for(int k=0;k<4;k++)value+=a[row*4+k]*b[k*4+col];if(!std::isfinite(value))return INFINITY;error=std::max(error,std::abs(value-(row==col?1.f:0.f)));}return error;}
struct CameraOrientation {double depth=0,handedness=NAN,worldUp=NAN;uint8_t category=0;};
inline CameraOrientation cameraOrientation(const CameraFrame& c){
 const auto& f=c.f;CameraOrientation result;
 double point[]={double(f[208])+10*f[212],double(f[209])+10*f[213],double(f[210])+10*f[214],1},clip[4]{};
 for(int row=0;row<4;++row)for(int col=0;col<4;++col)clip[row]+=f[row*4+col]*point[col];
 result.depth=clip[3];
 if(std::isfinite(clip[3])&&clip[3]!=0){double x=clip[0]/clip[3],y=clip[1]/clip[3];
  result.handedness=(f[0]-x*f[12])*(f[5]-y*f[13])-(f[1]-x*f[13])*(f[4]-y*f[12]);result.worldUp=f[6]-y*f[14];
 }
 result.category=uint8_t((f[214]>=-.05f?1:0)|(result.handedness<=0?2:0)|(result.worldUp<=0?4:0));return result;
}
inline bool cameraCandidatePreferred(CameraValidation kind,uint32_t frame,CameraValidation bestKind,uint32_t bestFrame){
 if(kind==CameraValidation::Invalid)return false;
 if(bestKind==CameraValidation::Invalid)return true;
 if(kind!=bestKind)return kind==CameraValidation::Valid;
 return int32_t(frame-bestFrame)>0;
}
inline bool cameraViewPreferred(const CameraFrame& frame,CameraValidation kind,const CameraFrame& best,CameraValidation bestKind){
 if(kind==CameraValidation::Invalid)return false;
 if(bestKind==CameraValidation::Invalid)return true;
 if(kind!=bestKind)return kind==CameraValidation::Valid;
 int rank=cameraSourceRank(frame.sourceType()),bestRank=cameraSourceRank(best.sourceType());
 return rank!=bestRank?rank>bestRank:int32_t(frame.frame()-best.frame())>0;
}
inline CameraCheck inspectCamera(const CameraFrame& c,float w,float h){
 const float*f=c.f.data();for(int i=0;i<32;i++)if(!std::isfinite(f[i])||std::abs(f[i])>1e7f)return {CameraValidation::Invalid,CameraReject::TransformValues,i,f[i]};
 // Offset 860 (slot 215) is the opaque int32 vp_auto_tbn, not a float.
 for(int i=160;i<224;i++)if(i!=215&&(!std::isfinite(f[i])||std::abs(f[i])>1e7f))return {CameraValidation::Invalid,CameraReject::CameraValues,i,f[i]};
 if(!cameraViewportMatches(f[216],f[217],w,h,c.sourceType()==5))return {CameraValidation::Invalid,CameraReject::Viewport};
 float norm=f[212]*f[212]+f[213]*f[213]+f[214]*f[214];
 if(norm<.94f||norm>1.06f)return {CameraValidation::Invalid,CameraReject::DirectionNorm};
 if(!c.frame())return {CameraValidation::Invalid,CameraReject::FrameIndex};
 if(f[211]<0)return {CameraValidation::Invalid,CameraReject::CameraTime};
 if(!std::isfinite(f[235])||f[235]>.05f)return {CameraValidation::Invalid,CameraReject::MapAmount};
 if(matrixInverseError(f,f+16)>=.025f)return {CameraValidation::Invalid,CameraReject::TransformInverse};
 if(matrixInverseError(f+176,f+192)>=.025f)return {CameraValidation::Invalid,CameraReject::ProjectionInverse};
 // A reflected matrix and its reflected inverse still multiply to identity. Check
 // the actual projection on the camera's forward ray, independent of world yaw.
 auto orientation=cameraOrientation(c);
 if(!std::isfinite(orientation.depth)||orientation.depth<=.1)return {CameraValidation::Invalid,CameraReject::ForwardDepth};
 // In the supported upright, downward view, XY retains positive NDC orientation
 // and world +Z moves toward NDC +Y (screen up). A double reflection keeps the
 // determinant's sign, so the independent world-up check is necessary too.
 if(orientation.category)return {CameraValidation::OrientationInvalid,CameraReject::Orientation};
 return {CameraValidation::Valid,CameraReject::None};
}
inline CameraValidation validateCamera(const CameraFrame& c,float w,float h){return inspectCamera(c,w,h).result;}
inline CameraReject cameraFreshness(uint32_t advance,float seconds){
 if(!advance)return CameraReject::FrameStatic;
 if(advance>2000)return CameraReject::FrameJump;
 if(seconds<0)return CameraReject::TimeReversed;
 if(seconds>5)return CameraReject::TimeJump;
 return CameraReject::None;
}
inline bool validCamera(const CameraFrame& c,float w,float h){return validateCamera(c,w,h)==CameraValidation::Valid;}
// Integer game coordinates are cell centers. Keep the complete projective mapping.
inline Grid gridFromCamera(const CameraFrame& c,float w,float h,float z,bool allowDirectionAdjustment=false){
 auto validation=validateCamera(c,w,h);
 Grid g;const auto&m=c.f;if(validation==CameraValidation::Invalid||
  (validation==CameraValidation::OrientationInvalid&&!allowDirectionAdjustment)||!std::isfinite(z)||std::abs(z)>10000||std::abs(m[214])<1e-6f)return g;
 double distance=(z-m[210])/m[214];double x0=std::round(m[208]+m[212]*distance),y0=std::round(m[209]+m[213]*distance);
 double d=m[12]*x0+m[13]*y0+m[14]*z+m[15];if(!std::isfinite(d)||d<.1)return g;
 double X=m[0]*x0+m[1]*y0+m[2]*z+m[3],Y=m[4]*x0+m[5]*y0+m[6]*z+m[7];
 g.origin={float((X/d+1)*w*.5),float((1-Y/d)*h*.5)};
 g.u={float(w*.5*(m[0]+m[12])/d),float(h*.5*(m[12]-m[4])/d)};
 g.v={float(w*.5*(m[1]+m[13])/d),float(h*.5*(m[13]-m[5])/d)};
 g.px=float(m[12]/d);g.py=float(m[13]/d);g.valid=true;g.confidence=1;
 if(!g.usable()||length(g.at(1,0)-g.origin)<3||length(g.at(0,1)-g.origin)<3)g.valid=false;
 return g;
}
}
