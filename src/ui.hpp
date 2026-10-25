#pragma once
#include "core.hpp"
#include "grid.hpp"
#include "grid_refresh.hpp"
#include "errors.hpp"
#include "bindings.hpp"
#include "level_watch.hpp"
#include "imgui.h"
#include <string>
#include <set>
namespace sketch {
class App {
public:
 Document document;Grid grid;Kind tool=Kind::Stroke;bool manualPerspective=true;int manualColumns=8,manualRows=7;
 bool gridFromGame=false,gridPlaneOverride=false;float gridPlaneZ=1;
 bool gridConfirmed=false;bool editing=false,gridEnabled=false,tint=false,manual=false,wantExit=false;
 GridRefresh gridRefresh;
 LevelWatch levelWatch;bool autoClearOnLevel=false,levelPaused=false;
 float penWidth=3,eraseRadius=18,pinRadius=26,markOpacity=.78f,tintOpacity=.18f;
 std::array<uint32_t,5> colors{{IM_COL32(238,111,113,255),IM_COL32(255,255,255,255),IM_COL32(113,187,244,255),IM_COL32(128,214,163,255),IM_COL32(240,204,109,255)}};
 int colorIndex=0;uint32_t selectedColor()const{return colors[colorIndex];}
 Bindings bindings;bool settingsOpen=false;int capturingBinding=-1;int captureAfterFrame=0;bool inputSuppressed=false;
 ImGuiKey captureKey=ImGuiKey_None;ImGuiKeyChord captureMods=0;
 std::string bindingStatus,preferencesStatus="设置与键位会自动保存，重启游戏后恢复";ErrorId inputError=ErrorId::None;float numberRadius=26;ImFont* numberFont=nullptr;std::string status;
 std::vector<Point> calibration;Mark current;bool drawing=false,erasing=false,eraseChanged=false;Snapshot eraseBefore;Point eraseLast;
 bool numbering=false,numberConnected=false,numberChanged=false;Snapshot numberBefore;Point numberLast;std::set<std::pair<int,int>> numberedCells;
 float oldWidth=0,oldHeight=0,uiScale=1;
 Point panelMin{},panelMax{};
 void frame(float width,float height);
 void acceptDetection(const Detection& result);
 void rejectGameGrid(ErrorId error);
 bool observeLevel(const LevelIdentity* identity,uint64_t now);
 bool setEditing(bool enabled);
 void requestGrid(bool resized=false);
 void finishGesture();
 void cancelGesture(){drawing=false;current.points.clear();}
 void startManual(bool perspective=true){finishGesture();cancelGesture();manual=true;gridFromGame=false;gridRefresh.cancel();manualPerspective=perspective;calibration.clear();gridEnabled=true;status=perspective?"先设列数与行数，再点四个外角：左上、右上、右下、左下":"依次点击：格子中心、横向相邻中心、纵向相邻中心";}
 void resize(float width,float height);
private:
 void panel();void settingsPanel();void shortcuts();void numberCells(Point cell);void restartNumbers();void canvas(float width,float height);void drawMark(ImDrawList* dl,const Mark&m,bool preview=false);void drawGrid(ImDrawList* dl,float width,float height);
};
}
