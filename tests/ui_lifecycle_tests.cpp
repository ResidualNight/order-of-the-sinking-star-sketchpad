#include "ui.hpp"
#include <cassert>
#include <iostream>
#include <set>
using namespace sketch;
int main(){
 // A first opening and every reopening refresh, even with grid/tint hidden or a manual grid.
 App app;app.resize(3840,2160);assert(!app.gridRefresh.requested);
 Mark pin;pin.kind=Kind::Pin;pin.points={{960,540}};app.document.add(pin);
 app.grid.valid=true;app.grid.origin={200,300};app.gridConfirmed=true;app.gridFromGame=false;
 app.gridPlaneOverride=true;app.gridPlaneZ=1.75f;
 assert(app.setEditing(true));assert(app.gridRefresh.requested&&!app.gridConfirmed);
 assert(!app.gridEnabled&&!app.tint);assert(app.document.marks.size()==1&&app.document.nextPin==2);
 assert(app.gridPlaneOverride&&app.gridPlaneZ==1.75f);
 app.gridRefresh.finish();assert(!app.setEditing(true)&&!app.gridRefresh.requested);
 assert(app.setEditing(false));app.gridConfirmed=true;
 assert(app.setEditing(true)&&app.gridRefresh.requested&&!app.gridConfirmed);
 // Resize cancels an old in-flight request, rescales all mark history once and pauses snapping.
 app.gridRefresh.pending=true;app.gridConfirmed=true;
 app.resize(1920,1080);assert(app.gridRefresh.requested&&!app.gridRefresh.pending&&!app.gridConfirmed);
 assert(distance(app.document.marks[0].points[0],{480,270})<.001f);
 auto deadline=app.gridRefresh.notBefore;app.resize(1920,1080);assert(app.gridRefresh.notBefore==deadline);
 app.resize(0,0);app.resize(NAN,1080);assert(app.oldWidth==1920&&app.oldHeight==1080);
 app.gridRefresh.finish();app.setEditing(false);app.gridRefresh.finish();
 app.resize(2560,1440);assert(app.gridRefresh.requested);assert(distance(app.document.marks[0].points[0],{640,360})<.001f);
 assert(app.document.undo()&&app.document.marks.empty());assert(app.document.redo());
 assert(distance(app.document.marks[0].points[0],{640,360})<.001f);
 // Choosing manual calibration while automatic localization is pending wins immediately.
 app.gridRefresh.pending=true;app.startManual();assert(app.manual&&!app.gridRefresh.requested&&!app.gridRefresh.pending);
 // Continuous resizing coalesces; later requests cannot revive a previous viewport's result.
 GridRefresh refresh;refresh.request(true,100);assert(!refresh.ready(349)&&refresh.ready(350));
 refresh.pending=true;refresh.request(true,200);assert(!refresh.pending&&!refresh.ready(449));
 refresh.request(false,210);assert(!refresh.ready(449)&&refresh.ready(450));
 refresh.pending=true;refresh.cancel();assert(!refresh.ready(1000)&&!refresh.pending);
 refresh.request(false,1100);assert(refresh.ready(1100));refresh.finish();assert(!refresh.requested);
 // Feedback identifiers stay distinct; a failure must never leave an old automatic grid trusted.
 std::set<std::string> codes,diagnostics;
 for(unsigned i=1;i<static_cast<unsigned>(ErrorId::Count);++i){
  auto id=static_cast<ErrorId>(i);const auto& info=errorInfo(id);
  assert(codes.insert(info.code).second&&diagnostics.insert(info.diagnostic).second);
  assert(errorMessage(id).find(std::string("[")+info.code+"] ")==0);
 }
 assert(std::string(errorInfo(ErrorId::FreshCameraFrameUnavailable).code)=="SS-GRID-006");
 assert(std::string(errorInfo(ErrorId::CameraNotFound).code)=="SS-GRID-010");
 assert(std::string(errorInfo(ErrorId::CameraOrientationInvalid).code)=="SS-GRID-011");
 App failed;failed.resize(1920,1080);failed.document.add(pin);
 failed.grid=app.grid;failed.gridEnabled=true;failed.tint=true;
 failed.gridPlaneOverride=true;failed.gridPlaneZ=1.75f;
 for(unsigned i=static_cast<unsigned>(ErrorId::UnsupportedGameBuild);i<=static_cast<unsigned>(ErrorId::UnknownGameGrid);++i){
  auto id=static_cast<ErrorId>(i);failed.gridConfirmed=true;failed.gridFromGame=true;
  failed.gridRefresh.request();failed.gridRefresh.pending=true;auto origin=failed.grid.origin;
  failed.rejectGameGrid(id);
  assert(failed.status==errorMessage(id)&&!failed.gridRefresh.requested&&!failed.gridRefresh.pending);
  assert(!failed.gridConfirmed&&!failed.gridFromGame&&failed.gridEnabled&&failed.tint);
  assert(distance(origin,failed.grid.origin)<.001f&&failed.gridPlaneOverride&&failed.gridPlaneZ==1.75f);
  assert(failed.document.marks.size()==1&&failed.document.nextPin==2);
  failed.requestGrid();assert(failed.gridRefresh.requested&&failed.status.find("[SS-")==std::string::npos);
 }
 failed.rejectGameGrid(ErrorId::None);assert(failed.status==errorMessage(ErrorId::UnknownGameGrid));
 failed.rejectGameGrid(static_cast<ErrorId>(-1));assert(failed.status==errorMessage(ErrorId::UnknownGameGrid));
 failed.startManual();assert(failed.manual&&!failed.gridRefresh.requested&&failed.status.find("[SS-")==std::string::npos);
 assert(failed.document.undo()&&failed.document.marks.empty());assert(failed.document.redo());
 Detection invalid;failed.acceptDetection(invalid);assert(failed.status==errorMessage(ErrorId::ImageGridUnreliable));
 Detection candidate;candidate.grid=failed.grid;candidate.grid.confidence=.2f;
 failed.acceptDetection(candidate);assert(failed.status==errorMessage(ErrorId::ImageGridUnconfirmed)&&!failed.gridConfirmed);
 candidate.grid.confidence=1;failed.acceptDetection(candidate);assert(failed.gridConfirmed&&failed.status.find("[SS-")==std::string::npos);
 std::cout<<"PASS: first open, reopen with hidden/manual grid, resize during localization, mark history scaling, zero-size rejection, manual cancellation, resize debounce and coded failure/retry recovery\n";
}
