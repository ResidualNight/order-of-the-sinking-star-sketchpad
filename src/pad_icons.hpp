#pragma once
#include "pad_bindings.hpp"
#include "imgui.h"
#include <initializer_list>
namespace sketch {namespace padicons {
inline ImU32 ink(PadControl c){switch(c){case PadControl::A:return IM_COL32(115,220,104,255);case PadControl::B:return IM_COL32(255,115,105,255);case PadControl::X:return IM_COL32(104,181,255,255);case PadControl::Y:return IM_COL32(255,216,83,255);default:return IM_COL32(238,203,137,255);}}
inline void draw(ImDrawList*dl,ImVec2 top,float size,PadControl c){
 auto p=[&](float x,float y){return ImVec2(top.x+x*size,top.y+y*size);};auto col=ink(c);float t=std::max(1.f,size*.055f);
 auto line=[&](float x,float y,float xx,float yy){dl->AddLine(p(x,y),p(xx,yy),col,t);};
 auto text=[&](const char*value,float scale){float fs=size*scale;auto ts=ImGui::GetFont()->CalcTextSizeA(fs,1e5f,0,value);dl->AddText(ImGui::GetFont(),fs,{top.x+(size-ts.x)*.5f,top.y+(size-ts.y)*.5f},col,value);};
 if(c>=PadControl::A&&c<=PadControl::Y){dl->AddCircleFilled(p(.5f,.5f),size*.43f,IM_COL32(17,24,33,245),32);dl->AddCircle(p(.5f,.5f),size*.43f,col,32,t);text(PadControlNames[int(c)],.66f);}
 else if(c==PadControl::LT||c==PadControl::RT){bool right=c==PadControl::RT;ImVec2 q[6];float xy[][2]={{.18f,.08f},{.8f,.08f},{.84f,.5f},{.7f,.83f},{.48f,.94f},{.15f,.94f}};for(int i=0;i<6;++i)q[i]=p(right?1-xy[i][0]:xy[i][0],xy[i][1]);dl->AddPolyline(q,6,col,ImDrawFlags_Closed,t);text(right?"RT":"LT",.48f);}
 else if(c==PadControl::LB||c==PadControl::RB){ImVec2 q[]={p(.08f,.7f),p(.08f,.4f),p(.24f,.24f),p(.77f,.24f),p(.92f,.4f),p(.92f,.7f)};dl->AddPolyline(q,6,col,ImDrawFlags_Closed,t);text(c==PadControl::LB?"LB":"RB",.46f);}
 else if(c==PadControl::Menu||c==PadControl::View){dl->AddCircle(p(.5f,.5f),size*.43f,col,32,t);if(c==PadControl::Menu){for(float y:{.32f,.5f,.68f})line(.27f,y,.73f,y);}else{dl->AddRect(p(.24f,.27f),p(.56f,.57f),col,0,0,t);dl->AddRectFilled(p(.42f,.44f),p(.76f,.74f),IM_COL32(17,24,33,255));dl->AddRect(p(.42f,.44f),p(.76f,.74f),col,0,0,t);}}
 else if(c==PadControl::Dpad){ImVec2 q[]={p(.36f,.08f),p(.64f,.08f),p(.64f,.36f),p(.92f,.36f),p(.92f,.64f),p(.64f,.64f),p(.64f,.92f),p(.36f,.92f),p(.36f,.64f),p(.08f,.64f),p(.08f,.36f),p(.36f,.36f)};dl->AddPolyline(q,12,col,ImDrawFlags_Closed,t);}
 else if(c==PadControl::LS||c==PadControl::RS||c==PadControl::LeftStick||c==PadControl::RightStick){bool left=c==PadControl::LS||c==PadControl::LeftStick;dl->AddRect(p(.14f,.28f),p(.86f,.44f),col,size*.06f,0,t);line(.3f,.44f,.3f,.85f);line(.7f,.44f,.7f,.85f);float fs=size*.37f;dl->AddText(ImGui::GetFont(),fs,p(.36f,.48f),col,left?"L":"R");if(c==PadControl::LS||c==PadControl::RS)dl->AddTriangleFilled(p(.38f,.05f),p(.62f,.05f),p(.5f,.23f),col);else{line(.2f,.12f,.8f,.12f);line(.2f,.12f,.32f,.03f);line(.8f,.12f,.68f,.03f);}}
 else {dl->AddCircle(p(.5f,.5f),size*.4f,col,32,t);line(.23f,.77f,.77f,.23f);}
}
struct Hint {PadControl control;const char*label;};
inline float hintWidth(Hint h,float size,float font){return size+4+ImGui::GetFont()->CalcTextSizeA(font,1e5f,0,h.label).x;}
inline void drawHint(ImDrawList*dl,ImVec2 p,Hint h,float size,float font){draw(dl,p,size,h.control);dl->AddText(ImGui::GetFont(),font,{p.x+size+4,p.y+(size-font)*.5f},IM_COL32(235,227,207,255),h.label);}
inline void row(std::initializer_list<Hint> hints,float size){float font=ImGui::GetFontSize();bool first=true;for(auto h:hints){float width=hintWidth(h,size,font);if(!first&&ImGui::GetWindowPos().x+ImGui::GetWindowContentRegionMax().x-ImGui::GetItemRectMax().x>=width+ImGui::GetStyle().ItemSpacing.x)ImGui::SameLine();auto p=ImGui::GetCursorScreenPos();drawHint(ImGui::GetWindowDrawList(),p,h,size,font);ImGui::Dummy({width,size});first=false;}}
inline void centered(ImDrawList*dl,ImVec2 p,std::initializer_list<Hint> hints,float size,float font){float width=0;for(auto h:hints)width+=hintWidth(h,size,font)+10;float x=p.x-(width-10)*.5f;for(auto h:hints){drawHint(dl,{x,p.y},h,size,font);x+=hintWidth(h,size,font)+10;}}
inline bool button(const char*id,PadControl c,const char*label,float size){float font=ImGui::GetFontSize(),width=hintWidth({c,label},size,font)+12;auto p=ImGui::GetCursorScreenPos();bool hit=ImGui::Button(id,{width,size+6});drawHint(ImGui::GetWindowDrawList(),{p.x+6,p.y+3},{c,label},size,font);return hit;}
inline void entry(bool& enabled,float s){ImGui::Checkbox("打开草稿纸##padEntry",&enabled);row({{PadControl::LT,"+"},{PadControl::RT,"同时按下"}},24*s);}
}}