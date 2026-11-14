#include "ui.hpp"
#include "preferences.hpp"
#include "imgui_internal.h"
#include <cassert>
#include <cstdio>
using namespace sketch;
static bool productionQueue=false;
struct Context{Context(){ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigInputTrickleEventQueue=productionQueue;unsigned char*p;int w,h;io.Fonts->GetTexDataAsRGBA32(&p,&w,&h);io.AddMousePosEvent(-100,-100);}~Context(){ImGui::DestroyContext();}};
static PadInput neutral(){PadInput in;in.connected=true;return in;}
static void frame(App&a,PadInput in=neutral(),float w=1920,float h=1080){auto&io=ImGui::GetIO();io.DisplaySize={w,h};io.DeltaTime=1.f/60;a.padPrepare(in);ImGui::NewFrame();a.frame(w,h);ImGui::Render();if(a.wantExit){a.wantExit=false;a.setEditing(false);}}
static void press(App&a,uint16_t key){auto in=neutral();in.buttons=key;frame(a,in);frame(a);frame(a);}
static void rt(App&a){auto in=neutral();in.rt=255;frame(a,in);frame(a);}
static void open(App&a){frame(a);a.openFromPad();a.gridRefresh.cancel();a.status.clear();frame(a);frame(a);assert(a.padLayout&&a.padPointer&&a.padReady);}
static void tools(){Context context;App a;open(a);a.padCursor={700,400};rt(a);assert(a.document.marks.size()==1&&a.document.marks[0].kind==Kind::Pin);press(a,padui::Y);assert(a.document.marks.empty());a.document.redo();assert(a.document.marks.size()==1);
 a.choosePadTool(1);frame(a);a.padCursor={600,350};rt(a);assert(a.drawing&&a.document.marks.size()==1);a.padCursor={800,450};frame(a);rt(a);assert(!a.drawing&&a.document.marks.size()==2&&a.document.marks.back().kind==Kind::Arrow);
 rt(a);assert(a.drawing);press(a,padui::B);assert(!a.drawing&&a.editing&&a.document.marks.size()==2);
 a.choosePadTool(0);frame(a);auto held=neutral();held.rt=255;frame(a,held);a.padCursor.x+=70;frame(a,held);frame(a);assert(a.document.marks.size()==3&&a.document.marks.back().kind==Kind::Stroke);
 a.choosePadTool(3);frame(a);a.grid.valid=true;a.grid.origin={600,300};a.grid.u={60,0};a.grid.v={0,60};a.gridEnabled=a.gridConfirmed=true;a.padCursor={600,300};frame(a,held);a.padCursor={780,300};frame(a,held);frame(a);assert(a.document.marks.size()==7&&a.document.nextNumber==5);press(a,padui::Y);assert(a.document.marks.size()==3&&a.document.nextNumber==1);
 a.padCursor={700,400};auto erase=neutral();erase.lt=255;frame(a,erase);frame(a);assert(a.document.marks.size()<3);press(a,padui::Y);assert(a.document.marks.size()==3);
 auto before=a.document.marks.size();press(a,padui::A);assert(a.padQuick);frame(a,held);frame(a);assert(a.document.marks.size()==before);press(a,padui::B);assert(!a.padQuick&&a.editing);frame(a,held);frame(a);assert(a.document.marks.size()==before+1);
 press(a,padui::B);assert(!a.editing);
}
static void lifecycle(){Context context;App a;open(a);a.choosePadTool(0);frame(a);auto held=neutral();held.rt=255;frame(a,held);assert(a.drawing);frame(a,{});assert(!a.drawing&&!a.numbering&&!a.erasing&&!a.padPointer);frame(a,held);assert(!a.drawing&&!a.padReady);frame(a);frame(a,held);assert(a.drawing);a.setEditing(false);assert(!a.drawing);a.setEditing(true);assert(!a.padLayout);a.setEditing(false);a.padUiMode=2;a.setEditing(true);assert(a.padLayout);a.setEditing(false);a.padUiMode=1;a.openFromPad();assert(!a.padLayout&&a.padPointer);
 auto encoded=encodePreferences(a);App copy;assert(decodePreferences(encoded,copy)&&copy.padUiMode==1&&copy.padEntryEnabled&&copy.padSpeed==1&&!copy.padLayout&&!copy.padPointer);a.padEntryEnabled=false;a.padSpeed=2.5f;encoded=encodePreferences(a);assert(decodePreferences(encoded,copy)&&!copy.padEntryEnabled&&copy.padSpeed==2.5f);assert(!decodePreferences("format=1\npad.ui_mode=3\n",copy));assert(!decodePreferences("format=1\npad.speed=nan\n",copy));
}
static void layout(){Context context;App a;open(a);for(auto size:{ImVec2(640,360),ImVec2(1280,720),ImVec2(1920,1200),ImVec2(2560,1600),ImVec2(3440,1440),ImVec2(3840,2160),ImVec2(5120,1440)}){a.padQuick=false;frame(a,neutral(),size.x,size.y);frame(a,neutral(),size.x,size.y);auto bar=ImGui::FindWindowByName("##PadToolbar");assert(bar&&bar->Pos.x>=0&&bar->Pos.y>=0&&bar->Pos.x+bar->Size.x<=size.x+1&&bar->Pos.y+bar->Size.y<=size.y+1);a.padQuick=true;for(int page=0;page<3;++page){a.padPage=page;frame(a,neutral(),size.x,size.y);frame(a,neutral(),size.x,size.y);auto menu=ImGui::FindWindowByName("草稿纸快捷面板");assert(menu&&menu->Size.x<=size.x&&menu->Size.y<=size.y);}}}
static void movement(){for(int hz:{30,60,144}){Point p{100,100};auto in=neutral();in.lx=.7f;for(int i=0;i<hz;++i)p=padui::move(p,in,1.f/hz,1920,1080,1);float expected=100+1080*.8f*std::pow((.7f-.22f)/.78f,1.5f);assert(std::abs(p.x-expected)<.1f);}Grid g;g.valid=true;g.origin={500,400};g.u={-60,0};g.v={0,60};assert(padui::step({500,400},{1,0},&g,1920,1080).x==560);auto in=neutral();in.lx=1;in.rx=.5f;auto p=padui::move({0,0},in,.01f,1000,1000,1);assert(p.x<2);}
static void navigation(){Context context;App a;open(a);rt(a);auto count=a.document.marks.size();press(a,padui::A);assert(a.padQuick&&a.padReady);auto&g=*ImGui::GetCurrentContext();assert(g.NavWindow&&g.NavId);
 ImGuiWindow*body=nullptr;for(auto window:g.Windows)if(window->ParentWindow&&std::string(window->Name).find("##padContents")!=std::string::npos)body=window;assert(body);auto clearId=body->GetID("清空所有标记...");
 for(int i=0;i<40&&g.NavId!=clearId;++i)press(a,padui::Down);
 assert(g.NavId==clearId);press(a,padui::A);assert(a.padConfirmClear&&a.document.marks.size()==count);press(a,padui::B);assert(!a.padConfirmClear&&a.padQuick&&a.document.marks.size()==count);press(a,padui::B);assert(!a.padQuick&&a.editing);
}
static PadInput wheelInput(int index,bool held=true){auto in=neutral();in.buttons=held?padui::X:0;if(index>=0){float angle=index*6.28318530718f/7;in.lx=std::sin(angle);in.ly=std::cos(angle);}return in;}
static void holdWheel(App&a,int index){for(int i=0;i<18;++i)frame(a,wheelInput(index));assert(a.padWheel&&a.padWheelVisible&&a.padWheelSelection==index);}
static void selectWheel(App&a,int index){holdWheel(a,index);frame(a);assert(!a.padWheel&&!a.padReady);frame(a);frame(a);assert(a.padReady);}
static void wheelTests(){Context context;App a;open(a);a.padCursor={700,400};rt(a);auto p=a.padCursor;auto original=a.tool;
 // Short tap does nothing, even with a selected direction; no old X redo.
 press(a,padui::Y);frame(a,wheelInput(0));frame(a);frame(a);assert(a.tool==original&&a.document.marks.empty());a.document.redo();
 for(int i=0;i<4;++i){selectWheel(a,i);assert(int(a.tool)==i&&!a.padEraseTool);}
 assert(distance(a.padCursor,p)<.01f);auto count=a.document.marks.size();
 holdWheel(a,4);auto mixed=wheelInput(4);mixed.rt=mixed.lt=255;mixed.rx=1;mixed.buttons|=padui::RB|padui::Y|padui::A;frame(a,mixed);assert(a.document.marks.size()==count&&!a.padQuick&&distance(a.padCursor,p)<.01f);
 // Clear directly on X release, one undo restores the document.
 frame(a);assert(a.document.marks.empty());frame(a);press(a,padui::Y);assert(a.document.marks.size()==count);
 a.grid.valid=true;a.grid.origin={300,200};a.grid.u={60,0};a.grid.v={0,60};a.gridEnabled=a.gridConfirmed=true;
 selectWheel(a,5);assert(a.tint&&a.monochromeTint&&a.gridConfirmed);selectWheel(a,5);assert(!a.tint&&a.monochromeTint);
 auto encoded=encodePreferences(a);App restored;assert(decodePreferences(encoded,restored)&&restored.monochromeTint&&!restored.padWheel);assert(!decodePreferences("format=1\ntint.monochrome=2\n",restored));
 a.gridLevelSettling=true;selectWheel(a,6);assert(!a.gridLevelSettling&&a.gridEnabled&&!a.gridConfirmed&&a.document.marks.size()==count);a.gridRefresh.cancel();
 // Center/B cancellation, disconnect, resize and closed session never execute.
 holdWheel(a,4);frame(a,wheelInput(-1));frame(a);frame(a);assert(a.document.marks.size()==count);
 holdWheel(a,4);auto cancel=wheelInput(4);cancel.buttons|=padui::B;frame(a,cancel);frame(a);frame(a);assert(!a.padWheel&&a.editing&&a.document.marks.size()==count);
 holdWheel(a,4);frame(a,{});assert(!a.padWheel&&a.document.marks.size()==count);frame(a,wheelInput(-1));frame(a);frame(a);
 holdWheel(a,4);frame(a,wheelInput(4),1280,720);assert(!a.padWheel&&a.document.marks.size()==count);frame(a,neutral(),1280,720);frame(a,neutral(),1280,720);
 for(int i=0;i<18;++i)frame(a,wheelInput(4),1280,720);assert(a.padWheelVisible);a.setEditing(false);frame(a);assert(!a.padWheel&&a.document.marks.size()==count);
 // Selection compass and sector hysteresis are independent from frame rate.
 for(int i=0;i<7;++i){auto in=wheelInput(i);assert(padui::wheelSelection(in.lx,in.ly)==i);}
 assert(padui::wheelSelection(.1f,.1f)==-1);float edge=3.14159265f/7;assert(padui::wheelSelection(std::sin(edge+.02f),std::cos(edge+.02f),0)==0);assert(padui::wheelSelection(std::sin(edge+.12f),std::cos(edge+.12f),0)==1);
}
static void wheelStyle(){Context context;App a;open(a);assert(a.padWheelScale==.8f&&a.padWheelOpacity==1);
 for(auto size:{ImVec2(640,360),ImVec2(1280,720),ImVec2(1920,1080),ImVec2(2560,1600),ImVec2(3440,1440),ImVec2(3840,2160),ImVec2(5120,1440)}){
  float scale=std::clamp(size.y/1080.f,.75f,2.4f),old=std::min({175.f*scale,size.y*.31f,size.x*.4f});
  assert(std::abs(padui::wheelRadius(size.x,size.y,scale,.8f)-old*.8f)<.001f);
  for(float factor:{.5f,.8f,1.f,1.5f}){float r=padui::wheelRadius(size.x,size.y,scale,factor);assert(r<=size.y*.31f+.01f&&r<=size.x*.4f+.01f);}
 }
 a.padWheelScale=1.25f;a.padWheelOpacity=.4f;auto saved=encodePreferences(a);App restored;assert(decodePreferences(saved,restored)&&restored.padWheelScale==1.25f&&restored.padWheelOpacity==.4f);
 assert(decodePreferences("format=1\n",restored)&&restored.padWheelScale==.8f&&restored.padWheelOpacity==1);
 for(auto invalid:{"pad.wheel_scale=0.4","pad.wheel_scale=1.6","pad.wheel_scale=nan","pad.wheel_opacity=0.2","pad.wheel_opacity=1.1"}){auto before=encodePreferences(a);assert(!decodePreferences(std::string("format=1\n")+invalid+"\n",a));assert(encodePreferences(a)==before);}
 holdWheel(a,2);auto dl=ImGui::GetForegroundDrawList();assert(dl->VtxBuffer.Size>0);unsigned alpha=(dl->VtxBuffer[0].col>>IM_COL32_A_SHIFT)&255;assert(alpha==45);
 a.padWheelOpacity=1;frame(a,wheelInput(2));dl=ImGui::GetForegroundDrawList();assert(((dl->VtxBuffer[0].col>>IM_COL32_A_SHIFT)&255)==112);
}
static PadInput controlInput(PadControl key){auto in=neutral();switch(key){case PadControl::LT:in.lt=255;break;case PadControl::RT:in.rt=255;break;default:for(int mask=1;mask<=32768;mask*=2){in.buttons=uint16_t(mask);if(padControls(in,0)&padControlBit(key))return in;}in.buttons=0;}return in;}
static void pressControl(App&a,PadControl c){frame(a,controlInput(c));frame(a);frame(a);}
static void restartAndWholeErase(){Context context;App a;open(a);a.gridEnabled=false;a.padCursor={650,350};
 a.choosePadTool(3);frame(a);rt(a);a.padCursor.x+=100;rt(a);assert(a.document.nextNumber==3&&a.document.marks.size()==2);
 press(a,padui::Back);assert(a.document.nextNumber==1&&a.document.marks.size()==2);press(a,padui::Y);assert(a.document.nextNumber==3&&a.document.marks.size()==2);
 for(int tool:{0,1,2,4}){a.choosePadTool(tool);frame(a);auto history=a.document.undoStack.size();press(a,padui::Back);assert(a.document.nextNumber==3&&a.document.undoStack.size()==history);}
 a.choosePadTool(3);frame(a);press(a,padui::A);press(a,padui::Back);assert(a.document.nextNumber==3&&a.padQuick);press(a,padui::B);
 a.document=Document{};a.choosePadTool(0);frame(a);Mark stroke;stroke.points={{500,350},{700,350},{900,450}};a.document.add(stroke);Mark untouched=stroke;untouched.points={{100,600},{300,600}};a.document.add(untouched);
 a.padCursor={680,350};auto erase=neutral();erase.lt=255;auto history=a.document.undoStack.size();frame(a,erase);assert(a.document.marks.size()==1&&a.document.marks[0].points[0].x==100);a.padCursor={200,600};frame(a,erase);frame(a);assert(a.document.marks.empty()&&a.document.undoStack.size()==history+1);press(a,padui::Y);assert(a.document.marks.size()==2&&a.document.marks[0].points.size()==3);assert(a.document.redo()&&a.document.marks.empty());
 // A crossing sweep catches a thin distant segment without sampling gaps.
 Document d;stroke.points={{500,0},{500,1000}};d.add(stroke);assert(d.eraseWholeSweep({0,500},{1000,500},0)&&d.marks.empty());d.add(stroke);assert(!d.eraseWholeSweep({0,0},{100,0},1)&&d.marks.size()==1);
 Mark arrow;arrow.kind=Kind::Arrow;arrow.width=10;arrow.points={{0,0},{100,0}};d=Document{};d.add(arrow);assert(d.eraseWholeSweep({60,18},{60,18},0)&&d.marks.empty());
 d=Document{};stroke.points={{0,0},{100,0}};d.add(stroke);assert(d.eraseSweep({50,0},{50,0},10)&&d.marks.size()==2); // Mouse stays partial.
}
static void bindingsTests(){Context context;App a;open(a);assert(a.padBindings.valid());a.gridEnabled=false;a.padCursor={700,400};
 // All physical controls can drive drawing, with paired role swaps and rearm.
 for(int k=1;k<int(PadControl::Count);++k){assert(a.assignPadBinding(PadAction::Draw,PadControl(k)));frame(a);frame(a);a.choosePadTool(2);frame(a);auto count=a.document.marks.size();pressControl(a,PadControl(k));assert(a.document.marks.size()==count+1);}
 a.padBindings=PadBindings{};assert(a.assignPadBinding(PadAction::Wheel,PadControl::RT));assert(a.padBindings.get(PadAction::Draw)==PadControl::X);frame(a);frame(a);
 auto hold=controlInput(PadControl::RT);hold.ly=1;for(int i=0;i<18;++i)frame(a,hold);assert(a.padWheelVisible);frame(a);frame(a);assert(a.tool==Kind::Stroke); // Release selects via rebound trigger.
 assert(a.assignPadBinding(PadAction::Quick,PadControl::B));frame(a);frame(a);press(a,padui::B);assert(a.padQuick);press(a,padui::A);assert(!a.padQuick&&a.editing);
 assert(a.assignPadBinding(PadAction::RestartNumber,PadControl::Y));frame(a);frame(a);a.choosePadTool(3);frame(a);a.document.nextNumber=7;press(a,padui::Y);assert(a.document.nextNumber==1);press(a,padui::Back);assert(a.document.nextNumber==7);
 assert(a.assignPadBinding(PadAction::Redo,PadControl::LS));frame(a);frame(a);pressControl(a,PadControl::LS);assert(a.document.nextNumber==1);
 auto saved=encodePreferences(a);App restored;assert(decodePreferences(saved,restored)&&restored.padBindings.keys==a.padBindings.keys&&!restored.padHeld&&!restored.padReady);
 for(auto invalid:{"pad.bind.draw=0","pad.bind.cancel=0","pad.bind.wheel=999","pad.bind.undo=3","pad.bind.redo=-1"}){assert(!decodePreferences(std::string("format=1\n")+invalid+"\n",a));assert(encodePreferences(a)==saved);}
 assert(decodePreferences("format=1\n",restored)&&restored.padBindings.keys==PadBindings{}.keys);
 auto before=a.padBindings.keys;assert(!a.assignPadBinding(PadAction::Quick,PadControl::None)&&a.padBindings.keys==before);
 // Rebinding while held cannot draw or exit until all components return neutral.
 a.padBindings=PadBindings{};a.choosePadTool(2);frame(a);hold=controlInput(PadControl::A);assert(a.assignPadBinding(PadAction::Draw,PadControl::A));auto count=a.document.marks.size();for(int i=0;i<4;++i)frame(a,hold);assert(!a.padReady&&!a.padQuick&&a.document.marks.size()==count);frame(a);frame(a);press(a,padui::A);assert(a.document.marks.size()==count+1);
}
static void rebindNavigation(){Context context;App a;open(a);press(a,padui::Menu);assert(a.padQuick&&a.padPage==2);auto&g=*ImGui::GetCurrentContext();ImGuiWindow*body=nullptr;for(auto window:g.Windows)if(window->ParentWindow&&std::string(window->Name).find("##padContents")!=std::string::npos)body=window;assert(body);
 auto header=body->GetID("手柄改键");for(int i=0;i<40&&g.NavId!=header;++i)press(a,padui::Down);assert(g.NavId==header);press(a,padui::A);press(a,padui::Down);
 auto combo=ImHashStr("##binding",0,body->GetID(0));assert(g.NavId==combo);press(a,padui::A);assert(!g.OpenPopupStack.empty());press(a,padui::Down);press(a,padui::A);assert(a.padBindings.get(PadAction::Draw)==PadControl::Menu&&a.padBindings.get(PadAction::Settings)==PadControl::RT&&a.padQuick);
 press(a,padui::B);assert(!a.padQuick&&a.editing);a.padCursor={700,400};press(a,padui::Menu);assert(a.document.marks.size()==1);pressControl(a,PadControl::RT);assert(a.padQuick&&a.padPage==2);
}
static bool sameSnapshot(const Snapshot&a,const Snapshot&b){if(a.nextNumber!=b.nextNumber||a.marks.size()!=b.marks.size())return false;for(size_t i=0;i<a.marks.size();++i){auto&x=a.marks[i];auto&y=b.marks[i];if(x.kind!=y.kind||x.color!=y.color||x.width!=y.width||x.radius!=y.radius||x.number!=y.number||x.points.size()!=y.points.size())return false;for(size_t j=0;j<x.points.size();++j)if(x.points[j].x!=y.points[j].x||x.points[j].y!=y.points[j].y)return false;}return true;}
static void auditBindings(){
 for(int action=0;action<int(PadAction::Count);++action)for(int key=0;key<int(PadControl::Count);++key){PadBindings b;auto old=b.keys;bool ok=b.assign(PadAction(action),PadControl(key));if(ok){assert(b.valid()&&b.get(PadAction(action))==PadControl(key));for(int i=0;i<int(PadAction::Count);++i)for(int j=i+1;j<int(PadAction::Count);++j)assert(b.keys[i]==PadControl::None||b.keys[i]!=b.keys[j]);}else assert(b.keys==old);}
 Context context;for(int key=1;key<int(PadControl::Count);++key){App a;open(a);assert(a.assignPadBinding(PadAction::Quick,PadControl(key)));frame(a);frame(a);auto cancel=a.padBindings.get(PadAction::Cancel);pressControl(a,PadControl(key));assert(a.padQuick&&a.document.marks.empty());pressControl(a,cancel);assert(!a.padQuick&&a.editing&&a.document.marks.empty());pressControl(a,cancel);assert(!a.editing);}
}
static void auditHistory(){Context context;App a;open(a);a.gridEnabled=false;a.padCursor={600,350};Mark stroke;stroke.width=7;stroke.color=0xff123456;stroke.points={{500,350},{600,350},{700,350}};a.document.add(stroke);Mark number;number.kind=Kind::Number;number.points={{700,450}};a.document.add(number);auto original=a.document.snapshot();auto history=a.document.undoStack.size();auto erase=controlInput(PadControl::LT);frame(a,erase);auto deleted=a.document.snapshot();assert(deleted.marks.size()==1&&deleted.nextNumber==2);
 auto undoHeld=erase;undoHeld.buttons=padui::Y;frame(a,undoHeld);assert(sameSnapshot(a.document.snapshot(),original));for(int i=0;i<5;++i)frame(a,erase);assert(sameSnapshot(a.document.snapshot(),original)&&!a.padReady);frame(a);frame(a);assert(a.document.undoStack.size()==history&&a.document.redoStack.size()==1);
 // Empty erase and an already-at-one restart must not destroy redo.
 a.padCursor={1200,700};frame(a,erase);frame(a);assert(a.document.undoStack.size()==history&&a.document.redoStack.size()==1);assert(a.document.redo());assert(sameSnapshot(a.document.snapshot(),deleted));assert(a.document.undo());
 a.choosePadTool(3);frame(a);auto beforeReset=a.document.snapshot();press(a,padui::Back);auto reset=a.document.snapshot();assert(reset.nextNumber==1&&reset.marks.size()==2);auto resets=a.document.undoStack.size();press(a,padui::Back);assert(a.document.undoStack.size()==resets);press(a,padui::Y);assert(sameSnapshot(a.document.snapshot(),beforeReset));assert(a.document.redo());assert(sameSnapshot(a.document.snapshot(),reset));
 a.padCursor={900,450};rt(a);auto added=a.document.snapshot();assert(added.marks.back().number==1&&added.nextNumber==2);press(a,padui::Y);assert(sameSnapshot(a.document.snapshot(),reset));press(a,padui::Y);assert(sameSnapshot(a.document.snapshot(),beforeReset));assert(a.document.redo()&&sameSnapshot(a.document.snapshot(),reset));assert(a.document.redo()&&sameSnapshot(a.document.snapshot(),added));
 // Disconnect commits exactly one pending erase transaction.
 a.padCursor={600,350};history=a.document.undoStack.size();frame(a,erase);frame(a,{});assert(!a.erasing&&a.document.undoStack.size()==history+1);assert(a.document.undo()&&sameSnapshot(a.document.snapshot(),added));
}
static void auditMixedUndo(){Context context;App a;open(a);a.gridEnabled=false;a.padCursor={600,350};Mark stroke;stroke.points={{500,350},{700,350}};a.document.add(stroke);auto original=a.document.snapshot();auto erase=controlInput(PadControl::LT);frame(a,erase);assert(a.document.marks.empty());auto&io=ImGui::GetIO();io.AddKeyEvent(ImGuiMod_Ctrl,true);io.AddKeyEvent(ImGuiKey_Z,true);frame(a,erase);assert(sameSnapshot(a.document.snapshot(),original));io.AddKeyEvent(ImGuiKey_Z,false);io.AddKeyEvent(ImGuiMod_Ctrl,false);for(int i=0;i<4;++i)frame(a,erase);assert(sameSnapshot(a.document.snapshot(),original));frame(a);frame(a);assert(a.document.redoStack.size()==1);}
static void auditNumberGesture(){Context context;App a;open(a);a.choosePadTool(3);frame(a);a.grid.valid=true;a.grid.origin={600,300};a.grid.u={60,0};a.grid.v={0,60};a.gridEnabled=a.gridConfirmed=true;auto blank=a.document.snapshot();auto draw=controlInput(PadControl::RT);a.padCursor={600,300};frame(a,draw);a.padCursor={780,300};frame(a,draw);auto numbered=a.document.snapshot();assert(numbered.marks.size()==4&&numbered.nextNumber==5);auto resetHeld=draw;resetHeld.buttons=padui::Back;frame(a,resetHeld);auto restarted=a.document.snapshot();assert(restarted.marks.size()==4&&restarted.nextNumber==1&&a.document.undoStack.size()==2);for(int i=0;i<4;++i)frame(a,draw);assert(sameSnapshot(a.document.snapshot(),restarted));frame(a);frame(a);press(a,padui::Y);assert(sameSnapshot(a.document.snapshot(),numbered));press(a,padui::Y);assert(sameSnapshot(a.document.snapshot(),blank));auto redoCount=a.document.redoStack.size();press(a,padui::Back);assert(a.document.redoStack.size()==redoCount&&a.document.undoStack.empty());assert(a.document.redo()&&sameSnapshot(a.document.snapshot(),numbered));assert(a.document.redo()&&sameSnapshot(a.document.snapshot(),restarted));
 // A fresh edit after undo replaces the redo branch only when it changes data.
 press(a,padui::Y);auto erased=controlInput(PadControl::LT);a.padCursor={600,300};frame(a,erased);frame(a);assert(a.document.redoStack.empty());assert(a.document.undo()&&sameSnapshot(a.document.snapshot(),numbered));
}
static void auditConfirmRebind(){Context context;App a;open(a);press(a,padui::Menu);auto&g=*ImGui::GetCurrentContext();ImGuiWindow*body=nullptr;for(auto window:g.Windows)if(window->ParentWindow&&std::string(window->Name).find("##padContents")!=std::string::npos)body=window;assert(body);auto header=body->GetID("手柄改键");for(int i=0;i<40&&g.NavId!=header;++i)press(a,padui::Down);assert(g.NavId==header);press(a,padui::A);auto combo=ImHashStr("##binding",0,body->GetID(int(PadAction::Quick)));for(int i=0;i<20&&g.NavId!=combo;++i)press(a,padui::Down);assert(g.NavId==combo);press(a,padui::A);press(a,padui::Down);press(a,padui::A);assert(a.padBindings.get(PadAction::Quick)==PadControl::B&&a.padBindings.get(PadAction::Cancel)==PadControl::A&&a.padQuick);
 press(a,padui::A);assert(!a.padQuick&&a.editing);press(a,padui::B);assert(a.padQuick&&a.padPage==0);auto clear=body->GetID("清空所有标记...");for(int i=0;i<40&&g.NavId!=clear;++i)press(a,padui::Down);assert(g.NavId==clear);press(a,padui::B);assert(a.padConfirmClear);press(a,padui::A);assert(!a.padConfirmClear&&a.padQuick);press(a,padui::A);assert(!a.padQuick&&a.editing);
}
static void auditKeyboardHistoryFences(){Context context;auto&io=ImGui::GetIO();for(auto action:{Action::Redo,Action::Clear,Action::RestartNumber}){App a;open(a);a.gridEnabled=false;a.choosePadTool(3);frame(a);a.padCursor={600,350};rt(a);auto before=a.document.snapshot();if(action==Action::Redo){assert(a.document.undo());}assert(a.bindings.assign(action,{ImGuiKey_R}));auto draw=controlInput(PadControl::RT);frame(a,draw);io.AddKeyEvent(ImGuiKey_R,true);frame(a,draw);auto after=a.document.snapshot();assert(!a.padReady);io.AddKeyEvent(ImGuiKey_R,false);for(int i=0;i<4;++i)frame(a,draw);assert(sameSnapshot(after,a.document.snapshot()));frame(a);frame(a);assert(a.padReady);a.setEditing(false);frame(a);}}
static void finalUiBoundary(){Context c;App a;open(a);a.gridEnabled=false;a.padCursor={50,50};rt(a);assert(a.document.marks.empty());a.padCursor={200,200};rt(a);assert(a.document.marks.size()==1); // No phantom fallback-window blockage.
 a.document=Document{};Mark m;m.points={{930,900},{970,900}};a.document.add(m);auto before=a.document.snapshot();auto erase=controlInput(PadControl::LT);a.padCursor={600,900};frame(a,erase);a.padCursor={1000,1000};frame(a,erase);a.padCursor={1300,900};frame(a,erase);frame(a);assert(sameSnapshot(a.document.snapshot(),before));
 // A whole panel crossed between two samples also interrupts erasure.
 a.document=Document{};m.points={{1090,1000},{1110,1000}};a.document.add(m);before=a.document.snapshot();a.padCursor={500,900};frame(a,erase);a.padCursor={1400,1050};frame(a,erase);frame(a);assert(sameSnapshot(a.document.snapshot(),before));
 // Erase two real marks on opposite sides; the UI gap is not erased, one undo restores both.
 a.document=Document{};m.points={{590,900},{610,900}};a.document.add(m);m.points={{1290,900},{1310,900}};a.document.add(m);before=a.document.snapshot();auto undo=a.document.undoStack.size();a.padCursor={600,900};frame(a,erase);a.padCursor={1000,1000};frame(a,erase);a.padCursor={1300,900};frame(a,erase);frame(a);assert(a.document.marks.empty()&&a.document.undoStack.size()==undo+1);press(a,padui::Y);assert(sameSnapshot(a.document.snapshot(),before));
 a.document=Document{};a.choosePadTool(0);frame(a);auto draw=controlInput(PadControl::RT);a.padCursor={600,900};frame(a,draw);a.padCursor={650,900};frame(a,draw);a.padCursor={1000,1000};frame(a,draw);a.padCursor={1300,900};frame(a,draw);frame(a);assert(a.document.marks.size()==1&&a.document.marks[0].points.back().x==650&&!a.drawing);press(a,padui::Y);assert(a.document.marks.empty());
 // Numbering uses the same gap rule without breaking its grouped history.
 a.choosePadTool(3);frame(a);a.grid.valid=true;a.grid.origin={500,900};a.grid.u={50,0};a.grid.v={0,50};a.gridEnabled=a.gridConfirmed=true;a.padCursor={500,900};frame(a,draw);a.padCursor={1400,1050};frame(a,draw);frame(a);assert(a.document.marks.size()==2);press(a,padui::Y);assert(a.document.marks.empty()&&a.document.nextNumber==1);
}
static void finalPopupLifecycle(){Context c;App a;open(a);press(a,padui::A);auto&g=*ImGui::GetCurrentContext();ImGuiWindow*body=nullptr;for(auto w:g.Windows)if(w->ParentWindow&&std::string(w->Name).find("##padContents")!=std::string::npos)body=w;auto id=body->GetID("清空所有标记...");for(int i=0;i<40&&g.NavId!=id;++i)press(a,padui::Down);press(a,padui::A);assert(a.padConfirmClear);a.setEditing(false);frame(a);frame(a);assert(g.OpenPopupStack.empty());a.openFromPad();frame(a);frame(a);press(a,padui::A);assert(a.padQuick&&!a.padConfirmClear&&g.OpenPopupStack.empty());press(a,padui::B);assert(!a.padQuick&&a.editing);}
int main(){for(bool trickle:{false,true}){productionQueue=trickle;finalUiBoundary();finalPopupLifecycle();auditConfirmRebind();auditKeyboardHistoryFences();auditNumberGesture();auditBindings();auditHistory();auditMixedUndo();rebindNavigation();restartAndWholeErase();bindingsTests();wheelStyle();wheelTests();movement();tools();lifecycle();layout();navigation();}std::puts("controller UI (immediate and production queued input): tools/history, two-point arrow, grouped numbering/erase, menu guards, disconnect/rearm, mode/preferences, projected stepping seven layouts and radial hold/release/actions/cancellation passed");}
