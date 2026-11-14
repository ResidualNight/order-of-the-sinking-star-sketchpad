#include "ui.hpp"
#include "version.hpp"
#include "imgui_internal.h"
#include <cstdio>
#include "pad_icons.hpp"
#include "controller_wheel.hpp"
namespace sketch {
namespace {
// Use virtual-pointer coordinates, not the OS mouse hover. Also reject a path
// crossing a panel between samples (fast sticks / a delayed render frame).
bool padUiBlocksSegment(Point a,Point b){
 for(auto window:ImGui::GetCurrentContext()->Windows){
  if(!window->Active||window->Hidden||window->IsFallbackWindow||(window->Flags&ImGuiWindowFlags_NoMouseInputs))continue;
  auto r=window->OuterRectClipped;if(r.Min.x>=r.Max.x||r.Min.y>=r.Max.y)continue;float enter=0,leave=1;auto d=b-a;
  auto clip=[&](float start,float delta,float lo,float hi){if(std::abs(delta)<1e-6f)return start>=lo&&start<=hi;float t0=(lo-start)/delta,t1=(hi-start)/delta;if(t0>t1)std::swap(t0,t1);enter=std::max(enter,t0);leave=std::min(leave,t1);return enter<=leave;};
  if(clip(a.x,d.x,r.Min.x,r.Max.x)&&clip(a.y,d.y,r.Min.y,r.Max.y))return true;
 }return false;
}
}
void App::openFromPad(){padOpening=true;setEditing(true);}
void App::resetPadGesture(){padWheel=padWheelVisible=false;padWheelSelection=-1;finishGesture();cancelGesture();padReady=false;padRt=padLt=false;padRtPrevious=false;padDirection=0;padEraseConnected=false;}
void App::choosePadTool(int index){resetPadGesture();padEraseTool=index==4;if(index<4)tool=static_cast<Kind>(index);}
void App::padPrepare(const PadInput&input){
 padInput=input;padHeld=padControls(input,padHeld);auto&io=ImGui::GetIO();bool menu=editing&&padMenu()&&!padWheel&&input.connected;
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
 key(ImGuiKey_GamepadFaceDown,padDown(PadAction::Quick));
 key(ImGuiKey_GamepadFaceRight,padDown(PadAction::Cancel));
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
  if(padHit(PadAction::Cancel)){resetPadGesture();return;}
  if(!padDown(PadAction::Wheel)){
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
  if((padHit(PadAction::Cancel))&&!padNavBusy&&!popup){
   if(reportOpen)reportOpen=false;else if(settingsOpen)settingsOpen=false;else if(cameraPickerOpen)cancelCameraPicker();else padQuick=false;
   resetPadGesture();return;
  }
  if((padHit(PadAction::Settings))&&!padNavBusy&&!popup){if(settingsOpen)settingsOpen=false;else if(padQuick)padQuick=false;resetPadGesture();return;}
  if(padQuick&&!padNavBusy&&!popup){if(padHit(PadAction::Previous))padPage=(padPage+2)%3;if(padHit(PadAction::Next))padPage=(padPage+1)%3;}
  return;
 }
 if(padHit(PadAction::Cancel)){if(drawing||erasing||numbering)resetPadGesture();else wantExit=true;return;}
 if(padHit(PadAction::Quick)||padHit(PadAction::Settings)){resetPadGesture();padQuick=true;padPage=(padHit(PadAction::Settings))?2:0;return;}
 if(padHit(PadAction::Previous)||padHit(PadAction::Next)){int index=padEraseTool?4:int(tool);choosePadTool((index+((padHit(PadAction::Next))?1:4))%5);return;}
 if(padHit(PadAction::Wheel)){resetPadGesture();padWheel=true;padWheelStarted=ImGui::GetTime();padWheelSelection=padui::wheelSelection(padInput.lx,padInput.ly);return;}
 if(padHit(PadAction::Undo)){resetPadGesture();document.undo();return;}
 if(padHit(PadAction::Redo)){resetPadGesture();document.redo();return;}
 if(padHit(PadAction::RestartNumber)&&tool==Kind::Number&&!padEraseTool){resetPadGesture();restartNumbers();return;}
 padCursor=move(padCursor,padInput,ImGui::GetIO().DeltaTime,w,h,padSpeed);
 uint16_t direction=padInput.buttons&(Up|Down|Left|Right);double now=ImGui::GetTime();
 if(direction&&(direction!=padDirection||now>=padRepeatAt)){
  Point d{float(bool(direction&Right))-float(bool(direction&Left)),float(bool(direction&Down))-float(bool(direction&Up))};
  if(d.x!=0&&d.y!=0)d.y=0;
  if(length(d)>0)padCursor=step(padCursor,d,snappingReady()?&grid:nullptr,w,h);
  padRepeatAt=now+(direction!=padDirection?.3:.11);
 }
 padDirection=direction;
 padRt=padDown(PadAction::Draw);padLt=padDown(PadAction::Erase);
}
void App::padCanvas(float w,float h){
 if(!editing||!padReady||padMenu()||inputSuppressed)return;
 auto p=padCursor;bool over=padUiBlocksSegment(p,p);
 bool inside=p.x>=0&&p.y>=0&&p.x<w&&p.y<h&&!over;
 bool erase=padLt||(padEraseTool&&padRt);bool press=padRt&&!padRtPrevious;
 if(erasing&&!erase)finishGesture();
 bool snapNumbers=tool==Kind::Number&&snappingReady()&&!padEraseTool;
 if(numbering&&(!padRt||erase||!snapNumbers))finishGesture();
 if(numbering){if(inside){if(numberConnected&&padUiBlocksSegment(grid.at(numberLast.x,numberLast.y),p))numberConnected=false;numberCells(grid.cell(p));}else numberConnected=false;}
 if(!erase||!inside)padEraseConnected=false;
 if(drawing&&tool==Kind::Stroke&&(!inside||padUiBlocksSegment(current.points.back(),p))){document.add(current);cancelGesture();}
 auto dl=ImGui::GetBackgroundDrawList();auto fg=ImGui::GetForegroundDrawList();
 if(inside&&erase){cancelGesture();if(!erasing){eraseBefore=document.snapshot();erasing=true;eraseChanged=false;padEraseConnected=false;}Point from=padEraseConnected&&!padUiBlocksSegment(eraseLast,p)?eraseLast:p;eraseChanged=document.eraseWholeSweep(from,p,eraseRadius*uiScale)||eraseChanged;eraseLast=p;padEraseConnected=true;}
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
 float s=uiScale;auto key=[&](PadAction a){return padBindings.get(a);};const char*tools[]={"画笔","箭头","星标","编号","整笔橡皮"};
 if(padLayout){
  bool numberHint=tool==Kind::Number&&!padEraseTool;
  float width=std::min(w-16*s,850*s),height=std::max(115*s,95.f)+(numberHint?28*s:0);
  ImGui::SetNextWindowPos({(w-width)*.5f,std::max(0.f,h-height-8*s)},ImGuiCond_Always);ImGui::SetNextWindowSize({width,height},ImGuiCond_Always);
  ImGui::Begin("##PadToolbar",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNavFocus);
  auto pos=ImGui::GetWindowPos(),size=ImGui::GetWindowSize();panelMin={pos.x,pos.y};panelMax={pos.x+size.x,pos.y+size.y};
  ImGui::BeginDisabled(padMenu());float bw=(ImGui::GetContentRegionAvail().x-4*ImGui::GetStyle().ItemSpacing.x)/5;
  for(int i=0;i<5;++i){if(i)ImGui::SameLine();bool selected=i==4?padEraseTool:!padEraseTool&&int(tool)==i;if(selected)ImGui::PushStyleColor(ImGuiCol_Button,{.42f,.33f,.15f,1});if(ImGui::Button(tools[i],{bw,29*s}))choosePadTool(i);if(selected)ImGui::PopStyleColor();}
  padicons::row({{key(PadAction::Draw),"绘制"},{key(PadAction::Erase),"整笔擦除"},{key(PadAction::Previous),"上个"},{key(PadAction::Next),"下个"},{key(PadAction::Undo),"撤销"},{key(PadAction::Wheel),"长按轮盘"}},24*s);
  if(padicons::button("##padQuick",key(PadAction::Quick),"快捷面板",24*s)){resetPadGesture();padQuick=true;padPage=0;}ImGui::SameLine();if(padicons::button("##padSettings",key(PadAction::Settings),"设置",24*s)){resetPadGesture();padQuick=true;padPage=2;}ImGui::SameLine();if(padicons::button("##padExit",key(PadAction::Cancel),"返回游戏",24*s))wantExit=true;
  if(numberHint)padicons::centered(ImGui::GetWindowDrawList(),{pos.x+size.x*.5f,pos.y+size.y-30*s},{{key(PadAction::RestartNumber),"编号从 1 重新开始"}},24*s,ImGui::GetFontSize());
  ImGui::EndDisabled();ImGui::End();
  ImGui::SetNextWindowPos({12*s,12*s},ImGuiCond_Always);ImGui::SetNextWindowSize({std::min(w-24*s,540*s),0});
  ImGui::Begin("##PadStatus",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoNavFocus|ImGuiWindowFlags_AlwaysAutoResize);
  auto notice=updateNotice(availableUpdate);if(!notice.empty())ImGui::TextColored({.43f,.89f,.56f,1},"%s",notice.c_str());
  ImGui::Text("草稿纸 %s · %s · 吸附 %s",CurrentVersion,tools[padEraseTool?4:int(tool)],snappingReady()?"已就绪":"未启用");
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
 padicons::row({{PadControl::Dpad,"选择"},{key(PadAction::Quick),"确认"},{key(PadAction::Cancel),"返回"},{key(PadAction::Previous),"上页"},{key(PadAction::Next),"下页"}},22*s);ImGui::Separator();
 ImGui::BeginChild("##padContents",{0,-36*s},ImGuiChildFlags_NavFlattened);
 if(std::abs(padInput.ry)>.3f&&ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows))ImGui::SetScrollY(ImGui::GetScrollY()-padInput.ry*500*s*std::clamp(ImGui::GetIO().DeltaTime,0.f,.05f));
 if(padPage==0){
  const char*names[]={"红色","白色","蓝色","自定义 1","自定义 2"};
  for(int i=0;i<5;++i){if(ImGui::RadioButton(names[i],colorIndex==i))colorIndex=i;}
  ImGui::SetNextItemWidth(200*s);ImGui::SliderFloat("笔触",&penWidth,1,12,"%.1f",ImGuiSliderFlags_NoInput);
  ImGui::SetNextItemWidth(200*s);ImGui::SliderFloat("整笔擦除范围",&eraseRadius,5,60,"%.0f",ImGuiSliderFlags_NoInput);
  ImGui::SetNextItemWidth(200*s);ImGui::SliderFloat("星标大小",&pinRadius,16,48,"%.0f",ImGuiSliderFlags_NoInput);
  ImGui::SetNextItemWidth(200*s);ImGui::SliderFloat("编号大小",&numberRadius,16,48,"%.0f",ImGuiSliderFlags_NoInput);
  ImGui::SetNextItemWidth(200*s);ImGui::SliderFloat("标记透明度",&markOpacity,.2f,.95f,"%.2f",ImGuiSliderFlags_NoInput);
  if(ImGui::Button("重做上次撤销")){resetPadGesture();document.redo();}
  ImGui::BeginDisabled(tool!=Kind::Number||padEraseTool);if(ImGui::Button("编号从 1 开始（保留已有标记）"))restartNumbers();ImGui::EndDisabled();
  if(ImGui::Button("清空所有标记...")){padConfirmClear=true;ImGui::OpenPopup("确认清空");}
  for(int i=3;i<5;++i){ImGui::PushID(i);if(ImGui::CollapsingHeader(names[i])){auto c=ImGui::ColorConvertU32ToFloat4(colors[i]);bool changed=ImGui::SliderFloat("红",&c.x,0,1,"%.2f",ImGuiSliderFlags_NoInput);changed=ImGui::SliderFloat("绿",&c.y,0,1,"%.2f",ImGuiSliderFlags_NoInput)||changed;changed=ImGui::SliderFloat("蓝",&c.z,0,1,"%.2f",ImGuiSliderFlags_NoInput)||changed;if(changed)colors[i]=ImGui::ColorConvertFloat4ToU32(c);}ImGui::PopID();}
 }else if(padPage==1){
  if(ImGui::Checkbox("网格与吸附",&gridEnabled)&&gridEnabled&&!gridConfirmed&&!gridRecoveryActive&&!gridDirectionPending&&!levelPaused)requestGrid();
  if(ImGui::Checkbox("棋盘染色",&tint)&&tint&&!gridConfirmed&&!gridRecoveryActive&&!gridDirectionPending&&!levelPaused)requestGrid();
  ImGui::SliderFloat("染色透明度",&tintOpacity,.04f,.5f,"%.2f",ImGuiSliderFlags_NoInput);
  if(ImGui::Button("重新识别网格")){requestImmediateGrid();}
  if(ImGui::Button("临时选择相机")){padQuick=false;resetPadGesture();startCameraPicker();}
  gridTuningPanel();
 }else{
  ImGui::Combo("界面模式（下次打开）",&padUiMode,"自动（按打开方式）\0固定键鼠\0固定手柄\0");
  padicons::entry(padEntryEnabled,s);
  ImGui::TextWrapped("两个扳机在 180ms 内一起压下即打开；单独扳机增加约 180ms 等待。不需要按下摇杆。");
  padBindingsPanel();
  ImGui::SliderFloat("光标速度",&padSpeed,.25f,3.f,"%.2f",ImGuiSliderFlags_NoInput);
  float wheelSize=padWheelScale*100,transparency=(1-padWheelOpacity)*100;
  if(ImGui::SliderFloat("轮盘大小",&wheelSize,50,150,"%.0f%%",ImGuiSliderFlags_NoInput))padWheelScale=wheelSize/100;
  if(ImGui::SliderFloat("轮盘透明度",&transparency,0,75,"%.0f%%",ImGuiSliderFlags_NoInput))padWheelOpacity=1-transparency/100;
  ImGui::TextWrapped("默认大小 80%%；透明度越高越通透。小窗口会自动限制轮盘大小。");
  if(ImGui::Button("恢复轮盘默认")){padWheelScale=.8f;padWheelOpacity=1;}
  ImGui::Checkbox("切换关卡自动清除标注",&autoClearOnLevel);
  if(ImGui::Button(padLayout?"本次切换到键鼠界面":"本次切换到手柄界面")){resetPadGesture();padLayout=!padLayout;padQuick=false;}
  padicons::row({{PadControl::LeftStick,"移动"},{PadControl::RightStick,"微调 / 滚动"},{PadControl::Dpad,"逐格移动"}},24*s);
  padicons::row({{key(PadAction::Draw),"绘制 / 放置"},{key(PadAction::Cancel),"取消"}},24*s);
  ImGui::TextWrapped("箭头按两次绘制键定起点和终点；自由画笔按住绘制键。橡皮命中即删除整条笔迹，可撤销。编号重置快捷键仅在编号模式下生效。");
  ImGui::TextWrapped("%s",preferencesStatus.c_str());
  if(ImGui::CollapsingHeader("输入诊断"))ImGui::TextWrapped("%s",padStatus.c_str());
  reportButton();
 }
 if(ImGui::BeginPopupModal("确认清空",&padConfirmClear,ImGuiWindowFlags_AlwaysAutoResize)){
  if(padReady&&(padHit(PadAction::Cancel))){padConfirmClear=false;ImGui::CloseCurrentPopup();resetPadGesture();}
  ImGui::TextUnformatted("清空当前全部标记？");padicons::row({{key(PadAction::Undo),"可撤销"}},24*s);if(ImGui::Button("取消")){padConfirmClear=false;ImGui::CloseCurrentPopup();}ImGui::SetItemDefaultFocus();ImGui::SameLine();if(ImGui::Button("确认清空")){finishGesture();cancelGesture();document.clear();padConfirmClear=false;ImGui::CloseCurrentPopup();}ImGui::EndPopup();
 }
 ImGui::EndChild();if(padicons::button("##padReturn",key(PadAction::Cancel),"返回画布",24*s))open=false;
 ImGui::End();if(!open){padQuick=false;resetPadGesture();}
}
}

