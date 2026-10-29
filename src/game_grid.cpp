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
 // Missing actors prevent automatic height selection, not either reader's
 // manual preview at the supplied height. Decide only after trying both.
 return true;
}
void GameGridReader::scan(){
 auto previous=std::move(candidates);
 candidates.clear();lastScan=GetTickCount64();++scanStats.scans;cameraDiagnostics.beginScan();
 struct ThreadBasic{LONG status;void*teb;ULONG_PTR clientProcess,clientThread,affinity;LONG priority,basePriority;};
 using QueryThread=LONG(NTAPI*)(HANDLE,ULONG,void*,ULONG,ULONG*);auto query=reinterpret_cast<QueryThread>(GetProcAddress(GetModuleHandleW(L"ntdll.dll"),"NtQueryInformationThread"));if(!query){++scanStats.queryApiFailures;scanStats.systemError=scanStats.queryApiError=GetLastError();candidates=std::move(previous);return;}
 HANDLE snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD,0);if(snapshot==INVALID_HANDLE_VALUE){++scanStats.snapshotFailures;scanStats.systemError=scanStats.snapshotError=GetLastError();candidates=std::move(previous);return;}
 THREADENTRY32 thread{};thread.dwSize=sizeof(thread);
 if(Thread32First(snapshot,&thread)){do{if(thread.th32OwnerProcessID!=GetCurrentProcessId()||(!scanOwnThread&&thread.th32ThreadID==GetCurrentThreadId()))continue;
  ++scanStats.threads;HANDLE handle=OpenThread(THREAD_QUERY_INFORMATION,FALSE,thread.th32ThreadID);if(!handle){++scanStats.openThreadFailures;scanStats.systemError=scanStats.openThreadError=GetLastError();continue;}ThreadBasic info{};LONG status=query(handle,0,&info,sizeof(info),nullptr);CloseHandle(handle);if(status<0){++scanStats.queryThreadFailures;scanStats.threadQueryStatus=status;scanStats.systemError=DWORD(status);continue;}
  uintptr_t tib[3]{};if(!read(reinterpret_cast<uintptr_t>(info.teb),tib,sizeof(tib))){++scanStats.tibReadFailures;scanStats.systemError=scanStats.tibReadError=GetLastError();continue;}uintptr_t low=tib[2],high=tib[1];if(high<=low||low<0x10000){++scanStats.badStackRanges;continue;}
  ++scanStats.stackRanges;
  scanCameraStack(low,high,[&](uintptr_t address,const unsigned char* data){float viewport[2];std::memcpy(viewport,data+864,8);uint32_t sourceType=0;std::memcpy(&sourceType,data+904,4);
   bool matches=cameraViewportMatches(viewport[0],viewport[1],width,height,sourceType==5);
   bool previewMatches=cameraViewportPreviewMatches(viewport[0],viewport[1],width,height,sourceType==5);
   if(!previewMatches&&!CameraDiagnostics::plausibleViewport(viewport[0],viewport[1]))return;
   // Candidate storage lives on the heap so scanning cannot discover our own local copies.
   auto frame=std::make_unique<CameraFrame>();std::memcpy(frame->f.data(),data,960);
   if(matches)observeScene(address,*frame);
   if(!matches)cameraDiagnostics.probeViewport(*frame);
   if(!previewMatches)return;
   ++scanStats.viewportMatches;
   auto check=inspectCamera(*frame,width,height);cameraDiagnostics.record(CameraStage::Scan,check,frame.get());
   if(check.result==CameraValidation::Valid){++scanStats.valid;rememberCandidate({address,frame->frame(),frame->f[211],frame->sourceType()});observeCamera(address,*frame,CameraStage::Scan);}
   else if(check.result==CameraValidation::OrientationInvalid){++scanStats.orientation;rejectedCameraOrientation=true;rememberCandidate({address,frame->frame(),frame->f[211],frame->sourceType()});observeCamera(address,*frame,CameraStage::Scan);}
   else {++scanStats.invalid;
    auto preview=inspectCamera(*frame,width,height,true,true);
    if(preview.result!=CameraValidation::Invalid){
     if(matrixInverseError(frame->f.data()+176,frame->f.data()+192)>=.025f)++projectionPreviewChecks;
     if(!matches)++aspectPreviewChecks;
     rememberCandidate({address,frame->frame(),frame->f[211],frame->sourceType()});observeCamera(address,*frame,CameraStage::Scan);
    }
   }
  },scanStats);
 }while(Thread32Next(snapshot,&thread));}
 DWORD enumerationError=GetLastError();if(enumerationError!=ERROR_NO_MORE_FILES){++scanStats.enumerationFailures;scanStats.enumerationError=enumerationError;}CloseHandle(snapshot);
 for(auto old:previous)if(std::none_of(candidates.begin(),candidates.end(),[&](auto now){return now.address==old.address;}))rememberCandidate(old);
}
void GameGridReader::observeScene(uintptr_t address,const CameraFrame& frame){
 if(!scenePoseValid||!sceneCameraEvidence(scenePose,frame,width,height))return;
 ++sceneMatches;
 if(std::find(sceneSources.begin(),sceneSources.end(),address)==sceneSources.end()&&sceneSources.size()<PickMaxCandidates)sceneSources.push_back(address);
 // Optional corroboration only. The scene pose is read independently; never
 // invent frame counters or treat a stable live pose as a fresh stack frame.
 if(scenePriorFrame&&cameraFreshness(frame.frame()-scenePriorFrame,frame.f[211]-scenePriorTime)==CameraReject::None){
  ++sceneFresh;if(sceneChoice&&sceneChoice->pose.position==scenePose.position&&sceneChoice->pose.orientation==scenePose.orientation&&sceneChoice->pose.projection==scenePose.projection)sceneChoice->sceneCorroborated=true;
 }
 if(!sceneLatestFrame||int32_t(frame.frame()-sceneLatestFrame)>0){sceneLatestFrame=frame.frame();sceneLatestTime=frame.f[211];}
}
void GameGridReader::offerScene(){
 if(!scenePoseValid)return;
 bool actorLayer=false;float z=forcedZ;
 if(heightOverride){z=forcedZ;actorLayer=false;}
 // Choose height using the new projection, not a potentially corrupt matrix.
 if(!heightOverride||picking){double nearest=1e100;std::array<float,16> matrix{};std::array<float,3> dir{};
  if(scenePose.project(width,height,matrix,dir))for(auto actor:actors){const auto& m=matrix;
   double d=m[12]*actor[0]+m[13]*actor[1]+m[14]*actor[2]+m[15];if(d<=.1)continue;
   double x=(m[0]*actor[0]+m[1]*actor[1]+m[2]*actor[2]+m[3])/d,y=(m[4]*actor[0]+m[5]*actor[1]+m[6]*actor[2]+m[7])/d;
   if(x*x+y*y<nearest){nearest=x*x+y*y;z=actor[2];actorLayer=true;}
  }
 }
 if(!scenePose.grid(width,height,z).usable()&&actorLayer&&z!=forcedZ&&scenePose.grid(width,height,forcedZ).usable()){
  z=forcedZ;actorLayer=false;++previewHeightFallbacks;
 }
 if(scenePose.grid(width,height,z).usable()){
  if(!sceneChoice)sceneChoice=std::make_unique<CameraOption>();
  sceneChoice->token=(requestSerial<<32)|0xffffffffu;sceneChoice->scenePose=true;sceneChoice->pose=scenePose;
  sceneChoice->sceneCorroborated=false;
  sceneChoice->suggestedZ=z;sceneChoice->actorLayer=actorLayer;sceneChoice->directionCategory=scenePose.category(width,height);sceneChoice->copies=1;
 }else ++previewGeometryRejected;
}
void GameGridReader::rememberCandidate(Candidate candidate){
 auto existing=std::find_if(candidates.begin(),candidates.end(),[&](auto c){return c.address==candidate.address;});
 if(existing!=candidates.end()){*existing=candidate;return;}
 if(candidates.size()<PickMaxCandidates){candidates.push_back(candidate);return;}
 auto worst=std::min_element(candidates.begin(),candidates.end(),[](auto a,auto b){return cameraSourceRank(a.sourceType)<cameraSourceRank(b.sourceType);});
 if(cameraSourceRank(candidate.sourceType)>cameraSourceRank(worst->sourceType))*worst=candidate;
}
void GameGridReader::observeCamera(uintptr_t address,const CameraFrame& frame,CameraStage stage){
 // Compare only with observations made earlier in this request. Two old copies
 // discovered in the same scan are not evidence of a newly rendered frame.
 auto exact=std::find_if(observations.begin(),observations.end(),[&](const auto& old){return old.address==address;});
 // Freeze the preceding round while this round updates its addresses. Compare
 // against the newest matching baseline, so an old duplicate cannot masquerade
 // as new merely because another address held an even older copy.
 auto prior=[&](const Observation& old){return old.round<observationRound?old.frame.get():old.previous.get();};
 const CameraFrame* baseline=nullptr;uintptr_t baselineAddress=0;
 if(exact!=observations.end()){auto old=prior(*exact);if(old&&old->sourceType()==frame.sourceType()){baseline=old;baselineAddress=address;}}
 for(const auto& old:observations){auto view=prior(old);
  if(view&&sameCameraView(*view,frame,false)&&(!baseline||int32_t(view->frame()-baseline->frame())>0)){baseline=view;baselineAddress=old.address;}
 }
 if(baseline){
  uint32_t advance=frame.frame()-baseline->frame();float seconds=frame.f[211]-baseline->f[211];
  auto fresh=cameraFreshness(advance,seconds);cameraDiagnostics.record(CameraStage::Fresh,{fresh==CameraReject::None?CameraValidation::Valid:CameraValidation::Invalid,fresh},&frame,advance,seconds);
  if(fresh==CameraReject::None){
   if(stage==CameraStage::Scan)++scanFresh;else ++pollFresh;
   if(baselineAddress!=address)++movedFresh;
   if(auxiliaryCamera(frame.sourceType())){auxiliarySeen=true;++auxiliaryFresh;}
   offerChoice(address,frame);
   if(!picking&&!auxiliaryCamera(frame.sourceType())&&validateCamera(frame,width,height)!=CameraValidation::Invalid){
    auto kind=validateCamera(frame,width,height);
    if(!capturedCamera||cameraViewPreferred(frame,kind,*capturedCamera,capturedKind)){
     if(!capturedCamera)capturedCamera=std::make_unique<CameraFrame>();
     *capturedCamera=frame;capturedKind=kind;capturedAt=GetTickCount64();capturedStage=stage;
    }
   }
  }
 }
 // Keep baselines on the heap; never leave full camera copies on scanned stacks.
 if(exact==observations.end()){
  Observation item;item.address=address;item.round=observationRound;item.frame=std::make_unique<CameraFrame>(frame);
  if(observations.size()<MaxObservations)observations.push_back(std::move(item));
  else {
   int lowest=2;for(const auto& old:observations)lowest=std::min(lowest,cameraSourceRank(old.frame->sourceType()));
   if(cameraSourceRank(frame.sourceType())>=lowest){
    for(size_t n=0;n<MaxObservations;++n){size_t slot=observationCursor++%MaxObservations;
     if(cameraSourceRank(observations[slot].frame->sourceType())==lowest){observations[slot]=std::move(item);break;}}
   }
  }
 }else {
  if(exact->round<observationRound){if(!exact->previous)exact->previous=std::make_unique<CameraFrame>();*exact->previous=*exact->frame;}
  *exact->frame=frame;exact->round=observationRound;
 }
}
void GameGridReader::captureCameras(bool early){
 capturedCamera.reset();capturedKind=CameraValidation::Invalid;capturePrepared=true;++observationRound;
 scenePriorFrame=sceneLatestFrame;scenePriorTime=sceneLatestTime;
 scenePoseValid=sceneReader.available()&&sceneReader.sample(read,manager,scenePose);++sceneReads;
 offerScene();
 // A captured manual snapshot has already passed initial freshness checks.
 // Keep it through transient reads, like stack choices; level/manager checks
 // still gate delivery. New requests/cancellation clear both sources.
 auto sceneCached=sceneSources;for(auto address:sceneCached){auto frame=std::make_unique<CameraFrame>();
  if(read(address,frame->f.data(),960))observeScene(address,*frame);}
 if(early)++earlyCaptures;
 for(auto c:candidates){auto frame=std::make_unique<CameraFrame>();
  if(!read(c.address,frame->f.data(),960)){cameraDiagnostics.record(CameraStage::Poll,{CameraValidation::Invalid,CameraReject::ReadFailure},nullptr,0,0,GetLastError());continue;}
  if(acceptsCamera(*frame,CameraStage::Poll))observeCamera(c.address,*frame,CameraStage::Poll);
 }
 // Re-scan even when only a reflection is fresh. Retain validated scan-time
 // copies immediately, before later overlay calls can overwrite their sources.
 if(GetTickCount64()-lastScan>=scanIntervalMs)scan();
}
void GameGridReader::captureBeforeOverlay(float w,float h){
 if(w==width&&h==height&&levelValidated)captureCameras(true);
}
bool GameGridReader::localizationContext(){
 uintptr_t active=0;LevelIdentity current;
 if(!get(image+0xb69600,active)||active!=manager||!readValidatedLevel(read,image,current)||current!=pickerLevel)return fail(ErrorId::LevelChanged);
 return true;
}
std::vector<std::string> GameGridReader::failureDetails()const{
 if(error==ErrorId::None&&!sceneChoice&&choices.empty())return {};
 std::vector<std::string> lines;char context[480];
 std::snprintf(context,sizeof(context),"context output=%.0fx%.0f elapsed_ms=%llu verified_build=%d level_validated=%d logical=%dx%d actors=%zu height_override=%d requested_z=%.6g selected_z=%.6g remaining_candidates=%zu",width,height,static_cast<unsigned long long>(GetTickCount64()-started),supported?1:0,levelValidated?1:0,levelValidated?columns:0,levelValidated?rows:0,levelValidated?actors.size():0,heightOverride?1:0,forcedZ,planeSelected?planeZ:NAN,candidates.size());lines.emplace_back(context);
 lines.push_back("scan"+scanDiagnostic());
 lines.push_back("capture rounds="+std::to_string(observationRound)+" early="+std::to_string(earlyCaptures)+" history="+std::to_string(observations.size())+" scan_fresh="+std::to_string(scanFresh)+" poll_fresh="+std::to_string(pollFresh)+" moved_fresh="+std::to_string(movedFresh)+" auxiliary_fresh="+std::to_string(auxiliaryFresh));
 lines.push_back("recovery projection_preview_checks="+std::to_string(projectionPreviewChecks)+" aspect_preview_checks="+std::to_string(aspectPreviewChecks)+" choices="+std::to_string(choices.size())+" geometry_rejected="+std::to_string(previewGeometryRejected)+" height_fallbacks="+std::to_string(previewHeightFallbacks)+" preview_only=1 projection_error<=1 composition_error<0.025");
 lines.push_back(std::string("scene_camera profile=retail-r13-2026-10-17b status=")+sceneReader.status+" reads="+std::to_string(sceneReads)+" matches="+std::to_string(sceneMatches)+" fresh="+std::to_string(sceneFresh)+" preview="+std::to_string(sceneChoice?1:0)+" corroborated="+std::to_string(sceneChoice&&sceneChoice->sceneCorroborated?1:0)+" metadata_sources="+std::to_string(sceneSources.size())+" stack_required=0 manual_confirmation=1 prefer_scene="+std::to_string(preferScene?1:0));
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
 observations.clear();capturedCamera.reset();capturedKind=CameraValidation::Invalid;capturePrepared=auxiliarySeen=false;
 observationRound=capturedAt=0;observationCursor=choiceSerial=earlyCaptures=scanFresh=pollFresh=movedFresh=auxiliaryFresh=0;
 projectionPreviewChecks=aspectPreviewChecks=previewGeometryRejected=previewHeightFallbacks=0;
 sceneChoice.reset();sceneSources.clear();sceneLatestFrame=scenePriorFrame=0;sceneLatestTime=scenePriorTime=0;sceneReads=sceneMatches=sceneFresh=0;
 error=ErrorId::None;diagnostic.clear();directionCategory=0;rejectedCameraOrientation=false;scanStats={};cameraDiagnostics={};levelValidated=false;planeSelected=false;++requestSerial;
 width=w;height=h;heightOverride=overrideHeight;forcedZ=z;started=GetTickCount64();lastScan=started;
 verifyBuild();if(!readLevel())return false;
 if(!readValidatedLevel(read,image,pickerLevel))return fail(ErrorId::LevelChanged);levelValidated=true;
 sceneReader=SceneCameraReader{};sceneReader.discover(read,image);
 scenePoseValid=sceneReader.available()&&sceneReader.sample(read,manager,scenePose);
 std::vector<Candidate> live;for(auto c:candidates){auto frame=std::make_unique<CameraFrame>();if(!read(c.address,frame->f.data(),960)){cameraDiagnostics.record(CameraStage::Cache,{CameraValidation::Invalid,CameraReject::ReadFailure},nullptr,0,0,GetLastError());continue;}observeScene(c.address,*frame);if(acceptsCamera(*frame,CameraStage::Cache))live.push_back({c.address,frame->frame(),frame->f[211],frame->sourceType()});}
 candidates=std::move(live);
 for(auto c:candidates){auto frame=std::make_unique<CameraFrame>();if(read(c.address,frame->f.data(),960)&&acceptsCamera(*frame,CameraStage::Cache))observeCamera(c.address,*frame,CameraStage::Cache);}
 // Initial scan cannot produce a fresh result: all baselines share round zero.
 if(candidates.empty()||scenePoseValid)scan();diagnostic="waiting_for_fresh_camera_frame";return true;
}
GameGridState GameGridReader::poll(Grid& grid){
 if(!localizationContext())return GameGridState::Unavailable;
 if(!capturePrepared)captureCameras(false);capturePrepared=false;
 if(preferScene&&sceneChoice)return previewResult();
 auto now=GetTickCount64();auto best=std::move(capturedCamera);auto bestKind=capturedKind;
 if(!best||now-capturedAt>CaptureLifetimeMs)bestKind=CameraValidation::Invalid;
 if(bestKind==CameraValidation::Invalid){
  if(now-started>1800){
   if(sceneChoice||!choices.empty())return previewResult();
   fail(auxiliarySeen?ErrorId::AuxiliaryCameraOnly:rejectedCameraOrientation?ErrorId::CameraOrientationInvalid:(candidates.empty()?ErrorId::CameraNotFound:ErrorId::FreshCameraFrameUnavailable));return GameGridState::Unavailable;
  }
  return GameGridState::Waiting;
 }
 planeZ=forcedZ;if(!heightOverride){double nearest=1e100;const auto&m=best->f;
  for(auto p:actors){double d=m[12]*p[0]+m[13]*p[1]+m[14]*p[2]+m[15];if(d<=.1)continue;
   double x=(m[0]*p[0]+m[1]*p[1]+m[2]*p[2]+m[3])/d,y=(m[4]*p[0]+m[5]*p[1]+m[6]*p[2]+m[7])/d;
   double distance=x*x+y*y;if(distance<nearest){nearest=distance;planeZ=p[2];}
  }
  if(nearest==1e100)return recoverOrWait(actors.empty()?ErrorId::ActorLayerUnavailable:ErrorId::VisibleActorLayerUnavailable);
 }
 planeSelected=true;
 uintptr_t activeManager=0;if(!get(image+0xb69600,activeManager)||activeManager!=manager){fail(ErrorId::LevelChanged);return GameGridState::Unavailable;}
 grid=gridFromCamera(*best,width,height,planeZ,true);if(!grid.usable())return recoverOrWait(ErrorId::ProjectedGridInvalid);
 directionCategory=cameraOrientation(*best).category;
 char message[560];std::snprintf(message,sizeof(message),"game_data layout=r67271 verified_build=%d frame=%u logical=%dx%d z=%.3f render=%.0fx%.0f output=%.0fx%.0f candidates=%zu manager=0x%llx direction_category=%u manual_direction=%d source_type=%u source=%s capture=%s capture_age_ms=%llu",supported?1:0,best->frame(),columns,rows,planeZ,1.f/best->f[216],1.f/best->f[217],width,height,candidates.size(),static_cast<unsigned long long>(manager),unsigned(directionCategory),directionCategory?1:0,best->sourceType(),cameraSourceName(best->sourceType()),CameraStageNames[size_t(capturedStage)],static_cast<unsigned long long>(now-capturedAt));diagnostic=message;return GameGridState::Ready;
}
GameGridState GameGridReader::previewResult(){
 error=ErrorId::None;diagnostic="camera_preview manual_confirmation=1 scene="+std::to_string(sceneChoice?1:0)+" stack_choices="+std::to_string(choices.size());
 return GameGridState::Preview;
}
GameGridState GameGridReader::recoverOrWait(ErrorId reason){
 if(sceneChoice||!choices.empty())return previewResult();
 if(GetTickCount64()-started<=1800)return GameGridState::Waiting;
 fail(reason);return GameGridState::Unavailable;
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
 if(inspectCamera(frame,width,height,true,true).result==CameraValidation::Invalid)return;
 bool projectionPreview=matrixInverseError(frame.f.data()+176,frame.f.data()+192)>=.025f;
 bool aspectPreview=!cameraViewportMatches(frame.f[216],frame.f[217],width,height,frame.sourceType()==5);
 bool actorLayer=false;float z=heightOverride&&!picking?forcedZ:suggestedPlane(frame,actorLayer);
 if(!gridFromCamera(frame,width,height,z,true,projectionPreview,aspectPreview).usable()){
  if(actorLayer&&z!=forcedZ&&gridFromCamera(frame,width,height,forcedZ,true,projectionPreview,aspectPreview).usable()){
   z=forcedZ;actorLayer=false;++previewHeightFallbacks;
  }else {++previewGeometryRejected;return;}
 }
 auto it=std::find_if(choices.begin(),choices.end(),[&](const auto& item){return sameCameraView(item.option.camera,frame,false);});
 if(it==choices.end()){
  if(choices.size()>=PickMaxOptions){
   auto priority=[](const CameraFrame& c,bool projectionPreview){return cameraSourceRank(c.sourceType())*4+(projectionPreview?0:2)+(cameraOrientation(c).category==0?1:0);};
   auto worst=std::min_element(choices.begin(),choices.end(),[&](const auto& a,const auto& b){return priority(a.option.camera,a.option.projectionUnverified)<priority(b.option.camera,b.option.projectionUnverified);});
   if(priority(frame,projectionPreview)<=priority(worst->option.camera,worst->option.projectionUnverified))return;
   choices.erase(worst);
  }
  // Emplace directly on the heap: no camera-sized temporary on a scanned stack.
  choices.emplace_back();it=choices.end()-1;it->option.token=(requestSerial<<32)|++choiceSerial;it->option.camera=frame;
  it->option.directionCategory=cameraOrientation(frame).category;it->option.suggestedZ=z;it->option.actorLayer=actorLayer;
  it->option.projectionUnverified=projectionPreview;it->option.aspectUnverified=aspectPreview;
 }
 auto source=std::find_if(it->sources.begin(),it->sources.end(),[&](auto c){return c.address==address;});
 Candidate value{address,frame.frame(),frame.f[211],frame.sourceType()};
 if(source!=it->sources.end())*source=value;else if(it->sources.size()<PickMaxSources)it->sources.push_back(value);
 it->option.copies=it->sources.size();
 if((it->option.projectionUnverified&&!projectionPreview)||
    (it->option.projectionUnverified==projectionPreview&&int32_t(frame.frame()-it->option.camera.frame())>0)){
  it->option.camera=frame;it->option.projectionUnverified=projectionPreview;it->option.aspectUnverified=aspectPreview;it->option.suggestedZ=z;it->option.actorLayer=actorLayer;
 }
}
GameGridState GameGridReader::pollPicking(){
 if(!picking){fail(ErrorId::CameraChoiceExpired);return GameGridState::Unavailable;}
 if(!pickerContext(width,height))return GameGridState::Unavailable;
 if(!capturePrepared)captureCameras(false);capturePrepared=false;
 auto now=GetTickCount64();
 if(now-started>=PickCollectMs){
  if(sceneChoice||!choices.empty())return GameGridState::Ready;
  fail(rejectedCameraOrientation?ErrorId::CameraOrientationInvalid:(candidates.empty()?ErrorId::CameraNotFound:ErrorId::FreshCameraFrameUnavailable));return GameGridState::Unavailable;
 }
 return GameGridState::Waiting;
}
std::vector<CameraOption> GameGridReader::pickerOptions()const{
 std::vector<CameraOption> result;result.reserve(choices.size());for(const auto& c:choices)result.push_back(c.option);
 std::stable_sort(result.begin(),result.end(),[](const auto& a,const auto& b){
  auto ar=cameraSourceRank(a.camera.sourceType()),br=cameraSourceRank(b.camera.sourceType());
  if(ar!=br)return ar>br;
  if(a.projectionUnverified!=b.projectionUnverified)return !a.projectionUnverified;
  return (a.directionCategory==0)>(b.directionCategory==0);
 });if(sceneChoice){if(result.size()>=PickMaxOptions)result.pop_back();result.insert(result.begin(),*sceneChoice);}return result;
}
}
