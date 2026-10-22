#pragma once
#include "core.hpp"
#include "grid.hpp"
#include "imgui.h"
#include <string>
namespace sketch {
class App {
public:
 Document document;Grid grid;Kind tool=Kind::Stroke;bool manualPerspective=true;int manualColumns=8,manualRows=7;
 bool gridFromGame=false,gridPlaneOverride=false;float gridPlaneZ=1;
 bool gridConfirmed=false;bool editing=false,gridEnabled=false,tint=false,needsGrid=false,manual=false,wantExit=false;
 float penWidth=3,eraseRadius=18,pinRadius=26,markOpacity=.78f,tintOpacity=.18f;
 uint32_t color=IM_COL32(240,204,109,255);std::string status;
 std::vector<Point> calibration;Mark current;bool drawing=false,erasing=false,eraseChanged=false;Snapshot eraseBefore;Point eraseLast;
 float oldWidth=0,oldHeight=0,uiScale=1;
 void frame(float width,float height);
 void acceptDetection(const Detection& result);
 void finishGesture();
 void cancelGesture(){drawing=false;current.points.clear();}
 void startManual(bool perspective=true){finishGesture();cancelGesture();manual=true;gridFromGame=false;needsGrid=false;manualPerspective=perspective;calibration.clear();gridEnabled=true;status=perspective?"先设列数与行数，再点四个外角：左上、右上、右下、左下":"依次点击：格子中心、横向相邻中心、纵向相邻中心";}
 void resize(float width,float height);
private:
 void panel();void canvas(float width,float height);void drawMark(ImDrawList* dl,const Mark&m,bool preview=false);void drawGrid(ImDrawList* dl,float width,float height);
};
}