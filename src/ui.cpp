#include "ui.hpp"
#include <cstdio>
namespace sketch {
static ImVec2 im(Point p){return{p.x,p.y};}
static Point pt(ImVec2 p){return{p.x,p.y};}
static ImU32 faded(uint32_t c,float alpha){return(c&0xffffffu)|(uint32_t(std::clamp(alpha,0.f,1.f)*255)<<24);}
static constexpr ImU32 Gold=IM_COL32(224,190,109,255),Pale=IM_COL32(245,225,173,255);
static void keycap(ImDrawList* dl,ImVec2 a,ImVec2 b,const char* key,bool on=false){
 dl->AddRectFilled({a.x+2,a.y+3},{b.x+2,b.y+3},IM_COL32(0,0,0,95),5);
 dl->AddRectFilled(a,b,on?IM_COL32(73,66,39,250):IM_COL32(22,32,48,250),5);
 dl->AddRect(a,b,IM_COL32(104,83,43,255),5,0,2);
 dl->AddRect({a.x+2,a.y+2},{b.x-2,b.y-2},on?Pale:Gold,3,0,1.3f);
 dl->AddLine({a.x+5,a.y+5},{b.x-5,a.y+5},IM_COL32(255,239,160,90),1);
 float font=ImGui::GetFontSize()*.86f;auto sz=ImGui::GetFont()->CalcTextSizeA(font,1e6,0,key);
 dl->AddText(ImGui::GetFont(),font,{(a.x+b.x-sz.x)*.5f,(a.y+b.y-sz.y)*.5f-1},on?IM_COL32(255,237,126,255):Gold,key);
}
static bool row(const char* id,const char* key,const char* text,bool on=false){
 float s=ImGui::GetFontSize()/19.f;ImVec2 p=ImGui::GetCursorScreenPos();float h=34*s,w=ImGui::GetContentRegionAvail().x;
 bool clicked=ImGui::InvisibleButton(id,{w,h});auto dl=ImGui::GetWindowDrawList();if(ImGui::IsItemHovered())dl->AddRectFilled(p,{p.x+w,p.y+h},IM_COL32(215,181,97,22),3);
 keycap(dl,{p.x+2*s,p.y+3*s},{p.x+76*s,p.y+30*s},key,on);
 dl->AddText({p.x+89*s,p.y+7*s},on?Pale:IM_COL32(193,194,188,255),text);if(on)dl->AddCircleFilled({p.x+w-8*s,p.y+17*s},2.8*s,Gold);
 return clicked;
}
void App::resize(float w,float h){if(oldWidth>0&&oldHeight>0&&(w!=oldWidth||h!=oldHeight)){float sx=w/oldWidth,sy=h/oldHeight;finishGesture();document.rescale(sx,sy);grid.origin={grid.origin.x*sx,grid.origin.y*sy};grid.u={grid.u.x*sx,grid.u.y*sy};grid.v={grid.v.x*sx,grid.v.y*sy};cancelGesture();finishGesture();manual=false;calibration.clear();}oldWidth=w;oldHeight=h;}
void App::acceptDetection(const Detection&r){needsGrid=false;if(r.grid.usable()&&r.grid.confidence>=.55f){grid=r.grid;gridFromGame=false;gridConfirmed=true;status="已识别网格，请检查格子中心；可随时手动调整";}else if(r.grid.usable()&&!gridConfirmed){grid=r.grid;gridFromGame=false;gridConfirmed=false;status="检测到候选网格，请确认或手动调整；暂不启用吸附";}else{status=gridConfirmed?"自动识别不可靠，保留当前网格；可手动调整":"未能可靠识别网格，请点击调整网格或按 Shift+G";}}
void App::finishGesture(){if(erasing&&eraseChanged)document.record(eraseBefore);erasing=false;eraseChanged=false;}
void App::drawMark(ImDrawList*dl,const Mark&m,bool preview){if(m.points.empty())return;float alpha=markOpacity*(preview?.65f:1.f);ImU32 ink=faded(m.color,alpha);
 if(m.kind==Kind::Stroke){if(m.points.size()==1)dl->AddCircleFilled(im(m.points[0]),m.width*.5f,ink);else{std::vector<ImVec2> points;for(auto&p:m.points)points.push_back(im(p));dl->AddPolyline(points.data(),int(points.size()),ink,0,m.width);}return;}
 if(m.kind==Kind::Arrow){Point a=m.points[0],b=m.points.back(),delta=b-a;float n=length(delta);if(n<1)return;Point d=delta*(1/n),normal{-d.y,d.x};float head=std::min(n*.45f,std::max(12.f*uiScale,m.width*4));dl->AddLine(im(a),im(b),ink,m.width);dl->AddTriangleFilled(im(b),im(b-d*head+normal*(head*.43f)),im(b-d*head-normal*(head*.43f)),ink);return;}
 Point c=m.points[0];float radius=m.radius;ImVec2 verts[16],inner[16];for(int i=0;i<16;i++){float a=-1.5707963f+i*3.14159265f/8,r=radius*(i%2?.57f:1.f);verts[i]={c.x+std::cos(a)*r,c.y+std::sin(a)*r};inner[i]={c.x+std::cos(a)*r*.84f,c.y+std::sin(a)*r*.84f};}
 dl->AddConcavePolyFilled(verts,16,faded(IM_COL32(19,32,52,255),alpha));dl->AddPolyline(verts,16,faded(IM_COL32(76,61,39,255),alpha),ImDrawFlags_Closed,4*uiScale);dl->AddPolyline(verts,16,faded(IM_COL32(224,192,114,255),alpha),ImDrawFlags_Closed,1.7f*uiScale);dl->AddPolyline(inner,16,faded(IM_COL32(255,232,158,255),alpha*.65f),ImDrawFlags_Closed,.8f*uiScale);
 char label[24];std::snprintf(label,sizeof(label),"%d",m.number);float fs=radius*(m.number>=100?.61f:.88f);auto size=ImGui::GetFont()->CalcTextSizeA(fs,1e6,0,label);dl->AddText(ImGui::GetFont(),fs,{c.x-size.x*.5f+1,c.y-size.y*.5f+1},faded(IM_COL32(0,0,0,255),alpha),label);dl->AddText(ImGui::GetFont(),fs,{c.x-size.x*.5f,c.y-size.y*.5f},faded(IM_COL32(255,229,147,255),std::min(.98f,alpha+.12f)),label);
}
void App::drawGrid(ImDrawList*dl,float w,float h){if(!grid.usable())return;Point corners[]={{0,0},{w,0},{w,h},{0,h}};float xmin=1e6,ymin=1e6,xmax=-1e6,ymax=-1e6;for(auto p:corners){auto q=grid.cell(p);if(!std::isfinite(q.x)||!std::isfinite(q.y))return;xmin=std::min(xmin,q.x);xmax=std::max(xmax,q.x);ymin=std::min(ymin,q.y);ymax=std::max(ymax,q.y);}int x0=int(std::floor(xmin))-1,x1=int(std::ceil(xmax))+1,y0=int(std::floor(ymin))-1,y1=int(std::ceil(ymax))+1;if(x1-x0>220||y1-y0>220)return;
 dl->PushClipRect({0,0},{w,h},true);
 if(tint&&gridConfirmed)for(int y=y0;y<y1;y++)for(int x=x0;x<x1;x++){if(grid.denominator(x-.5f,y-.5f)<=.02f||grid.denominator(x+.5f,y+.5f)<=.02f)continue;ImVec2 q[]={im(grid.at(x-.5f,y-.5f)),im(grid.at(x+.5f,y-.5f)),im(grid.at(x+.5f,y+.5f)),im(grid.at(x-.5f,y+.5f))};dl->AddConvexPolyFilled(q,4,faded(((x+y)&1)?IM_COL32(29,48,79,255):IM_COL32(241,206,125,255),tintOpacity));}
 if(editing&&gridEnabled){ImU32 gc=IM_COL32(234,213,143,90);for(int x=x0;x<=x1;x++)dl->AddLine(im(grid.at(x-.5f,float(y0)-.5f)),im(grid.at(x-.5f,float(y1)+.5f)),gc,1);for(int y=y0;y<=y1;y++)dl->AddLine(im(grid.at(float(x0)-.5f,y-.5f)),im(grid.at(float(x1)+.5f,y-.5f)),gc,1);if(x1-x0<90&&y1-y0<90)for(int y=y0;y<=y1;y++)for(int x=x0;x<=x1;x++)dl->AddCircleFilled(im(grid.at(float(x),float(y))),1.5f*uiScale,IM_COL32(255,232,155,140),6);}
 dl->PopClipRect();
}
void App::panel(){float s=uiScale;ImGui::SetNextWindowPos({18*s,22*s},ImGuiCond_FirstUseEver);ImGui::SetNextWindowSize({326*s,0},ImGuiCond_Always);ImGui::SetNextWindowSizeConstraints({326*s,0},{326*s,ImGui::GetIO().DisplaySize.y-36*s});
 ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{17*s,14*s});ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{7*s,5*s});
 ImGui::Begin("##StarSketch",nullptr,ImGuiWindowFlags_NoTitleBar|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_AlwaysAutoResize);
 auto dl=ImGui::GetWindowDrawList();ImVec2 wp=ImGui::GetWindowPos(),ws=ImGui::GetWindowSize();dl->AddRect({wp.x+4*s,wp.y+4*s},{wp.x+ws.x-4*s,wp.y+ws.y-4*s},IM_COL32(173,140,74,100),5*s);
 ImGui::TextColored(ImVec4(.94f,.80f,.48f,1),"沉星草稿");ImGui::SameLine();ImGui::TextDisabled("  屏幕标注");ImGui::Separator();
 if(row("pen","B","自由画笔",tool==Kind::Stroke)){cancelGesture();tool=Kind::Stroke;}
 if(row("arrow","A","箭头",tool==Kind::Arrow)){cancelGesture();tool=Kind::Arrow;}
 if(row("pin","P","编号星标",tool==Kind::Pin)){cancelGesture();tool=Kind::Pin;}
 row("eraser","右键","按住擦除");
 if(row("grid","G","网格与吸附",gridEnabled)){gridEnabled=!gridEnabled;if(gridEnabled&&!gridConfirmed)needsGrid=true;}
 if(row("tint","T","棋盘染色",tint)){tint=!tint;if(tint&&!gridConfirmed)needsGrid=true;}
 if(row("manual","Shift+G","透视四点校准",manual))startManual();
 if(manual&&manualPerspective){ImGui::SetNextItemWidth(145*s);ImGui::InputInt("棋盘列数",&manualColumns);ImGui::SetNextItemWidth(145*s);ImGui::InputInt("棋盘行数",&manualRows);manualColumns=std::clamp(manualColumns,1,200);manualRows=std::clamp(manualRows,1,200);ImGui::Text("已选 %d / 4 个外角",int(calibration.size()));}
 if(ImGui::Button("快速三点校准",{0,25*s}))startManual(false);
 if(ImGui::Button("重新识别",{137*s,29*s})){needsGrid=true;gridEnabled=true;manual=false;calibration.clear();}ImGui::SameLine();if(ImGui::Button("退出绘画  E",{137*s,29*s}))wantExit=true;
 if(grid.usable()&&!gridConfirmed){if(ImGui::Button("采用候选网格",{0,29*s})){gridConfirmed=true;status="已采用候选网格，可继续精调";}}
 ImGui::Spacing();ImGui::Separator();
 static uint32_t colors[]={IM_COL32(240,204,109,255),IM_COL32(238,111,113,255),IM_COL32(113,187,244,255),IM_COL32(128,214,163,255),IM_COL32(248,240,211,255)};
 for(int i=0;i<5;i++){ImGui::PushID(i);ImVec4 c=ImGui::ColorConvertU32ToFloat4(colors[i]);if(ImGui::ColorButton("##color",c,ImGuiColorEditFlags_NoTooltip,{37*s,24*s}))color=colors[i];if(color==colors[i]){auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();dl->AddRect(a,b,Pale,2,0,2);}ImGui::PopID();if(i<4)ImGui::SameLine();}
 ImGui::SetNextItemWidth(155*s);ImGui::SliderFloat("笔触",&penWidth,1,12,"%.1f");ImGui::SetNextItemWidth(155*s);ImGui::SliderFloat("橡皮",&eraseRadius,5,60,"%.0f");ImGui::SetNextItemWidth(155*s);ImGui::SliderFloat("星标大小",&pinRadius,16,48,"%.0f");ImGui::SetNextItemWidth(155*s);ImGui::SliderFloat("标记透明度",&markOpacity,.2f,.95f,"%.2f");ImGui::SetNextItemWidth(155*s);ImGui::SliderFloat("染色透明度",&tintOpacity,.04f,.5f,"%.2f");
 if(grid.usable()&&ImGui::CollapsingHeader("网格精调")){ImGui::PushItemWidth(177*s);
 if(gridFromGame||gridPlaneOverride){if(ImGui::DragFloat("高度层",&gridPlaneZ,.05f,-1000,1000,"%.2f")){gridPlaneOverride=true;needsGrid=true;}if(ImGui::Button("使用角色所在层")){gridPlaneOverride=false;needsGrid=true;}}
 bool changed=ImGui::DragFloat2("基点",&grid.origin.x,.5f,0,0,"%.1f");changed=ImGui::DragFloat2("横向步长",&grid.u.x,.2f,0,0,"%.1f")||changed;changed=ImGui::DragFloat2("纵向步长",&grid.v.x,.2f,0,0,"%.1f")||changed;if(std::abs(grid.px)+std::abs(grid.py)>.00001f)changed=ImGui::DragFloat2("透视参数",&grid.px,.00001f,-.2f,.2f,"%.5f")||changed;if(changed)gridFromGame=false;
 ImGui::PopItemWidth();if(ImGui::Button("格距 ×2")){grid.scaleStep(2);gridFromGame=false;}ImGui::SameLine();if(ImGui::Button("格距 ÷2")){if(length(grid.u)>16&&length(grid.v)>16){grid.scaleStep(.5f);gridFromGame=false;}}}
 ImGui::Separator();ImGui::TextColored(ImVec4(.79f,.72f,.51f,1),"C 清空  ·  Ctrl+Z 撤销  ·  Ctrl+Y 重做");ImGui::TextDisabled("Esc 取消 / 返回游戏");if(!status.empty()){ImGui::PushTextWrapPos(0);ImGui::TextColored(ImVec4(.76f,.80f,.78f,1),"%s",status.c_str());ImGui::PopTextWrapPos();}
 ImGui::End();ImGui::PopStyleVar(2);
}
void App::canvas(float w,float h){auto dl=ImGui::GetBackgroundDrawList();drawGrid(dl,w,h);for(auto&m:document.marks)drawMark(dl,m);
 if(!editing)return;auto&io=ImGui::GetIO();Point mouse=pt(io.MousePos);bool overUI=ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)||ImGui::IsAnyItemActive();bool inside=mouse.x>=0&&mouse.y>=0&&mouse.x<w&&mouse.y<h;
 if(!io.MouseDown[1])finishGesture();
 if(manual){for(size_t i=0;i<calibration.size();i++){dl->AddCircle(im(calibration[i]),7*uiScale,Gold,24,2*uiScale);if(i>0)dl->AddLine(im(calibration[0]),im(calibration[i]),Gold,2*uiScale);}if(!overUI&&inside&&ImGui::IsMouseClicked(0)){calibration.push_back(mouse);if(calibration.size()==size_t(manualPerspective?4:3)){Grid candidate;if(manualPerspective)candidate=Grid::fromQuad(calibration,manualColumns,manualRows);else{candidate.origin=calibration[0];candidate.u=calibration[1]-calibration[0];candidate.v=calibration[2]-calibration[0];candidate.valid=true;candidate.confidence=1;}if(candidate.usable()){grid=candidate;gridConfirmed=true;manual=false;status=manualPerspective?"透视网格已生效，横纵格距随画面远近变化":"三点网格已生效，可分别精调横纵格距";}else{calibration.clear();status="校准点无效，请按提示顺序重新选择";}}}return;}
 if(inside&&!overUI&&io.MouseDown[1]){cancelGesture();if(!erasing){eraseBefore=document.snapshot();eraseLast=mouse;erasing=true;eraseChanged=false;}eraseChanged=document.eraseSweep(eraseLast,mouse,eraseRadius*uiScale)||eraseChanged;eraseLast=mouse;}
 if(inside&&!overUI&&!io.MouseDown[1]){
  Point point=(tool!=Kind::Stroke&&gridEnabled&&gridConfirmed)?grid.snap(mouse):mouse;
  if(ImGui::IsMouseClicked(0)){current={};current.kind=tool;current.width=penWidth*uiScale;current.color=color;current.radius=pinRadius*uiScale;current.points={point};if(tool==Kind::Pin){document.add(current);current.points.clear();}else{drawing=true;if(tool==Kind::Arrow)current.points.push_back(point);}}
  if(drawing&&io.MouseDown[0]){if(tool==Kind::Stroke){if(distance(current.points.back(),point)>.7f*uiScale)current.points.push_back(point);}else current.points.back()=point;}
  if(!drawing&&tool==Kind::Pin){Mark preview;preview.kind=Kind::Pin;preview.points={point};preview.radius=pinRadius*uiScale;preview.number=document.nextPin;bool occupied=false;for(const auto&mark:document.marks)if(mark.kind==Kind::Pin&&!mark.points.empty()&&distance(mark.points[0],point)<std::max(mark.radius,preview.radius)){occupied=true;break;}if(!occupied)drawMark(ImGui::GetForegroundDrawList(),preview,true);}
 }
 if(drawing&&ImGui::IsMouseReleased(0)){if(current.kind==Kind::Stroke||distance(current.points.front(),current.points.back())>3*uiScale)document.add(current);cancelGesture();}
 if(drawing)drawMark(dl,current,true);
 if(inside&&!overUI&&io.MouseDown[1]){auto fg=ImGui::GetForegroundDrawList();fg->AddCircle(im(mouse),eraseRadius*uiScale,IM_COL32(250,224,165,220),48,1.5f);fg->AddCircleFilled(im(mouse),2*uiScale,Gold);}
}
void App::frame(float w,float h){resize(w,h);uiScale=std::clamp(h/1080.f,.75f,2.4f);ImGui::GetIO().FontGlobalScale=19.f/32.f*uiScale;
 if(editing){auto&io=ImGui::GetIO();if(!io.KeyCtrl&&!io.KeyAlt){if(ImGui::IsKeyPressed(ImGuiKey_B,false)){cancelGesture();tool=Kind::Stroke;}if(ImGui::IsKeyPressed(ImGuiKey_A,false)){cancelGesture();tool=Kind::Arrow;}if(ImGui::IsKeyPressed(ImGuiKey_P,false)){cancelGesture();tool=Kind::Pin;}if(ImGui::IsKeyPressed(ImGuiKey_G,false)){if(io.KeyShift)startManual();else{gridEnabled=!gridEnabled;if(gridEnabled&&!gridConfirmed)needsGrid=true;}}if(ImGui::IsKeyPressed(ImGuiKey_T,false)){tint=!tint;if(tint&&!gridConfirmed)needsGrid=true;}if(ImGui::IsKeyPressed(ImGuiKey_C,false)){finishGesture();cancelGesture();document.clear();}}
  if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Z,false)){finishGesture();cancelGesture();document.undo();}if(io.KeyCtrl&&ImGui::IsKeyPressed(ImGuiKey_Y,false)){finishGesture();cancelGesture();document.redo();}
  if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){if(manual){manual=false;calibration.clear();status="已取消网格调整";}else if(drawing)cancelGesture();else wantExit=true;}
  panel();
 }else{float s=uiScale;auto dl=ImGui::GetForegroundDrawList();keycap(dl,{20*s,h-51*s},{57*s,h-18*s},"E");dl->AddText({68*s,h-43*s},IM_COL32(238,215,154,205),"草稿纸");}
 canvas(w,h);
}
}