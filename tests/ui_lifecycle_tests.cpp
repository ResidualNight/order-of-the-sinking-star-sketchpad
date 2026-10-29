#include "ui.hpp"
#include "camera_fixture.hpp"
#include "imgui_internal.h"
#include <cassert>
#include <iostream>
#include <set>
using namespace sketch;
void checkPreferences();
void scenePosePreviewInteraction();
namespace {
void frame(App& app,float width=1920,float height=1080){auto&io=ImGui::GetIO();io.DisplaySize={width,height};io.DeltaTime=1.f/60;ImGui::NewFrame();app.frame(width,height);ImGui::Render();if(app.wantExit){app.wantExit=false;app.setEditing(false);}}
void key(App& app,ImGuiKey k,ImGuiKeyChord mods=0){
 auto&io=ImGui::GetIO();io.AddKeyEvent(ImGuiMod_Ctrl,(mods&ImGuiMod_Ctrl)!=0);io.AddKeyEvent(ImGuiMod_Alt,(mods&ImGuiMod_Alt)!=0);io.AddKeyEvent(ImGuiMod_Shift,(mods&ImGuiMod_Shift)!=0);io.AddKeyEvent(k,true);frame(app);
 io.AddKeyEvent(k,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);io.AddKeyEvent(ImGuiMod_Alt,false);io.AddKeyEvent(ImGuiMod_Shift,false);frame(app);frame(app);
}
void click(App& app,Point p,int button=0){auto&io=ImGui::GetIO();io.AddMousePosEvent(p.x,p.y);frame(app);io.AddMouseButtonEvent(button,true);frame(app);io.AddMouseButtonEvent(button,false);frame(app);}
void levelInteractions(){
 App app;assert(!app.autoClearOnLevel);app.resize(1920,1080);Mark number;number.kind=Kind::Number;number.points={{700,500}};app.document.add(number);
 LevelIdentity a{11,12,9,{-1,2,-1},"first"},b=a;b.name="second";
 assert(!app.observeLevel(&a,0)&&!app.observeLevel(&a,200)&&!app.observeLevel(&a,400)&&!app.observeLevel(&a,600));assert(app.document.marks.size()==1);
 app.gridConfirmed=true;app.gridFromGame=true;app.gridRefresh.pending=true;app.gridRefresh.requested=true;
 assert(!app.observeLevel(&b,800)&&app.levelPaused&&!app.gridConfirmed&&!app.gridRefresh.pending&&!app.gridRefresh.requested);
 assert(!app.observeLevel(&a,1000)&&!app.levelPaused&&app.gridRefresh.requested&&app.document.marks.size()==1);
 app.gridRefresh.finish();assert(!app.observeLevel(&b,1200)&&!app.observeLevel(&b,1400)&&!app.observeLevel(&b,1600));
 assert(app.observeLevel(&b,1800)&&app.document.marks.size()==1&&app.document.nextNumber==2); // Default off.
 app.autoClearOnLevel=true;assert(!app.observeLevel(&b,2000)&&app.document.marks.size()==1); // Enabling never clears retroactively.
 app.gridEnabled=true;app.tint=true;app.colorIndex=3;app.penWidth=6;app.drawing=true;app.current=number;app.current.kind=Kind::Stroke;
 assert(!app.observeLevel(&a,2200)&&!app.drawing);assert(!app.observeLevel(&a,2400)&&!app.observeLevel(&a,2600));
 assert(app.observeLevel(&a,2800)&&app.document.marks.empty()&&app.document.nextNumber==1&&app.gridRefresh.requested);
 assert(app.gridEnabled&&app.tint&&app.colorIndex==3&&app.penWidth==6);assert(app.document.undo()&&app.document.marks.size()==1&&app.document.nextNumber==2);
 assert(!app.observeLevel(nullptr,3000)&&app.document.marks.size()==1);assert(!app.observeLevel(&a,3200)&&app.document.marks.size()==1);
 app.setEditing(true);app.resize(1600,900);app.startCameraPicker();assert(!app.observeLevel(&a,3400)&&app.document.marks.size()==1); // View/UI/calibration are not identity changes.
 assert(!app.observeLevel(&b,3600)&&!app.observeLevel(&a,3800)&&app.document.marks.size()==1); // Short candidate does not clear.
}
void numberingInteractions(){
 auto&io=ImGui::GetIO();App app;frame(app);app.setEditing(true);app.gridRefresh.cancel();app.tool=Kind::Number;
 app.grid.valid=true;app.grid.origin={600,300};app.grid.u={60,0};app.grid.v={0,60};app.gridEnabled=app.gridConfirmed=true;
 auto move=[&](Point p){io.AddMousePosEvent(p.x,p.y);frame(app);};
 auto down=[&](Point p){move(p);io.AddMouseButtonEvent(0,true);frame(app);};
 auto up=[&](){io.AddMouseButtonEvent(0,false);frame(app);};
 down({600,300});frame(app);assert(app.document.marks.size()==1&&app.numbering);
 move({840,300});assert(app.document.marks.size()==5);move({600,300});move({840,300});assert(app.document.marks.size()==5);
 move({840,420});assert(app.document.marks.size()==7);up();assert(!app.numbering&&app.document.undoStack.size()==1);
 for(int i=0;i<7;i++)assert(app.document.marks[i].number==i+1);
 for(int i=0;i<5;i++)assert(distance(app.document.marks[i].points[0],{600.f+60*i,300})<.01f);
 assert(app.document.nextNumber==8);key(app,ImGuiKey_Z,ImGuiMod_Ctrl);assert(app.document.marks.empty()&&app.document.nextNumber==1);
 key(app,ImGuiKey_Y,ImGuiMod_Ctrl);assert(app.document.marks.size()==7&&app.document.nextNumber==8);
 // Both key orders consume their component shortcuts, including C released last.
 io.AddKeyEvent(ImGuiKey_N,true);frame(app);io.AddKeyEvent(ImGuiKey_C,true);frame(app);
 auto history=app.document.undoStack.size();frame(app);frame(app);assert(app.document.marks.size()==7&&app.document.nextNumber==1&&app.document.undoStack.size()==history);
 io.AddKeyEvent(ImGuiKey_N,false);frame(app);io.AddKeyEvent(ImGuiKey_C,false);frame(app);frame(app);assert(app.document.marks.size()==7);
 key(app,ImGuiKey_Z,ImGuiMod_Ctrl);assert(app.document.nextNumber==8&&app.document.marks.size()==7);
 key(app,ImGuiKey_Y,ImGuiMod_Ctrl);down({600,480});move({720,480});up();
 assert(app.document.marks.size()==10&&app.document.marks[7].number==1&&app.document.marks[9].number==3);
 key(app,ImGuiKey_Z,ImGuiMod_Ctrl);assert(app.document.marks.size()==7&&app.document.nextNumber==1);
 key(app,ImGuiKey_Z,ImGuiMod_Ctrl);assert(app.document.marks.size()==7&&app.document.nextNumber==8);
 key(app,ImGuiKey_Y,ImGuiMod_Ctrl);key(app,ImGuiKey_Y,ImGuiMod_Ctrl);
 io.AddKeyEvent(ImGuiKey_C,true);frame(app);assert(app.document.marks.size()==10);io.AddKeyEvent(ImGuiKey_N,true);frame(app);
 assert(app.document.marks.size()==10&&app.document.nextNumber==1);
 io.AddMouseButtonEvent(0,true);frame(app);assert(app.document.marks.size()==10);io.AddMouseButtonEvent(0,false);frame(app);
 io.AddKeyEvent(ImGuiKey_C,false);frame(app);io.AddKeyEvent(ImGuiKey_N,false);frame(app);frame(app);assert(app.document.marks.size()==10);
 app.tool=Kind::Stroke;key(app,ImGuiKey_N);assert(app.tool==Kind::Number);key(app,ImGuiKey_C);assert(app.document.marks.empty());
 key(app,ImGuiKey_Z,ImGuiMod_Ctrl);assert(app.document.marks.size()==10);
 // Single-key placement stays single-key placement when snapping is off or not trusted.
 app.gridEnabled=false;down({800,700});move({1000,700});up();assert(app.document.marks.size()==11&&distance(app.document.marks.back().points[0],{800,700})<.01f);
 app.gridEnabled=true;app.gridConfirmed=false;down({800,800});move({1000,800});up();assert(app.document.marks.size()==12);
 app.gridConfirmed=true;app.grid.valid=false;down({900,850});move({1000,850});up();assert(app.document.marks.size()==13);
 app.grid.valid=true;app.document.clear();down({600,540});move({50,100});move({1080,540});move({-30,600});move({1200,540});up();assert(app.document.marks.size()==3);
 key(app,ImGuiKey_Z,ImGuiMod_Ctrl);assert(app.document.marks.empty());
 down({960,600});move({1140,780});up();assert(app.document.marks.size()==4);
 for(int i=0;i<4;i++)assert(distance(app.document.marks[i].points[0],{960.f+60*i,600.f+60*i})<.01f);
 app.document.clear();down({600,600});move({720,600});app.requestGrid();assert(!app.numbering&&app.document.marks.size()==3);
 move({900,600});app.gridConfirmed=true;move({1020,600});up();assert(app.document.marks.size()==3);
 key(app,ImGuiKey_Z,ImGuiMod_Ctrl);assert(app.document.marks.empty());
 // Perspective traversal uses grid cells rather than a screen-space distance threshold.
 app.grid=Grid::fromQuad({{600,300},{1200,300},{1350,850},{500,850}},8,7);assert(app.grid.usable());app.gridConfirmed=true;
 down(app.grid.at(1,2));move(app.grid.at(6,2));up();assert(app.document.marks.size()==6);
 for(int i=0;i<6;i++)assert(distance(app.document.marks[i].points[0],app.grid.at(float(i+1),2))<.02f);
 // Capturing two ordinary keys retains conflict handling and gives N+C a remappable action.
 app.settingsOpen=true;app.capturingBinding=int(Action::RestartNumber);app.captureKey=ImGuiKey_None;app.captureAfterFrame=ImGui::GetFrameCount();
 io.AddKeyEvent(ImGuiKey_R,true);frame(app);assert(app.capturingBinding>=0);io.AddKeyEvent(ImGuiKey_V,true);frame(app);
 assert(app.capturingBinding==-1&&app.bindings.get(Action::RestartNumber)==(KeyBinding{ImGuiKey_V,0,ImGuiKey_R}));
 assert(app.bindings.label(Action::RestartNumber)=="R+V");assert(!app.bindings.assign(Action::Pen,{ImGuiKey_R,0,ImGuiKey_V}));
 assert(!app.bindings.assign(Action::Erase,{ImGuiKey_R}));assert(!Bindings::supported(Action::RestartNumber,{ImGuiKey_R,0,ImGuiKey_R}));
 io.AddKeyEvent(ImGuiKey_R,false);io.AddKeyEvent(ImGuiKey_V,false);frame(app);key(app,ImGuiKey_Escape);
 auto count=app.document.marks.size();io.AddKeyEvent(ImGuiKey_R,true);frame(app);io.AddKeyEvent(ImGuiKey_V,true);frame(app);
 assert(app.document.nextNumber==1&&app.document.marks.size()==count);io.AddKeyEvent(ImGuiKey_R,false);io.AddKeyEvent(ImGuiKey_V,false);frame(app);frame(app);
 // Native polling can deliver both ordinary keys in a single frame.
 app.settingsOpen=true;app.capturingBinding=int(Action::RestartNumber);app.captureKey=ImGuiKey_None;app.captureAfterFrame=ImGui::GetFrameCount();
 io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_U,true);io.AddKeyEvent(ImGuiKey_J,true);frame(app);
 assert(app.capturingBinding==-1&&app.bindings.get(Action::RestartNumber)==(KeyBinding{ImGuiKey_U,ImGuiMod_Ctrl,ImGuiKey_J}));
 io.AddKeyEvent(ImGuiKey_U,false);io.AddKeyEvent(ImGuiKey_J,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);frame(app);key(app,ImGuiKey_Escape);
 app.bindings.reset();app.gridEnabled=false;click(app,{1450,700});count=app.document.marks.size();assert(app.document.nextNumber==2);
 io.AddKeyEvent(ImGuiKey_C,true);io.AddKeyEvent(ImGuiKey_N,true);frame(app);assert(app.document.nextNumber==1&&app.document.marks.size()==count);
 io.AddKeyEvent(ImGuiKey_C,false);io.AddKeyEvent(ImGuiKey_N,false);frame(app);frame(app);assert(app.document.marks.size()==count);
 app.setEditing(false);
}
void directionInteractions(){
 App app;app.resize(1920,1080);app.setEditing(true);app.tint=true;
 Grid expected=Grid::fromQuad({{600,280},{1300,320},{1400,920},{500,860}},8,7);
 Grid reflected=expected.reflected(1920,1080,false,true);
 app.acceptGameGrid(reflected,6,1920,1080,1);
 assert(app.gridDirectionPending&&!app.gridConfirmed&&app.gridFromGame&&app.gridEnabled&&app.status.rfind("[SS-",0)!=0&&!app.hasReportableError());
 assert(app.gridDirections[6]==-1&&!app.gridRefresh.requested);
 // A preview is visible but tools still place at the cursor until confirmed.
 app.tool=Kind::Pin;Point cursor{1123,743};click(app,cursor);
 assert(app.document.marks.size()==1&&distance(app.document.marks[0].points[0],cursor)<.001f);
 app.adjustGridDirection(2);assert(app.gridDirectionPending&&!app.gridConfirmed&&app.gridDirections[6]==-1);
 for(int x=0;x<8;++x)for(int y=0;y<7;++y)assert(distance(app.grid.at(float(x),float(y)),expected.at(float(x),float(y)))<.01f);
 app.confirmGridDirection();assert(app.gridConfirmed&&!app.gridDirectionPending&&app.gridDirections[6]==2);
 assert(distance(app.document.marks[0].points[0],cursor)<.001f);
 frame(app);click(app,cursor);assert(distance(app.document.marks.back().points[0],expected.snap(cursor))<.01f);
 app.requestGrid();assert(!app.gridFromGame&&!app.gridDirectionPending&&!app.gridConfirmed&&app.gridDirections[6]==2);
 app.adjustGridDirection(1);assert(!app.gridConfirmed); // An old base cannot revive after refresh.
 app.acceptGameGrid(reflected,6,1920,1080,1);assert(app.gridConfirmed&&app.gridDirection==2&&!app.gridDirectionPending);
 auto point=app.grid.at(3,4);assert(distance(point,expected.at(3,4))<.01f);
 // Preferences describe a direction, not screen dimensions or grid geometry.
 Grid larger=Grid::fromQuad({{800,373.3333f},{1733.3333f,426.6667f},{1866.6667f,1226.6667f},{666.6667f,1146.6667f}},8,7);
 app.acceptGameGrid(larger.reflected(2560,1440,false,true),6,2560,1440,1);
 assert(app.gridConfirmed&&distance(app.grid.at(3,4),larger.at(3,4))<.01f);
 app.acceptGameGrid(expected,0,1920,1080,1);assert(app.gridConfirmed&&app.gridDirection==0&&distance(app.grid.at(3,4),point)<.01f);
 app.acceptGameGrid(expected,4,1920,1080,1);assert(!app.gridConfirmed&&app.gridDirectionPending);
 app.confirmGridDirection();assert(app.gridDirections[4]==0); // Remember explicitly choosing the original orientation.
 app.adjustGridDirection(3);assert(!app.gridConfirmed&&app.gridDirectionPending&&app.gridDirections[4]==0);
 app.startCameraPicker();assert(!app.gridDirectionPending&&!app.gridFromGame&&!app.gridConfirmed);
 app.confirmGridDirection();assert(!app.gridConfirmed&&app.gridDirections[4]==0);
 app.acceptGameGrid(reflected,6,1920,1080,1);app.adjustGridDirection(0);app.rejectGameGrid(ErrorId::CameraNotFound);
 assert(!app.gridDirectionPending&&!app.gridFromGame&&!app.gridConfirmed&&app.gridDirections[6]==2);
 app.acceptGameGrid(Grid{},6,1920,1080,1);assert(!app.gridConfirmed&&app.status==errorMessage(ErrorId::ProjectedGridInvalid));
}
void pickerInteractions(){
 App app;app.resize(1920,1080);app.setEditing(true);app.tint=true;
 Mark mark;mark.kind=Kind::Pin;mark.points={{1500,900}};app.document.add(mark);app.gridDirections[6]=1;app.gridPlaneOverride=true;app.gridPlaneZ=7;auto directions=app.gridDirections;
 app.startCameraPicker();assert(app.cameraPickerOpen&&app.pickerCollecting&&app.pickerRefreshRequested&&!app.gridConfirmed&&!app.gridRefresh.requested);
 click(app,{1100,700});assert(app.document.marks.size()==1&&!app.adoptCameraPreview());
 std::vector<CameraOption> options(2);options[0].token=1;options[0].camera=gameCameraFixture();options[0].copies=2;options[0].suggestedZ=1;
 options[1]=options[0];options[1].token=2;
 for(int base:{0,192})for(int col=0;col<4;++col)options[1].camera.f[base+4+col]*=-1;
 for(int base:{16,176})for(int row=0;row<4;++row)options[1].camera.f[base+row*4+1]*=-1;
 options[1].directionCategory=6;
 uint32_t auxiliaryType=5;std::memcpy(options[1].camera.f.data()+226,&auxiliaryType,4);
 app.acceptCameraOptions(options,1920,1080);assert(app.cameraOptions.size()==2&&app.selectedCamera==0&&!app.pickerCollecting&&!app.gridConfirmed);
 auto normal=app.cameraPickerPreview();assert(normal.usable());app.selectCamera(1);frame(app);app.pickerFlip=2;
 auto corrected=app.cameraPickerPreview();assert(distance(normal.at(2,3),corrected.at(2,3))<.01f);
 app.pickerHeightOverride=true;app.pickerZ=1.75f;assert(app.cameraPickerPreview().usable()&&distance(app.cameraPickerPreview().origin,normal.origin)>1);
 auto chosen=app.cameraPickerPreview();options[1].camera.f[0]=NAN; // Original source/copy may already be overwritten.
 app.failCameraPicker(ErrorId::CameraChoiceExpired);assert(app.status.find("[SS-GRID-012]")==0);
 assert(app.adoptCameraPreview()); // Explicit confirmation uses exactly the preview, without a live source.
 assert(app.gridConfirmed&&app.gridPinned&&!app.cameraPickerOpen&&!app.gridFromGame&&app.cameraOptions.empty()&&app.gridDirections==directions&&app.gridPlaneOverride&&app.gridPlaneZ==7);
 assert(app.status.find("[SS-")==std::string::npos&&distance(app.document.marks[0].points[0],mark.points[0])<.001f);
 for(int x=-2;x<=4;++x)for(int y=-2;y<=4;++y)assert(distance(app.grid.at(float(x),float(y)),chosen.at(float(x),float(y)))<.001f);
 app.setEditing(false);app.setEditing(true);assert(app.gridPinned&&app.gridConfirmed&&!app.gridRefresh.requested);
 assert(distance(app.grid.at(2,3),chosen.at(2,3))<.001f);frame(app);
 app.tool=Kind::Pin;Point cursor{1103,743};click(app,cursor);assert(app.document.marks.size()==2&&distance(app.document.marks.back().points[0],chosen.snap(cursor))<.01f);
 LevelIdentity first{21,8,7,{0,0,0},"first"},second=first;second.name="second";
 for(uint64_t time:{0,200,400,600})app.observeLevel(&first,time);
 app.observeLevel(nullptr,800);assert(app.gridPinned&&!app.gridConfirmed&&app.levelPaused);
 app.observeLevel(&first,1000);assert(app.gridPinned&&app.gridConfirmed&&!app.levelPaused&&!app.gridRefresh.requested);
 for(uint64_t time:{1200,1400,1600,1800})app.observeLevel(&second,time);
 assert(!app.gridPinned&&!app.gridConfirmed&&app.gridRefresh.requested&&app.document.marks.size()==2);
 options[1]=options[0];
 app.startCameraPicker();app.acceptCameraOptions(options,1920,1080);app.failCameraPicker(ErrorId::CameraChoiceExpired);
 assert(app.cameraPickerOpen&&app.cameraPickerPreview().usable());
 app.startCameraPicker();assert(app.cameraOptions.empty()&&app.pickerCollecting);
 app.acceptCameraOptions({},1920,1080);assert(!app.adoptCameraPreview()&&app.status.find("[SS-GRID-010]")==0);
 frame(app);key(app,ImGuiKey_Escape);assert(!app.cameraPickerOpen&&app.editing&&app.gridDirections==directions&&app.document.marks.size()==2);
 app.startCameraPicker();app.acceptCameraOptions(options,1920,1080);app.resize(1280,720);assert(!app.cameraPickerOpen&&app.cameraOptions.empty()&&app.gridRefresh.requested&&!app.gridConfirmed);
 app.startCameraPicker();app.setEditing(false);assert(!app.cameraPickerOpen&&app.cameraOptions.empty());
}
void recoveryPreviewInteractions(){
 App app;app.resize(1920,1080);app.setEditing(true);app.tint=true;app.requestGrid();
 Mark mark;mark.kind=Kind::Pin;mark.points={{1500,900}};app.document.add(mark);auto directions=app.gridDirections;
 std::vector<CameraOption> options(1);options[0].camera=gameCameraFixture();options[0].suggestedZ=1;
 std::fill(options[0].camera.f.begin()+176,options[0].camera.f.begin()+192,0.f);options[0].projectionUnverified=true;
 app.acceptCameraRecoveryOptions(options,1920,1080);frame(app);
 assert(!app.cameraPickerOpen&&!app.pickerCollecting&&!app.pickerRefreshRequested&&!app.gridRefresh.requested&&app.gridRecoveryPending);
 assert(!app.gridConfirmed&&!app.snappingReady()&&app.grid.usable()&&app.status.rfind("[SS-",0)!=0&&!app.hasReportableError());
 auto preview=app.grid;key(app,ImGuiKey_G);assert(app.gridEnabled&&!app.cameraPickerOpen&&!app.gridRefresh.requested);
 key(app,ImGuiKey_G);key(app,ImGuiKey_G);assert(app.gridRecoveryPending&&!app.gridRefresh.requested);
 assert(app.adoptRecoveryGrid()&&app.gridPinned&&app.snappingReady()&&!app.cameraPickerOpen&&app.gridDirections==directions);
 assert(distance(app.grid.at(2,3),preview.at(2,3))<.001f&&app.document.marks.size()==1);
 app.setEditing(false);app.setEditing(true);assert(app.gridPinned&&app.snappingReady());frame(app);
 key(app,ImGuiKey_G,ImGuiMod_Shift);assert(app.cameraPickerOpen&&app.pickerCollecting);
 app.cancelCameraPicker();assert(app.gridPinned&&app.snappingReady());
 app.requestGrid();app.acceptCameraRecoveryOptions(options,1920,1080);app.resize(1280,720);assert(!app.gridRecoveryPending&&!app.gridPinned);
 app.resize(2560,1600);options[0].camera=gameCameraFixture();options[0].camera.f[216]=1.f/2560;options[0].camera.f[217]=1.f/1440;
 options[0].projectionUnverified=false;options[0].aspectUnverified=true;
 app.acceptCameraRecoveryOptions(options,2560,1600);app.recoveryFit=int(CameraFit::Contain);app.updateRecoveryMapping();
 auto fitted=app.grid;assert(fitted.usable()&&!app.snappingReady()&&!app.cameraPickerOpen);
 assert(app.adoptRecoveryGrid()&&app.snappingReady());assert(distance(app.grid.at(2,3),fitted.at(2,3))<.001f);
 app.startCameraPicker();app.acceptCameraOptions(options,2560,1600);app.pickerFit=int(CameraFit::Cover);app.cancelCameraPicker();
 assert(app.gridPinned&&distance(app.grid.at(2,3),fitted.at(2,3))<.001f);
 app.requestGrid();assert(!app.recoveryOption&&app.recoveryFit==0&&app.recoveryAspect==0&&!app.gridPinned);
 app.startCameraPicker();assert(app.pickerFit==0&&app.pickerAspect==0);
 app.acceptCameraOptions(options,2560,1600);app.pickerFit=int(CameraFit::Contain);auto chosen=app.cameraPickerPreview();
 assert(app.adoptCameraPreview()&&distance(app.grid.at(2,3),chosen.at(2,3))<.001f);
}
void errorReportInteractions(){
 App app;app.resize(1920,1080);app.setEditing(true);app.gridRefresh.finish();assert(!app.hasReportableError());
 app.status=errorMessage(ErrorId::CameraNotFound);app.grid.valid=true;app.grid.origin={700,400};app.grid.u={60,0};app.grid.v={0,60};app.gridEnabled=app.gridConfirmed=true;
 Mark mark;mark.kind=Kind::Pin;mark.points={{1500,900}};app.document.add(mark);auto original=app.grid;auto directions=app.gridDirections;
 app.openErrorReport();assert(app.reportOpen&&app.hasReportableError()&&app.reportError==app.status&&!app.reportRequested&&!app.reportCopyRequested&&!app.snappingReady());
 app.reportContext.outputWidth=3840;app.reportContext.outputHeight=2160;app.reportContext.windowWidth=1920;app.reportContext.windowHeight=1080;app.reportContext.graphics="显卡：测试设备";app.reportContext.gridRequest=42;
 app.levelWatch.known=true;app.levelWatch.current={11,12,9,{-1,2,-1},"test_level"};
 auto captured=app.reportDiagnostics();app.status="正在检查相机";
 assert(app.reportDiagnostics().error==captured.error&&app.reportDiagnostics().status==app.status);
 assert(captured.level=="test_level (12 x 9)"&&captured.cameraKey==app.bindings.label(Action::Calibrate)&&captured.gridConfirmed);
 frame(app);key(app,ImGuiKey_C);assert(app.document.marks.size()==1&&app.reportOpen);
 key(app,ImGuiKey_Escape);assert(!app.reportOpen&&app.editing&&app.snappingReady());
 assert(distance(app.grid.at(2,3),original.at(2,3))<.001f&&app.gridDirections==directions);
 app.openErrorReport();key(app,ImGuiKey_G,ImGuiMod_Shift);assert(!app.reportOpen&&app.cameraPickerOpen&&app.pickerCollecting&&!app.reportError.empty());
 app.failCameraPicker(ErrorId::CameraNotFound);app.openErrorReport();assert(app.reportOpen&&!app.reportRequested);
 app.reportFeedback.clear();for(int i=0;i<80;++i)app.reportFeedback+="long diagnostic status\n";
 frame(app,640,360);frame(app,640,360);auto window=ImGui::FindWindowByName("汇报错误");assert(window&&window->Pos.x>=0&&window->Pos.y>=0&&window->Pos.x+window->Size.x<=641&&window->Pos.y+window->Size.y<=361);
 assert(window->ScrollMax.y>0); // Small viewports can scroll to the combined action.
 app.reportOpen=false;app.cancelCameraPicker();app.reportError.clear();
 app.gameBuildWarning=ErrorId::UnsupportedGameBuild;app.status="ready";app.openErrorReport();assert(app.reportOpen&&app.reportError==errorMessage(ErrorId::UnsupportedGameBuild));
 app.setEditing(false);assert(!app.reportOpen&&app.document.marks.size()==1);
 ReportEnvironment environment{"2026-10-29T12:00:00Z","Windows 10.0 (build 26100)","x64",16,32768};
 auto text=formatReportDiagnostics(captured,environment);
 for(auto expected:{"3840 x 2160","1920 x 1080","Windows 10.0 (build 26100)","显卡：测试设备","2026-10-29T12:00:00Z","[SS-GRID-010]","test_level","Shift+G","定位请求：42",CurrentVersion})assert(text.find(expected)!=std::string::npos);
 assert(text.find("未取得")!=std::string::npos); // Missing fields remain explicit, not guessed.
 auto missing=formatReportDiagnostics({},{});assert(missing.find("游戏输出分辨率：未取得")!=std::string::npos&&missing.find("系统：未取得")!=std::string::npos);
 for(unsigned long error:{0ul,5ul}){
  unsigned calls=0;auto copied=performDiagnostics(captured,environment,[&](const std::string& payload){++calls;assert(payload==text);return error;});
  assert(calls==1&&copied.diagnosticsCopied==!error&&!copied.addressCopied&&!copied.folderOpened&&copied.action==FeedbackAction::CopyDiagnostics);
  auto message=feedbackResultMessage(copied);assert(message.find(error?"[SS-REPORT-003]":"已复制诊断信息")==0);
 }
 auto actualEnvironment=collectReportEnvironment();assert(!actualEnvironment.utc.empty()&&!actualEnvironment.windows.empty()&&actualEnvironment.processors>0&&actualEnvironment.memoryMiB>0);
 const std::wstring folder=L"C:\\游戏\\含 空格的目录\\";
 for(unsigned long copyError:{0ul,5ul})for(unsigned long folderError:{0ul,3ul}){
  int sequence=0;auto result=performFeedback(folder,[&]{assert(sequence++==0);return copyError;},[&](const std::wstring& path){assert(sequence++==1&&path==folder);return folderError;});
  assert(sequence==2&&result.addressCopied==!copyError&&result.folderOpened==!folderError);
  auto message=feedbackResultMessage(result);assert(!message.empty());
  if(folderError)assert(message.find("[SS-REPORT-001]")==0);
  else if(copyError)assert(message.find("[SS-REPORT-002]")==0);
  else assert(message.find(FeedbackAddress)!=std::string::npos);
 }
}
void pinnedGridRecovery(){
 App app;app.resize(1920,1080);app.setEditing(true);
 CameraOption option;option.camera=gameCameraFixture();option.suggestedZ=1;
 auto adopt=[&](){app.startCameraPicker();app.acceptCameraOptions({option},1920,1080);assert(app.adoptCameraPreview());frame(app);};
 adopt();auto original=app.grid;auto adoptions=app.cameraPreviewAdoptions;
 Mark mark;mark.kind=Kind::Pin;mark.points={{1450,800}};app.document.add(mark);
 auto preserved=[&](){assert(app.gridPinned&&app.gridConfirmed&&app.snappingReady()&&!app.cameraPickerOpen&&!app.pickerPreviousPinned&&!app.pickerPreviousGrid.valid);
  assert(distance(app.grid.at(2,3),original.at(2,3))<.001f&&app.cameraPreviewAdoptions==adoptions&&app.document.marks.size()==1);};
 // Refreshing the list must not replace the backup with an empty preview.
 app.startCameraPicker();app.startCameraPicker();app.acceptCameraOptions({},1920,1080);app.cancelCameraPicker();preserved();
 app.startCameraPicker();frame(app);key(app,ImGuiKey_Escape);preserved();
 app.startCameraPicker();app.setEditing(false);preserved();app.setEditing(true);assert(!app.gridRefresh.requested);frame(app);
 // Explicit relocalization and resize invalidate the backup as well as the current pin.
 app.startCameraPicker();app.requestGrid();assert(!app.gridPinned&&!app.pickerPreviousPinned&&app.gridRefresh.requested);app.cancelCameraPicker();assert(!app.gridPinned);
 adopt();app.startCameraPicker();app.resize(1280,720);assert(!app.gridPinned&&!app.pickerPreviousPinned&&!app.cameraPickerOpen&&app.gridRefresh.requested);
 app.resize(1920,1080);adopt();adoptions=app.cameraPreviewAdoptions;
 LevelIdentity first{21,8,7,{0,0,0},"first"},second=first;second.name="second";
 for(uint64_t time:{0,200,400,600})app.observeLevel(&first,time);
 // A loading sample closes the picker but keeps its prior pin paused.
 app.startCameraPicker();app.observeLevel(nullptr,800);assert(app.gridPinned&&!app.gridConfirmed&&app.levelPaused&&!app.snappingReady()&&!app.cameraPickerOpen);
 frame(app);
 app.gridEnabled=false;key(app,ImGuiKey_G);app.tint=false;key(app,ImGuiKey_T);
 assert(app.gridPinned&&app.gridEnabled&&app.tint&&!app.gridRefresh.requested&&!app.snappingReady());
 // Even an accidental confirmed flag cannot bypass the level pause for placement.
 app.gridConfirmed=true;app.tool=Kind::Pin;Point cursor{1123,743};assert(distance(original.snap(cursor),cursor)>1);
 click(app,cursor);assert(app.document.marks.size()==2&&distance(app.document.marks.back().points[0],cursor)<.001f);
 app.gridFromGame=true;app.gameGridBase=original;auto direction=app.gridDirection;app.adjustGridDirection(direction^1);app.confirmGridDirection();assert(app.gridDirection==direction);
 app.observeLevel(&first,1000);assert(app.gridPinned&&app.gridConfirmed&&!app.levelPaused&&!app.gridRefresh.requested);
 // Closing a zero-size viewport finishes an active grouped gesture once and retains geometry/history.
 app.tool=Kind::Number;auto&io=ImGui::GetIO();io.AddMousePosEvent(1100,730);frame(app);io.AddMouseButtonEvent(0,true);frame(app);assert(app.numbering);
 auto count=app.document.marks.size(),history=app.document.undoStack.size();frame(app,0,0);assert(!app.numbering&&app.document.undoStack.size()==history+1);
 frame(app,0,0);assert(app.document.undoStack.size()==history+1);io.AddMouseButtonEvent(0,false);frame(app);
 assert(app.document.marks.size()==count&&app.gridPinned&&app.gridConfirmed&&distance(app.grid.at(2,3),original.at(2,3))<.001f);
 app.startCameraPicker();for(uint64_t time:{1200,1400,1600,1800})app.observeLevel(&second,time);
 assert(!app.gridPinned&&!app.pickerPreviousPinned&&!app.cameraPickerOpen&&app.gridRefresh.requested);
 // Tiny positive viewports also produce finite UI geometry, including the picker and settings.
 app.startCameraPicker();app.settingsOpen=true;frame(app,8,8);
 for(auto list:ImGui::GetDrawData()->CmdLists)for(auto vertex:list->VtxBuffer)assert(std::isfinite(vertex.pos.x)&&std::isfinite(vertex.pos.y));
 app.setEditing(false);
}
void interactions(){
 ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigInputTrickleEventQueue=false;io.Fonts->AddFontDefault();unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 App app;frame(app);key(app,ImGuiKey_E);assert(app.editing&&app.gridRefresh.requested);
 // Every default has one owner; modifiers match exactly (G is independent of Shift+G).
 for(int i=0;i<int(Action::Count);++i)assert(app.bindings.conflict(static_cast<Action>(i),app.bindings.get(static_cast<Action>(i)))==-1);
 key(app,ImGuiKey_G,ImGuiMod_Shift);assert(app.cameraPickerOpen&&app.gridEnabled);assert(!app.tint);
 key(app,ImGuiKey_Escape);assert(!app.cameraPickerOpen&&app.editing);
 app.gridEnabled=false;key(app,ImGuiKey_G,ImGuiMod_Ctrl);assert(!app.gridEnabled);key(app,ImGuiKey_G);assert(app.gridEnabled);
 app.gridRefresh.cancel();app.gridConfirmed=true;app.grid.valid=true;app.grid.origin={600,400};app.grid.u={80,0};app.grid.v={0,80};
 // A shared swatch supplies new stars, arrows and pen marks; later edits preserve existing marks.
 app.colorIndex=3;app.colors[3]=IM_COL32(20,230,90,255);key(app,ImGuiKey_P);click(app,{783,561});
 assert(app.document.marks.size()==1&&app.document.marks[0].kind==Kind::Pin&&app.document.marks[0].number==0);
 assert(distance(app.document.marks[0].points[0],{760,560})<.01f&&app.document.marks[0].color==app.colors[3]&&app.document.nextNumber==1);
 app.colors[3]=IM_COL32(220,60,210,255);key(app,ImGuiKey_B);click(app,{1000,800});assert(app.document.marks.back().color==app.selectedColor()&&app.document.marks[0].color!=app.selectedColor());
 key(app,ImGuiKey_A);io.AddMousePosEvent(1100,600);frame(app);io.AddMouseButtonEvent(0,true);frame(app);io.AddMousePosEvent(1250,600);frame(app);io.AddMouseButtonEvent(0,false);frame(app);assert(app.document.marks.back().kind==Kind::Arrow&&app.document.marks.back().color==app.selectedColor());
 key(app,ImGuiKey_N);click(app,{943,561});assert(app.document.marks.back().kind==Kind::Number&&app.document.marks.back().number==1&&app.document.nextNumber==2);
 key(app,ImGuiKey_Z,ImGuiMod_Ctrl);assert(app.document.nextNumber==1);key(app,ImGuiKey_Y,ImGuiMod_Ctrl);assert(app.document.nextNumber==2);
 // Capture cannot clear the document, switch tools, erase or close the overlay.
 auto count=app.document.marks.size();app.settingsOpen=true;app.capturingBinding=int(Action::Number);app.captureAfterFrame=ImGui::GetFrameCount();
 key(app,ImGuiKey_C);assert(app.settingsOpen&&app.editing&&app.capturingBinding==int(Action::Number));assert(app.document.marks.size()==count&&app.inputError==ErrorId::KeyBindingConflict&&app.bindingStatus.find("[SS-INPUT-001]")==0);
 key(app,ImGuiKey_D,ImGuiMod_Ctrl|ImGuiMod_Shift);assert(app.capturingBinding==-1&&app.bindings.get(Action::Number)==(KeyBinding{ImGuiKey_D,ImGuiMod_Ctrl|ImGuiMod_Shift}));
 app.capturingBinding=int(Action::Toggle);app.captureAfterFrame=ImGui::GetFrameCount();key(app,ImGuiKey_E);assert(app.editing&&app.settingsOpen&&app.capturingBinding==-1);
 app.capturingBinding=int(Action::Pen);key(app,ImGuiKey_Escape);assert(app.settingsOpen&&app.capturingBinding==-1);key(app,ImGuiKey_Escape);assert(!app.settingsOpen&&app.editing);
 key(app,ImGuiKey_P);key(app,ImGuiKey_N);assert(app.tool==Kind::Pin);key(app,ImGuiKey_D,ImGuiMod_Ctrl);assert(app.tool==Kind::Pin);key(app,ImGuiKey_D,ImGuiMod_Ctrl|ImGuiMod_Shift);assert(app.tool==Kind::Number);
 assert(!app.bindings.assign(Action::Pen,{ImGuiKey_Escape}));assert(!app.bindings.assign(Action::Pen,{ImGuiKey_MouseLeft}));assert(!app.bindings.assign(Action::Pen,{ImGuiKey_LeftCtrl}));
 assert(!app.bindings.assign(Action::Pen,{ImGuiKey_D,ImGuiMod_Super}));
 assert(app.bindings.assign(Action::Toggle,{ImGuiKey_F6,ImGuiMod_Ctrl}));assert(app.bindings.label(Action::Toggle)=="Ctrl+F6");
 key(app,ImGuiKey_E);assert(app.editing);key(app,ImGuiKey_F6,ImGuiMod_Ctrl);assert(!app.editing);key(app,ImGuiKey_F6);assert(!app.editing);key(app,ImGuiKey_F6,ImGuiMod_Ctrl);assert(app.editing);
 assert(app.colors[3]==IM_COL32(220,60,210,255)&&app.document.marks.size()==count);
 // A keyboard eraser is held, records once, and leaves number sequencing unchanged.
 assert(app.bindings.assign(Action::Erase,{ImGuiKey_R}));io.AddMousePosEvent(920,560);frame(app);io.AddKeyEvent(ImGuiKey_R,true);frame(app);frame(app);io.AddKeyEvent(ImGuiKey_R,false);frame(app);
 assert(app.document.marks.size()==count-1&&app.document.nextNumber==2);key(app,ImGuiKey_Z,ImGuiMod_Ctrl);assert(app.document.marks.size()==count);
 app.settingsOpen=true;app.capturingBinding=int(Action::Erase);app.captureAfterFrame=ImGui::GetFrameCount();click(app,{1500,800},2);assert(app.bindings.get(Action::Erase).key==ImGuiKey_MouseMiddle&&app.document.marks.size()==count);key(app,ImGuiKey_Escape);
 app.bindings.reset();assert(app.bindings.get(Action::Number).key==ImGuiKey_N&&app.bindings.get(Action::Erase).key==ImGuiKey_MouseRight);
 key(app,ImGuiKey_C);assert(app.document.marks.empty()&&app.document.nextNumber==1);key(app,ImGuiKey_Z,ImGuiMod_Ctrl);assert(app.document.marks.size()==count&&app.document.nextNumber==2);
 App fresh;assert(fresh.bindings.get(Action::Toggle).key==ImGuiKey_E&&fresh.colors[3]!=app.colors[3]);
 // Number rendering ignores the shared swatch; stars keep it without any numeral geometry.
 app.setEditing(false);app.gridEnabled=false;app.tint=false;app.document.marks.clear();Mark sample;sample.kind=Kind::Number;sample.number=123;sample.color=IM_COL32(255,0,0,255);sample.points={{900,500}};app.document.marks.push_back(sample);frame(app);
 bool black=false,white=false;for(auto vertex:ImGui::GetBackgroundDrawList()->VtxBuffer){auto rgb=vertex.col&0xffffff;assert(rgb==0||rgb==0xffffff);black=black||rgb==0;white=white||rgb==0xffffff;}assert(black&&white);
 sample.kind=Kind::Pin;app.document.marks={sample};frame(app);bool red=false,gold=false,shadow=false;
 std::set<uint32_t> starColors={0,0xff,IM_COL32(23,18,12,0),IM_COL32(76,61,39,0),IM_COL32(224,192,114,0),IM_COL32(255,232,158,0),IM_COL32(232,202,132,0),IM_COL32(126,89,40,0),IM_COL32(181,143,73,0)};
 for(auto vertex:ImGui::GetBackgroundDrawList()->VtxBuffer){auto rgb=vertex.col&0xffffff;assert(starColors.count(rgb));red=red||rgb==0xff;gold=gold||rgb==(IM_COL32(224,192,114,0)&0xffffff);shadow=shadow||rgb==IM_COL32(23,18,12,0);}assert(red&&gold&&shadow);
 numberingInteractions();
 directionInteractions();
 pickerInteractions();
 recoveryPreviewInteractions();
 scenePosePreviewInteraction();
 errorReportInteractions();
 pinnedGridRecovery();
 App updates;updates.setEditing(true);frame(updates);frame(updates);
 auto greenVertices=[](){size_t count=0;auto color=ImGui::ColorConvertFloat4ToU32({.43f,.89f,.56f,1});
  for(auto list:ImGui::GetDrawData()->CmdLists)for(auto vertex:list->VtxBuffer)if(vertex.col==color)++count;return count;
 };
 assert(greenVertices()==0);ReleaseVersion newer;assert(ReleaseVersion::parse(CurrentVersion,newer));++newer.parts[2];updates.availableUpdate=newer.packed();
 frame(updates);frame(updates);assert(greenVertices()>0);
 updates.settingsOpen=true;frame(updates);assert(greenVertices()>0);updates.settingsOpen=false;
 updates.availableUpdate=0;frame(updates);frame(updates);assert(greenVertices()==0); // Network failure/no update adds no UI.
 auto grayVertices=[](){size_t count=0;auto color=ImGui::ColorConvertFloat4ToU32({.60f,.60f,.60f,1});
  for(auto list:ImGui::GetDrawData()->CmdLists)for(auto vertex:list->VtxBuffer)if(vertex.col==color)++count;return count;
 };
 assert(grayVertices()==0);updates.gameBuildWarning=ErrorId::UnsupportedGameBuild;
 updates.grid.valid=true;updates.grid.origin={600,300};updates.grid.u={60,0};updates.grid.v={0,60};
 updates.gridConfirmed=updates.gridFromGame=true;updates.gridRefresh.finish();updates.status="localized";
 frame(updates);frame(updates);assert(grayVertices()>0&&updates.gridConfirmed&&updates.gridFromGame&&updates.status=="localized");
 updates.requestGrid();assert(updates.gameBuildWarning==ErrorId::UnsupportedGameBuild&&updates.gridRefresh.requested);
 updates.rejectGameGrid(ErrorId::CameraNotFound);frame(updates);assert(grayVertices()>0&&updates.status==errorMessage(ErrorId::CameraNotFound));
 updates.startCameraPicker();frame(updates);assert(grayVertices()>0&&updates.cameraPickerOpen);
 updates.settingsOpen=true;frame(updates);assert(grayVertices()>0);updates.settingsOpen=false;
 updates.gameBuildWarning=ErrorId::None;frame(updates);frame(updates);assert(grayVertices()==0);
 ImGui::DestroyContext();
}
}
void scenePosePreviewInteraction(){
 App app;app.setEditing(true);app.resize(1920,1080);frame(app);
 CameraOption option;option.scenePose=true;option.token=73;option.pose.position={5,-1,20};
 option.pose.orientation={0,float(std::sin(.6)),0,float(std::cos(.6))};option.pose.projection={.61f,.2f,200};
 option.suggestedZ=1;option.directionCategory=option.pose.category(1920,1080);
 app.startCameraPicker();app.acceptCameraOptions({option},1920,1080);assert(!app.snappingReady());
 app.pickerHeightOverride=true;app.pickerZ=2;app.pickerFlip=1;auto chosen=app.cameraPickerPreview();assert(chosen.usable());frame(app);
 assert(app.adoptCameraPreview()&&app.gridPinned&&app.gridConfirmed);for(int x=-3;x<4;++x)assert(distance(app.grid.at(float(x),2),chosen.at(float(x),2))<.001f);
 app.setEditing(false);app.setEditing(true);assert(app.gridPinned&&!app.gridRefresh.requested);
 app.startCameraPicker();app.acceptCameraOptions({option},1920,1080);app.cancelCameraPicker();assert(app.gridPinned&&distance(app.grid.at(2,1),chosen.at(2,1))<.001f);
 app.startCameraPicker();app.acceptCameraOptions({option},1920,1080);app.requestOriginalGrid();
 assert(!app.cameraPickerOpen&&!app.gridPinned&&app.gridRefresh.requested&&!app.gridPreferScene);
 app.requestGrid();assert(app.gridPreferScene); // The fallback override lasts for this request only.
}
int main(){
 checkPreferences();
 levelInteractions();
 // A first opening and every reopening refresh, even with grid/tint hidden or a manual grid.
 App app;app.resize(3840,2160);assert(!app.gridRefresh.requested);
 Mark pin;pin.kind=Kind::Number;pin.points={{960,540}};app.document.add(pin);
 app.grid.valid=true;app.grid.origin={200,300};app.gridConfirmed=true;app.gridFromGame=false;
 app.gridPlaneOverride=true;app.gridPlaneZ=1.75f;
 assert(app.setEditing(true));assert(app.gridRefresh.requested&&!app.gridConfirmed);
 assert(!app.gridEnabled&&!app.tint);assert(app.document.marks.size()==1&&app.document.nextNumber==2);
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
 app.gridRefresh.pending=true;app.startCameraPicker();assert(app.cameraPickerOpen&&!app.gridRefresh.requested&&!app.gridRefresh.pending);
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
 assert(std::string(errorInfo(ErrorId::AuxiliaryCameraOnly).code)=="SS-GRID-013");
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
  assert(failed.document.marks.size()==1&&failed.document.nextNumber==2);
  failed.requestGrid();assert(failed.gridRefresh.requested&&failed.status.find("[SS-")==std::string::npos);
 }
 failed.rejectGameGrid(ErrorId::None);assert(failed.status==errorMessage(ErrorId::UnknownGameGrid));
 failed.rejectGameGrid(static_cast<ErrorId>(-1));assert(failed.status==errorMessage(ErrorId::UnknownGameGrid));
 failed.startCameraPicker();assert(failed.cameraPickerOpen&&!failed.gridRefresh.requested&&failed.status.find("[SS-")==std::string::npos);
 assert(failed.document.undo()&&failed.document.marks.empty());assert(failed.document.redo());
 failed.cancelCameraPicker();failed.grid=Grid::fromQuad({{600,280},{1300,320},{1400,920},{500,860}},8,7);
 Detection invalid;failed.acceptDetection(invalid);assert(failed.status==errorMessage(ErrorId::ImageGridUnreliable));
 Detection candidate;candidate.grid=failed.grid;candidate.grid.confidence=.2f;
 failed.acceptDetection(candidate);assert(failed.status==errorMessage(ErrorId::ImageGridUnconfirmed)&&!failed.gridConfirmed);
 candidate.grid.confidence=1;failed.acceptDetection(candidate);assert(failed.gridConfirmed&&failed.status.find("[SS-")==std::string::npos);
 interactions();
 std::cout<<"PASS: lifecycle/grid recovery, colors/rendering, numbering sweeps and grouped history, both N+C orders without clearing, restart/undo/redo, perspective traversal, UI gaps, snap loss, remappable two-key capture, binding conflicts, default-off level auto-clear, persistent preferences and atomic file recovery\n";
}