namespace sketch {
bool App::assignPadBinding(PadAction action,PadControl control){
 int other=padBindings.conflict(action,control);
 if(!padBindings.assign(action,control)){inputError=ErrorId::KeyBindingConflict;padBindingStatus=errorMessage(inputError);return false;}
 resetPadGesture();inputSuppressed=true;
 padBindingStatus=other>=0?std::string("已交换：")+PadActionLabels[int(action)]+" / "+PadActionLabels[other]:std::string("已修改：")+PadActionLabels[int(action)];return true;
}
void App::padBindingsPanel(){
 if(!ImGui::CollapsingHeader("手柄改键"))return;
 float s=uiScale;
 ImGui::TextWrapped("选择按键图标更改绑定。选中已占用的按键会交换两个功能；提示图标同步更新。双扳机入口、摇杆移动和方向键导航固定。重做可不分配，其他功能保留按键。");
 for(int i=0;i<int(PadAction::Count);++i){
  auto action=PadAction(i);ImGui::PushID(i);auto selected=padBindings.get(action);
  ImGui::SetNextItemWidth(170*s);auto pos=ImGui::GetCursorScreenPos();bool open=ImGui::BeginCombo("##binding","",ImGuiComboFlags_HeightLarge);
  if(open){
   for(int k=0;k<int(PadControl::Count);++k){auto control=PadControl(k);if(!k&&action!=PadAction::Redo)continue;
    ImGui::PushID(k);auto p=ImGui::GetCursorScreenPos();bool chosen=ImGui::Selectable("##control",control==selected,0,{145*s,27*s});padicons::drawHint(ImGui::GetWindowDrawList(),p,{control,PadControlNames[k]},25*s,ImGui::GetFontSize());
    if(control==selected)ImGui::SetItemDefaultFocus();if(chosen)assignPadBinding(action,control);ImGui::PopID();
   }ImGui::EndCombo();
  }
  padicons::drawHint(ImGui::GetWindowDrawList(),{pos.x+5*s,pos.y+2*s},{padBindings.get(action),PadControlNames[int(padBindings.get(action))]},22*s,ImGui::GetFontSize());
  ImGui::SameLine();ImGui::TextWrapped("%s",PadActionLabels[i]);ImGui::PopID();
 }
 if(ImGui::Button("恢复默认手柄按键")){padBindings=PadBindings{};resetPadGesture();inputSuppressed=true;padBindingStatus="已恢复默认手柄按键";}
 if(!padBindingStatus.empty())ImGui::TextWrapped("%s",padBindingStatus.c_str());
}
}