#pragma once
#include "camera_grid.hpp"
#include "camera_scan.hpp"
#include "camera_diagnostics.hpp"
#include "camera_picker.hpp"
#include "errors.hpp"
#include "level_watch.hpp"
#include <string>
#include <vector>
namespace sketch {
enum class GameGridState {Waiting,Ready,Unavailable};
class GameGridReader {
 friend struct GameGridReaderTestAccess;
 struct Candidate{uintptr_t address=0;uint32_t frame=0;float time=0;};
 struct Choice{CameraOption option;std::vector<Candidate> sources;};
 std::vector<Candidate> candidates;std::vector<std::array<float,3>> actors;
 std::vector<Choice> choices;
 LevelIdentity pickerLevel;bool picking=false;
 uintptr_t image=0,manager=0;bool checked=false,supported=false,heightOverride=false,rejectedCameraOrientation=false;
 float width=0,height=0,forcedZ=1;uint64_t started=0,lastScan=0;
 CameraScanStats scanStats;
 CameraDiagnostics cameraDiagnostics;uint64_t requestSerial=0;bool levelValidated=false,planeSelected=false;
 std::string scanDiagnostic()const;
 bool readLevel();void scan();bool verifyBuild();
 bool pickerContext(float w,float h);float suggestedPlane(const CameraFrame&,bool& actorLayer)const;
 void offerChoice(uintptr_t address,const CameraFrame& frame);
 bool acceptsCamera(const CameraFrame& frame,CameraStage stage){
  auto check=inspectCamera(frame,width,height);cameraDiagnostics.record(stage,check,&frame);
  if(check.result==CameraValidation::OrientationInvalid)rejectedCameraOrientation=true;
  return check.result!=CameraValidation::Invalid;
 }
 bool fail(ErrorId id){error=id;diagnostic=errorInfo(id).diagnostic;return false;}
public:
 static constexpr size_t PickMaxOptions=24,PickMaxSources=8,PickMaxCandidates=512;
 static constexpr uint64_t PickCollectMs=1200;
 ErrorId error=ErrorId::None;
 std::string diagnostic;int columns=0,rows=0;float planeZ=1;
 uint8_t directionCategory=0;
 bool begin(float w,float h,bool overrideHeight,float z);
 bool readCurrentLevel(LevelIdentity& identity);
 ErrorId buildWarning()const{return checked&&!supported?ErrorId::UnsupportedGameBuild:ErrorId::None;}
 uint64_t requestId()const{return requestSerial;}
 std::vector<std::string> failureDetails()const;
 GameGridState poll(Grid& grid);
 bool beginPicking(float w,float h,float fallbackZ);
 GameGridState pollPicking();std::vector<CameraOption> pickerOptions()const;
 void cancelPicking(){picking=false;choices.clear();}
};
}
