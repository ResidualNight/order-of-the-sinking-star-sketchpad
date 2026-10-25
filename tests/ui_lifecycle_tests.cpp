#include "ui.hpp"
#include <cassert>
#include <iostream>
#include <set>
using namespace sketch;
void checkPreferences();
namespace {
void frame(App& app){auto&io=ImGui::GetIO();io.DisplaySize={1920,1080};io.DeltaTime=1.f/60;ImGui::NewFrame();app.frame(1920,1080);ImGui::Render();if(app.wantExit){app.wantExit=false;app.setEditing(false);}}
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
 app.setEditing(true);app.resize(1600,900);app.startManual();assert(!app.observeLevel(&a,3400)&&app.document.marks.size()==1); // View/UI/calibration are not identity changes.
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
void interactions(){
 ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigInputTrickleEventQueue=false;io.Fonts->AddFontDefault();unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 App app;frame(app);key(app,ImGuiKey_E);assert(app.editing&&app.gridRefresh.requested);
 // Every default has one owner; modifiers match exactly (G is independent of Shift+G).
 for(int i=0;i<int(Action::Count);++i)assert(app.bindings.conflict(static_cast<Action>(i),app.bindings.get(static_cast<Action>(i)))==-1);
 key(app,ImGuiKey_G,ImGuiMod_Shift);assert(app.manual&&app.gridEnabled);assert(!app.tint);
 key(app,ImGuiKey_Escape);assert(!app.manual&&app.editing);
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
 numberingInteractions();ImGui::DestroyContext();
}
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
  assert(failed.document.marks.size()==1&&failed.document.nextNumber==2);
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
 interactions();
 std::cout<<"PASS: lifecycle/grid recovery, colors/rendering, numbering sweeps and grouped history, both N+C orders without clearing, restart/undo/redo, perspective traversal, UI gaps, snap loss, remappable two-key capture, binding conflicts, default-off level auto-clear, persistent preferences and atomic file recovery\n";
}
