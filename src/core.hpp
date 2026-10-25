#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>
#include <limits>

namespace sketch {
struct Point { float x=0,y=0; Point operator+(Point b)const{return{x+b.x,y+b.y};} Point operator-(Point b)const{return{x-b.x,y-b.y};} Point operator*(float s)const{return{x*s,y*s};} };
inline float dot(Point a,Point b){return a.x*b.x+a.y*b.y;}
inline float length(Point a){return std::sqrt(dot(a,a));}
inline float distance(Point a,Point b){return length(a-b);}
inline float segmentDistance(Point p,Point a,Point b){auto d=b-a; float q=dot(d,d);return distance(p,a+d*(q>0?std::clamp(dot(p-a,d)/q,0.f,1.f):0.f));}
struct Grid {
 Point origin{},u{60,0},v{0,60}; float px=0,py=0;bool valid=false; float confidence=0;
 void scaleStep(float factor){u=u*factor;v=v*factor;px*=factor;py*=factor;}
 float denominator(float x,float y)const{return 1+px*x+py*y;}
 Point at(float x,float y)const{float d=denominator(x,y);if(std::abs(d)<.00001f)return{NAN,NAN};return(origin+u*x+v*y)*(1/d);}
 float determinant()const{Point a=at(1,0)-origin,b=at(0,1)-origin;return a.x*b.y-a.y*b.x;}
 bool usable()const{return valid&&std::isfinite(determinant())&&length(at(1,0)-origin)>=8&&length(at(0,1)-origin)>=8&&std::abs(determinant())>32;}
 Point cell(Point p)const{float a=u.x-p.x*px,b=v.x-p.x*py,c=u.y-p.y*px,d=v.y-p.y*py,det=a*d-b*c;Point q=p-origin;if(std::abs(det)<.00001f)return{NAN,NAN};return{(q.x*d-b*q.y)/det,(a*q.y-q.x*c)/det};}
 Point snap(Point p)const{if(!usable())return p;Point q=cell(p);if(!std::isfinite(q.x)||!std::isfinite(q.y))return p;Point result=at(std::round(q.x),std::round(q.y));return std::isfinite(result.x)&&std::isfinite(result.y)?result:p;}
 static Grid fromQuad(const std::vector<Point>&corners,int columns,int rows){
  Grid result;if(corners.size()!=4||columns<1||rows<1||columns>200||rows>200)return result;
  float sign=0;for(int i=0;i<4;i++){Point a=corners[(i+1)%4]-corners[i],b=corners[(i+2)%4]-corners[(i+1)%4];float cross=a.x*b.y-a.y*b.x;if(std::abs(cross)<1)return result;if(i==0)sign=cross;else if(sign*cross<=0)return result;}
  double matrix[8][9]{};Point cells[]={{-.5f,-.5f},{columns-.5f,-.5f},{columns-.5f,rows-.5f},{-.5f,rows-.5f}};
  for(int i=0;i<4;i++){double x=cells[i].x,y=cells[i].y,X=corners[i].x,Y=corners[i].y;double*rx=matrix[i*2],*ry=matrix[i*2+1];rx[0]=x;rx[1]=y;rx[2]=1;rx[6]=-X*x;rx[7]=-X*y;rx[8]=X;ry[3]=x;ry[4]=y;ry[5]=1;ry[6]=-Y*x;ry[7]=-Y*y;ry[8]=Y;}
  for(int col=0;col<8;col++){int pivot=col;for(int row=col+1;row<8;row++)if(std::abs(matrix[row][col])>std::abs(matrix[pivot][col]))pivot=row;if(std::abs(matrix[pivot][col])<1e-10)return result;if(pivot!=col)for(int j=col;j<9;j++)std::swap(matrix[col][j],matrix[pivot][j]);double scale=matrix[col][col];for(int j=col;j<9;j++)matrix[col][j]/=scale;for(int row=0;row<8;row++)if(row!=col){double factor=matrix[row][col];for(int j=col;j<9;j++)matrix[row][j]-=factor*matrix[col][j];}}
  double a[8];for(int i=0;i<8;i++)a[i]=matrix[i][8];result.origin={float(a[2]),float(a[5])};result.u={float(a[0]),float(a[3])};result.v={float(a[1]),float(a[4])};result.px=float(a[6]);result.py=float(a[7]);result.valid=true;result.confidence=1;
  if(!result.usable())result.valid=false;return result;
 }
};
enum class Kind{Stroke,Arrow,Pin,Number};
struct Mark{Kind kind=Kind::Stroke;std::vector<Point> points;uint32_t color=0x00b9daef;float width=3;int number=0;float radius=25;};
struct Snapshot{std::vector<Mark> marks;int nextNumber=1;};
class Document{
public:
 std::vector<Mark> marks;int nextNumber=1;
 std::vector<Snapshot> undoStack,redoStack;
 Snapshot snapshot()const{return{marks,nextNumber};}
 void record(const Snapshot& before){undoStack.push_back(before);if(undoStack.size()>64)undoStack.erase(undoStack.begin());redoStack.clear();}
 // Continuous gestures append immediately and record their starting snapshot once on release.
 void appendToGesture(Mark mark){mark.number=mark.kind==Kind::Number?nextNumber++:0;marks.push_back(std::move(mark));}
 void add(Mark mark){record(snapshot());appendToGesture(std::move(mark));}
 void restartNumbers(){if(nextNumber==1)return;record(snapshot());nextNumber=1;}
 void clear(){if(marks.empty()&&nextNumber==1)return;record(snapshot());marks.clear();nextNumber=1;}
 bool undo(){if(undoStack.empty())return false;redoStack.push_back(snapshot());auto s=std::move(undoStack.back());undoStack.pop_back();marks=std::move(s.marks);nextNumber=s.nextNumber;return true;}
 bool redo(){if(redoStack.empty())return false;undoStack.push_back(snapshot());auto s=std::move(redoStack.back());redoStack.pop_back();marks=std::move(s.marks);nextNumber=s.nextNumber;return true;}
 void rescale(float sx,float sy){auto scale=[&](std::vector<Mark>& items){for(auto&m:items){for(auto&p:m.points){p.x*=sx;p.y*=sy;}m.width*=std::sqrt(sx*sy);m.radius*=std::sqrt(sx*sy);}};scale(marks);for(auto&s:undoStack)scale(s.marks);for(auto&s:redoStack)scale(s.marks);}
 // Subtract an exact circle from each polyline segment. New fragments never reconnect across an erased gap.
 bool eraseCircle(Point center,float radius){
  bool changed=false;std::vector<Mark> output;output.reserve(marks.size());
  for(auto&m:marks){
   if(m.points.empty())continue;
   if(m.kind==Kind::Pin||m.kind==Kind::Number){if(distance(center,m.points[0])<=radius+m.radius){changed=true;continue;}output.push_back(m);continue;}
   if(m.kind==Kind::Arrow){Point a=m.points[0],b=m.points.back();float n=length(b-a);Point d=n>.01f?(b-a)*(1/n):Point{1,0};Point normal{-d.y,d.x};float h=std::max(12.f,m.width*4);Point h1=b-d*h+normal*(h*.45f),h2=b-d*h-normal*(h*.45f);
    if(segmentDistance(center,a,b)<=radius+m.width*.5f||segmentDistance(center,b,h1)<=radius+m.width*.5f||segmentDistance(center,b,h2)<=radius+m.width*.5f){changed=true;continue;}output.push_back(m);continue;}
   float r=radius+m.width*.5f;
   if(m.points.size()==1){if(distance(center,m.points[0])<=r){changed=true;continue;}output.push_back(m);continue;}
   Mark fragment=m;fragment.points.clear();
   auto flush=[&](){if(!fragment.points.empty()){output.push_back(fragment);fragment.points.clear();}};
   auto keep=[&](Point a,Point b){if(fragment.points.empty())fragment.points.push_back(a);else if(distance(fragment.points.back(),a)>.05f){flush();fragment.points.push_back(a);}if(distance(a,b)>.001f)fragment.points.push_back(b);};
   for(size_t i=1;i<m.points.size();++i){Point a=m.points[i-1],b=m.points[i],d=b-a,f=a-center;float aa=dot(d,d),bb=2*dot(f,d),cc=dot(f,f)-r*r;
    if(aa<.000001f){if(cc>0)keep(a,b);else{changed=true;flush();}continue;}
    float disc=bb*bb-4*aa*cc;
    if(disc<=0){if(cc>=0)keep(a,b);else{changed=true;flush();}continue;}
    float lo=std::max(0.f,(-bb-std::sqrt(disc))/(2*aa)),hi=std::min(1.f,(-bb+std::sqrt(disc))/(2*aa));
    if(lo>=hi){keep(a,b);continue;}changed=true;if(lo>0)keep(a,a+d*lo);flush();if(hi<1)keep(a+d*hi,b);
   }flush();
  }
  if(changed)marks=std::move(output);return changed;
 }
 bool eraseSweep(Point a,Point b,float radius){float len=distance(a,b);int steps=std::max(1,(int)std::ceil(len/std::max(1.f,radius*.45f)));bool changed=false;for(int i=0;i<=steps;++i)changed=eraseCircle(a+(b-a)*(float(i)/steps),radius)||changed;return changed;}
};
}
