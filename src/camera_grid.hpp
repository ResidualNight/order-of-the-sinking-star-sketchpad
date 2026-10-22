#pragma once
#include "core.hpp"
#include <array>
#include <cstring>
namespace sketch {
struct CameraFrame {
 std::array<float,240> f{};
 uint32_t frame() const {uint32_t n;std::memcpy(&n,f.data()+225,4);return n;}
};
inline float matrixInverseError(const float*a,const float*b){float error=0;for(int row=0;row<4;row++)for(int col=0;col<4;col++){float value=0;for(int k=0;k<4;k++)value+=a[row*4+k]*b[k*4+col];if(!std::isfinite(value))return INFINITY;error=std::max(error,std::abs(value-(row==col?1.f:0.f)));}return error;}
inline bool validCamera(const CameraFrame& c,float w,float h){
 const float*f=c.f.data();for(int i=0;i<32;i++)if(!std::isfinite(f[i])||std::abs(f[i])>1e7f)return false;
 for(int i=160;i<224;i++)if(!std::isfinite(f[i])||std::abs(f[i])>1e7f)return false;
 if(w<640||h<360||f[216]<=0||f[217]<=0||std::abs(1.f/f[216]-w)>1||std::abs(1.f/f[217]-h)>1)return false;
 float norm=f[212]*f[212]+f[213]*f[213]+f[214]*f[214];
 if(norm<.94f||norm>1.06f||!c.frame()||f[211]<0||!std::isfinite(f[235])||f[235]>.05f)return false;
 return matrixInverseError(f,f+16)<.025f&&matrixInverseError(f+176,f+192)<.025f;
}
// Integer game coordinates are cell centers. Keep the complete projective mapping.
inline Grid gridFromCamera(const CameraFrame& c,float w,float h,float z){
 Grid g;const auto&m=c.f;if(!std::isfinite(z)||std::abs(z)>10000||std::abs(m[214])<.05f)return g;
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
