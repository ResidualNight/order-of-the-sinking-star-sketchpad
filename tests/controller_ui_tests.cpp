#include "ui.hpp"
#include "preferences.hpp"
#include "imgui_internal.h"
#include <cassert>
#include <cstdio>
using namespace sketch;
struct Context{Context(){ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigInputTrickleEventQueue=false;unsigned char*p;int w,h;io.Fonts->GetTexDataAsRGBA32(&p,&w,&h);io.AddMousePosEvent(-100,-100);}~Context(){ImGui::DestroyContext();}};
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
 selectWheel(a,6);assert(a.gridEnabled&&!a.gridConfirmed&&a.document.marks.size()==count);a.gridRefresh.cancel();
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
int main(){wheelTests();movement();tools();lifecycle();layout();navigation();std::puts("controller UI: tools/history, two-point arrow, grouped numbering/erase, menu guards, disconnect/rearm, mode/preferences, projected stepping seven layouts and radial hold/release/actions/cancellation passed");}
