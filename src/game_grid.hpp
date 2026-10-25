#pragma once
#include "camera_grid.hpp"
#include "errors.hpp"
#include "level_watch.hpp"
#include <string>
#include <vector>
namespace sketch {
enum class GameGridState {Waiting,Ready,Unavailable};
class GameGridReader {
 struct Candidate{uintptr_t address=0;uint32_t frame=0;float time=0;};
 std::vector<Candidate> candidates;std::vector<std::array<float,3>> actors;
 uintptr_t image=0,manager=0;bool checked=false,supported=false,heightOverride=false,rejectedCameraOrientation=false;
 float width=0,height=0,forcedZ=1;uint64_t started=0,lastScan=0;
 bool readLevel();void scan();bool verifyBuild();
 bool acceptsCamera(const CameraFrame& frame){
  auto result=validateCamera(frame,width,height);
  if(result==CameraValidation::OrientationInvalid)rejectedCameraOrientation=true;
  return result==CameraValidation::Valid;
 }
 bool fail(ErrorId id){error=id;diagnostic=errorInfo(id).diagnostic;return false;}
public:
 ErrorId error=ErrorId::None;
 std::string diagnostic;int columns=0,rows=0;float planeZ=1;
 bool begin(float w,float h,bool overrideHeight,float z);
 bool readCurrentLevel(LevelIdentity& identity);
 GameGridState poll(Grid& grid);
};
}
