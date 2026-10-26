#pragma once
#include "camera_diagnostics.hpp"
#include "camera_fixture.hpp"
#include <cassert>
#include <set>
namespace sketch {
inline void checkCameraDiagnostics(){
 auto good=gameCameraFixture();assert(inspectCamera(good,3840,2160).reason==CameraReject::None);
 // vp_auto_tbn at byte 860 is an int32, including bit patterns that resemble float NaNs.
 for(uint32_t bits:{0xffffffffu,0x7fc00000u,0x7f800000u}){
  auto typed=good;std::memcpy(typed.f.data()+215,&bits,4);assert(inspectCamera(typed,3840,2160).reason==CameraReject::None);
  assert(distance(gridFromCamera(typed,3840,2160,1).at(2,3),gridFromCamera(good,3840,2160,1).at(2,3))<.001f);
  typed.f[216]=1/1600.f;typed.f[217]=1/900.f;CameraDiagnostics probe;probe.probeViewport(typed);
  assert(probe.sampleCount==1&&probe.samples[0].reason==CameraReject::Viewport);
 }
 auto expect=[&](CameraFrame c,CameraReject expected){auto check=inspectCamera(c,3840,2160);
  assert(check.reason==expected&&check.result==(expected==CameraReject::Orientation?CameraValidation::OrientationInvalid:CameraValidation::Invalid));
  assert(validateCamera(c,3840,2160)==check.result);return check;
 };
 auto bad=good;bad.f[0]=NAN;auto check=expect(bad,CameraReject::TransformValues);assert(check.badIndex==0&&std::isnan(check.badValue));
 bad=good;bad.f[208]=1e8f;check=expect(bad,CameraReject::CameraValues);assert(check.badIndex==208&&check.badValue==1e8f);
 bad=good;bad.f[214]=NAN;check=expect(bad,CameraReject::CameraValues);assert(check.badIndex==214);
 bad=good;bad.f[216]=1/1600.f;bad.f[217]=1/900.f;expect(bad,CameraReject::Viewport);
 bad=good;bad.f[212]=2;expect(bad,CameraReject::DirectionNorm);
 bad=good;bad.f[225]=0;expect(bad,CameraReject::FrameIndex);
 bad=good;bad.f[211]=-1;expect(bad,CameraReject::CameraTime);
 bad=good;bad.f[235]=1;expect(bad,CameraReject::MapAmount);bad.f[235]=NAN;expect(bad,CameraReject::MapAmount);
 bad=good;bad.f[16]+=1;expect(bad,CameraReject::TransformInverse);
 bad=good;bad.f[176]+=1;expect(bad,CameraReject::ProjectionInverse);
 bad=good;bad.f[210]=1000;expect(bad,CameraReject::ForwardDepth);
 auto mirrored=good;for(int base:{0,192})for(int col=0;col<4;++col)mirrored.f[base+col]*=-1;
 for(int base:{16,176})for(int row=0;row<4;++row)mirrored.f[base+row*4]*=-1;
 expect(mirrored,CameraReject::Orientation);
 bad=good;bad.f[235]=-.25f;assert(validCamera(bad,3840,2160)); // Preserve the previous map check, including its lower bound.
 assert(cameraFreshness(0,0)==CameraReject::FrameStatic&&cameraFreshness(2001,0)==CameraReject::FrameJump);
 assert(cameraFreshness(1,-.01f)==CameraReject::TimeReversed&&cameraFreshness(1,5.01f)==CameraReject::TimeJump);
 assert(cameraFreshness(1,0)==CameraReject::None&&cameraFreshness(2000,5)==CameraReject::None);
 assert(cameraFreshness(uint32_t(1)-uint32_t(0xffffffff),.01f)==CameraReject::None); // Frame wrap.
 CameraDiagnostics stats;
 for(int repeat=0;repeat<100;++repeat)for(size_t reason=1;reason<CameraDiagnostics::ReasonCount;++reason)
  stats.record(CameraStage::Poll,{CameraValidation::Invalid,CameraReject(reason)},&good,12,.2f);
 assert(stats.sampleCount==CameraDiagnostics::MaxSamples);
 std::set<CameraReject> reasons;for(size_t i=0;i<stats.sampleCount;++i)assert(reasons.insert(stats.samples[i].reason).second);
 assert(reasons.count(CameraReject::Viewport)&&reasons.count(CameraReject::ReadFailure));
 auto lines=stats.lines();assert(lines.size()==size_t(CameraStage::Count)+2+CameraDiagnostics::MaxSamples);
 for(auto& line:lines)assert(line.size()<1400&&line.find('\n')==std::string::npos&&line.find("manager=")==std::string::npos);
 assert(lines[size_t(CameraStage::Poll)].find("transform_inverse=100")!=std::string::npos);
 assert(lines[size_t(CameraStage::Poll)].find("frame_static=100")!=std::string::npos);
 CameraDiagnostics probe;bad=good;bad.f[216]=1/1600.f;bad.f[217]=1/900.f;
 assert(!validCamera(bad,3840,2160));
 for(size_t i=0;i<CameraDiagnostics::ProbeBudget+3;++i)probe.probeViewport(bad);
 assert(probe.probeChecks==64&&probe.probeSkipped==3&&probe.counts[size_t(CameraStage::Probe)][size_t(CameraReject::Viewport)]==64);
 assert(probe.sampleCount==1&&probe.samples[0].renderW>1599&&probe.samples[0].renderW<1601);
 assert(!validCamera(bad,3840,2160)); // Probing never changes admission.
 probe.beginScan();probe.probeViewport(bad);assert(probe.probeChecks==65);
 bad.f[0]=NAN;probe.probeViewport(bad);assert(probe.sampleCount==1&&probe.counts[size_t(CameraStage::Probe)][size_t(CameraReject::Viewport)]==65);
 assert(!CameraDiagnostics::plausibleViewport(NAN,1/1080.f)&&!CameraDiagnostics::plausibleViewport(1,1));
 CameraDiagnostics read;read.record(CameraStage::Cache,{CameraValidation::Invalid,CameraReject::ReadFailure},nullptr,0,0,299);
 assert(!read.samples[0].hasCamera&&read.samples[0].readError==299);
 read={};assert(read.sampleCount==0&&read.counts[size_t(CameraStage::Cache)][size_t(CameraReject::ReadFailure)]==0);
 CameraDiagnostics boundary;float justOver=std::nextafter(5.f,INFINITY);
 boundary.record(CameraStage::Fresh,{CameraValidation::Invalid,cameraFreshness(1,justOver)},&good,1,justOver);
 assert(boundary.lines().back().find("seconds=5.00000048")!=std::string::npos); // Do not round a rejected value back to the accepted boundary.
}
}
