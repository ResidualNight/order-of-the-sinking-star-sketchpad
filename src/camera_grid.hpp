#pragma once
#include "core.hpp"
#include <array>
#include <cstring>
namespace sketch {
struct CameraFrame {
 std::array<float,240> f{};
 uint32_t frame() const {uint32_t n;std::memcpy(&n,f.data()+225,4);return n;}
};
enum class CameraValidation {Invalid,Valid,OrientationInvalid};
inline float matrixInverseError(const float*a,const float*b){float error=0;for(int row=0;row<4;row++)for(int col=0;col<4;col++){float value=0;for(int k=0;k<4;k++)value+=a[row*4+k]*b[k*4+col];if(!std::isfinite(value))return INFINITY;error=std::max(error,std::abs(value-(row==col?1.f:0.f)));}return error;}
inline CameraValidation validateCamera(const CameraFrame& c,float w,float h){
 const float*f=c.f.data();for(int i=0;i<32;i++)if(!std::isfinite(f[i])||std::abs(f[i])>1e7f)return CameraValidation::Invalid;
 for(int i=160;i<224;i++)if(!std::isfinite(f[i])||std::abs(f[i])>1e7f)return CameraValidation::Invalid;
 if(!std::isfinite(w)||!std::isfinite(h)||w<640||h<360||f[216]<=0||f[217]<=0||std::abs(1.f/f[216]-w)>1||std::abs(1.f/f[217]-h)>1)return CameraValidation::Invalid;
 float norm=f[212]*f[212]+f[213]*f[213]+f[214]*f[214];
 if(norm<.94f||norm>1.06f||!c.frame()||f[211]<0||!std::isfinite(f[235])||f[235]>.05f)return CameraValidation::Invalid;
 if(matrixInverseError(f,f+16)>=.025f||matrixInverseError(f+176,f+192)>=.025f)return CameraValidation::Invalid;
 // A reflected matrix and its reflected inverse still multiply to identity. Check
 // the actual projection on the camera's forward ray, independent of world yaw.
 double point[]={double(f[208])+10*f[212],double(f[209])+10*f[213],double(f[210])+10*f[214],1};
 double clip[4]{};for(int row=0;row<4;row++)for(int col=0;col<4;col++)clip[row]+=f[row*4+col]*point[col];
 if(!std::isfinite(clip[3])||clip[3]<=.1)return CameraValidation::Invalid;
 double x=clip[0]/clip[3],y=clip[1]/clip[3];
 double xx=f[0]-x*f[12],xy=f[1]-x*f[13],yx=f[4]-y*f[12],yy=f[5]-y*f[13];
 double handedness=xx*yy-xy*yx,worldUp=f[6]-y*f[14];
 // In the supported upright, downward view, XY retains positive NDC orientation
 // and world +Z moves toward NDC +Y (screen up). A double reflection keeps the
 // determinant's sign, so the independent world-up check is necessary too.
 if(f[214]>=-.05f||handedness<=0||worldUp<=0)return CameraValidation::OrientationInvalid;
 return CameraValidation::Valid;
}
inline bool validCamera(const CameraFrame& c,float w,float h){return validateCamera(c,w,h)==CameraValidation::Valid;}
// Integer game coordinates are cell centers. Keep the complete projective mapping.
inline Grid gridFromCamera(const CameraFrame& c,float w,float h,float z){
 Grid g;const auto&m=c.f;if(!validCamera(c,w,h)||!std::isfinite(z)||std::abs(z)>10000)return g;
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
