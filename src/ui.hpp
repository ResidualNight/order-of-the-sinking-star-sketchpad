#pragma once
#include "core.hpp"
#include "grid.hpp"
#include "grid_refresh.hpp"
#include "errors.hpp"
#include "bindings.hpp"
#include "level_watch.hpp"
#include "update_notice.hpp"
#include "camera_picker.hpp"
#include "feedback.hpp"
#include "imgui.h"
#include <string>
#include <set>
#include <memory>
namespace sketch {
class App {
public:
 Document document;Grid grid;Kind tool=Kind::Stroke;
 bool gridFromGame=false,gridPlaneOverride=false;float gridPlaneZ=1;
 // Only the user's flip choice persists, separately for each raw direction
 // category. A normal camera must not inherit a reflected camera's correction.
 std::array<int,8> gridDirections{{0,-1,-1,-1,-1,-1,-1,-1}};
 Grid gameGridBase;int gridDirectionCategory=0,gridDirection=0;
 float gridOutputWidth=0,gridOutputHeight=0;bool gridDirectionPending=false;
 bool gridConfirmed=false,gridPinned=false;bool editing=false,gridEnabled=false,tint=false,cameraPickerOpen=false,wantExit=false;
 Grid pickerPreviousGrid;bool pickerPreviousPinned=false;uint64_t cameraPreviewAdoptions=0;
 GridRefresh gridRefresh;
 bool gridPreferScene=true; // One request only; never persisted.
 bool gridRecoveryPending=false,gridRecoveryScene=false;
 std::unique_ptr<CameraOption> recoveryOption;
 int recoveryAspect=0,recoveryFit=0,pickerAspect=0,pickerFit=0;
 void updateRecoveryMapping();
 bool adoptRecoveryGrid();
 uint64_t availableUpdate=0;
 ErrorId gameBuildWarning=ErrorId::None;
 LevelWatch levelWatch;bool autoClearOnLevel=false,levelPaused=false;
 float penWidth=3,eraseRadius=18,pinRadius=26,markOpacity=.78f,tintOpacity=.18f;
 std::array<uint32_t,5> colors{{IM_COL32(238,111,113,255),IM_COL32(255,255,255,255),IM_COL32(113,187,244,255),IM_COL32(128,214,163,255),IM_COL32(240,204,109,255)}};
 int colorIndex=0;uint32_t selectedColor()const{return colors[colorIndex];}
 Bindings bindings;bool settingsOpen=false;int capturingBinding=-1;int captureAfterFrame=0;bool inputSuppressed=false;
 ImGuiKey captureKey=ImGuiKey_None;ImGuiKeyChord captureMods=0;
 std::string bindingStatus,preferencesStatus="设置与键位会自动保存，重启游戏后恢复";ErrorId inputError=ErrorId::None;float numberRadius=26;ImFont* numberFont=nullptr;std::string status;
 std::vector<CameraOption> cameraOptions;int selectedCamera=-1,pickerFlip=0,pickerPlaceFrames=0;
 bool reportOpen=false,reportRequested=false,reportCopyRequested=false,reportBusy=false;std::string reportError,reportFeedback;
 ReportDiagnostics reportContext;
 bool pickerRefreshRequested=false,pickerCollecting=false,pickerHeightOverride=false;
 float pickerWidth=0,pickerHeight=0,pickerZ=1;
 Mark current;bool drawing=false,erasing=false,eraseChanged=false;Snapshot eraseBefore;Point eraseLast;
 bool numbering=false,numberConnected=false,numberChanged=false;Snapshot numberBefore;Point numberLast;std::set<std::pair<int,int>> numberedCells;
 float oldWidth=0,oldHeight=0,uiScale=1;
 Point panelMin{},panelMax{};
 void frame(float width,float height);
 void acceptDetection(const Detection& result);
 void acceptGameGrid(const Grid& result,int directionCategory,float width,float height,float planeZ);
 void adjustGridDirection(int direction);
 void confirmGridDirection();
 void rejectGameGrid(ErrorId error);
 bool observeLevel(const LevelIdentity* identity,uint64_t now);
 bool setEditing(bool enabled);
 void requestGrid(bool resized=false);
 void requestOriginalGrid(){requestGrid();gridPreferScene=false;gridEnabled=true;}
 void finishGesture();
 void cancelGesture(){drawing=false;current.points.clear();}
 void startCameraPicker();void cancelCameraPicker(bool restorePrevious=true);
 bool snappingReady()const{return gridEnabled&&gridConfirmed&&!levelPaused&&!cameraPickerOpen&&!reportOpen&&grid.usable();}
 void acceptCameraOptions(const std::vector<CameraOption>& options,float width,float height);
 void acceptCameraRecoveryOptions(const std::vector<CameraOption>& options,float width,float height);
 void selectCamera(int index);Grid cameraPickerPreview()const;const CameraOption* selectedCameraOption()const;
 bool adoptCameraPreview();void failCameraPicker(ErrorId error);
 void resize(float width,float height);
 bool hasReportableError()const;
 void openErrorReport();void checkReportCamera();
 ReportDiagnostics reportDiagnostics()const;
private:
 void reportButton();void errorReportPanel(float width,float height);
 void panel();void cameraPickerPanel(float width,float height);void settingsPanel();void shortcuts();void numberCells(Point cell);void restartNumbers();void canvas(float width,float height);void drawMark(ImDrawList* dl,const Mark&m,bool preview=false);void drawGrid(ImDrawList* dl,float width,float height,const Grid* preview=nullptr);
};
}
