#include "camera_scan.hpp"
#include "camera_grid.hpp"
#include "camera_fixture.hpp"
#include "quarter_camera_fixture.hpp"
#include "game_grid.hpp"
#include <cassert>
#include <iostream>
#include <set>
#include <memory>
using namespace sketch;
namespace sketch {
// Use owned fixture memory and cached addresses while retaining real guarded reads.
struct GameGridReaderTestAccess {
 static void fixture(GameGridReader& reader,uintptr_t* managerSlot,bool verified,CameraFrame* camera){
  reader.checked=true;reader.supported=verified;reader.image=reinterpret_cast<uintptr_t>(managerSlot)-0xb69600;
  reader.candidates={{reinterpret_cast<uintptr_t>(camera),camera->frame(),camera->f[211]}};
 }
 static void append(GameGridReader& reader,CameraFrame* camera){reader.candidates.push_back({reinterpret_cast<uintptr_t>(camera),camera->frame(),camera->f[211]});}
 static void sceneImage(GameGridReader& reader,uintptr_t image,CameraFrame* camera){reader.checked=true;reader.supported=true;reader.image=image;reader.candidates={{reinterpret_cast<uintptr_t>(camera),camera->frame(),camera->f[211]}};}
 static void preferOriginal(GameGridReader& reader){reader.preferScene=false;}
 static void finishCollection(GameGridReader& reader){reader.started=GetTickCount64()-GameGridReader::PickCollectMs;}
 static void timeout(GameGridReader& reader){reader.started=GetTickCount64()-1801;}
 static void expireCapture(GameGridReader& reader){reader.capturedAt=GetTickCount64()-GameGridReader::CaptureLifetimeMs-1;}
 static void scanFixture(GameGridReader& reader,CameraFrame* memory){
  ++reader.observationRound;reader.capturePrepared=true;
  CameraScanStats stats;scanCameraStack(reinterpret_cast<uintptr_t>(memory),reinterpret_cast<uintptr_t>(memory)+960,[&](uintptr_t address,const unsigned char* data){
   auto frame=std::make_unique<CameraFrame>();std::memcpy(frame->f.data(),data,960);
   if(reader.acceptsCamera(*frame,CameraStage::Scan))reader.observeCamera(address,*frame,CameraStage::Scan);
  },stats);assert(stats.bytes==960);
 }
 static void offer(GameGridReader& reader,uintptr_t address,const CameraFrame& frame){reader.offerChoice(address,frame);}
 static void actors(GameGridReader& reader,std::initializer_list<std::array<float,3>> positions){reader.actors=positions;}
 static size_t historySize(const GameGridReader& reader){return reader.observations.size();}
 static size_t movedCount(const GameGridReader& reader){return reader.movedFresh;}
 static void pressure(GameGridReader& reader,const CameraFrame& main){
  auto auxiliary=std::make_unique<CameraFrame>(main);uint32_t type=5;std::memcpy(auxiliary->f.data()+226,&type,4);
  for(size_t i=0;i<GameGridReader::MaxObservations+16;++i){++reader.observationRound;reader.observeCamera(0x300000+i,*auxiliary,CameraStage::Scan);}
  assert(reader.observations.size()==GameGridReader::MaxObservations);
  assert(std::any_of(reader.observations.begin(),reader.observations.end(),[](const auto& old){return old.frame->sourceType()==8;}));
  reader.candidates.clear();for(size_t i=0;i<GameGridReader::PickMaxCandidates;++i)reader.rememberCandidate({0x400000+i,1,1,5});
  reader.rememberCandidate({0x500000,1,1,8});
  assert(reader.candidates.size()==GameGridReader::PickMaxCandidates&&std::any_of(reader.candidates.begin(),reader.candidates.end(),[](auto c){return c.sourceType==8;}));
 }
};
}
void checkSceneCameraReaderFixture(uintptr_t image,CameraFrame* raw,const SceneCameraPose& pose){
 GameGridReader reader;GameGridReaderTestAccess::sceneImage(reader,image,raw);
 *raw={};assert(reader.begin(3840,2160,false,1)); // No actors or stack metadata.
 Grid grid;assert(reader.poll(grid)==GameGridState::Preview&&reader.error==ErrorId::None&&!grid.usable());
 auto independent=reader.pickerOptions();assert(independent.size()==1&&independent[0].scenePose&&!independent[0].sceneCorroborated);
 assert(independent[0].camera.frame()==0); // Never manufacture engine counters.
 *raw=gameCameraFixture();raw->f[0]=NAN;GameGridReaderTestAccess::sceneImage(reader,image,raw);assert(reader.begin(3840,2160,true,1));
 assert(reader.poll(grid)==GameGridState::Preview);
 uint32_t next=raw->frame()+1;std::memcpy(raw->f.data()+225,&next,4);raw->f[211]+=.016f;raw->f[235]=1;
 assert(reader.poll(grid)==GameGridState::Preview&&!reader.pickerOptions()[0].sceneCorroborated); // Map stack evidence cannot corroborate the independent manual pose.
 raw->f[235]=0;next=raw->frame()+1;std::memcpy(raw->f.data()+225,&next,4);raw->f[211]+=.016f;
 assert(reader.poll(grid)==GameGridState::Preview);auto options=reader.pickerOptions();assert(!options.empty()&&options[0].scenePose);
 auto projected=cameraOptionGrid(options[0],3840,2160,1,0);assert(projected.usable()&&length(projected.origin-pose.grid(3840,2160,1).origin)<.001f);
 // Like original stack choices, a freshly captured manual snapshot survives
 // a later transient source failure during bounded picker collection.
 *raw=gameCameraFixture();GameGridReaderTestAccess::sceneImage(reader,image,raw);
 assert(reader.beginPicking(3840,2160,1));raw->f[0]=NAN;
 GameGridReaderTestAccess::actors(reader,{{5,100,20}});
 next=raw->frame()+1;std::memcpy(raw->f.data()+225,&next,4);raw->f[211]+=.016f;
 assert(reader.pollPicking()==GameGridState::Waiting);
 options=reader.pickerOptions();assert(!options.empty()&&options[0].scenePose);
 assert(options[0].suggestedZ==1&&!options[0].actorLayer);
 auto orientation=reinterpret_cast<float*>(*reinterpret_cast<uintptr_t*>(image+0xb69600)+0x3db0);
 float saved=orientation[0];orientation[0]=NAN;
 GameGridReaderTestAccess::finishCollection(reader);
 assert(reader.pollPicking()==GameGridState::Ready);
 auto retained=reader.pickerOptions();assert(!retained.empty()&&retained[0].scenePose);
 assert(retained[0].pose.orientation==options[0].pose.orientation);
 orientation[0]=saved;reader.cancelPicking();assert(reader.pickerOptions().empty());
 // A failed new-source signature retains the original strict Ready path.
 auto code=reinterpret_cast<unsigned char*>(image)+0x1100;*code^=1;*raw=gameCameraFixture();
 GameGridReaderTestAccess::sceneImage(reader,image,raw);assert(reader.begin(3840,2160,true,1));
 next=raw->frame()+1;std::memcpy(raw->f.data()+225,&next,4);raw->f[211]+=.016f;
 assert(reader.poll(grid)==GameGridState::Ready&&grid.usable());for(const auto& option:reader.pickerOptions())assert(!option.scenePose);*code^=1;
 // Invalid scene source does not mask an old-source preview without actors.
 *code^=1;GameGridReaderTestAccess::sceneImage(reader,image,raw);assert(reader.begin(3840,2160,false,1));
 next=raw->frame()+1;std::memcpy(raw->f.data()+225,&next,4);raw->f[211]+=.016f;
 assert(reader.poll(grid)==GameGridState::Preview&&reader.error==ErrorId::None&&!reader.pickerOptions()[0].scenePose);
 // Both sources unavailable: wait for the request budget before reporting.
 *raw={};GameGridReaderTestAccess::sceneImage(reader,image,raw);assert(reader.begin(3840,2160,false,1));
 assert(reader.poll(grid)==GameGridState::Waiting);GameGridReaderTestAccess::timeout(reader);
 assert(reader.poll(grid)==GameGridState::Unavailable&&reader.error!=ErrorId::None&&reader.pickerOptions().empty());*code^=1;
 // Explicit original preference still keeps the independent source as fallback.
 GameGridReaderTestAccess::sceneImage(reader,image,raw);GameGridReaderTestAccess::preferOriginal(reader);
 assert(reader.begin(3840,2160,false,1)&&reader.poll(grid)==GameGridState::Waiting);
 GameGridReaderTestAccess::timeout(reader);assert(reader.poll(grid)==GameGridState::Preview&&reader.error==ErrorId::None&&reader.pickerOptions()[0].scenePose);
}
void checkAdvisoryBuild(){
 struct Type{uint32_t tag=7,pad=0;int64_t size=1320,count=8;uintptr_t data=0;};
 struct Array{int64_t count=1;uintptr_t data=0;int64_t capacity=1;};
 struct Geometry{int64_t x=8,y=7,count=56;uint8_t wrapping=0,pad[3]{};int32_t corner[3]{};};
 struct Text{int64_t count;uintptr_t data;};
 std::array<unsigned char,0xe00> manager{};std::array<unsigned char,1320> settings{};
 uintptr_t managerSlot=reinterpret_cast<uintptr_t>(manager.data()),entity=reinterpret_cast<uintptr_t>(settings.data());
 const char typeName[]="Settings",levelName[]="fixture";Type type;type.data=reinterpret_cast<uintptr_t>(typeName);
 Array array;array.data=reinterpret_cast<uintptr_t>(&entity);Geometry geometry;Text name{7,reinterpret_cast<uintptr_t>(levelName)};
 auto put=[](auto& bytes,size_t offset,const auto& value){std::memcpy(bytes.data()+offset,&value,sizeof(value));};
 put(manager,0xdd0,array);put(settings,240,managerSlot);put(settings,264,reinterpret_cast<uintptr_t>(&type));
 put(settings,256,uint32_t(7));put(settings,816,geometry);put(settings,1296,name);
 for(bool verified:{false,true}){
  auto camera=std::make_unique<CameraFrame>(gameCameraFixture());GameGridReader reader;
  GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());
  LevelIdentity level;assert(reader.readCurrentLevel(level)&&level.name==levelName);
  assert(reader.buildWarning()==(verified?ErrorId::None:ErrorId::UnsupportedGameBuild));
  assert(reader.begin(3840,2160,true,1)&&reader.error==ErrorId::None);
  uint32_t next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,sizeof(next));camera->f[211]+=.016f;
  Grid grid;assert(reader.poll(grid)==GameGridState::Ready&&grid.usable()&&reader.error==ErrorId::None);
  assert(reader.buildWarning()==(verified?ErrorId::None:ErrorId::UnsupportedGameBuild));
  // Warnings cannot bypass invalid layouts, invalid cameras or freshness.
  put(settings,240,uintptr_t(0));assert(!reader.readCurrentLevel(level));
  assert(!reader.begin(3840,2160,true,1)&&reader.error==ErrorId::SettingsGridInvalid);
  put(settings,240,managerSlot);
  assert(reader.begin(3840,2160,true,1));Grid stale;
  assert(reader.poll(stale)==GameGridState::Waiting&&!stale.usable());
  camera->f[0]=NAN;assert(reader.poll(stale)==GameGridState::Waiting&&!stale.usable());
  // Direction-only failures enter the real reader's fresh-frame path. A
  // user-confirmable preview still cannot bypass stale or corrupt data.
  *camera=gameCameraFixture();
  for(int base:{0,192})for(int col=0;col<4;++col)camera->f[base+4+col]*=-1;
  for(int base:{16,176})for(int row=0;row<4;++row)camera->f[base+row*4+1]*=-1;
  GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());
  assert(reader.begin(3840,2160,true,1));
  assert(reader.poll(stale)==GameGridState::Waiting&&!stale.usable());
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,sizeof(next));camera->f[211]+=.016f;
  Grid preview;assert(reader.poll(preview)==GameGridState::Ready&&preview.usable()&&reader.directionCategory==6);
  auto corrected=preview.reflected(3840,2160,false,true);
  auto expected=gridFromCamera(gameCameraFixture(),3840,2160,1);
  assert(distance(corrected.at(2,3),expected.at(2,3))<.01f);
  *camera=gameCameraFixture();auto duplicate=std::make_unique<CameraFrame>(*camera);
  GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());GameGridReaderTestAccess::append(reader,duplicate.get());
  assert(reader.beginPicking(3840,2160,1));GameGridReaderTestAccess::finishCollection(reader);
  assert(reader.pollPicking()==GameGridState::Unavailable&&reader.pickerOptions().empty()); // Static copies cannot become options.
  assert(reader.beginPicking(3840,2160,1));
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;*duplicate=*camera;
  GameGridReaderTestAccess::finishCollection(reader);assert(reader.pollPicking()==GameGridState::Ready);
  auto options=reader.pickerOptions();assert(options.size()==1&&options[0].copies==2&&options[0].token);
  put(settings,256,uint32_t(8));assert(reader.pollPicking()==GameGridState::Unavailable&&reader.error==ErrorId::LevelChanged);put(settings,256,uint32_t(7));
  *camera=options[0].camera;*duplicate=*camera;assert(reader.beginPicking(3840,2160,1));
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;*duplicate=*camera;
  GameGridReaderTestAccess::finishCollection(reader);assert(reader.pollPicking()==GameGridState::Ready&&reader.pickerOptions()[0].token!=options[0].token);
  reader.cancelPicking();assert(reader.pickerOptions().empty());
  // Log-shaped scenario: a native-size copy is corrupt while a quarter-size
  // category-3 view remains consistent. It must reach the same fresh-frame
  // path in automatic and picker requests, without accepting the corrupt copy.
  for(Point output:std::vector<Point>{{3840,2160},{3200,1800},{2560,1440},{1920,1080},{1920,1200},{3440,1440},{5120,1440},{1365,767}}){
  *camera=quarterCameraFixture(output.x,output.y);*duplicate=gameCameraFixture();duplicate->f[178]=NAN;
  duplicate->f[216]=1/output.x;duplicate->f[217]=1/output.y;
  GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());GameGridReaderTestAccess::append(reader,duplicate.get());
  assert(reader.begin(output.x,output.y,true,1));
  assert(reader.poll(stale)==GameGridState::Waiting);
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
  assert(reader.poll(preview)==GameGridState::Ready&&preview.usable()&&reader.directionCategory==3);
  if(output.x==3840&&output.y==2160)assert(reader.diagnostic.find("render=960x540 output=3840x2160")!=std::string::npos);
  assert(reader.beginPicking(output.x,output.y,1));
  GameGridReaderTestAccess::finishCollection(reader);
  assert(reader.pollPicking()==GameGridState::Unavailable&&reader.pickerOptions().empty());
  assert(reader.beginPicking(output.x,output.y,1));
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
  GameGridReaderTestAccess::finishCollection(reader);
  assert(reader.pollPicking()==GameGridState::Ready);
  options=reader.pickerOptions();assert(options.size()==1&&options[0].directionCategory==3);
  assert(cameraOptionGrid(options[0],output.x,output.y,1,0).usable());
  reader.cancelPicking();
  }
  // Internal 16:9 on 16:10 / classic / ultrawide outputs is preview-only.
  // Stale frames, corrupt primary matrices and map views remain rejected.
  for(Point output:std::vector<Point>{{2560,1600},{1920,1200},{1600,1200},{1280,1024},{2560,1080},{3440,1440},{5120,1440}}){
   *camera=gameCameraFixture();camera->f[216]=1.f/2560;camera->f[217]=1.f/1440;
   GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());
   assert(reader.begin(output.x,output.y,true,1));GameGridReaderTestAccess::timeout(reader);
   assert(reader.poll(preview)==GameGridState::Unavailable&&reader.pickerOptions().empty());
   assert(reader.begin(output.x,output.y,true,1));
   next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
   GameGridReaderTestAccess::scanFixture(reader,camera.get());GameGridReaderTestAccess::timeout(reader);
   assert(reader.poll(preview)==GameGridState::Preview&&reader.error==ErrorId::None);
   options=reader.pickerOptions();assert(options.size()==1&&options[0].aspectUnverified&&!options[0].projectionUnverified);
   assert(cameraOptionGrid(options[0],output.x,output.y,1,0).usable());
   assert(validateCamera(*camera,output.x,output.y)==CameraValidation::Invalid);
   for(bool map:{false,true}){
    if(map)camera->f[235]=1;else camera->f[0]=NAN;
    assert(reader.begin(output.x,output.y,true,1));
    next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
    GameGridReaderTestAccess::timeout(reader);assert(reader.poll(preview)==GameGridState::Unavailable&&reader.pickerOptions().empty());
    camera->f[0]=gameCameraFixture().f[0];
   }
  }
  // A complete fresh copy captured before overlay work survives later source
  // overwrite. It is request-local, bounded in age, and still checks the level.
  *camera=gameCameraFixture();GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());
  assert(reader.begin(3840,2160,true,1));
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
  reader.captureBeforeOverlay(3840,2160);camera->f[178]=NAN;
  assert(reader.poll(preview)==GameGridState::Ready&&preview.usable());
  assert(reader.diagnostic.find("source_type=8 source=main capture=poll")!=std::string::npos);
  *camera=gameCameraFixture();GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());assert(reader.begin(3840,2160,true,1));
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
  reader.captureBeforeOverlay(3840,2160);GameGridReaderTestAccess::expireCapture(reader);
  assert(reader.poll(preview)==GameGridState::Waiting);
  assert(reader.poll(preview)==GameGridState::Waiting); // A static copy cannot renew an expired capture.
  reader.captureBeforeOverlay(3840,2160);put(settings,256,uint32_t(8));
  assert(reader.poll(preview)==GameGridState::Unavailable&&reader.error==ErrorId::LevelChanged);put(settings,256,uint32_t(7));
  // Reacquire an equal, validated projection at a new temporary address and
  // different internal extent. Initial/static copies cannot produce freshness.
  *camera=gameCameraFixture();*duplicate=*camera;GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());
  assert(reader.begin(3840,2160,true,1));camera->f[178]=NAN;
  GameGridReaderTestAccess::scanFixture(reader,duplicate.get());assert(reader.poll(preview)==GameGridState::Waiting);
  next=duplicate->frame()+1;std::memcpy(duplicate->f.data()+225,&next,4);duplicate->f[211]+=.016f;
  duplicate->f[216]*=2;duplicate->f[217]*=2;
  GameGridReaderTestAccess::scanFixture(reader,duplicate.get());duplicate->f[178]=NAN;
  assert(reader.poll(preview)==GameGridState::Ready&&reader.diagnostic.find("capture=scan")!=std::string::npos);
  *camera=gameCameraFixture();*duplicate=*camera;GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());assert(reader.begin(3840,2160,true,1));
  camera->f[178]=NAN;next=duplicate->frame()+1;std::memcpy(duplicate->f.data()+225,&next,4);duplicate->f[211]+=.016f;
  GameGridReaderTestAccess::scanFixture(reader,duplicate.get());duplicate->f[178]=NAN;
  assert(reader.poll(preview)==GameGridState::Ready&&GameGridReaderTestAccess::movedCount(reader)==1);
  *camera=gameCameraFixture();*duplicate=*camera;
  next=duplicate->frame()+1;std::memcpy(duplicate->f.data()+225,&next,4);duplicate->f[211]+=.016f;
  GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());GameGridReaderTestAccess::append(reader,duplicate.get());assert(reader.begin(3840,2160,true,1));
  assert(reader.poll(preview)==GameGridState::Waiting); // Both copies predate this request's first new frame.
  auto oldCopy=std::make_unique<CameraFrame>(*duplicate);GameGridReaderTestAccess::scanFixture(reader,oldCopy.get());
  assert(reader.poll(preview)==GameGridState::Waiting); // A moved old duplicate is still not a new frame.
  // The captured picker preview also survives an overwritten source, with no
  // adoption re-read path. Known auxiliary views remain explicitly manual.
  *camera=gameCameraFixture();uint32_t auxiliary=5;std::memcpy(camera->f.data()+226,&auxiliary,4);
  GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());assert(reader.begin(3840,2160,true,1));
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
  GameGridReaderTestAccess::timeout(reader);assert(reader.poll(preview)==GameGridState::Preview&&reader.error==ErrorId::None);
  assert(reader.beginPicking(3840,2160,1));
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
  reader.captureBeforeOverlay(3840,2160);camera->f[178]=NAN;GameGridReaderTestAccess::finishCollection(reader);
  assert(reader.pollPicking()==GameGridState::Ready&&reader.pickerOptions().size()==1);
  assert(reader.pickerOptions()[0].camera.sourceType()==5&&cameraOptionGrid(reader.pickerOptions()[0],3840,2160,1,0).usable());
  // An automatic recovery preview must honor an explicitly chosen height.
  *camera=gameCameraFixture();std::memcpy(camera->f.data()+226,&auxiliary,4);
  GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());assert(reader.begin(3840,2160,true,3));
  GameGridReaderTestAccess::actors(reader,{{5,5,1}});
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
  GameGridReaderTestAccess::timeout(reader);assert(reader.poll(preview)==GameGridState::Preview);
  options=reader.pickerOptions();assert(options.size()==1&&options[0].suggestedZ==3&&!options[0].actorLayer);
  // The suggested actor can be in front of a reflected camera while its
  // horizontal plane is behind the forward ray. Keep the usable supplied
  // picker height instead of dropping the entire fresh camera.
  assert(reader.beginPicking(3840,2160,1));GameGridReaderTestAccess::actors(reader,{{5,100,20}});
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
  GameGridReaderTestAccess::finishCollection(reader);assert(reader.pollPicking()==GameGridState::Ready);
  options=reader.pickerOptions();assert(options.size()==1&&options[0].suggestedZ==1&&!options[0].actorLayer);
  // A validated auxiliary camera with no usable geometry still reports 013;
  // recovery must not enable an invalid grid merely to suppress the error.
  assert(reader.begin(3840,2160,true,20));
  next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
  GameGridReaderTestAccess::timeout(reader);assert(reader.poll(preview)==GameGridState::Unavailable&&reader.error==ErrorId::AuxiliaryCameraOnly);
  assert(reader.pickerOptions().empty());
  {auto detail=reader.failureDetails();assert(std::any_of(detail.begin(),detail.end(),[](const auto& line){return line.find("geometry_rejected=1")!=std::string::npos;}));}
  // A strict main camera can be numerically sound yet unusable at this plane.
  // Its failure must not hide another source's visible manual grid.
  *duplicate=quarterCameraFixture();std::memcpy(duplicate->f.data()+226,&auxiliary,4);
  *camera=gameCameraFixture();GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());GameGridReaderTestAccess::append(reader,duplicate.get());
  assert(gridFromCamera(*duplicate,3840,2160,20,true).usable());assert(reader.begin(3840,2160,true,20));
  for(auto item:{camera.get(),duplicate.get()}){next=item->frame()+1;std::memcpy(item->f.data()+225,&next,4);item->f[211]+=.016f;}
  assert(reader.poll(preview)==GameGridState::Preview&&reader.error==ErrorId::None&&!reader.pickerOptions().empty());
  for(Point internal:std::vector<Point>{{480,270},{640,360},{320,180}}){
   *camera=quarterCameraFixture();std::memcpy(camera->f.data()+226,&auxiliary,4);camera->f[216]=1/internal.x;camera->f[217]=1/internal.y;
   GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());assert(reader.begin(3840,2160,true,1));
   next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
   GameGridReaderTestAccess::timeout(reader);assert(reader.poll(preview)==GameGridState::Preview&&reader.error==ErrorId::None);
   assert(reader.beginPicking(3840,2160,1));next=camera->frame()+1;std::memcpy(camera->f.data()+225,&next,4);camera->f[211]+=.016f;
   GameGridReaderTestAccess::finishCollection(reader);assert(reader.pollPicking()==GameGridState::Ready&&reader.pickerOptions().size()==1);
  }
  // A list full of auxiliary views cannot crowd out a later normal main view.
  reader.cancelPicking();auto option=std::make_unique<CameraFrame>(gameCameraFixture());
  for(size_t i=0;i<GameGridReader::PickMaxOptions;++i){uint32_t source=5;std::memcpy(option->f.data()+226,&source,4);option->f[208]=float(i);GameGridReaderTestAccess::offer(reader,0x100000+i,*option);}
  *option=gameCameraFixture();GameGridReaderTestAccess::offer(reader,0x200000,*option);
  options=reader.pickerOptions();assert(options.size()==GameGridReader::PickMaxOptions&&options[0].camera.sourceType()==8);
  std::set<uint64_t> tokens;for(const auto&o:options)assert(tokens.insert(o.token).second);
  // A new request cannot reuse a fresh snapshot or its cross-address history.
  *camera=gameCameraFixture();GameGridReaderTestAccess::fixture(reader,&managerSlot,verified,camera.get());assert(reader.begin(3840,2160,true,1));
  assert(GameGridReaderTestAccess::historySize(reader)==1&&reader.poll(preview)==GameGridState::Waiting);
  GameGridReaderTestAccess::pressure(reader,*camera);
 }
}
void checkCameraWorker();
void checkSceneCamera(const char* filename);
int main(int argc,char** argv){
 checkCameraWorker();
 checkAdvisoryBuild();
 // A large sparse stack with a guard, a hole, a chunk boundary and adjacent
 // readable regions with different protections. No game process is involved.
 constexpr size_t Reserve=16*1024*1024,Page=4096;
 auto memory=static_cast<unsigned char*>(VirtualAlloc(nullptr,Reserve,MEM_RESERVE,PAGE_NOACCESS));assert(memory);
 assert(VirtualAlloc(memory,36*Page,MEM_COMMIT,PAGE_READWRITE)==memory);
 assert(VirtualAlloc(memory+12*1024*1024,2*Page,MEM_COMMIT,PAGE_READWRITE));
 auto camera=gameCameraFixture();
 std::set<uintptr_t> expected;
 for(size_t offset:std::array<size_t,4>{Page+64,Page+64*1024-480,32*Page-480,12*1024*1024+64}){
  std::memcpy(memory+offset,camera.f.data(),960);expected.insert(reinterpret_cast<uintptr_t>(memory+offset));
 }
 // A viewport-looking block still needs all the actual camera checks.
 auto broken=camera;broken.f[0]=NAN;std::memcpy(memory+3*Page,broken.f.data(),960);
 auto quarter=camera;quarter.f[216]*=4;quarter.f[217]*=4;
 std::memcpy(memory+4*Page+64,quarter.f.data(),960);expected.insert(reinterpret_cast<uintptr_t>(memory+4*Page+64));
 auto directionOnly=quarterCameraFixture();std::memcpy(memory+6*Page+64,directionOnly.f.data(),960);
 std::set<uintptr_t> directionFound;
 DWORD old=0;assert(VirtualProtect(memory,Page,PAGE_READWRITE|PAGE_GUARD,&old));
 assert(VirtualProtect(memory+32*Page,Page,PAGE_READONLY,&old));
 // The old whole-stack read fails as soon as a single page is inaccessible.
 // Use the reserved hole here so the assertion itself never touches the guard.
 std::vector<unsigned char> oldBuffer(Reserve-Page);SIZE_T done=0;
 assert(!ReadProcessMemory(GetCurrentProcess(),memory+Page,oldBuffer.data(),oldBuffer.size(),&done));
 CameraScanStats stats;std::set<uintptr_t> found;size_t visits=0;
 auto visit=[&](uintptr_t address,const unsigned char* bytes){
  ++visits;CameraFrame frame;std::memcpy(frame.f.data(),bytes,960);
  if(validCamera(frame,3840,2160))assert(found.insert(address).second);
  if(validateCamera(frame,3840,2160)==CameraValidation::OrientationInvalid)assert(directionFound.insert(address).second);
 };
 scanCameraStack(reinterpret_cast<uintptr_t>(memory),reinterpret_cast<uintptr_t>(memory)+Reserve,visit,stats);
 assert(found==expected&&visits>0&&stats.unreadableRegions>=2&&stats.readFailures==0);
 assert(directionFound==std::set<uintptr_t>{reinterpret_cast<uintptr_t>(memory+6*Page+64)});
 assert(stats.bytes==37*Page);
 MEMORY_BASIC_INFORMATION guard{};assert(VirtualQuery(memory,&guard,sizeof(guard)));
 assert(guard.Protect&PAGE_GUARD); // Scanning must not grow or disturb the game stack.
 assert(stats.guardRegions==1&&stats.uncommittedRegions>0&&stats.virtualQueryFailures==0);
 assert(VirtualFree(memory,0,MEM_RELEASE));
 // An unverified EXE proceeds to guarded layout reads and reports the concrete
 // layout failure independently of its compatibility warning.
 GameGridReader reader;assert(reader.failureDetails().empty());
 assert(!reader.begin(3200,1800,false,1)&&reader.error==ErrorId::EntityManagerUnavailable&&reader.requestId()==1);
 assert(reader.buildWarning()==ErrorId::UnsupportedGameBuild);
 auto details=reader.failureDetails();assert(details.size()>=10&&details[0].find("output=3200x1800")!=std::string::npos);
 assert(details[0].find("verified_build=0")!=std::string::npos&&details[1].find("scans=0")!=std::string::npos);
 for(const auto& line:details)assert(line.size()<1400&&line.find('\n')==std::string::npos);
 assert(!reader.begin(1920,1080,true,1.75f)&&reader.requestId()==2);
 assert(reader.failureDetails()[0].find("height_override=1 requested_z=1.75")!=std::string::npos);
 std::cout<<"PASS: sparse stack over 8 MiB, guard preserved, unreadable pages skipped, chunk/region-spanning cameras and invalid frame rejection\n";
 checkSceneCamera(argc>1?argv[1]:nullptr);
}
