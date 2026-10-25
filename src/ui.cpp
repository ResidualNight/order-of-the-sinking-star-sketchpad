#include "ui.hpp"
#include <cstdio>
#include <utility>
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
 float font=ImGui::GetFontSize()*.86f;auto sz=ImGui::GetFont()->CalcTextSizeA(font,1e6,0,key);if(sz.x>b.x-a.x-8){font*=(b.x-a.x-8)/sz.x;sz=ImGui::GetFont()->CalcTextSizeA(font,1e6,0,key);}
 dl->AddText(ImGui::GetFont(),font,{(a.x+b.x-sz.x)*.5f,(a.y+b.y-sz.y)*.5f-1},on?IM_COL32(255,237,126,255):Gold,key);
}
static bool row(const char* id,const char* key,const char* text,bool on=false){
 float s=ImGui::GetFontSize()/19.f;ImVec2 p=ImGui::GetCursorScreenPos();float h=34*s,w=ImGui::GetContentRegionAvail().x;
 bool clicked=ImGui::InvisibleButton(id,{w,h});auto dl=ImGui::GetWindowDrawList();if(ImGui::IsItemHovered())dl->AddRectFilled(p,{p.x+w,p.y+h},IM_COL32(215,181,97,22),3);
 keycap(dl,{p.x+2*s,p.y+3*s},{p.x+76*s,p.y+30*s},key,on);
 dl->AddText({p.x+89*s,p.y+7*s},on?Pale:IM_COL32(193,194,188,255),text);if(on)dl->AddCircleFilled({p.x+w-8*s,p.y+17*s},2.8*s,Gold);
 return clicked;
}
bool App::setEditing(bool enabled){
 if(editing==enabled)return false;
 editing=enabled;
 if(enabled)requestGrid();
 else{finishGesture();cancelGesture();manual=false;calibration.clear();settingsOpen=false;capturingBinding=-1;captureKey=ImGuiKey_None;}
 return true;
}
void App::requestGrid(bool resized){
 finishGesture();cancelGesture();manual=false;calibration.clear();
 gridConfirmed=false;gridRefresh.request(resized);
 status=resized?"画面尺寸已变化，等待稳定后自动定位网格":"正在按游戏数据定位网格";
}
void App::resize(float w,float h){
 if(!std::isfinite(w)||!std::isfinite(h)||w<=0||h<=0)return;
 if(oldWidth>0&&oldHeight>0&&(w!=oldWidth||h!=oldHeight)){
  float sx=w/oldWidth,sy=h/oldHeight;
  finishGesture();document.rescale(sx,sy);
  grid.origin={grid.origin.x*sx,grid.origin.y*sy};grid.u={grid.u.x*sx,grid.u.y*sy};grid.v={grid.v.x*sx,grid.v.y*sy};
  requestGrid(true);
 }
 oldWidth=w;oldHeight=h;
}
void App::rejectGameGrid(ErrorId error){
 gridRefresh.finish();gridConfirmed=false;gridFromGame=false;
 status=errorMessage(error==ErrorId::None?ErrorId::UnknownGameGrid:error);
}
bool App::observeLevel(const LevelIdentity* identity,uint64_t now){
 bool changed=levelWatch.observe(identity,now);
 if(changed){
  finishGesture();cancelGesture();if(autoClearOnLevel)document.clear();
  levelPaused=false;requestGrid();
  status=autoClearOnLevel?"已切换关卡，标注已清除，编号从 1 开始；可撤销":"已切换关卡，按设置保留标注，正在重新定位网格";
 }else if(levelWatch.transitioning()){
  if(!levelPaused){finishGesture();cancelGesture();manual=false;calibration.clear();gridConfirmed=false;gridFromGame=false;gridRefresh.cancel();levelPaused=true;}
 }else if(levelPaused&&levelWatch.available&&levelWatch.known){levelPaused=false;requestGrid();}
 return changed;
}
void App::acceptDetection(const Detection&r){gridRefresh.finish();if(r.grid.usable()&&r.grid.confidence>=.55f){grid=r.grid;gridFromGame=false;gridConfirmed=true;status="已识别网格，请检查格子中心；可随时手动调整";}else if(r.grid.usable()&&!gridConfirmed){grid=r.grid;gridFromGame=false;gridConfirmed=false;status=errorMessage(ErrorId::ImageGridUnconfirmed);}else{status=errorMessage(ErrorId::ImageGridUnreliable);if(gridConfirmed)status+="；保留当前网格";}}
void App::finishGesture(){if(erasing&&eraseChanged)document.record(eraseBefore);erasing=false;eraseChanged=false;if(numbering&&numberChanged)document.record(numberBefore);numbering=false;numberChanged=false;numberConnected=false;numberedCells.clear();}
void App::restartNumbers(){finishGesture();cancelGesture();document.restartNumbers();status="后续编号从 1 开始，已保留现有编号";inputSuppressed=true;}
void App::numberCells(Point cell){
 if(!std::isfinite(cell.x)||!std::isfinite(cell.y)||std::abs(cell.x)>1e6f||std::abs(cell.y)>1e6f){numberConnected=false;return;}
 Point from=numberConnected?numberLast:cell;
 int x=int(std::round(from.x)),y=int(std::round(from.y)),endX=int(std::round(cell.x)),endY=int(std::round(cell.y));
 int steps=std::abs(endX-x)+std::abs(endY-y);
 // An ill-conditioned inverse must not generate an unbounded batch of marks.
 if(steps>4096){numberConnected=false;return;}
 auto append=[&](){
  Point position=grid.at(float(x),float(y));if(!std::isfinite(position.x)||!std::isfinite(position.y)||!numberedCells.insert({x,y}).second)return;
  Mark mark;mark.kind=Kind::Number;mark.points={position};mark.radius=numberRadius*uiScale;mark.color=selectedColor();document.appendToGesture(std::move(mark));numberChanged=true;
 };
 append();float dx=cell.x-from.x,dy=cell.y-from.y;int sx=dx>0?1:-1,sy=dy>0?1:-1;
 float tx=dx!=0?(float(x)+sx*.5f-from.x)/dx:INFINITY,ty=dy!=0?(float(y)+sy*.5f-from.y)/dy:INFINITY;
 float stepX=dx!=0?1/std::abs(dx):INFINITY,stepY=dy!=0?1/std::abs(dy):INFINITY;
 // Traverse exact half-cell boundaries. A diagonal corner enters only the diagonal cell.
 for(int i=0;i<steps&&(x!=endX||y!=endY);++i){
  if(std::abs(tx-ty)<1e-6f){x+=sx;y+=sy;tx+=stepX;ty+=stepY;}
  else if(tx<ty){x+=sx;tx+=stepX;}else{y+=sy;ty+=stepY;}
  append();
 }
 numberLast=cell;numberConnected=true;
}
void App::drawMark(ImDrawList*dl,const Mark&m,bool preview){if(m.points.empty())return;float alpha=markOpacity*(preview?.65f:1.f);ImU32 ink=faded(m.color,alpha);
 if(m.kind==Kind::Stroke){if(m.points.size()==1)dl->AddCircleFilled(im(m.points[0]),m.width*.5f,ink);else{std::vector<ImVec2> points;for(auto&p:m.points)points.push_back(im(p));dl->AddPolyline(points.data(),int(points.size()),ink,0,m.width);}return;}
 if(m.kind==Kind::Arrow){Point a=m.points[0],b=m.points.back(),delta=b-a;float n=length(delta);if(n<1)return;Point d=delta*(1/n),normal{-d.y,d.x};float head=std::min(n*.45f,std::max(12.f*uiScale,m.width*4));dl->AddLine(im(a),im(b),ink,m.width);dl->AddTriangleFilled(im(b),im(b-d*head+normal*(head*.43f)),im(b-d*head-normal*(head*.43f)),ink);return;}
 Point c=m.points[0];float radius=m.radius;
 if(m.kind==Kind::Number){
  char label[24];std::snprintf(label,sizeof(label),"%d",m.number);float fs=radius*1.5f;
  auto font=numberFont?numberFont:ImGui::GetFont();auto size=font->CalcTextSizeA(fs,1e6,0,label);if(size.x>radius*1.8f){fs*=radius*1.8f/size.x;size=font->CalcTextSizeA(fs,1e6,0,label);}
  ImVec2 origin{c.x-size.x*.5f,c.y-size.y*.5f};float bold=std::max(.7f,radius*.04f),outline=std::max(1.5f,radius*.085f);
  // Stamp the full silhouette around the glyph, then a lightly expanded white fill.
  for(int i=0;i<16;i++){float a=i*6.2831853f/16;dl->AddText(font,fs,{origin.x+std::cos(a)*(outline+bold),origin.y+std::sin(a)*(outline+bold)},faded(IM_COL32(0,0,0,255),alpha),label);}
  for(int i=0;i<8;i++){float a=i*6.2831853f/8;dl->AddText(font,fs,{origin.x+std::cos(a)*bold,origin.y+std::sin(a)*bold},faded(IM_COL32(255,255,255,255),alpha),label);}
  dl->AddText(font,fs,origin,faded(IM_COL32(255,255,255,255),alpha),label);return;
 }
 ImVec2 verts[16],inner[16],shadow[16],softShadow[16];for(int i=0;i<16;i++){float a=-1.5707963f+i*3.14159265f/8,r=radius*(i%2?.57f:1.f),x=std::cos(a)*r,y=std::sin(a)*r;verts[i]={c.x+x,c.y+y};inner[i]={c.x+x*.84f,c.y+y*.84f};shadow[i]={c.x+x*1.04f+2.5f*uiScale,c.y+y*1.04f+4*uiScale};softShadow[i]={c.x+x*1.12f+3*uiScale,c.y+y*1.12f+6*uiScale};}
 dl->AddConcavePolyFilled(softShadow,16,faded(IM_COL32(0,0,0,255),alpha*.25f));
 dl->AddConcavePolyFilled(shadow,16,faded(IM_COL32(23,18,12,255),alpha*.8f));
 // Faceted antique-gold bevel: lit upper edges, deep bronze lower edges.
 for(int i=0;i<16;i++){int j=(i+1)%16;ImVec2 face[]={verts[i],verts[j],inner[j],inner[i]};float y=(verts[i].y+verts[j].y)*.5f-c.y;ImU32 shade=y<-radius*.1f?IM_COL32(232,202,132,255):y>radius*.1f?IM_COL32(126,89,40,255):IM_COL32(181,143,73,255);dl->AddConvexPolyFilled(face,4,faded(shade,alpha));}
 dl->AddConcavePolyFilled(inner,16,ink);
 dl->AddPolyline(verts,16,faded(IM_COL32(76,61,39,255),alpha),ImDrawFlags_Closed,4*uiScale);
 dl->AddPolyline(verts,16,faded(IM_COL32(224,192,114,255),alpha),ImDrawFlags_Closed,1.7f*uiScale);
 dl->AddPolyline(inner,16,faded(IM_COL32(255,232,158,255),alpha*.65f),ImDrawFlags_Closed,.8f*uiScale);
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
 auto dl=ImGui::GetWindowDrawList();ImVec2 wp=ImGui::GetWindowPos(),ws=ImGui::GetWindowSize();panelMin=pt(wp);panelMax={wp.x+ws.x,wp.y+ws.y};dl->AddRect({wp.x+4*s,wp.y+4*s},{wp.x+ws.x-4*s,wp.y+ws.y-4*s},IM_COL32(173,140,74,100),5*s);
 ImGui::TextColored(ImVec4(.94f,.80f,.48f,1),"沉星草稿");ImGui::SameLine();ImGui::TextDisabled("  屏幕标注");ImGui::Separator();
 ImGui::BeginDisabled(settingsOpen);
 if(row("pen",bindings.label(Action::Pen).c_str(),"自由画笔",tool==Kind::Stroke)){cancelGesture();tool=Kind::Stroke;}
 if(row("arrow",bindings.label(Action::Arrow).c_str(),"箭头",tool==Kind::Arrow)){cancelGesture();tool=Kind::Arrow;}
 if(row("pin",bindings.label(Action::Pin).c_str(),"星标",tool==Kind::Pin)){cancelGesture();tool=Kind::Pin;}
 if(row("number",bindings.label(Action::Number).c_str(),"编号",tool==Kind::Number)){cancelGesture();tool=Kind::Number;}
 if(tool==Kind::Number){ImGui::TextDisabled("下个编号：%d",document.nextNumber);ImGui::TextWrapped("开启吸附后，按住左键滑动快速编号");}
 if(row("restartNumber",bindings.label(Action::RestartNumber).c_str(),"编号从 1 开始"))restartNumbers();
 row("eraser",bindings.label(Action::Erase).c_str(),"按住擦除");
 if(row("grid",bindings.label(Action::Grid).c_str(),"网格与吸附",gridEnabled)){gridEnabled=!gridEnabled;if(gridEnabled&&!gridConfirmed)requestGrid();}
 if(row("tint",bindings.label(Action::Tint).c_str(),"棋盘染色",tint)){tint=!tint;if(tint&&!gridConfirmed)requestGrid();}
 if(row("manual",bindings.label(Action::Calibrate).c_str(),"透视四点校准",manual))startManual();
 if(manual&&manualPerspective){ImGui::SetNextItemWidth(145*s);ImGui::InputInt("棋盘列数",&manualColumns);ImGui::SetNextItemWidth(145*s);ImGui::InputInt("棋盘行数",&manualRows);manualColumns=std::clamp(manualColumns,1,200);manualRows=std::clamp(manualRows,1,200);ImGui::Text("已选 %d / 4 个外角",int(calibration.size()));}
 if(ImGui::Button("快速三点校准",{0,25*s}))startManual(false);
 if(ImGui::Button("重新识别",{137*s,29*s})){requestGrid();gridEnabled=true;}ImGui::SameLine();if(ImGui::Button(("退出绘画  "+bindings.label(Action::Toggle)).c_str(),{137*s,29*s}))wantExit=true;
 if(grid.usable()&&!gridConfirmed){if(ImGui::Button("采用候选网格",{0,29*s})){gridRefresh.cancel();gridConfirmed=true;status="已采用候选网格，可继续精调";}}
 if(gridEnabled&&ImGui::CollapsingHeader("网格精调")){ImGui::PushItemWidth(145*s);ImGui::BeginDisabled(!grid.usable());
 if(gridFromGame||gridPlaneOverride){if(ImGui::DragFloat("高度层",&gridPlaneZ,.05f,-1000,1000,"%.2f")){gridPlaneOverride=true;requestGrid();}if(ImGui::Button("使用角色所在层")){gridPlaneOverride=false;requestGrid();}}
 bool changed=ImGui::DragFloat2("基点",&grid.origin.x,.5f,0,0,"%.1f");changed=ImGui::DragFloat2("横向步长",&grid.u.x,.2f,0,0,"%.1f")||changed;changed=ImGui::DragFloat2("纵向步长",&grid.v.x,.2f,0,0,"%.1f")||changed;if(std::abs(grid.px)+std::abs(grid.py)>.00001f)changed=ImGui::DragFloat2("透视参数",&grid.px,.00001f,-.2f,.2f,"%.5f")||changed;if(changed){gridFromGame=false;gridRefresh.cancel();gridConfirmed=grid.usable();}
 ImGui::PopItemWidth();if(ImGui::Button("格距 ×2")){grid.scaleStep(2);gridFromGame=false;gridRefresh.cancel();gridConfirmed=grid.usable();}ImGui::SameLine();if(ImGui::Button("格距 ÷2")){if(length(grid.u)>16&&length(grid.v)>16){grid.scaleStep(.5f);gridFromGame=false;gridRefresh.cancel();gridConfirmed=grid.usable();}}ImGui::EndDisabled();}
 ImGui::Spacing();ImGui::Separator();
 ImGui::TextDisabled("画笔 / 箭头 / 星标共用颜色");
 for(int i=0;i<5;i++){ImGui::PushID(i);ImVec4 c=ImGui::ColorConvertU32ToFloat4(colors[i]);if(ImGui::ColorButton("##color",c,ImGuiColorEditFlags_NoTooltip,{37*s,24*s}))colorIndex=i;if(colorIndex==i){auto a=ImGui::GetItemRectMin(),b=ImGui::GetItemRectMax();dl->AddRect(a,b,Pale,2,0,2);}if(ImGui::IsItemHovered())ImGui::SetTooltip("%s",i<3?(i==0?"红色":i==1?"白色":"蓝色"):"自定义颜色：在键位与设置中编辑");ImGui::PopID();if(i<4)ImGui::SameLine();}

 ImGui::Separator();if(ImGui::Button("键位与设置",{0,29*s})){finishGesture();cancelGesture();settingsOpen=true;capturingBinding=-1;bindingStatus.clear();}
 ImGui::TextWrapped("%s 清空 · %s 撤销 · %s 重做",bindings.label(Action::Clear).c_str(),bindings.label(Action::Undo).c_str(),bindings.label(Action::Redo).c_str());ImGui::TextDisabled("Esc 取消 / 返回游戏");if(!status.empty()){ImGui::PushTextWrapPos(0);ImGui::TextColored(ImVec4(.76f,.80f,.78f,1),"%s",status.c_str());ImGui::PopTextWrapPos();}
 ImGui::EndDisabled();
 ImGui::End();ImGui::PopStyleVar(2);
}
void App::canvas(float w,float h){auto dl=ImGui::GetBackgroundDrawList();drawGrid(dl,w,h);for(auto&m:document.marks)drawMark(dl,m);
 if(!editing||settingsOpen||inputSuppressed)return;auto&io=ImGui::GetIO();Point mouse=pt(io.MousePos);
 // ImGui drops HoveredWindow during a drag that began on the external canvas.
 bool overPanel=mouse.x>=panelMin.x&&mouse.y>=panelMin.y&&mouse.x<panelMax.x&&mouse.y<panelMax.y;
 bool overUI=overPanel||ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow)||ImGui::IsAnyItemActive();bool inside=mouse.x>=0&&mouse.y>=0&&mouse.x<w&&mouse.y<h;bool eraseHeld=bindings.active(Action::Erase,true);
 if(erasing&&!eraseHeld)finishGesture();
 bool snapNumbers=tool==Kind::Number&&gridEnabled&&gridConfirmed&&grid.usable()&&!manual;
 if(numbering&&(!io.MouseDown[0]||eraseHeld||!snapNumbers))finishGesture();
 if(numbering){if(inside&&!overUI)numberCells(grid.cell(mouse));else numberConnected=false;}
 if(manual){for(size_t i=0;i<calibration.size();i++){dl->AddCircle(im(calibration[i]),7*uiScale,Gold,24,2*uiScale);if(i>0)dl->AddLine(im(calibration[0]),im(calibration[i]),Gold,2*uiScale);}if(!overUI&&inside&&ImGui::IsMouseClicked(0)){calibration.push_back(mouse);if(calibration.size()==size_t(manualPerspective?4:3)){Grid candidate;if(manualPerspective)candidate=Grid::fromQuad(calibration,manualColumns,manualRows);else{candidate.origin=calibration[0];candidate.u=calibration[1]-calibration[0];candidate.v=calibration[2]-calibration[0];candidate.valid=true;candidate.confidence=1;}if(candidate.usable()){grid=candidate;gridConfirmed=true;manual=false;status=manualPerspective?"透视网格已生效，横纵格距随画面远近变化":"三点网格已生效，可分别精调横纵格距";}else{calibration.clear();status=errorMessage(ErrorId::InvalidCalibration);}}}return;}
 if(inside&&!overUI&&eraseHeld){cancelGesture();if(!erasing){eraseBefore=document.snapshot();eraseLast=mouse;erasing=true;eraseChanged=false;}eraseChanged=document.eraseSweep(eraseLast,mouse,eraseRadius*uiScale)||eraseChanged;eraseLast=mouse;}
 if(inside&&!overUI&&!eraseHeld){
  Point point=(tool!=Kind::Stroke&&gridEnabled&&gridConfirmed)?grid.snap(mouse):mouse;
  if(ImGui::IsMouseClicked(0)){current={};current.kind=tool;current.width=penWidth*uiScale;current.color=selectedColor();current.radius=(tool==Kind::Number?numberRadius:pinRadius)*uiScale;current.points={point};if(snapNumbers){numberBefore=document.snapshot();numbering=true;numberChanged=false;numberConnected=false;numberedCells.clear();numberCells(grid.cell(mouse));current.points.clear();}else if(tool==Kind::Pin||tool==Kind::Number){document.add(current);current.points.clear();}else{drawing=true;if(tool==Kind::Arrow)current.points.push_back(point);}}
  if(drawing&&io.MouseDown[0]){if(tool==Kind::Stroke){if(distance(current.points.back(),point)>.7f*uiScale)current.points.push_back(point);}else current.points.back()=point;}
  if(!drawing&&(tool==Kind::Pin||tool==Kind::Number)){Mark preview;preview.kind=tool;preview.color=selectedColor();preview.points={point};preview.radius=(tool==Kind::Number?numberRadius:pinRadius)*uiScale;preview.number=tool==Kind::Number?document.nextNumber:0;bool occupied=false;for(const auto&mark:document.marks)if(mark.kind==tool&&!mark.points.empty()&&distance(mark.points[0],point)<std::max(mark.radius,preview.radius)){occupied=true;break;}if(!occupied)drawMark(ImGui::GetForegroundDrawList(),preview,true);}
 }
 if(drawing&&ImGui::IsMouseReleased(0)){if(current.kind==Kind::Stroke||distance(current.points.front(),current.points.back())>3*uiScale)document.add(current);cancelGesture();}
 if(drawing)drawMark(dl,current,true);
 if(inside&&!overUI&&eraseHeld){auto fg=ImGui::GetForegroundDrawList();fg->AddCircle(im(mouse),eraseRadius*uiScale,IM_COL32(250,224,165,220),48,1.5f);fg->AddCircleFilled(im(mouse),2*uiScale,Gold);}
}
void App::settingsPanel(){
 if(!settingsOpen)return;float s=uiScale;
 ImGui::SetNextWindowPos({ImGui::GetIO().DisplaySize.x*.5f,ImGui::GetIO().DisplaySize.y*.5f},ImGuiCond_Appearing,{.5f,.5f});
 ImGui::SetNextWindowSize({600*s,std::min(720*s,ImGui::GetIO().DisplaySize.y-40*s)},ImGuiCond_Always);
 ImGui::PushStyleColor(ImGuiCol_TitleBg,{.035f,.055f,.085f,1});ImGui::PushStyleColor(ImGuiCol_TitleBgActive,{.12f,.14f,.16f,1});
 if(ImGui::Begin("键位与设置",&settingsOpen,ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse)){
  ImGui::BeginChild("##settingsBody",{0,-38*s},ImGuiChildFlags_None);
  ImGui::Checkbox("切换关卡自动清除标注",&autoClearOnLevel);
  ImGui::TextWrapped("初始默认关闭，会记住你的选择。开启后，确认切换关卡再清除标注并将编号归 1；可撤销。加载中或读取未确认时保留标注。");
  ImGui::TextDisabled("关卡监测：%s",levelWatch.known&&levelWatch.available&&!levelWatch.transitioning()?"已就绪":"等待可信数据");
  ImGui::TextWrapped("%s",preferencesStatus.c_str());
  ImGui::Separator();
  if(ImGui::CollapsingHeader("画笔、标记与颜色")){
 ImGui::SetNextItemWidth(155*s);ImGui::SliderFloat("笔触",&penWidth,1,12,"%.1f");ImGui::SetNextItemWidth(155*s);ImGui::SliderFloat("橡皮",&eraseRadius,5,60,"%.0f");ImGui::SetNextItemWidth(155*s);ImGui::SliderFloat("星标大小",&pinRadius,16,48,"%.0f");ImGui::SetNextItemWidth(155*s);ImGui::SliderFloat("编号大小",&numberRadius,16,48,"%.0f");ImGui::SetNextItemWidth(155*s);ImGui::SliderFloat("标记透明度",&markOpacity,.2f,.95f,"%.2f");ImGui::SetNextItemWidth(155*s);ImGui::SliderFloat("染色透明度",&tintOpacity,.04f,.5f,"%.2f");

   for(int i=3;i<5;++i){ImGui::PushID(i);auto c=ImGui::ColorConvertU32ToFloat4(colors[i]);float rgb[]={c.x,c.y,c.z};ImGui::SetNextItemWidth(200*s);if(ImGui::ColorEdit3(i==3?"自定义颜色 1":"自定义颜色 2",rgb,ImGuiColorEditFlags_NoAlpha|ImGuiColorEditFlags_NoSidePreview))colors[i]=ImGui::ColorConvertFloat4ToU32({rgb[0],rgb[1],rgb[2],1});ImGui::PopID();}
  }
  if(ImGui::CollapsingHeader("快捷键",ImGuiTreeNodeFlags_DefaultOpen)){
  ImGui::TextWrapped("点击按键后输入新组合；支持两个普通键同时按住，如 N+C，单键松开确认。也可搭配 Ctrl / Alt / Shift。Esc 取消录入或关闭窗口。");
  ImGui::TextWrapped("擦除也可使用右键或中键。设置和键位会永久保存，草稿不会保存。");ImGui::Separator();
  for(int i=0;i<int(Action::Count);++i){auto action=static_cast<Action>(i);ImGui::PushID(i);
   if(ImGui::Button(capturingBinding==i?(captureKey==ImGuiKey_None?"请按新组合...":"松开确认 / 加按第二键"):bindings.label(action).c_str(),{230*s,29*s})){capturingBinding=i;captureKey=ImGuiKey_None;captureAfterFrame=ImGui::GetFrameCount();bindingStatus.clear();}
   ImGui::SameLine();ImGui::TextUnformatted(ActionLabels[i]);ImGui::PopID();
  }
  }else{capturingBinding=-1;captureKey=ImGuiKey_None;}
  ImGui::Separator();if(!bindingStatus.empty())ImGui::TextWrapped("%s",bindingStatus.c_str());
  ImGui::EndChild();
  if(ImGui::Button("恢复默认键位")){bindings.reset();capturingBinding=-1;captureKey=ImGuiKey_None;bindingStatus="已恢复默认键位";inputSuppressed=true;}
  ImGui::SameLine();if(ImGui::Button("完成")){settingsOpen=false;capturingBinding=-1;inputSuppressed=true;}
 }
 ImGui::End();ImGui::PopStyleColor(2);if(!settingsOpen){capturingBinding=-1;inputSuppressed=true;}
}
void App::shortcuts(){
 auto&io=ImGui::GetIO();
 if(settingsOpen){
  if(io.WantTextInput||ImGui::IsAnyItemActive()||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)){capturingBinding=-1;captureKey=ImGuiKey_None;return;}
  if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){if(capturingBinding>=0){capturingBinding=-1;captureKey=ImGuiKey_None;bindingStatus="已取消改键";}else settingsOpen=false;inputSuppressed=true;return;}
  if(capturingBinding>=0&&ImGui::GetFrameCount()>captureAfterFrame){
   ImGuiKey pressed=ImGuiKey_None,secondPressed=ImGuiKey_None;for(auto key:KeyboardKeys)if(ImGui::IsKeyPressed(key.key,false)){if(pressed==ImGuiKey_None)pressed=key.key;else{secondPressed=key.key;break;}}
   auto action=static_cast<Action>(capturingBinding);
   if(action==Action::Erase){if(ImGui::IsMouseClicked(1))pressed=ImGuiKey_MouseRight;if(ImGui::IsMouseClicked(2))pressed=ImGuiKey_MouseMiddle;}
   KeyBinding binding{ImGuiKey_None};
   if(captureKey==ImGuiKey_None&&pressed!=ImGuiKey_None){captureKey=pressed;captureMods=io.KeyMods;if(secondPressed!=ImGuiKey_None)binding={secondPressed,io.KeyMods,captureKey};}
   else if(captureKey!=ImGuiKey_None&&pressed!=ImGuiKey_None&&pressed!=captureKey&&ImGui::IsKeyDown(captureKey))binding={pressed,io.KeyMods,captureKey};
   if(captureKey!=ImGuiKey_None&&ImGui::IsKeyReleased(captureKey))binding={captureKey,captureMods};
   if(binding.key!=ImGuiKey_None&&Bindings::supported(action,binding)){
    int conflict=bindings.conflict(action,binding);
    if(conflict>=0){inputError=ErrorId::KeyBindingConflict;bindingStatus=errorMessage(inputError)+"："+ActionLabels[conflict];}
    else{bindings.assign(action,binding);bindingStatus=std::string(ActionLabels[capturingBinding])+" 已改为 "+bindings.label(action);capturingBinding=-1;}
    captureKey=ImGuiKey_None;inputSuppressed=true;
   }
  }
  return;
 }
 if(inputSuppressed){bool held=io.MouseDown[0]||io.MouseDown[1]||io.MouseDown[2];for(auto key:KeyboardKeys)held=held||ImGui::IsKeyDown(key.key);if(!held&&io.KeyMods==0)inputSuppressed=false;return;}
 if(editing&&(io.WantTextInput||ImGui::IsAnyItemActive()||ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId)))return;
 if(bindings.active(Action::Toggle)){setEditing(!editing);inputSuppressed=true;return;}
 if(!editing)return;
 if(bindings.active(Action::RestartNumber)){restartNumbers();return;}
 for(auto choice:{std::pair<Action,Kind>{Action::Pen,Kind::Stroke},{Action::Arrow,Kind::Arrow},{Action::Pin,Kind::Pin},{Action::Number,Kind::Number}})if(bindings.active(choice.first)){finishGesture();cancelGesture();tool=choice.second;}
 if(bindings.active(Action::Grid)){gridEnabled=!gridEnabled;if(gridEnabled&&!gridConfirmed)requestGrid();}
 if(bindings.active(Action::Tint)){tint=!tint;if(tint&&!gridConfirmed)requestGrid();}
 if(bindings.active(Action::Calibrate))startManual();
 if(bindings.active(Action::Clear)){finishGesture();cancelGesture();document.clear();}
 if(bindings.active(Action::Undo)){finishGesture();cancelGesture();document.undo();}
 if(bindings.active(Action::Redo)){finishGesture();cancelGesture();document.redo();}
 if(ImGui::IsKeyPressed(ImGuiKey_Escape,false)){if(manual){manual=false;calibration.clear();status="已取消网格调整";}else if(drawing)cancelGesture();else wantExit=true;}
}
void App::frame(float w,float h){resize(w,h);uiScale=std::clamp(h/1080.f,.75f,2.4f);ImGui::GetIO().FontGlobalScale=19.f/32.f*uiScale;
 bindings.update();shortcuts();
 if(editing){panel();settingsPanel();}
 else{float s=uiScale;auto dl=ImGui::GetForegroundDrawList();auto label=bindings.label(Action::Toggle);float width=std::max(37.f*s,ImGui::CalcTextSize(label.c_str()).x+14*s);keycap(dl,{20*s,h-51*s},{20*s+width,h-18*s},label.c_str());dl->AddText({31*s+width,h-43*s},IM_COL32(238,215,154,205),"草稿纸");}
 canvas(w,h);
}
}
