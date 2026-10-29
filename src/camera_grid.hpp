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
// Cross-aspect extents are ambiguous: admit only bounded common modes for
// manual previews. The strict automatic viewport rule above stays unchanged.
inline bool cameraViewportPreviewMatches(float iw,float ih,float w,float h,bool reflection=false){
 if(cameraViewportMatches(iw,ih,w,h,reflection))return true;
 if(!cameraViewportMatches(1/w,1/h,w,h)||!std::isfinite(iw)||!std::isfinite(ih)||iw<=0||ih<=0)return false;
 float cw=1/iw,ch=1/ih;if(cw<160||ch<90||cw>16384||ch>16384)return false;
 bool common=false;for(float ratio:{4.f/3,5.f/4,16.f/9,16.f/10,21.f/9,64.f/27,43.f/18,32.f/9})if(std::abs(cw-ch*ratio)<=1)common=true;
 if(!common)return false;
 for(float scale:{1.f,.5f,.25f}){
  if(std::abs(cw-w*scale)<=1||std::abs(ch-h*scale)<=1)return true;
  for(float base:{720.f,768.f,900.f,1024.f,1080.f,1200.f,1440.f,1600.f,2160.f})if((scale==1||reflection)&&std::abs(ch-base*scale)<=1)return true;
 }return false;
}
// Independent forward check for preview-only recovery: projection * view must
// reproduce the primary transform actually used to draw the grid.
inline double cameraCompositionError(const CameraFrame& c){
 const auto& f=c.f;double error=0;
 for(int row=0;row<4;++row)for(int col=0;col<4;++col){double value=0;
  for(int k=0;k<4;++k)value+=double(f[192+row*4+k])*f[160+k*4+col];
  if(!std::isfinite(value))return INFINITY;
  error=std::max(error,std::abs(value-f[row*4+col])/std::max(1.,std::abs(double(f[row*4+col]))));
 }return error;
}
struct CameraOrientation {double depth=0,handedness=NAN,worldUp=NAN;uint8_t category=0;};
inline CameraOrientation projectionOrientation(const float* f,const float* origin,const float* direction){
 CameraOrientation result;
 double point[]={double(origin[0])+10*direction[0],double(origin[1])+10*direction[1],double(origin[2])+10*direction[2],1},clip[4]{};
 for(int row=0;row<4;++row)for(int col=0;col<4;++col)clip[row]+=f[row*4+col]*point[col];
 result.depth=clip[3];
 if(std::isfinite(clip[3])&&clip[3]!=0){double x=clip[0]/clip[3],y=clip[1]/clip[3];
  result.handedness=(f[0]-x*f[12])*(f[5]-y*f[13])-(f[1]-x*f[13])*(f[4]-y*f[12]);result.worldUp=f[6]-y*f[14];
 }
 result.category=uint8_t((direction[2]>=-.05f?1:0)|(result.handedness<=0?2:0)|(result.worldUp<=0?4:0));return result;
}
inline CameraOrientation cameraOrientation(const CameraFrame& c){return projectionOrientation(c.f.data(),c.f.data()+208,c.f.data()+212);}
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
inline CameraCheck inspectCamera(const CameraFrame& c,float w,float h,bool allowProjectionPreview=false,bool allowAspectPreview=false){
 const float*f=c.f.data();for(int i=0;i<32;i++)if(!std::isfinite(f[i])||std::abs(f[i])>1e7f)return {CameraValidation::Invalid,CameraReject::TransformValues,i,f[i]};
 // Offset 860 (slot 215) is the opaque int32 vp_auto_tbn, not a float.
 for(int i=160;i<224;i++)if(i!=215&&(!std::isfinite(f[i])||std::abs(f[i])>1e7f))return {CameraValidation::Invalid,CameraReject::CameraValues,i,f[i]};
 if(!(allowAspectPreview?cameraViewportPreviewMatches(f[216],f[217],w,h,c.sourceType()==5):cameraViewportMatches(f[216],f[217],w,h,c.sourceType()==5)))return {CameraValidation::Invalid,CameraReject::Viewport};
 float norm=f[212]*f[212]+f[213]*f[213]+f[214]*f[214];
 if(norm<.94f||norm>1.06f)return {CameraValidation::Invalid,CameraReject::DirectionNorm};
 if(!c.frame())return {CameraValidation::Invalid,CameraReject::FrameIndex};
 if(f[211]<0)return {CameraValidation::Invalid,CameraReject::CameraTime};
 if(!std::isfinite(f[235])||f[235]>.05f)return {CameraValidation::Invalid,CameraReject::MapAmount};
 if(matrixInverseError(f,f+16)>=.025f)return {CameraValidation::Invalid,CameraReject::TransformInverse};
 float projectionError=matrixInverseError(f+176,f+192);
 if(projectionError>=.025f&&!(allowProjectionPreview&&c.sourceType()==8&&projectionError<=1.f&&cameraCompositionError(c)<.025))
  return {CameraValidation::Invalid,CameraReject::ProjectionInverse};
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
inline Grid gridFromProjection(const float* m,const float* origin,const float* direction,float w,float h,float z){
 Grid g;if(!std::isfinite(z)||std::abs(z)>10000||std::abs(direction[2])<1e-6f)return g;
 double distance=(z-origin[2])/direction[2];double x0=std::round(origin[0]+direction[0]*distance),y0=std::round(origin[1]+direction[1]*distance);
 double d=m[12]*x0+m[13]*y0+m[14]*z+m[15];if(!std::isfinite(d)||d<.1)return g;
 double X=m[0]*x0+m[1]*y0+m[2]*z+m[3],Y=m[4]*x0+m[5]*y0+m[6]*z+m[7];
 g.origin={float((X/d+1)*w*.5),float((1-Y/d)*h*.5)};
 g.u={float(w*.5*(m[0]+m[12])/d),float(h*.5*(m[12]-m[4])/d)};
 g.v={float(w*.5*(m[1]+m[13])/d),float(h*.5*(m[13]-m[5])/d)};
 g.px=float(m[12]/d);g.py=float(m[13]/d);g.valid=true;g.confidence=1;
 if(!g.usable()||length(g.at(1,0)-g.origin)<3||length(g.at(0,1)-g.origin)<3)g.valid=false;
 return g;
}
inline Grid gridFromCamera(const CameraFrame& c,float w,float h,float z,bool allowDirectionAdjustment=false,bool allowProjectionPreview=false,bool allowAspectPreview=false){
 auto validation=inspectCamera(c,w,h,allowProjectionPreview,allowAspectPreview).result;
 if(validation==CameraValidation::Invalid||(validation==CameraValidation::OrientationInvalid&&!allowDirectionAdjustment))return {};
 return gridFromProjection(c.f.data(),c.f.data()+208,c.f.data()+212,w,h,z);
}
}
