#include "ui.hpp"
#include "imgui_internal.h"
#include <cstdio>
#include "controller_wheel.hpp"
namespace sketch {
void App::openFromPad(){padOpening=true;setEditing(true);}
void App::resetPadGesture(){padWheel=padWheelVisible=false;padWheelSelection=-1;finishGesture();cancelGesture();padReady=false;padRt=padLt=false;padRtPrevious=false;padDirection=0;}
void App::choosePadTool(int index){resetPadGesture();padEraseTool=index==4;if(index<4)tool=static_cast<Kind>(index);}
void App::padPrepare(const PadInput&input){
 padInput=input;auto&io=ImGui::GetIO();bool menu=editing&&padMenu()&&!padWheel&&input.connected;
 bool meaningful=input.buttons||input.lt>160||input.rt>160||length(padui::stick(input.lx,input.ly))>.2f||length(padui::stick(input.rx,input.ry))>.2f;
 if(editing&&input.connected&&meaningful&&!padPointer&&!io.MouseDown[0]&&!io.MouseDown[1]&&!drawing&&!erasing&&!numbering){resetPadGesture();padPointer=true;}
 if(!input.connected&&padPointer){resetPadGesture();padPointer=false;}
 io.ConfigFlags&=~ImGuiConfigFlags_NavEnableGamepad;
 if(menu)io.ConfigFlags|=ImGuiConfigFlags_NavEnableGamepad;
 if(input.connected)io.BackendFlags|=ImGuiBackendFlags_HasGamepad;else io.BackendFlags&=~ImGuiBackendFlags_HasGamepad;
 // Remember the layer BEFORE NewFrame handles the B event, preventing one
 // physical press from cancelling a slider/popup and closing its parent too.
 padNavBusy=ImGui::IsAnyItemActive()||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId);
 auto key=[&](ImGuiKey k,bool down){io.AddKeyEvent(k,menu&&padReady&&down);};
 key(ImGuiKey_GamepadFaceDown,(input.buttons&padui::A)!=0);
 key(ImGuiKey_GamepadFaceRight,(input.buttons&padui::B)!=0);
 auto stick=padui::stick(input.lx,input.ly);
 key(ImGuiKey_GamepadDpadLeft,(input.buttons&padui::Left)||stick.x<-.35f);
 key(ImGuiKey_GamepadDpadRight,(input.buttons&padui::Right)||stick.x>.35f);
 key(ImGuiKey_GamepadDpadUp,(input.buttons&padui::Up)||stick.y<-.35f);
 key(ImGuiKey_GamepadDpadDown,(input.buttons&padui::Down)||stick.y>.35f);
}
void App::padActions(float w,float h){
 using namespace padui;
 padPressed=uint16_t(padInput.buttons&~padPrevious);
 if(!editing)return;
 if(!padInput.connected){padReady=false;return;}
 if(padPointer&&!padMenu()&&ImGui::IsMouseClicked(0)&&!drawing&&!erasing&&!numbering){resetPadGesture();padPointer=false;inputSuppressed=true;return;}
 if(!padPointer)return;
 if(padWidth<=0||padHeight<=0)padCursor={w*.5f,h*.5f};
 else if(w!=padWidth||h!=padHeight){padCursor={padCursor.x*w/padWidth,padCursor.y*h/padHeight};resetPadGesture();}
 padWidth=w;padHeight=h;
 if(padWheel){
  if(padPressed&B){resetPadGesture();return;}
  if(!(padInput.buttons&X)){
   // Use the last held sample: X release and stick spring-back can arrive together.
   int selected=padWheelVisible?padWheelSelection:-1;resetPadGesture();if(selected>=0)applyPadWheel(selected);return;
  }
  padWheelVisible=ImGui::GetTime()-padWheelStarted>=.22;
  padWheelSelection=padui::wheelSelection(padInput.lx,padInput.ly,padWheelSelection);
  return;
 }
 if(!padReady){
  if(!padInput.buttons&&padInput.lt<=30&&padInput.rt<=30&&length(stick(padInput.lx,padInput.ly))==0&&length(stick(padInput.rx,padInput.ry))==0)padReady=true;
  return;
 }
 bool popup=ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId);
 if(padMenu()){
  if((padPressed&B)&&!padNavBusy&&!popup){
   if(reportOpen)reportOpen=false;else if(settingsOpen)settingsOpen=false;else if(cameraPickerOpen)cancelCameraPicker();else padQuick=false;
   resetPadGesture();return;
  }
  if((padPressed&Menu)&&!padNavBusy&&!popup){if(settingsOpen)settingsOpen=false;else if(padQuick)padQuick=false;resetPadGesture();return;}
  if(padQuick&&!padNavBusy&&!popup){if(padPressed&LB)padPage=(padPage+2)%3;if(padPressed&RB)padPage=(padPage+1)%3;}
  return;
 }
 if(padPressed&B){if(drawing||erasing||numbering)resetPadGesture();else wantExit=true;return;}
 if(padPressed&(A|Menu)){resetPadGesture();padQuick=true;padPage=(padPressed&Menu)?2:0;return;}
 if(padPressed&(LB|RB)){int index=padEraseTool?4:int(tool);choosePadTool((index+((padPressed&RB)?1:4))%5);return;}
 if(padPressed&X){resetPadGesture();padWheel=true;padWheelStarted=ImGui::GetTime();padWheelSelection=padui::wheelSelection(padInput.lx,padInput.ly);return;}
 if(padPressed&Y){resetPadGesture();document.undo();return;}
 padCursor=move(padCursor,padInput,ImGui::GetIO().DeltaTime,w,h,padSpeed);
 uint16_t direction=padInput.buttons&(Up|Down|Left|Right);double now=ImGui::GetTime();
 if(direction&&(direction!=padDirection||now>=padRepeatAt)){
  Point d{float(bool(direction&Right))-float(bool(direction&Left)),float(bool(direction&Down))-float(bool(direction&Up))};
  if(d.x!=0&&d.y!=0)d.y=0;
  if(length(d)>0)padCursor=step(padCursor,d,snappingReady()?&grid:nullptr,w,h);
  padRepeatAt=now+(direction!=padDirection?.3:.11);
 }
 padDirection=direction;
 padRt=padInput.rt>(padRt?55:100);padLt=padInput.lt>(padLt?55:100);
}
void App::padCanvas(float w,float h){
 if(!editing||!padReady||padMenu()||inputSuppressed)return;
 auto p=padCursor;bool over=p.x>=panelMin.x&&p.y>=panelMin.y&&p.x<panelMax.x&&p.y<panelMax.y;
 bool inside=p.x>=0&&p.y>=0&&p.x<w&&p.y<h&&!over;
 bool erase=padLt||(padEraseTool&&padRt);bool press=padRt&&!padRtPrevious;
 if(erasing&&!erase)finishGesture();
 bool snapNumbers=tool==Kind::Number&&snappingReady()&&!padEraseTool;
 if(numbering&&(!padRt||erase||!snapNumbers))finishGesture();
 if(numbering){if(inside)numberCells(grid.cell(p));else numberConnected=false;}
 auto dl=ImGui::GetBackgroundDrawList();auto fg=ImGui::GetForegroundDrawList();
 if(inside&&erase){cancelGesture();if(!erasing){eraseBefore=document.snapshot();eraseLast=p;erasing=true;eraseChanged=false;}eraseChanged=document.eraseSweep(eraseLast,p,eraseRadius*uiScale)||eraseChanged;eraseLast=p;}
 if(inside&&!erase&&!padEraseTool){
  Point target=(tool!=Kind::Stroke&&snappingReady())?grid.snap(p):p;
  if(press){
   if(tool==Kind::Arrow&&drawing){current.points.back()=target;if(distance(current.points.front(),target)>3*uiScale)document.add(current);cancelGesture();}
   else{
    current={};current.kind=tool;current.width=penWidth*uiScale;current.color=selectedColor();current.radius=(tool==Kind::Number?numberRadius:pinRadius)*uiScale;current.points={target};
    if(snapNumbers){numberBefore=document.snapshot();numbering=true;numberChanged=false;numberConnected=false;numberedCells.clear();numberCells(grid.cell(p));current.points.clear();}
    else if(tool==Kind::Pin||tool==Kind::Number){document.add(current);current.points.clear();}
    else{drawing=true;if(tool==Kind::Arrow)current.points.push_back(target);}
   }
  }
  if(drawing){if(tool==Kind::Arrow)current.points.back()=target;else if(padRt&&distance(current.points.back(),target)>.7f*uiScale)current.points.push_back(target);}
  if(!drawing&&(tool==Kind::Pin||tool==Kind::Number)){Mark preview;preview.kind=tool;preview.color=selectedColor();preview.points={target};preview.radius=(tool==Kind::Number?numberRadius:pinRadius)*uiScale;preview.number=document.nextNumber;drawMark(fg,preview,true);}
 }
 if(drawing&&tool==Kind::Stroke&&!padRt&&padRtPrevious){document.add(current);cancelGesture();}
 if(drawing)drawMark(dl,current,true);
 if(inside){auto ink=IM_COL32(249,224,153,240);float r=erase?eraseRadius*uiScale:8*uiScale;fg->AddCircle({p.x,p.y},r,ink,32,1.7f);fg->AddLine({p.x-13*uiScale,p.y},{p.x+13*uiScale,p.y},ink);fg->AddLine({p.x,p.y-13*uiScale},{p.x,p.y+13*uiScale},ink);}
}
void App::padPanel(float w,float h){
 float s=uiScale;const char*tools[]={"画笔","箭头","星标","编号","橡皮"};
 if(padLayout){
  float width=std::min(w-16*s,850*s),height=std::max(105*s,95.f);
  ImGui::SetNextWindowPos({(w-width)*.5f,std::max(0.f,h-height-8*s)},ImGuiCond_Always);ImGui::SetNextWindowSize({width,height},ImGuiCond_Always);
  ImGui::Begin("##PadToolbar",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNavFocus);
  auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();panelMin={pos.x,pos.y};panelMax={pos.x+size.x,pos.y+size.y};
  ImGui::BeginDisabled(padMenu());float bw=(ImGui::GetContentRegionAvail().x-4*ImGui::GetStyle().ItemSpacing.x)/5;
  for(int i=0;i<5;++i){if(i)ImGui::SameLine();bool selected=i==4?padEraseTool:!padEraseTool&&int(tool)==i;if(selected)ImGui::PushStyleColor(ImGuiCol_Button,{.42f,.33f,.15f,1});if(ImGui::Button(tools[i],{bw,29*s}))choosePadTool(i);if(selected)ImGui::PopStyleColor();}
  ImGui::TextUnformatted("RT 绘制  LT 擦除  LB/RB 工具  Y 撤销  长按 X 轮盘");
  if(ImGui::Button("A 快捷面板")){resetPadGesture();padQuick=true;padPage=0;}ImGui::SameLine();if(ImGui::Button("Menu 设置")){resetPadGesture();padQuick=true;padPage=2;}ImGui::SameLine();if(ImGui::Button("B 返回游戏"))wantExit=true;
  ImGui::EndDisabled();ImGui::End();
  ImGui::SetNextWindowPos({12*s,12*s},ImGuiCond_Always);ImGui::SetNextWindowSize({std::min(w-24*s,540*s),0});
  ImGui::Begin("##PadStatus",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_AlwaysAutoResize);
  auto notice=updateNotice(availableUpdate);if(!notice.empty())ImGui::TextColored({.43f,.89f,.56f,1},"%s",notice.c_str());
  ImGui::Text("手柄预览 6 · %s · 吸附 %s",tools[padEraseTool?4:int(tool)],snappingReady()?"已就绪":"未启用");
  ImGui::ColorButton("当前颜色",ImGui::ColorConvertU32ToFloat4(selectedColor()),ImGuiColorEditFlags_NoTooltip,{20*s,14*s});ImGui::SameLine();ImGui::Text("笔宽 %.1f · 下个编号 %d",penWidth,document.nextNumber);
  if(!padInput.connected)ImGui::TextWrapped("手柄已断开，可用鼠标继续操作。");
  if(!status.empty())ImGui::TextWrapped("%s",status.c_str());
  if(gameBuildWarning!=ErrorId::None)ImGui::TextWrapped("%s",errorMessage(gameBuildWarning).c_str());
  ImGui::End();
 }
 if(!padQuick)return;
 float width=std::min(w-20*s,570*s),height=std::min(h-28*s,620*s);
 ImGui::SetNextWindowPos({w*.5f,h*.5f},ImGuiCond_Always,{.5f,.5f});ImGui::SetNextWindowSize({width,height},ImGuiCond_Always);
 bool open=true;ImGui::Begin("草稿纸快捷面板",&open,ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoCollapse);
 if(ImGui::IsWindowAppearing())ImGui::SetWindowFocus();
 const char*pages[]={"颜色与工具","网格与相机","手柄与设置"};
 for(int i=0;i<3;++i){if(i)ImGui::SameLine();ImGui::PushID(i);if(ImGui::Button(pages[i]))padPage=i;if(i==padPage)ImGui::SetItemDefaultFocus();ImGui::PopID();}
 ImGui::TextDisabled("方向键选择 · A 确认 · B 返回 · LB/RB 翻页");ImGui::Separator();
 ImGui::BeginChild("##padContents",{0,-36*s},ImGuiChildFlags_NavFlattened);
 if(std::abs(padInput.ry)>.3f&&ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))ImGui::SetScrollY(ImGui::GetScrollY()-padInput.ry*500*s*std::clamp(ImGui::GetIO().DeltaTime,0.f,.05f));
 if(padPage==0){
  const char*names[]={"红色","白色","蓝色","自定义 1","自定义 2"};
  for(int i=0;i<5;++i){if(ImGui::RadioButton(names[i],colorIndex==i))colorIndex=i;}
  ImGui::SetNextItemWidth(200*s);ImGui::SliderFloat("笔触",&penWidth,1,12,"%.1f",ImGuiSliderFlags_NoInput);
  ImGui::SetNextItemWidth(200*s);ImGui::SliderFloat("橡皮半径",&eraseRadius,5,60,"%.0f",ImGuiSliderFlags_NoInput);
  ImGui::SetNextItemWidth(200*s);ImGui::SliderFloat("星标大小",&pinRadius,16,48,"%.0f",ImGuiSliderFlags_NoInput);
  ImGui::SetNextItemWidth(200*s);ImGui::SliderFloat("编号大小",&numberRadius,16,48,"%.0f",ImGuiSliderFlags_NoInput);
  ImGui::SetNextItemWidth(200*s);ImGui::SliderFloat("标记透明度",&markOpacity,.2f,.95f,"%.2f",ImGuiSliderFlags_NoInput);
  if(ImGui::Button("重做上次撤销")){resetPadGesture();document.redo();}
  if(ImGui::Button("编号从 1 开始（保留已有标记）"))restartNumbers();
  if(ImGui::Button("清空所有标记...")){padConfirmClear=true;ImGui::OpenPopup("确认清空");}
  for(int i=3;i<5;++i){ImGui::PushID(i);if(ImGui::CollapsingHeader(names[i])){auto c=ImGui::ColorConvertU32ToFloat4(colors[i]);bool changed=ImGui::SliderFloat("红",&c.x,0,1,"%.2f",ImGuiSliderFlags_NoInput);changed=ImGui::SliderFloat("绿",&c.y,0,1,"%.2f",ImGuiSliderFlags_NoInput)||changed;changed=ImGui::SliderFloat("蓝",&c.z,0,1,"%.2f",ImGuiSliderFlags_NoInput)||changed;if(changed)colors[i]=ImGui::ColorConvertFloat4ToU32(c);}ImGui::PopID();}
 }else if(padPage==1){
  if(ImGui::Checkbox("网格与吸附",&gridEnabled)&&gridEnabled&&!gridConfirmed&&!gridRecoveryActive&&!gridDirectionPending&&!levelPaused)requestGrid();
  if(ImGui::Checkbox("棋盘染色",&tint)&&tint&&!gridConfirmed&&!gridRecoveryActive&&!gridDirectionPending&&!levelPaused)requestGrid();
  ImGui::SliderFloat("染色透明度",&tintOpacity,.04f,.5f,"%.2f",ImGuiSliderFlags_NoInput);
  if(ImGui::Button("重新识别网格")){requestGrid();gridEnabled=true;}
  if(ImGui::Button("临时选择相机")){padQuick=false;resetPadGesture();startCameraPicker();}
  gridTuningPanel();
 }else{
  ImGui::Combo("界面模式（下次打开）",&padUiMode,"自动（按打开方式）\0固定键鼠\0固定手柄\0");
  ImGui::Checkbox("LT + RT 打开草稿纸",&padEntryEnabled);
  ImGui::TextWrapped("两个扳机在 180ms 内一起压下即打开；单独扳机增加约 180ms 等待。不需要按下摇杆。");
  ImGui::SliderFloat("光标速度",&padSpeed,.25f,3.f,"%.2f",ImGuiSliderFlags_NoInput);
  ImGui::Checkbox("切换关卡自动清除标注",&autoClearOnLevel);
  if(ImGui::Button(padLayout?"本次切换到键鼠界面":"本次切换到手柄界面")){resetPadGesture();padLayout=!padLayout;padQuick=false;}
  ImGui::TextWrapped("左摇杆移动，右摇杆微调；方向键逐格移动。箭头用两次 RT 定起点和终点，B 取消。自由画笔按住 RT 绘制。");
  ImGui::TextWrapped("%s",preferencesStatus.c_str());
  if(ImGui::CollapsingHeader("输入诊断"))ImGui::TextWrapped("%s",padStatus.c_str());
  reportButton();
 }
 if(ImGui::BeginPopupModal("确认清空",&padConfirmClear,ImGuiWindowFlags_AlwaysAutoResize)){
  if(padReady&&(padPressed&padui::B)){padConfirmClear=false;ImGui::CloseCurrentPopup();resetPadGesture();}
  ImGui::TextUnformatted("清空当前全部标记？可用 Y 撤销。");if(ImGui::Button("取消")){padConfirmClear=false;ImGui::CloseCurrentPopup();}ImGui::SetItemDefaultFocus();ImGui::SameLine();if(ImGui::Button("确认清空")){finishGesture();cancelGesture();document.clear();padConfirmClear=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
 }
 ImGui::EndChild();if(ImGui::Button("B 返回画布"))open=false;
 ImGui::End();if(!open){padQuick=false;resetPadGesture();}
}
}
