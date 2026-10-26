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
 // Try the r67271 layout on unverified builds too; every read and layout check
 // remains required. The executable fingerprint is a compatibility warning.
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
 auto previous=picking?std::move(candidates):std::vector<Candidate>{};
 candidates.clear();lastScan=GetTickCount64();++scanStats.scans;cameraDiagnostics.beginScan();
 struct ThreadBasic{LONG status;void*teb;ULONG_PTR clientProcess,clientThread,affinity;LONG priority,basePriority;};
 using QueryThread=LONG(NTAPI*)(HANDLE,ULONG,void*,ULONG,ULONG*);auto query=reinterpret_cast<QueryThread>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryInformationThread"));if(!query){++scanStats.queryApiFailures;scanStats.systemError=scanStats.queryApiError=GetLastError();candidates=std::move(previous);return;}
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snapshot==INVALID_HANDLE_VALUE){++scanStats.snapshotFailures;scanStats.systemError=scanStats.snapshotError=GetLastError();candidates=std::move(previous);return;}
 THREADENTRY32 thread{};thread.dwSize=sizeof(thread);
 if(Thread32First(snapshot,&thread)){do{if(thread.th32OwnerProcessID!=GetCurrentProcessId())continue;
  ++scanStats.threads;HANDLE handle=OpenThread(THREAD_QUERY_INFORMATION,FALSE,thread.th32ThreadID);if(!handle){++scanStats.openThreadFailures;scanStats.systemError=scanStats.openThreadError=GetLastError();continue;}ThreadBasic info{};LONG status=query(handle,0,&info,sizeof(info),nullptr);CloseHandle(handle);if(status<0){++scanStats.queryThreadFailures;scanStats.threadQueryStatus=status;scanStats.systemError=DWORD(status);continue;}
  uintptr_t tib[3]{};if(!read(reinterpret_cast<uintptr_t>(info.teb),tib,sizeof(tib))){++scanStats.tibReadFailures;scanStats.systemError=scanStats.tibReadError=GetLastError();continue;}uintptr_t low=tib[2],high=tib[1];if(high<=low||low<0x10000){++scanStats.badStackRanges;continue;}
  ++scanStats.stackRanges;
  scanCameraStack(low,high,[&](uintptr_t address,const unsigned char* data){float viewport[2];std::memcpy(viewport,data+864,8);
   bool matches=cameraViewportMatches(viewport[0],viewport[1],width,height);
   if(!matches&&!CameraDiagnostics::plausibleViewport(viewport[0],viewport[1]))return;
   // Candidate storage lives on the heap so scanning cannot discover our own local copies.
   auto frame=std::make_unique<CameraFrame>();std::memcpy(frame->f.data(),data,960);
   if(!matches){cameraDiagnostics.probeViewport(*frame);return;} // Diagnostics only; never add a mismatched frame.
   ++scanStats.viewportMatches;
   auto check=inspectCamera(*frame,width,height);cameraDiagnostics.record(CameraStage::Scan,check,frame.get());
   if(check.result==CameraValidation::Valid){++scanStats.valid;if(!picking||candidates.size()<PickMaxCandidates)candidates.push_back({address,frame->frame(),frame->f[211]});}
   else if(check.result==CameraValidation::OrientationInvalid){++scanStats.orientation;rejectedCameraOrientation=true;if(!picking||candidates.size()<PickMaxCandidates)candidates.push_back({address,frame->frame(),frame->f[211]});}
   else ++scanStats.invalid;
  },scanStats);
 }while(Thread32Next(snapshot,&thread));}
 DWORD enumerationError=GetLastError();if(enumerationError!=ERROR_NO_MORE_FILES){++scanStats.enumerationFailures;scanStats.enumerationError=enumerationError;}CloseHandle(snapshot);
 for(auto old:previous){auto it=std::find_if(candidates.begin(),candidates.end(),[&](auto now){return now.address==old.address;});if(it!=candidates.end())*it=old;else if(candidates.size()<PickMaxCandidates)candidates.push_back(old);}
}
std::vector<std::string> GameGridReader::failureDetails()const{
 if(error==ErrorId::None)return {};
 std::vector<std::string> lines;char context[480];
 std::snprintf(context,sizeof(context),"context output=%.0fx%.0f elapsed_ms=%llu verified_build=%d level_validated=%d logical=%dx%d actors=%zu height_override=%d requested_z=%.6g selected_z=%.6g remaining_candidates=%zu",width,height,static_cast<unsigned long long>(GetTickCount64()-started),supported?1:0,levelValidated?1:0,levelValidated?columns:0,levelValidated?rows:0,levelValidated?actors.size():0,heightOverride?1:0,forcedZ,planeSelected?planeZ:NAN,candidates.size());lines.emplace_back(context);
 lines.push_back("scan"+scanDiagnostic());
 char memory[1000];std::snprintf(memory,sizeof(memory),"scan_access query_api_failures=%zu query_api_error=0x%lx snapshot_failures=%zu snapshot_error=0x%lx enumeration_failures=%zu enumeration_error=0x%lx open_thread_failures=%zu open_thread_error=0x%lx query_thread_failures=%zu query_ntstatus=0x%lx tib_read_failures=%zu tib_read_error=0x%lx bad_stack_ranges=%zu virtual_query_failures=%zu virtual_query_error=0x%lx guard_regions=%zu uncommitted_regions=%zu retry_recovered=%zu retry_failures=%zu read_error=0x%lx",scanStats.queryApiFailures,scanStats.queryApiError,scanStats.snapshotFailures,scanStats.snapshotError,scanStats.enumerationFailures,scanStats.enumerationError,scanStats.openThreadFailures,scanStats.openThreadError,scanStats.queryThreadFailures,static_cast<unsigned long>(scanStats.threadQueryStatus),scanStats.tibReadFailures,scanStats.tibReadError,scanStats.badStackRanges,scanStats.virtualQueryFailures,scanStats.virtualQueryError,scanStats.guardRegions,scanStats.uncommittedRegions,scanStats.retryRecovered,scanStats.retryFailures,scanStats.readError);lines.emplace_back(memory);
 auto camera=cameraDiagnostics.lines();lines.insert(lines.end(),camera.begin(),camera.end());return lines;
}
std::string GameGridReader::scanDiagnostic()const{
 char detail[480];std::snprintf(detail,sizeof(detail)," scans=%zu threads=%zu stacks=%zu bytes=%zu unreadable=%zu read_failures=%zu viewport_matches=%zu invalid=%zu orientation=%zu valid=%zu system_error=0x%lx",scanStats.scans,scanStats.threads,scanStats.stackRanges,scanStats.bytes,scanStats.unreadableRegions,scanStats.readFailures,scanStats.viewportMatches,scanStats.invalid,scanStats.orientation,scanStats.valid,static_cast<unsigned long>(scanStats.systemError));return detail;
}
bool GameGridReader::verifyBuild(){
 if(!checked){checked=true;supported=fingerprint();image=reinterpret_cast<uintptr_t>(GetModuleHandleW(nullptr));}
 return supported;
}
bool GameGridReader::readCurrentLevel(LevelIdentity& identity){
 verifyBuild();return readValidatedLevel(read,image,identity);
}
bool GameGridReader::begin(float w,float h,bool overrideHeight,float z){
 cancelPicking();
 error=ErrorId::None;diagnostic.clear();directionCategory=0;rejectedCameraOrientation=false;scanStats={};cameraDiagnostics={};levelValidated=false;planeSelected=false;++requestSerial;
 width=w;height=h;heightOverride=overrideHeight;forcedZ=z;started=GetTickCount64();
 verifyBuild();if(!readLevel())return false;levelValidated=true;
 std::vector<Candidate> live;for(auto c:candidates){auto frame=std::make_unique<CameraFrame>();if(!read(c.address,frame->f.data(),960)){cameraDiagnostics.record(CameraStage::Cache,{CameraValidation::Invalid,CameraReject::ReadFailure},nullptr,0,0,GetLastError());continue;}if(acceptsCamera(*frame,CameraStage::Cache))live.push_back({c.address,frame->frame(),frame->f[211]});}
 candidates=std::move(live);if(candidates.empty())scan();diagnostic="waiting_for_fresh_camera_frame";return true;
}
GameGridState GameGridReader::poll(Grid& grid){
 auto now=GetTickCount64();auto best=std::make_unique<CameraFrame>();CameraValidation bestKind=CameraValidation::Invalid;
 for(auto c:candidates){auto frame=std::make_unique<CameraFrame>();if(!read(c.address,frame->f.data(),960)){cameraDiagnostics.record(CameraStage::Poll,{CameraValidation::Invalid,CameraReject::ReadFailure},nullptr,0,0,GetLastError());continue;}if(!acceptsCamera(*frame,CameraStage::Poll))continue;
  uint32_t advance=frame->frame()-c.frame;float seconds=frame->f[211]-c.time;
  auto fresh=cameraFreshness(advance,seconds);cameraDiagnostics.record(CameraStage::Fresh,{fresh==CameraReject::None?CameraValidation::Valid:CameraValidation::Invalid,fresh},frame.get(),advance,seconds);
  if(fresh!=CameraReject::None)continue;auto kind=validateCamera(*frame,width,height);
  if(cameraCandidatePreferred(kind,frame->frame(),bestKind,best->frame())){*best=*frame;bestKind=kind;}
 }
 if(bestKind==CameraValidation::Invalid){
  if(now-started>1800){fail(rejectedCameraOrientation?ErrorId::CameraOrientationInvalid:(candidates.empty()?ErrorId::CameraNotFound:ErrorId::FreshCameraFrameUnavailable));return GameGridState::Unavailable;}
  if(now-lastScan>=250)scan();return GameGridState::Waiting;
 }
 planeZ=forcedZ;if(!heightOverride){double nearest=1e100;const auto&m=best->f;
  for(auto p:actors){double d=m[12]*p[0]+m[13]*p[1]+m[14]*p[2]+m[15];if(d<=.1)continue;
   double x=(m[0]*p[0]+m[1]*p[1]+m[2]*p[2]+m[3])/d,y=(m[4]*p[0]+m[5]*p[1]+m[6]*p[2]+m[7])/d;
   double distance=x*x+y*y;if(distance<nearest){nearest=distance;planeZ=p[2];}
  }
  if(nearest==1e100){fail(ErrorId::VisibleActorLayerUnavailable);return GameGridState::Unavailable;}
 }
 planeSelected=true;
 uintptr_t activeManager=0;if(!get(image+0xb69600,activeManager)||activeManager!=manager){fail(ErrorId::LevelChanged);return GameGridState::Unavailable;}
 grid=gridFromCamera(*best,width,height,planeZ,true);if(!grid.usable()){fail(ErrorId::ProjectedGridInvalid);return GameGridState::Unavailable;}
 directionCategory=cameraOrientation(*best).category;
 char message[400];std::snprintf(message,sizeof(message),"game_data layout=r67271 verified_build=%d frame=%u logical=%dx%d z=%.3f render=%.0fx%.0f output=%.0fx%.0f candidates=%zu manager=0x%llx direction_category=%u manual_direction=%d",supported?1:0,best->frame(),columns,rows,planeZ,1.f/best->f[216],1.f/best->f[217],width,height,candidates.size(),static_cast<unsigned long long>(manager),unsigned(directionCategory),directionCategory?1:0);diagnostic=message;return GameGridState::Ready;
}
float GameGridReader::suggestedPlane(const CameraFrame& frame,bool& actorLayer)const{
 actorLayer=false;float z=forcedZ;double nearest=1e100;const auto& m=frame.f;
 for(auto p:actors){double d=m[12]*p[0]+m[13]*p[1]+m[14]*p[2]+m[15];if(d<=.1)continue;
  double x=(m[0]*p[0]+m[1]*p[1]+m[2]*p[2]+m[3])/d,y=(m[4]*p[0]+m[5]*p[1]+m[6]*p[2]+m[7])/d;
  if(x*x+y*y<nearest){nearest=x*x+y*y;z=p[2];actorLayer=true;}
 }return z;
}
bool GameGridReader::pickerContext(float w,float h){
 if(w!=width||h!=height)return fail(ErrorId::CameraChoiceExpired);
 uintptr_t active=0;LevelIdentity current;
 if(!get(image+0xb69600,active)||active!=manager||!readValidatedLevel(read,image,current)||current!=pickerLevel)return fail(ErrorId::LevelChanged);
 return true;
}
bool GameGridReader::beginPicking(float w,float h,float fallbackZ){
 if(!std::isfinite(fallbackZ)||std::abs(fallbackZ)>10000)return fail(ErrorId::ProjectedGridInvalid);
 verifyBuild();LevelIdentity before;
 if(!readValidatedLevel(read,image,before)){if(!begin(w,h,true,fallbackZ))return false;return fail(ErrorId::LevelChanged);}
 // Height is explicitly editable in the picker; unavailable actors do not
 // prevent previewing a validated camera at the supplied fallback height.
 if(!begin(w,h,true,fallbackZ))return false;
 if(!readValidatedLevel(read,image,pickerLevel)||pickerLevel!=before||pickerLevel.columns!=columns||pickerLevel.rows!=rows)return fail(ErrorId::LevelChanged);
 picking=true;if(candidates.size()>PickMaxCandidates)candidates.resize(PickMaxCandidates);return true;
}
void GameGridReader::offerChoice(uintptr_t address,const CameraFrame& frame){
 auto it=std::find_if(choices.begin(),choices.end(),[&](const auto& item){return sameCameraView(item.option.camera,frame);});
 if(it==choices.end()){
  if(choices.size()>=PickMaxOptions)return;
  // Emplace directly on the heap: no camera-sized temporary on a scanned stack.
  choices.emplace_back();it=choices.end()-1;it->option.token=(requestSerial<<32)|choices.size();it->option.camera=frame;
  it->option.directionCategory=cameraOrientation(frame).category;it->option.suggestedZ=suggestedPlane(frame,it->option.actorLayer);
 }
 auto source=std::find_if(it->sources.begin(),it->sources.end(),[&](auto c){return c.address==address;});
 Candidate value{address,frame.frame(),frame.f[211]};
 if(source!=it->sources.end())*source=value;else if(it->sources.size()<PickMaxSources)it->sources.push_back(value);
 it->option.copies=it->sources.size();
 if(int32_t(frame.frame()-it->option.camera.frame())>0)it->option.camera=frame;
}
GameGridState GameGridReader::pollPicking(){
 if(!picking){fail(ErrorId::CameraChoiceExpired);return GameGridState::Unavailable;}
 if(!pickerContext(width,height))return GameGridState::Unavailable;
 auto now=GetTickCount64();
 for(auto c:candidates){auto frame=std::make_unique<CameraFrame>();
  if(!read(c.address,frame->f.data(),960)){cameraDiagnostics.record(CameraStage::Poll,{CameraValidation::Invalid,CameraReject::ReadFailure},nullptr,0,0,GetLastError());continue;}
  if(!acceptsCamera(*frame,CameraStage::Poll))continue;
  auto fresh=cameraFreshness(frame->frame()-c.frame,frame->f[211]-c.time);cameraDiagnostics.record(CameraStage::Fresh,{fresh==CameraReject::None?CameraValidation::Valid:CameraValidation::Invalid,fresh},frame.get(),frame->frame()-c.frame,frame->f[211]-c.time);
  if(fresh==CameraReject::None)offerChoice(c.address,*frame);
 }
 if(now-started>=PickCollectMs){
  if(!choices.empty())return GameGridState::Ready;
  fail(rejectedCameraOrientation?ErrorId::CameraOrientationInvalid:(candidates.empty()?ErrorId::CameraNotFound:ErrorId::FreshCameraFrameUnavailable));return GameGridState::Unavailable;
 }
 if(now-lastScan>=250)scan();return GameGridState::Waiting;
}
std::vector<CameraOption> GameGridReader::pickerOptions()const{
 std::vector<CameraOption> result;result.reserve(choices.size());for(const auto& c:choices)result.push_back(c.option);return result;
}
}
