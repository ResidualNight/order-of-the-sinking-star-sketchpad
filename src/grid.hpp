#pragma once
#include "core.hpp"
#include <string>
#include <cstdio>
namespace sketch {
struct Image{int width=0,height=0;std::vector<uint8_t> rgba;};
struct Detection{Grid grid;std::string diagnostic;};
struct GridAxis{float angle=0,period=0,phase=0,score=0;};
inline float sample(const std::vector<float>& a,float x){int i=(int)std::floor(x);if(i<0||i+1>=(int)a.size())return 0;return a[i]*(1-(x-i))+a[i+1]*(x-i);}
inline Detection detectGrid(const Image& source){
 Detection result;if(source.width<100||source.height<100||source.rgba.size()<size_t(source.width)*source.height*4){result.diagnostic="invalid_image";return result;}
 float reduction=std::min(1.f,960.f/source.width);int w=int(source.width*reduction),h=int(source.height*reduction);
 std::vector<float> gray(w*h),green(w*h);
 for(int y=0;y<h;y++)for(int x=0;x<w;x++){int sx=std::min(source.width-1,int(x/reduction)),sy=std::min(source.height-1,int(y/reduction));auto p=&source.rgba[(sy*source.width+sx)*4];gray[y*w+x]=(p[1]-.5f*(p[0]+p[2]))/255;green[y*w+x]=std::max(0.f,(p[1]-.5f*(p[0]+p[2]))/255.f);}
 struct Edge{float x,y,gx,gy,weight;};std::vector<Edge> edges;
 int x0=w*.10f,x1=w*.90f,y0=h*.32f,y1=h*.80f;
 for(int y=y0+1;y<y1-1;y+=2)for(int x=x0+1;x<x1-1;x+=2){float gx=gray[y*w+x+1]-gray[y*w+x-1],gy=gray[(y+1)*w+x]-gray[(y-1)*w+x];float mag=std::sqrt(gx*gx+gy*gy);if(mag>.008f)edges.push_back({float(x),float(y),gx/mag,gy/mag,std::min(.08f,mag)*(1+2*green[y*w+x])});}
 if(edges.size()<120){
  for(int y=0;y<h;y++)for(int x=0;x<w;x++){int sx=std::min(source.width-1,int(x/reduction)),sy=std::min(source.height-1,int(y/reduction));auto p=&source.rgba[(sy*source.width+sx)*4];gray[y*w+x]=(.299f*p[0]+.587f*p[1]+.114f*p[2])/255;}
  edges.clear();for(int y=y0+1;y<y1-1;y+=2)for(int x=x0+1;x<x1-1;x+=2){float gx=gray[y*w+x+1]-gray[y*w+x-1],gy=gray[(y+1)*w+x]-gray[(y-1)*w+x];float mag=std::sqrt(gx*gx+gy*gy);if(mag>.025f)edges.push_back({float(x),float(y),gx/mag,gy/mag,std::min(.15f,mag)});}
 }
 if(edges.size()<120){result.diagnostic="insufficient_edges";return result;}
 int count=2*(w+h);float offset=w+h;float normalizer=0;for(auto&e:edges)normalizer+=e.weight;normalizer/=std::max(w,h);
 auto axis=[&](float center){GridAxis best;
  for(int step=-5;step<=5;step++){float angle=(center+step*.6f)*3.14159265f/180;float nx=std::cos(angle),ny=std::sin(angle);float minT=1e9f,maxT=-1e9f;for(auto q:std::vector<Point>{{float(x0),float(y0)},{float(x1),float(y0)},{float(x1),float(y1)},{float(x0),float(y1)}}){float t=q.x*nx+q.y*ny+offset;minT=std::min(minT,t);maxT=std::max(maxT,t);}std::vector<float> profile(count),support(count);
   for(auto&e:edges){float align=std::abs(e.gx*nx+e.gy*ny);if(align<.8f)continue;float t=e.x*nx+e.y*ny+offset;int k=int(t);if(k>=1&&k+1<count){float val=e.weight*std::pow(align,6.f);profile[k]+=val*(1-(t-k));profile[k+1]+=val*(t-k);support[k]+=1;}}
   std::vector<float> filtered(count);float total=0;int active=0;
   for(int k=8;k<count-8;k++){float mean=0;for(int j=-7;j<=7;j++)mean+=profile[k+j];float v=(profile[k-1]+2*profile[k]+profile[k+1])*.25f-mean/15.f;filtered[k]=v;total+=std::abs(v);if(std::abs(v)>.001f)active++;}
   if(active<8||total<5)continue;
   float maxValue=*std::max_element(filtered.begin(),filtered.end());float avg=total/std::max(1,active),profileNorm=0;for(float q:filtered)profileNorm+=q*q;profileNorm=std::sqrt(profileNorm);if(maxValue<=0||profileNorm<=0)continue;
   // Limit periods to gameplay-sized cells; this suppresses small decorative tile detail.
   float minPeriod=std::max(12.f,w/60.f),maxPeriod=std::min(w/16.f,h/3.f);
   for(float period=minPeriod;period<=maxPeriod;period+=.5f){
    for(float phase=0;phase<period;phase+=1.f){float sum=0,energy=0;int n=0,hits=0;
     for(float t=phase;t<count-2;t+=period){if(t<minT||t>maxT)continue;float v=sample(filtered,t);if(v>avg*.65f)hits++;sum+=v;energy+=v*v;n++;}
     #ifdef SKETCH_GRID_DEBUG
     if(step==0 && period==40 && (phase==0||phase==39||phase==38))std::fprintf(stderr,"center=%.0f phase=%.0f active=%d avg=%.2f sum=%.2f n=%d hits=%d total=%.2f\n",center,phase,active,avg,sum,n,hits,total);
#endif
     if(n<4||hits<4)continue;
     float score=(sum/std::sqrt(float(n)))/(profileNorm+.0001f);float coverage=float(hits)/std::max(1,n);score*=coverage;score*=1.f+.08f*std::log(period/minPeriod);
     if(score>best.score){best={angle,period,phase-offset,score};}
    }
   }
  }return best;
 };
 auto a=axis(0),b=axis(90);float det=std::cos(a.angle)*std::sin(b.angle)-std::sin(a.angle)*std::cos(b.angle);
 if(a.score<.10f||b.score<.10f||std::abs(det)<.55f){result.diagnostic="low_confidence a="+std::to_string(a.score)+",b="+std::to_string(b.score)+",pa="+std::to_string(a.period)+",pb="+std::to_string(b.period);return result;}
 float ax=std::cos(a.angle),ay=std::sin(a.angle),bx=std::cos(b.angle),by=std::sin(b.angle);
 // Peaks describe cell boundaries. Offset half a period to produce cell centers.
 float da=a.phase+a.period*.5f,db=b.phase+b.period*.5f;
 Grid g;g.origin={(da*by-ay*db)/det/reduction,(ax*db-da*bx)/det/reduction};g.u={a.period*by/det/reduction,-a.period*bx/det/reduction};g.v={-b.period*ay/det/reduction,b.period*ax/det/reduction};g.valid=true;
 Point center{source.width*.5f,source.height*.5f};g.origin=g.snap(center);g.confidence=std::clamp(std::min(a.score,b.score)/.4f,0.f,1.f);
 result.grid=g;result.diagnostic="a="+std::to_string(a.score)+",b="+std::to_string(b.score)+",u="+std::to_string(length(g.u))+",v="+std::to_string(length(g.v));return result;
}
}