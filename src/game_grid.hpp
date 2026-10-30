#pragma once
#include "game_layout.hpp"
#include "camera_grid.hpp"
#include "camera_scan.hpp"
#include "camera_diagnostics.hpp"
#include "camera_picker.hpp"
#include "errors.hpp"
#include "level_watch.hpp"
#include <string>
#include <vector>
#include <memory>
namespace sketch {
enum class GameGridState {Waiting,Ready,Unavailable,Preview};
class GameGridReader {
 friend struct GameGridReaderTestAccess;
 friend struct GameGridWorkerTestAccess;
 friend class GameGridWorker;
 struct Candidate{uintptr_t address=0;uint32_t frame=0;float time=0;uint32_t sourceType=0;};
 struct Choice{CameraOption option;std::vector<Candidate> sources;};
 struct Observation{uintptr_t address=0;uint64_t round=0;std::unique_ptr<CameraFrame> frame,previous;};
 std::vector<Candidate> candidates;std::vector<std::array<float,3>> actors;
 std::vector<Choice> choices;
 std::vector<Observation> observations;std::unique_ptr<CameraFrame> capturedCamera;
 CameraValidation capturedKind=CameraValidation::Invalid;CameraStage capturedStage=CameraStage::Poll;
 uint64_t observationRound=0,capturedAt=0;size_t observationCursor=0,choiceSerial=0;
 size_t earlyCaptures=0,scanFresh=0,pollFresh=0,movedFresh=0,auxiliaryFresh=0;
 size_t projectionPreviewChecks=0,aspectPreviewChecks=0,previewGeometryRejected=0,previewHeightFallbacks=0;
 bool capturePrepared=false,auxiliarySeen=false,preferScene=true;
 SceneCameraReader sceneReader;SceneCameraPose scenePose;bool scenePoseValid=false;
 std::vector<uintptr_t> sceneSources;std::unique_ptr<CameraOption> sceneChoice;
 uint32_t sceneLatestFrame=0,scenePriorFrame=0;float sceneLatestTime=0,scenePriorTime=0;
 size_t sceneReads=0,sceneMatches=0,sceneFresh=0;
 LevelIdentity pickerLevel;bool picking=false;
 uintptr_t image=0,manager=0,managerRva=GameLayouts[0].managerRva;bool checked=false,supported=false,heightOverride=false,rejectedCameraOrientation=false;
 float width=0,height=0,forcedZ=1;uint64_t started=0,lastScan=0;
 CameraScanStats scanStats;
 uint64_t scanIntervalMs=250;bool scanOwnThread=true;
 CameraDiagnostics cameraDiagnostics;uint64_t requestSerial=0;bool levelValidated=false,planeSelected=false;
 std::string scanDiagnostic()const;
 bool readLevel();void scan();bool verifyBuild();
 void observeCamera(uintptr_t address,const CameraFrame& frame,CameraStage stage);
 void rememberCandidate(Candidate candidate);
 void captureCameras(bool early);bool localizationContext();
 void observeScene(uintptr_t address,const CameraFrame& frame);
 void offerScene();GameGridState previewResult();GameGridState recoverOrWait(ErrorId reason);
 bool pickerContext(float w,float h);float suggestedPlane(const CameraFrame&,bool& actorLayer)const;
 void offerChoice(uintptr_t address,const CameraFrame& frame);
 bool acceptsCamera(const CameraFrame& frame,CameraStage stage){
  auto check=inspectCamera(frame,width,height);cameraDiagnostics.record(stage,check,&frame);
  if(check.result==CameraValidation::Invalid){
   check=inspectCamera(frame,width,height,true,true);
   if(check.result!=CameraValidation::Invalid){
    if(matrixInverseError(frame.f.data()+176,frame.f.data()+192)>=.025f)++projectionPreviewChecks;
    if(!cameraViewportMatches(frame.f[216],frame.f[217],width,height,frame.sourceType()==5))++aspectPreviewChecks;
   }
  }
  if(check.result==CameraValidation::OrientationInvalid)rejectedCameraOrientation=true;
  return check.result!=CameraValidation::Invalid;
 }
 bool fail(ErrorId id){error=id;diagnostic=errorInfo(id).diagnostic;return false;}
public:
 static constexpr size_t PickMaxOptions=24,PickMaxSources=8,PickMaxCandidates=512;
 static constexpr size_t MaxObservations=512;
 static constexpr uint64_t CaptureLifetimeMs=250;
 static constexpr uint64_t PickCollectMs=1200;
 ErrorId error=ErrorId::None;
 std::string diagnostic;int columns=0,rows=0;float planeZ=1;
 uint8_t directionCategory=0;
 bool begin(float w,float h,bool overrideHeight,float z);
 void captureBeforeOverlay(float w,float h);
 bool readCurrentLevel(LevelIdentity& identity);
 ErrorId buildWarning()const{return checked&&!supported?ErrorId::UnsupportedGameBuild:ErrorId::None;}
 uint64_t requestId()const{return requestSerial;}
 std::vector<std::string> failureDetails()const;
 GameGridState poll(Grid& grid);
 bool beginPicking(float w,float h,float fallbackZ);
 GameGridState pollPicking();std::vector<CameraOption> pickerOptions()const;
 void cancelPicking(){picking=false;choices.clear();sceneChoice.reset();}
};
}
