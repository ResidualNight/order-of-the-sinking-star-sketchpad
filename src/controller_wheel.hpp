#pragma once
// Vector-only wheel art: no icon fonts, textures, OS cursor or input injection.
namespace sketch {namespace wheel {
constexpr float Pi=3.14159265358979323846f;
const char* const Names[]={"画笔","箭头","标记","编号","清屏","黑白染色","刷新网格"};
ImVec2 polar(ImVec2 center,float radius,float angle){return {center.x+std::sin(angle)*radius,center.y-std::cos(angle)*radius};}
void icon(ImDrawList*dl,ImVec2 c,float size,int index,ImU32 ink){
 auto p=[&](float x,float y){return ImVec2(c.x+x*size,c.y+y*size);};
 float t=std::max(1.4f,size*.10f);
 auto line=[&](float x,float y,float xx,float yy){dl->AddLine(p(x,y),p(xx,yy),ink,t);};
 if(index==0){ // brush handle and pointed bristles
  ImVec2 handle[]={p(-.15f,.1f),p(.53f,-.65f),p(.72f,-.46f),p(.05f,.29f)};dl->AddConvexPolyFilled(handle,4,ink);
  dl->AddBezierCubic(p(-.15f,.16f),p(-.6f,.18f),p(-.2f,.65f),p(-.72f,.65f),ink,t,14);
  dl->AddBezierCubic(p(-.72f,.65f),p(-.08f,.78f),p(.2f,.4f),p(.05f,.29f),ink,t,14);
 }else if(index==1){line(-.63f,.58f,.6f,-.58f);line(.6f,-.58f,.59f,.12f);line(.6f,-.58f,-.12f,-.58f);
 }else if(index==2){
  ImVec2 v[10];for(int k=0;k<10;++k)v[k]=polar(c,size*(k%2?.32f:.76f),k*Pi/5);dl->AddPolyline(v,10,ink,ImDrawFlags_Closed,t);
 }else if(index==3){dl->AddCircle(c,size*.78f,ink,40,t);line(-.22f,-.22f,.04f,-.46f);line(.04f,-.46f,.04f,.48f);line(-.26f,.48f,.32f,.48f);
 }else if(index==4){ // broom and clearing spark
  line(.1f,.05f,.65f,-.67f);ImVec2 broom[]={p(-.2f,-.05f),p(.25f,.24f),p(-.05f,.76f),p(-.73f,.31f)};dl->AddPolyline(broom,4,ink,ImDrawFlags_Closed,t);
  line(-.25f,.25f,-.45f,.49f);line(-.04f,.4f,-.21f,.65f);line(-.57f,-.64f,-.57f,-.2f);line(-.8f,-.42f,-.35f,-.42f);
 }else if(index==5){
  float edge=.68f;for(int y=0;y<2;++y)for(int x=0;x<2;++x)dl->AddRectFilled(p(-edge+x*edge,-edge+y*edge),p(x*edge,y*edge),((x+y)%2)?IM_COL32(22,25,30,255):IM_COL32(244,244,239,255));
  dl->AddRect(p(-edge,-edge),p(edge,edge),ink,2,0,t);
 }else{ // grid with clockwise refresh arc
  for(int k=-1;k<=1;++k){float v=k*.26f;line(v,-.37f,v,.37f);line(-.37f,v,.37f,v);}
  dl->PathArcTo(c,size*.76f,-Pi*.8f,Pi*.8f,32);dl->PathStroke(ink,0,t);
  auto end=ImVec2(c.x+std::cos(Pi*.8f)*size*.76f,c.y+std::sin(Pi*.8f)*size*.76f);
  dl->AddTriangleFilled(end,{end.x+size*.38f,end.y-size*.05f},{end.x+size*.12f,end.y+size*.28f},ink);
 }
}
}
void App::applyPadWheel(int index){
 if(index>=0&&index<4){choosePadTool(index);return;}
 if(index==4){document.clear();status="已清屏，可撤销";}
 else if(index==5){tint=!(tint&&monochromeTint);monochromeTint=true;if(tint&&!gridConfirmed&&!gridRecoveryActive&&!gridDirectionPending&&!levelPaused)requestGrid();}
 else if(index==6){requestImmediateGrid();}
}
void App::drawPadWheel(float w,float h){
 if(!editing||!padWheel||!padWheelVisible)return;
 auto dl=ImGui::GetForegroundDrawList();int firstVertex=dl->VtxBuffer.Size;
 float r=padui::wheelRadius(w,h,uiScale,padWheelScale),inner=r*.43f;
 ImVec2 center{w*.5f,h*.43f};auto gold=IM_COL32(232,199,126,255),white=IM_COL32(244,239,220,255);
 dl->AddRectFilled({0,0},{w,h},IM_COL32(2,5,11,112));dl->AddCircleFilled(center,r+7*uiScale,IM_COL32(5,10,18,238),96);
 constexpr float sector=2*wheel::Pi/7;
 for(int i=0;i<7;++i){float a=i*sector-sector*.5f+.018f,b=i*sector+sector*.5f-.018f;bool selected=i==padWheelSelection;
  ImU32 fill=selected?IM_COL32(124,94,40,245):IM_COL32(25,36,52,250);
  // Tessellate annulus into convex quads; a complete sector with a hole is concave.
  auto flags=dl->Flags;dl->Flags&=~ImDrawListFlags_AntiAliasedFill;
  for(int k=0;k<16;++k){float x=a+(b-a)*k/16,y=a+(b-a)*(k+1)/16;ImVec2 q[]={wheel::polar(center,inner,x),wheel::polar(center,r,x),wheel::polar(center,r,y),wheel::polar(center,inner,y)};dl->AddConvexPolyFilled(q,4,fill);}dl->Flags=flags;
  dl->AddLine(wheel::polar(center,inner,a),wheel::polar(center,r,a),IM_COL32(182,157,108,100),1);
  if(selected){for(int k=0;k<=24;++k)dl->PathLineTo(wheel::polar(center,r,a+(b-a)*k/24));dl->PathStroke(gold,0,3*uiScale);}
  wheel::icon(dl,wheel::polar(center,r*.72f,i*sector),r*.16f,i,selected?white:IM_COL32(205,211,215,255));
 }
 dl->AddCircle(center,inner-2*uiScale,IM_COL32(121,137,155,150),64,1.5f*uiScale);
 auto centered=[&](const char*text,float y,float font,ImU32 color){auto sz=ImGui::GetFont()->CalcTextSizeA(font,1e5f,0,text);dl->AddText(ImGui::GetFont(),font,{center.x-sz.x*.5f,y},color,text);};
 float font=std::min(25.f*uiScale,r*.18f);
 padicons::draw(dl,{center.x-font,center.y-font},font*2,padBindings.get(PadAction::Wheel));
 const char*name=padWheelSelection>=0?wheel::Names[padWheelSelection]:"选择功能";
 float labelY=center.y+r+12*uiScale;auto textSize=ImGui::GetFont()->CalcTextSizeA(font,1e5f,0,name);
 dl->AddRectFilled({center.x-textSize.x*.5f-18*uiScale,labelY-5*uiScale},{center.x+textSize.x*.5f+18*uiScale,labelY+font+7*uiScale},IM_COL32(7,13,22,240),6*uiScale);
 centered(name,labelY,font,white);
 float hintSize=std::min(23.f*uiScale,r*.16f),hintFont=std::min(17.f*uiScale,r*.115f);
 padicons::centered(dl,{center.x,labelY+font+13*uiScale},{{PadControl::LeftStick,"选择"},{padBindings.get(PadAction::Wheel),"松开执行"}},hintSize,hintFont);
 padicons::centered(dl,{center.x,labelY+font+41*uiScale},{{padBindings.get(PadAction::Cancel),"取消 / 摇杆回中取消"}},hintSize,hintFont);
 if(padWheelSelection==4)padicons::centered(dl,{center.x,labelY+font+69*uiScale},{{padBindings.get(PadAction::Undo),"清屏后可撤销"}},hintSize,hintFont);
 // Fade only vertices appended by this overlay, including its dimmer and labels.
 for(int i=firstVertex;i<dl->VtxBuffer.Size;++i){auto&color=dl->VtxBuffer[i].col;unsigned alpha=(color>>IM_COL32_A_SHIFT)&255;alpha=unsigned(alpha*padWheelOpacity+.5f);color=(color&~IM_COL32_A_MASK)|(alpha<<IM_COL32_A_SHIFT); }
}
}