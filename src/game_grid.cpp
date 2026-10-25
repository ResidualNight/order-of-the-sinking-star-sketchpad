#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <tlhelp32.h>
#include <bcrypt.h>
#include "game_grid.hpp"
#include <cstdio>
#include <algorithm>
#include <memory>
namespace sketch { namespace {
bool read(uintptr_t address,void*data,size_t size){SIZE_T done=0;return address>0x10000&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(address),data,size,&done)&&done==size;}
template<class T>bool get(uintptr_t address,T&data){return read(address,&data,sizeof(data));}
bool fingerprint(){
 static const unsigned char expected[]={0x0a,0xe1,0x0a,0x51,0x3d,0xae,0x25,0x28,0x6f,0xe9,0x3d,0x0d,0xb4,0xfa,0x8a,0x2d,0xf9,0xa4,0x73,0xbc,0x07,0xe1,0x73,0x4e,0x12,0xbc,0x1a,0xd3,0x01,0x66,0xdb,0x1c};
 wchar_t path[32768];if(!GetModuleFileNameW(nullptr,path,32768))return false;
 HANDLE file=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 BCRYPT_ALG_HANDLE algorithm=nullptr;BCRYPT_HASH_HANDLE hash=nullptr;bool success=false;
 if(BCryptOpenAlgorithmProvider(&algorithm,BCRYPT_SHA256_ALGORITHM,nullptr,0)>=0&&BCryptCreateHash(algorithm,&hash,nullptr,0,nullptr,0,0)>=0){
  unsigned char buffer[65536],digest[32];DWORD count=0;bool ok=true;
  for(;;){if(!ReadFile(file,buffer,sizeof(buffer),&count,nullptr)){ok=false;break;}if(!count)break;if(BCryptHashData(hash,buffer,count,0)<0){ok=false;break;}}
  success=ok&&BCryptFinishHash(hash,digest,sizeof(digest),0)>=0&&std::memcmp(expected,digest,32)==0;
 }
 if(hash)BCryptDestroyHash(hash);if(algorithm)BCryptCloseAlgorithmProvider(algorithm,0);CloseHandle(file);return success;
}
bool typeMatches(uintptr_t entity,const char*name,int64_t expectedSize){
 uintptr_t type=0;if(!get(entity+264,type))return false;
 struct Info{uint32_t tag,pad;int64_t size,count;uintptr_t data;}info{};
 if(!get(type,info)||info.tag!=7||info.size!=expectedSize||info.count!=int64_t(std::strlen(name)))return false;
 char value[64]{};return info.count>0&&info.count<63&&read(info.data,value,size_t(info.count))&&std::memcmp(value,name,size_t(info.count))==0;
}
bool pointerArray(uintptr_t address,int64_t maximum,std::vector<uintptr_t>&out){
 struct Array{int64_t count;uintptr_t data;int64_t capacity;}array{};
 if(!get(address,array)||array.count<1||array.count>maximum||array.capacity<array.count||array.capacity>100000)return false;
 out.resize(size_t(array.count));return read(array.data,out.data(),out.size()*sizeof(uintptr_t));
}
}
bool GameGridReader::readLevel(){
 // Relative offsets apply only after the exact r67271 executable hash has matched.
 if(!get(image+0xb69600,manager)||!manager)return fail(ErrorId::EntityManagerUnavailable);
 std::vector<uintptr_t> settings;if(!pointerArray(manager+0xdd0,4,settings))return fail(ErrorId::SettingsArrayUnavailable);
 uintptr_t selected=0;for(auto p:settings){uintptr_t owner=0;if(get(p+240,owner)&&owner==manager&&typeMatches(p,"Settings",1320)){selected=p;break;}}
 struct Gridlike{int64_t x,y,count;uint8_t wrapping,pad[3];int32_t corner[3];}g{};
 if(!selected||!get(selected+816,g)||g.x<1||g.y<1||g.x>2048||g.y>2048||g.count!=g.x*g.y)return fail(ErrorId::SettingsGridInvalid);
 columns=int(g.x);rows=int(g.y);actors.clear();std::vector<uintptr_t> guys;
 if(pointerArray(manager+0x920,32,guys))for(auto p:guys){uintptr_t owner=0;std::array<float,3>pos{};uint8_t destroyed=0;
  if(get(p+240,owner)&&owner==manager&&typeMatches(p,"Guy",1608)&&get(p,pos)&&get(p+280,destroyed)&&!destroyed&&std::all_of(pos.begin(),pos.end(),[](float v){return std::isfinite(v)&&std::abs(v)<10000;}))actors.push_back(pos);
 }
 if(!heightOverride&&actors.empty())return fail(ErrorId::ActorLayerUnavailable);return true;
}
void GameGridReader::scan(){
 candidates.clear();lastScan=GetTickCount64();
 struct ThreadBasic{LONG status;void*teb;ULONG_PTR clientProcess,clientThread,affinity;LONG priority,basePriority;};
 using QueryThread=LONG(NTAPI*)(HANDLE,ULONG,void*,ULONG,ULONG*);auto query=reinterpret_cast<QueryThread>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryInformationThread"));if(!query)return;
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snapshot==INVALID_HANDLE_VALUE)return;
 THREADENTRY32 thread{};thread.dwSize=sizeof(thread);std::vector<unsigned char> buffer;
 if(Thread32First(snapshot,&thread))do{if(thread.th32OwnerProcessID!=GetCurrentProcessId())continue;
  HANDLE handle=OpenThread(THREAD_QUERY_INFORMATION,FALSE,thread.th32ThreadID);if(!handle)continue;ThreadBasic info{};LONG status=query(handle,0,&info,sizeof(info),nullptr);CloseHandle(handle);if(status<0)continue;
  uintptr_t tib[3]{};if(!read(reinterpret_cast<uintptr_t>(info.teb),tib,sizeof(tib)))continue;uintptr_t low=tib[2],high=tib[1];if(high<=low||high-low>8*1024*1024)continue;
  buffer.resize(high-low);if(!read(low,buffer.data(),buffer.size()))continue;
  for(size_t offset=0;offset+960<=buffer.size();offset+=4){float viewport[2];std::memcpy(viewport,buffer.data()+offset+864,8);
   if(viewport[0]<=0||viewport[1]<=0||std::abs(1.f/viewport[0]-width)>1||std::abs(1.f/viewport[1]-height)>1)continue;
   // Candidate storage lives on the heap so scanning cannot discover our own local copies.
   auto frame=std::make_unique<CameraFrame>();std::memcpy(frame->f.data(),buffer.data()+offset,960);
   if(acceptsCamera(*frame))candidates.push_back({low+offset,frame->frame(),frame->f[211]});
  }
 }while(Thread32Next(snapshot,&thread));CloseHandle(snapshot);
}
bool GameGridReader::verifyBuild(){
 if(!checked){checked=true;supported=fingerprint();image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));}
 return supported;
}
bool GameGridReader::readCurrentLevel(LevelIdentity& identity){
 return verifyBuild()&&readValidatedLevel(read,image,identity);
}
bool GameGridReader::begin(float w,float h,bool overrideHeight,float z){
 error=ErrorId::None;diagnostic.clear();rejectedCameraOrientation=false;
 width=w;height=h;heightOverride=overrideHeight;forcedZ=z;started=GetTickCount64();
 if(!verifyBuild())return fail(ErrorId::UnsupportedGameBuild);if(!readLevel())return false;
 std::vector<Candidate> live;for(auto c:candidates){auto frame=std::make_unique<CameraFrame>();if(read(c.address,frame->f.data(),960)&&acceptsCamera(*frame))live.push_back({c.address,frame->frame(),frame->f[211]});}
 candidates=std::move(live);if(candidates.empty())scan();diagnostic="waiting_for_fresh_camera_frame";return true;
}
GameGridState GameGridReader::poll(Grid& grid){
 auto now=GetTickCount64();if(now-started>1800){fail(rejectedCameraOrientation?ErrorId::CameraOrientationInvalid:(candidates.empty()?ErrorId::CameraNotFound:ErrorId::FreshCameraFrameUnavailable));return GameGridState::Unavailable;}
 auto best=std::make_unique<CameraFrame>();bool have=false;
 for(auto c:candidates){auto frame=std::make_unique<CameraFrame>();if(!read(c.address,frame->f.data(),960)||!acceptsCamera(*frame))continue;
  uint32_t advance=frame->frame()-c.frame;float seconds=frame->f[211]-c.time;
  if(!advance||advance>2000||seconds<0||seconds>5)continue;if(!have||int32_t(frame->frame()-best->frame())>0){*best=*frame;have=true;}
 }
 if(!have){if(now-lastScan>=250)scan();return GameGridState::Waiting;}
 planeZ=forcedZ;if(!heightOverride){double nearest=1e100;const auto&m=best->f;
  for(auto p:actors){double d=m[12]*p[0]+m[13]*p[1]+m[14]*p[2]+m[15];if(d<=.1)continue;
   double x=(m[0]*p[0]+m[1]*p[1]+m[2]*p[2]+m[3])/d,y=(m[4]*p[0]+m[5]*p[1]+m[6]*p[2]+m[7])/d;
   double distance=x*x+y*y;if(distance<nearest){nearest=distance;planeZ=p[2];}
  }
  if(nearest==1e100){fail(ErrorId::VisibleActorLayerUnavailable);return GameGridState::Unavailable;}
 }
 uintptr_t activeManager=0;if(!get(image+0xb69600,activeManager)||activeManager!=manager){fail(ErrorId::LevelChanged);return GameGridState::Unavailable;}
 grid=gridFromCamera(*best,width,height,planeZ);if(!grid.usable()){fail(ErrorId::ProjectedGridInvalid);return GameGridState::Unavailable;}
 char message[240];std::snprintf(message,sizeof(message),"game_data r67271 frame=%u logical=%dx%d z=%.3f candidates=%zu manager=0x%llx",best->frame(),columns,rows,planeZ,candidates.size(),static_cast<unsigned long long>(manager));diagnostic=message;return GameGridState::Ready;
}
}
