#pragma once
#include "camera_fixture.hpp"
// Synthetic analogue of the log's 960x540, direction-category-3 candidate.
// No user memory/matrices are stored: reflect the existing fixture across world
// Z and clip Y while keeping the transform and projection inverses consistent.
inline sketch::CameraFrame quarterCameraFixture(float outputWidth=3840,float outputHeight=2160){
 auto c=gameCameraFixture();
 for(int base:{0,192})for(int col=0;col<4;++col)c.f[base+4+col]*=-1;
 for(int base:{16,176})for(int row=0;row<4;++row)c.f[base+row*4+1]*=-1;
 for(int row=0;row<4;++row)c.f[row*4+2]*=-1;
 for(int col=0;col<4;++col)c.f[24+col]*=-1;
 c.f[210]*=-1;c.f[214]*=-1;
 float aspectScale=(outputHeight/outputWidth)/(2160.f/3840.f);
 for(int base:{0,192})for(int col=0;col<4;++col)c.f[base+col]*=aspectScale;
 for(int base:{16,176})for(int row=0;row<4;++row)c.f[base+row*4]/=aspectScale;
 c.f[216]=4/outputWidth;c.f[217]=4/outputHeight;
 return c;
}
