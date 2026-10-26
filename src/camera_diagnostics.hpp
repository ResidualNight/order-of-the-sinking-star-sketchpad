#pragma once
#include "camera_grid.hpp"
#include <cstdio>
#include <string>
#include <vector>
namespace sketch {
enum class CameraStage {Scan,Cache,Poll,Fresh,Probe,Count};
inline constexpr const char* CameraStageNames[]={"scan","cache","poll","fresh","viewport_probe"};
static_assert(sizeof(CameraStageNames)/sizeof(CameraStageNames[0])==size_t(CameraStage::Count));
struct CameraEvidence {
 CameraReject reason=CameraReject::None;CameraStage stage=CameraStage::Scan;
 float renderW=0,renderH=0,norm=0,time=0,map=0,inverse=0,projection=0,directionZ=0;
 double depth=0,handedness=0,worldUp=0;
 uint32_t frame=0,advance=0;uint8_t directionCategory=0;float seconds=0;int badIndex=-1;float badValue=0;unsigned long readError=0;
 bool hasCamera=false;
};
struct CameraDiagnostics {
 static constexpr size_t ReasonCount=size_t(CameraReject::Count),MaxSamples=6,ProbeBudget=64;
 std::array<std::array<size_t,ReasonCount>,size_t(CameraStage::Count)> counts{};
 std::array<CameraEvidence,MaxSamples> samples{};size_t sampleCount=0;
 size_t probeChecks=0,probeSkipped=0,probeThisScan=0;
 void beginScan(){probeThisScan=0;}
 static unsigned priority(CameraReject reason){
  switch(reason){case CameraReject::Viewport:return 100;case CameraReject::ReadFailure:return 95;
   case CameraReject::FrameStatic:case CameraReject::FrameJump:case CameraReject::TimeReversed:case CameraReject::TimeJump:return 90;
   case CameraReject::TransformInverse:case CameraReject::ProjectionInverse:return 80;
   case CameraReject::Orientation:case CameraReject::ForwardDepth:return 70;
   default:return unsigned(reason);}
 }
 void record(CameraStage stage,const CameraCheck& check,const CameraFrame* camera=nullptr,uint32_t advance=0,float seconds=0,unsigned long readError=0){
  ++counts[size_t(stage)][size_t(check.reason)];if(check.reason==CameraReject::None)return;
  // One representative per rejection cause, with a fixed total bound. Prefer
  // viewport/read/freshness/matrix evidence over incidental numeric stack noise.
  for(size_t i=0;i<sampleCount;++i)if(samples[i].reason==check.reason)return;
  size_t slot=sampleCount;
  if(sampleCount==MaxSamples){slot=0;for(size_t i=1;i<MaxSamples;++i)if(priority(samples[i].reason)<priority(samples[slot].reason))slot=i;
   if(priority(check.reason)<=priority(samples[slot].reason))return;
  }else ++sampleCount;
  CameraEvidence item;item.stage=stage;item.reason=check.reason;item.advance=advance;item.seconds=seconds;item.readError=readError;item.badIndex=check.badIndex;item.badValue=check.badValue;
  if(camera){item.hasCamera=true;const auto& f=camera->f;
   item.renderW=f[216]>0?1/f[216]:NAN;item.renderH=f[217]>0?1/f[217]:NAN;
   item.frame=camera->frame();item.time=f[211];item.map=f[235];item.directionZ=f[214];
   item.norm=f[212]*f[212]+f[213]*f[213]+f[214]*f[214];
   item.inverse=matrixInverseError(f.data(),f.data()+16);item.projection=matrixInverseError(f.data()+176,f.data()+192);
   auto orientation=cameraOrientation(*camera);item.depth=orientation.depth;item.handedness=orientation.handedness;item.worldUp=orientation.worldUp;item.directionCategory=orientation.category;
  }
  samples[slot]=item;
 }
 // This is a diagnostic prefilter only. It does not admit any runtime camera.
 static bool plausibleViewport(float inverseW,float inverseH){
  if(!std::isfinite(inverseW)||!std::isfinite(inverseH)||inverseW<=0||inverseH<=0)return false;
  float w=1/inverseW,h=1/inverseH;
  return std::isfinite(w)&&std::isfinite(h)&&w>=160&&h>=90&&w<=16385&&h<=16385&&std::abs(w-std::round(w))<.1f&&std::abs(h-std::round(h))<.1f;
 }
 void probeViewport(const CameraFrame& camera){
  const auto& f=camera.f;float norm=f[212]*f[212]+f[213]*f[213]+f[214]*f[214];
  if(!plausibleViewport(f[216],f[217])||!std::isfinite(norm)||norm<.5f||norm>1.5f||!camera.frame()||!std::isfinite(f[211])||f[211]<0)return;
  if(probeThisScan>=ProbeBudget){++probeSkipped;return;}++probeThisScan;++probeChecks;
  for(int i=0;i<32;++i)if(!std::isfinite(f[i])||std::abs(f[i])>1e7f)return;
  for(int i=160;i<224;++i)if(i!=215&&(!std::isfinite(f[i])||std::abs(f[i])>1e7f))return;
  // Loose inverse checks suppress arbitrary memory that happens to contain a
  // viewport pair. The .25 threshold is ONLY for logging, never localization.
  if(matrixInverseError(f.data(),f.data()+16)>=.25f||matrixInverseError(f.data()+176,f.data()+192)>=.25f)return;
  record(CameraStage::Probe,{CameraValidation::Invalid,CameraReject::Viewport},&camera);
 }
 std::vector<std::string> lines()const{
  std::vector<std::string> result;
  for(size_t stage=0;stage<size_t(CameraStage::Count);++stage){std::string line="checks stage=";line+=CameraStageNames[stage];
   for(size_t reason=0;reason<ReasonCount;++reason)if(counts[stage][reason]){line+=' ';line+=reason?CameraRejectNames[reason]:"accepted";line+='=';line+=std::to_string(counts[stage][reason]);}
   result.push_back(line);
  }
  result.push_back("viewport_probe checks="+std::to_string(probeChecks)+" skipped_budget="+std::to_string(probeSkipped)+" per_scan_limit="+std::to_string(ProbeBudget));
  result.push_back("limits abs_value<=1e7 output=640..16384x360..16384 render=160..16385x90..16385 viewport_tolerance_px=1 modes=native,half,720p,1080p,1440p,2160p direction_norm2=0.94..1.06 frame>0 time>=0 map<=0.05 inverse_error<0.025 forward_depth>0.1 direction_z<-0.05 handedness>0 world_up>0 direction_only=manual_preview normal_preferred=1 fresh_frames=1..2000 fresh_seconds=0..5");
  for(size_t i=0;i<sampleCount;++i){const auto&s=samples[i];char line[1000];
   std::snprintf(line,sizeof(line),"sample=%zu stage=%s reason=%s has_camera=%d render=%.6fx%.6f frame=%u time=%.9g direction_norm2=%.9g map=%.9g inverse_error=%.9g projection_error=%.9g forward_depth=%.9g direction_z=%.9g handedness=%.9g world_up=%.9g advance=%u seconds=%.9g bad_index=%d bad_value=%.9g read_error=0x%lx",i,CameraStageNames[size_t(s.stage)],CameraRejectNames[size_t(s.reason)],s.hasCamera?1:0,s.renderW,s.renderH,s.frame,s.time,s.norm,s.map,s.inverse,s.projection,s.depth,s.directionZ,s.handedness,s.worldUp,s.advance,s.seconds,s.badIndex,s.badValue,s.readError);
   result.emplace_back(std::string(line)+" direction_category="+std::to_string(s.directionCategory));
  }
  return result;
 }
};
}
