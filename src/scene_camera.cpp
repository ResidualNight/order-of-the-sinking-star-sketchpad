#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "scene_camera.hpp"
#include <algorithm>
#include <initializer_list>
namespace sketch {
bool SceneCameraPose::valid()const{
 for(float v:position)if(!std::isfinite(v)||std::abs(v)>10000)return false;
 double norm=0;for(float v:orientation){if(!std::isfinite(v))return false;norm+=double(v)*v;}
 for(float v:projection)if(!std::isfinite(v))return false;
 return std::abs(norm-1)<=.002&&projection[0]>.01f&&projection[0]<3.13f&&projection[1]>0&&projection[2]>projection[1]&&projection[2]<=1000000;
}
bool SceneCameraPose::project(float w,float h,std::array<float,16>& m,std::array<float,3>& direction)const{
 if(!valid()||!cameraViewportMatches(1/w,1/h,w,h))return false;
 double x=orientation[0],y=orientation[1],z=orientation[2],q=orientation[3];
 double norm=std::sqrt(x*x+y*y+z*z+q*q);x/=norm;y/=norm;z/=norm;q/=norm;
 // The game's local axes are +X forward, +Y left, +Z up. View = inverse pose.
 double axes[3][3]={{1-2*(y*y+z*z),2*(x*y+z*q),2*(x*z-y*q)},
  {2*(x*y-z*q),1-2*(x*x+z*z),2*(y*z+x*q)},
  {2*(x*z+y*q),2*(y*z-x*q),1-2*(x*x+y*y)}};
 double view[16]{};for(int r=0;r<3;++r){for(int c=0;c<3;++c)view[4*r+c]=axes[r][c];
  for(int c=0;c<3;++c)view[4*r+3]-=axes[r][c]*position[c];direction[r]=float(axes[0][r]);}view[15]=1;
 double s=1/std::tan(double(projection[0])*.5),n=projection[1],f=projection[2];
 // r67271 reversed depth. Retain perspective; no nearest-axis approximation.
 double p[16]={0,-s*h/w,0,0, 0,0,s,0, -n/(f-n),0,0,f*n/(f-n), 1,0,0,0};
 for(int r=0;r<4;++r)for(int c=0;c<4;++c){double v=0;for(int k=0;k<4;++k)v+=p[4*r+k]*view[4*k+c];
  if(!std::isfinite(v)||std::abs(v)>1e7)return false;m[4*r+c]=float(v);}
 return true;
}
Grid SceneCameraPose::grid(float w,float h,float z)const{
 std::array<float,16> m{};std::array<float,3> d{};if(!project(w,h,m,d))return {};
 return gridFromProjection(m.data(),position.data(),d.data(),w,h,z);
}
uint8_t SceneCameraPose::category(float w,float h)const{
 std::array<float,16> m{};std::array<float,3> d{};if(!project(w,h,m,d))return 7;
 return projectionOrientation(m.data(),position.data(),d.data()).category;
}
bool sceneCameraEvidence(const SceneCameraPose& pose,const CameraFrame& frame,float w,float h){
 const auto& f=frame.f;
 if(frame.sourceType()!=8||!frame.frame()||!std::isfinite(f[211])||f[211]<0||f[211]>1e7||
    !std::isfinite(f[235])||f[235]<0||f[235]>.05f||!cameraViewportMatches(f[216],f[217],w,h))return false;
 std::array<float,16> matrix{};std::array<float,3> dir{};if(!pose.project(w,h,matrix,dir))return false;
 double dot=0,norm=0;for(int i=0;i<3;++i){
  if(!std::isfinite(f[208+i])||std::abs(f[208+i]-pose.position[i])>.05f||!std::isfinite(f[212+i]))return false;
  dot+=dir[i]*f[212+i];norm+=double(f[212+i])*f[212+i];
 }
 return norm>=.94&&norm<=1.06&&dot/std::sqrt(norm)>.9999;
}
namespace {
bool contains(uintptr_t base,uintptr_t size,uintptr_t at,size_t n){return at>=base&&n<=size&&at-base<=size-n;}
template<class T>bool get(SceneCameraReader::Read read,uintptr_t at,T& v){return at>0x10000&&read(at,&v,sizeof(v));}
int64_t find(const std::vector<unsigned char>& code,std::initializer_list<int> pattern){
 if(pattern.size()>code.size())return -1;int64_t found=-1;
 for(size_t i=0;i<=code.size()-pattern.size();++i){if(*pattern.begin()>=0&&code[i]!=*pattern.begin())continue;
  size_t k=0;bool match=true;for(int b:pattern){if(b>=0&&code[i+k]!=b){match=false;break;}++k;}
  if(match){if(found>=0)return -2;found=int64_t(i);}
 }return found;
}
bool matches(const std::vector<unsigned char>& code,int64_t at,std::initializer_list<int> p){
 if(at<0||size_t(at)>code.size()||p.size()>code.size()-size_t(at))return false;
 for(int b:p){if(b>=0&&code[size_t(at)]!=b)return false;++at;}return true;
}
template<class T>T value(const std::vector<unsigned char>& code,int64_t at){T v{};if(at>=0&&size_t(at)<=code.size()&&sizeof(v)<=code.size()-size_t(at))std::memcpy(&v,code.data()+at,sizeof(v));return v;}
}
bool SceneCameraReader::current(Read read)const{
 if(!available())return false;
 for(const auto& e:layout.evidence){unsigned char bytes[96]{};if(e.bytes.empty()||e.bytes.size()>sizeof(bytes)||
  !contains(layout.image,layout.size,e.address,e.bytes.size())||!read(e.address,bytes,e.bytes.size())||std::memcmp(bytes,e.bytes.data(),e.bytes.size()))return false;}
 return true;
}
bool SceneCameraReader::discover(Read read,uintptr_t image,uintptr_t managerRva){
 layout={};status="pe_unavailable";IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
 if(!get(read,image,dos)||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<64||dos.e_lfanew>0x100000||
    !get(read,image+dos.e_lfanew,nt)||nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
    nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt.FileHeader.NumberOfSections<1||nt.FileHeader.NumberOfSections>96||
    nt.FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)||nt.OptionalHeader.SizeOfImage<0x1000||nt.OptionalHeader.SizeOfImage>0x10000000)return false;
 layout.image=image;layout.size=nt.OptionalHeader.SizeOfImage;
 auto table=image+dos.e_lfanew+24+nt.FileHeader.SizeOfOptionalHeader;
 if(!contains(image,layout.size,table,nt.FileHeader.NumberOfSections*sizeof(IMAGE_SECTION_HEADER)))return false;
 uintptr_t base=0;std::vector<unsigned char> code;std::vector<IMAGE_SECTION_HEADER> sections;
 for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i){IMAGE_SECTION_HEADER section{};if(!get(read,table+i*sizeof(section),section))return false;sections.push_back(section);
  if(std::memcmp(section.Name,".text\0\0\0",8))continue;
  if(base||!(section.Characteristics&IMAGE_SCN_MEM_EXECUTE)||section.Misc.VirtualSize>64*1024*1024||!section.Misc.VirtualSize||
     !contains(image,layout.size,image+section.VirtualAddress,section.Misc.VirtualSize))return false;
  base=image+section.VirtualAddress;code.resize(section.Misc.VirtualSize);if(!read(base,code.data(),code.size()))return false;
 }
 status="signature_unavailable";
 int64_t gate=find(code,{0x49,0x8d,0xad,-1,-1,-1,-1,0x48,0x8b,0x05,-1,-1,-1,-1,0x48,0x85,0xc0,0x0f,0x84,-1,-1,-1,-1,0x48,0x83,0xf8,1,0x0f,0x85,-1,-1,-1,-1});
 int64_t gateR12=find(code,{0x49,0x8d,0xac,0x24,-1,-1,-1,-1,0x48,0x8b,0x05,-1,-1,-1,-1,0x48,0x85,0xc0,0x0f,0x84,-1,-1,-1,-1,0x48,0x83,0xf8,1,0x0f,0x85,-1,-1,-1,-1});
 if(gate==-2||gateR12==-2||(gate>=0&&gateR12>=0))return false;
 bool r12=gateR12>=0;if(r12)gate=gateR12;
 int64_t render=find(code,{0x4c,0x8d,0x80,-1,-1,-1,-1,0xf3,0x0f,0x10,0x88,-1,-1,-1,-1,0xf3,0x0f,0x10,0x98,-1,-1,-1,-1,0xf3,0x0f,0x10,0x90,-1,-1,-1,-1});
 int64_t link=find(code,{0x4c,0x8b,0x2d,-1,-1,-1,-1,0x49,0x8d,0x95,-1,-1,-1,-1,0x4d,0x8d,0x85,-1,-1,-1,-1,0x31,0xc9,0xe8,-1,-1,-1,-1});
 if(gate<0||render<47||link<0)return false;
 int64_t normal=gate+23+r12+value<int32_t>(code,gate+19+r12),frozen=gate+33+r12+value<int32_t>(code,gate+29+r12),list=normal+(r12?70:69),write=frozen-(r12?19:17);
 if(!(r12?matches(code,normal,{0x4c,0x89,0x74,0x24,-1,0x49,0x83,0xbc,0x24,-1,-1,-1,-1,0,0x0f,0x8e,-1,-1,-1,-1}):matches(code,normal,{0x4c,0x89,0x74,0x24,-1,0x49,0x83,0xbd,-1,-1,-1,-1,0,0x0f,0x8e,-1,-1,-1,-1}))||
    !matches(code,normal-23,{0x50,0xc3,0xa8,1,0x0f,0x85,-1,-1,-1,-1,0xf3,0x0f,0x10,0x1d,-1,-1,-1,-1,0xe9,-1,-1,-1,-1})||
    !(r12?matches(code,list,{0x49,0x8b,0x84,0x24,-1,-1,-1,-1,0x4a,0x8b,0x1c,0xf8}):matches(code,list,{0x49,0x8b,0x85,-1,-1,-1,-1,0x4c,0x8b,0x34,0xd8}))||
    !(r12?matches(code,write,{0x41,0x0f,0x13,0x84,0x24,-1,-1,-1,-1,0xf3,0x41,0x0f,0x11,0x94,0x24,-1,-1,-1,-1,0x49,0x8b,0x04,0x24,0x48,0x83,0xc0,0xfb,0x48,0x83,0xf8,0x14,0x0f,0x87,-1,-1,-1,-1}):matches(code,write,{0x41,0x0f,0x13,0x85,-1,-1,-1,-1,0xf3,0x41,0x0f,0x11,0x95,-1,-1,-1,-1,0x49,0x8b,0x45,0,0x48,0x83,0xc0,0xfb,0x48,0x83,0xf8,0x14,0x0f,0x87,-1,-1,-1,-1}))||
    !matches(code,render-47,{0x48,0x8d,0x90,-1,-1,-1,-1,0x0f,0xb6,0x0d,-1,-1,-1,-1,0xc0,0xe9,4,0x80,0xe1,1,0x4c,0x8b,0x05,-1,-1,-1,-1,0xf2,0x41,0x0f,0x10,0x40,0x38,0x0f,0x5b,0xc0,0x0f,0x28,0xc8,0x0f,0xc6,0xc8,0x55,0xf3,0x0f,0x5e,0xc1}))return false;
 status="layout_conflict";
 layout.position=value<uint32_t>(code,gate+3+r12);layout.orientation=value<uint32_t>(code,render+3);layout.projection=value<uint32_t>(code,render+11);layout.cameras=value<uint32_t>(code,normal+8+r12);
 int64_t slot=int64_t(base)+link+7+value<int32_t>(code,link+3),mode=int64_t(base)+gate+14+r12+value<int32_t>(code,gate+10+r12);
 if(layout.position<0x380||layout.position>0x100000||layout.orientation!=layout.position+24||layout.projection!=layout.orientation+16||
    layout.cameras<0x380||layout.cameras+16>layout.position||value<uint32_t>(code,normal+59+r12)!=layout.cameras||value<uint32_t>(code,list+3+r12)!=layout.cameras+8||
    value<uint32_t>(code,write+4+r12)!=layout.position||value<uint32_t>(code,frozen-4)!=layout.position+8||value<uint32_t>(code,render-44)!=layout.position||
    value<uint32_t>(code,render+19)!=layout.projection+4||value<uint32_t>(code,render+27)!=layout.projection+8||
    value<uint32_t>(code,link+10)!=layout.position||value<uint32_t>(code,link+17)!=layout.orientation||
    slot<=0||mode<=0||slot!=int64_t(image+managerRva)||mode==slot||!contains(image,layout.size,uintptr_t(mode),8))return false;
 bool writable=false,slotWritable=false;for(const auto& s:sections)if((s.Characteristics&(IMAGE_SCN_MEM_WRITE|IMAGE_SCN_MEM_EXECUTE))==IMAGE_SCN_MEM_WRITE){
  if(contains(image+s.VirtualAddress,s.Misc.VirtualSize,uintptr_t(mode),8))writable=true;
  if(contains(image+s.VirtualAddress,s.Misc.VirtualSize,uintptr_t(slot),8))slotWritable=true;
 }
 if(!writable||!slotWritable)return false;
 layout.mode=uintptr_t(mode);
 for(auto span:{std::pair<int64_t,size_t>{gate,size_t(33+r12)},{normal,size_t(19+r12)},{list,size_t(11+r12)},{write,size_t(35+2*r12)},{render-47,78},{link,28}}){
  if(span.first<0||size_t(span.first)>code.size()||span.second>code.size()-size_t(span.first))return false;
  Evidence e;e.address=base+uintptr_t(span.first);e.bytes.assign(code.begin()+span.first,code.begin()+span.first+span.second);layout.evidence.push_back(std::move(e));
 }
 layout.managerSlot=uintptr_t(slot);if(!current(read)){layout.managerSlot=0;status="code_changed";return false;}status="ready";return true;
}
bool SceneCameraReader::sample(Read read,uintptr_t manager,SceneCameraPose& output){
 status="code_changed";if(!current(read))return false;
 status="scene_identity";uintptr_t active=0;int64_t mode=-1;
 if(!get(read,layout.managerSlot,active)||active!=manager||!get(read,layout.mode,mode)||mode!=0)return false;
 struct List{int64_t count;uintptr_t data;};List list{};
 if(!get(read,manager+layout.cameras,list)||list.count<1||list.count>64||list.data<=0x10000)return false;
 struct Identity{uintptr_t address,owner,type;uint32_t id;};std::array<Identity,64> ids{};
 std::array<uintptr_t,64> pointers{};if(!read(list.data,pointers.data(),size_t(list.count)*8))return false;
 for(size_t i=0;i<size_t(list.count);++i){auto& item=ids[i];item.address=pointers[i];uint8_t destroyed=1;
  struct Type{uint32_t kind,pad;int64_t size,count;uintptr_t name;}type{};char name[14]{};
  if(!get(read,item.address+240,item.owner)||item.owner!=manager||!get(read,item.address+256,item.id)||!item.id||
     !get(read,item.address+264,item.type)||!contains(layout.image,layout.size,item.type,sizeof(type))||!get(read,item.type,type)||
     type.kind!=7||type.size!=1072||type.count!=14||!contains(layout.image,layout.size,type.name,14)||!read(type.name,name,14)||std::memcmp(name,"Camera_Control",14)||
     !get(read,item.address+280,destroyed)||destroyed)return false;
  for(size_t j=0;j<i;++j)if(ids[j].address==item.address)return false;
 }
 auto pose=[&](SceneCameraPose& p){return read(manager+layout.position,p.position.data(),12)&&read(manager+layout.orientation,p.orientation.data(),16)&&read(manager+layout.projection,p.projection.data(),12);};
 SceneCameraPose first{},second{};status="pose_unstable";
 if(!pose(first)||!first.valid()||!pose(second)||first.position!=second.position||first.orientation!=second.orientation||first.projection!=second.projection)return false;
 List after{};std::array<uintptr_t,64> again{};status="scene_identity";
 if(!get(read,manager+layout.cameras,after)||after.count!=list.count||after.data!=list.data||!read(after.data,again.data(),size_t(list.count)*8)||again!=pointers)return false;
 for(size_t i=0;i<size_t(list.count);++i){uintptr_t owner=0,type=0;uint32_t id=0;uint8_t dead=1;const auto& item=ids[i];
  if(!get(read,item.address+240,owner)||owner!=item.owner||!get(read,item.address+264,type)||type!=item.type||!get(read,item.address+256,id)||id!=item.id||!get(read,item.address+280,dead)||dead)return false;}
 if(!get(read,layout.managerSlot,active)||active!=manager||!get(read,layout.mode,mode)||mode!=0||!current(read))return false;
 output=second;status="ready";return true;
}
}
