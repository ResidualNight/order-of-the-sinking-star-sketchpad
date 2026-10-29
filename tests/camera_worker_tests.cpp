#include "game_grid_worker.hpp"
#include "camera_fixture.hpp"
#include <atomic>
#include <cassert>
#include <thread>
#include <iostream>
using namespace sketch;
namespace sketch {
struct GameGridWorkerTestAccess {
 static void fixture(GameGridWorker& worker,uintptr_t* managerSlot,CameraFrame* camera,bool verified=true){
  auto& reader=worker.levelReader;reader.checked=true;reader.supported=verified;reader.image=reinterpret_cast<uintptr_t>(managerSlot)-0xb69600;
  reader.candidates.clear();if(camera)reader.candidates={{reinterpret_cast<uintptr_t>(camera),camera->frame(),camera->f[211],camera->sourceType()}};
 }
 static bool idle(const GameGridWorker& worker){return worker.idle();}
 static std::weak_ptr<void> lifetime(const GameGridWorker& worker){return worker.service;}
};
}

namespace {
template<class Predicate>void until(Predicate predicate,uint64_t timeout=3500){
 auto start=GetTickCount64();while(!predicate()){assert(GetTickCount64()-start<timeout);Sleep(2);}
}
void idle(const GameGridWorker& worker){until([&]{return GameGridWorkerTestAccess::idle(worker);});}
void advance(CameraFrame& camera){uint32_t next=camera.frame()+1;std::memcpy(camera.f.data()+225,&next,4);camera.f[211]+=.016f;}
struct LevelFixture {
 struct Type{uint32_t tag=7,pad=0;int64_t size=1320,count=8;uintptr_t data=0;};
 struct Array{int64_t count=1;uintptr_t data=0;int64_t capacity=1;};
 struct Geometry{int64_t x=8,y=7,count=56;uint8_t wrapping=0,pad[3]{};int32_t corner[3]{};};
 struct Text{int64_t count;uintptr_t data;};
 std::array<unsigned char,0xe00> manager{};std::array<unsigned char,1320> settings{};
 uintptr_t managerSlot=reinterpret_cast<uintptr_t>(manager.data()),entity=reinterpret_cast<uintptr_t>(settings.data());
 const char typeName[9]="Settings",levelName[8]="fixture";Type type;
 LevelFixture(){
  type.data=reinterpret_cast<uintptr_t>(typeName);Array array;array.data=reinterpret_cast<uintptr_t>(&entity);Geometry geometry;Text name{7,reinterpret_cast<uintptr_t>(levelName)};
  auto put=[](auto& bytes,size_t offset,const auto& value){std::memcpy(bytes.data()+offset,&value,sizeof(value));};
  put(manager,0xdd0,array);put(settings,240,managerSlot);put(settings,264,reinterpret_cast<uintptr_t>(&type));
  id(7);put(settings,816,geometry);put(settings,1296,name);
 }
 void id(uint32_t value){std::memcpy(settings.data()+256,&value,4);}
};
}
void checkCameraWorker(){
 auto level=std::make_unique<LevelFixture>();GameGridWorker worker;
 // The complete main view exists on the producer's stack only between UI
 // polls. By the next simulated Present it is corrupt, matching the log's
 // projection_inverse failure. The real worker must capture it independently.
 HANDLE update=CreateEventW(nullptr,TRUE,FALSE,nullptr),exit=CreateEventW(nullptr,TRUE,FALSE,nullptr);assert(update&&exit);
 std::atomic<CameraFrame*> address{nullptr};std::atomic<bool> overwritten{false};
 std::thread producer([&]{
  CameraFrame source=gameCameraFixture();address.store(&source);
  assert(WaitForSingleObject(update,3500)==WAIT_OBJECT_0);advance(source);Sleep(60);
  source.f[176]+=1;assert(inspectCamera(source,3840,2160).reason==CameraReject::ProjectionInverse);overwritten.store(true);
  assert(WaitForSingleObject(exit,3500)==WAIT_OBJECT_0);
 });
 until([&]{return address.load()!=nullptr;});
 GameGridWorkerTestAccess::fixture(worker,&level->managerSlot,nullptr,false);
 assert(worker.begin(3840,2160,true,1));Sleep(60);SetEvent(update);
 until([&]{return overwritten.load();});
 Grid grid;assert(worker.poll(grid)==GameGridState::Ready&&grid.usable());
 assert(worker.diagnostic.find("sampling=background")!=std::string::npos&&worker.diagnostic.find("source_type=8 source=main")!=std::string::npos);
 assert(worker.buildWarning()==ErrorId::UnsupportedGameBuild);
 worker.cancel();idle(worker);SetEvent(exit);producer.join();CloseHandle(update);CloseHandle(exit);
 auto camera=std::make_unique<CameraFrame>(gameCameraFixture());
 auto begin=[&](bool picking=false){
  worker.cancel();idle(worker);*camera=gameCameraFixture();level->id(7);
  GameGridWorkerTestAccess::fixture(worker,&level->managerSlot,camera.get());
  assert(picking?worker.beginPicking(3840,2160,1):worker.begin(3840,2160,true,1));Sleep(30);
 };
 // Delayed delivery cannot extend the automatic 250ms capture lifetime.
 begin();advance(*camera);idle(worker);Sleep(260);
 Grid expired;assert(worker.poll(expired)==GameGridState::Preview&&worker.error==ErrorId::None&&!expired.usable());
 assert(!worker.pickerOptions().empty()&&cameraOptionGrid(worker.pickerOptions()[0],3840,2160,1,0).usable());
 // A completed result is still rejected when the stable level identity changes.
 begin();advance(*camera);idle(worker);level->id(8);
 assert(worker.poll(grid)==GameGridState::Unavailable&&worker.error==ErrorId::LevelChanged);
 // Manual collection keeps the validated fresh preview after source overwrite.
 begin(true);advance(*camera);Sleep(35);camera->f[178]=NAN;idle(worker);
 assert(worker.pollPicking()==GameGridState::Ready&&worker.pickerOptions().size()==1);
 assert(validCamera(worker.pickerOptions()[0].camera,3840,2160));
 // Rapid replacement is serialized by one service. Only the final request can
 // publish, and stopping a request does not wait on scans in the UI thread.
 worker.cancel();idle(worker);*camera=gameCameraFixture();
 GameGridWorkerTestAccess::fixture(worker,&level->managerSlot,camera.get());
 auto previous=worker.requestId();
 for(int i=0;i<30;++i)assert(worker.begin(3840,2160,true,1));
 assert(worker.beginPicking(3840,2160,1)&&worker.requestId()==previous+31);
 Sleep(35);advance(*camera);idle(worker);
 assert(worker.pollPicking()==GameGridState::Ready&&worker.pickerOptions().size()==1);
 assert(worker.pickerOptions()[0].token>>32==worker.requestId());
 worker.cancelPicking();assert(worker.pollPicking()==GameGridState::Unavailable&&worker.error==ErrorId::CameraChoiceExpired);
 idle(worker);
 // Reflections become visible recovery previews, with no automatic grid result.
 *camera=gameCameraFixture();uint32_t auxiliary=5;std::memcpy(camera->f.data()+226,&auxiliary,4);
 GameGridWorkerTestAccess::fixture(worker,&level->managerSlot,camera.get());assert(worker.begin(3840,2160,true,1));
 Sleep(35);advance(*camera);idle(worker);
 Grid unconfirmed;assert(worker.poll(unconfirmed)==GameGridState::Preview&&!unconfirmed.usable()&&worker.error==ErrorId::None);
 assert(worker.pickerOptions().size()==1&&worker.pickerOptions()[0].camera.sourceType()==5);
 assert(!worker.failureDetails().empty()&&worker.failureDetails().back().find("sampling=background")!=std::string::npos);
 // A failed redundant projection inverse reaches a fresh, usable preview,
 // never Ready. A later strict candidate still wins before the timeout.
 worker.cancel();idle(worker);*camera=gameCameraFixture();std::fill(camera->f.begin()+176,camera->f.begin()+192,0.f);
 GameGridWorkerTestAccess::fixture(worker,&level->managerSlot,camera.get());assert(worker.begin(3840,2160,true,1));
 Sleep(35);advance(*camera);idle(worker);
 assert(worker.poll(unconfirmed)==GameGridState::Preview&&!unconfirmed.usable());
 assert(worker.pickerOptions().size()==1&&worker.pickerOptions()[0].projectionUnverified);
 assert(cameraOptionGrid(worker.pickerOptions()[0],3840,2160,1,0).usable());
 worker.cancel();idle(worker);GameGridWorkerTestAccess::fixture(worker,&level->managerSlot,camera.get());assert(worker.begin(3840,2160,true,1));
 Sleep(35);advance(*camera);Sleep(25);
 // Restore only the unused inverse block, preserving the current fresh clock.
 auto inverse=std::make_unique<CameraFrame>(gameCameraFixture());std::copy(inverse->f.begin()+176,inverse->f.begin()+192,camera->f.begin()+176);advance(*camera);idle(worker);
 assert(worker.poll(grid)==GameGridState::Ready&&grid.usable());
 // Cancellation/destruction leave no request accessing fixture memory.
 worker.cancel();idle(worker);*camera=gameCameraFixture();
 std::weak_ptr<void> lifetime;
 {GameGridWorker temporary;GameGridWorkerTestAccess::fixture(temporary,&level->managerSlot,camera.get());lifetime=GameGridWorkerTestAccess::lifetime(temporary);assert(temporary.begin(3840,2160,true,1));}
 until([&]{return lifetime.expired();},1000);
 std::cout<<"PASS: background stack capture, delivery/level guards, request cancellation, auxiliary/projection recovery previews and strict candidate priority\n";
}

void checkSceneCameraWorker(uintptr_t image,CameraFrame* camera){
 GameGridWorker worker;auto slot=reinterpret_cast<uintptr_t*>(image+0xb69600);
 *camera={};GameGridWorkerTestAccess::fixture(worker,slot,camera);
 assert(worker.begin(3840,2160,false,1)); // No old source, frame clock, or actors.
 Grid grid;GameGridState state=GameGridState::Waiting;
 until([&]{state=worker.poll(grid);return state!=GameGridState::Waiting;});
 assert(state==GameGridState::Preview&&!worker.pickerOptions().empty()&&worker.pickerOptions()[0].scenePose);
 assert(worker.error==ErrorId::None&&!worker.pickerOptions()[0].sceneCorroborated);
 worker.cancel();idle(worker);GameGridWorkerTestAccess::fixture(worker,slot,camera);
 assert(worker.beginPicking(3840,2160,1));Sleep(60);advance(*camera);
 until([&]{state=worker.pollPicking();return state!=GameGridState::Waiting;});
 assert(state==GameGridState::Ready&&worker.pickerOptions()[0].scenePose);
 // New-source previews still cannot cross a changed initiating level at delivery.
 uint32_t changed=9,original=8;std::memcpy(reinterpret_cast<void*>(image+0xb01000+256),&changed,4);
 assert(worker.pollPicking()==GameGridState::Unavailable&&worker.error==ErrorId::LevelChanged);
 std::memcpy(reinterpret_cast<void*>(image+0xb01000+256),&original,4);worker.cancel();idle(worker);
 *camera=gameCameraFixture();GameGridWorkerTestAccess::fixture(worker,slot,camera);
 assert(worker.begin(3840,2160,true,1,false));Sleep(60);advance(*camera);
 until([&]{state=worker.poll(grid);return state!=GameGridState::Waiting;});
 assert(state==GameGridState::Ready&&grid.usable());for(const auto& option:worker.pickerOptions())assert(!option.scenePose);
 // Original preference is not a single-source failure trap.
 worker.cancel();idle(worker);*camera={};GameGridWorkerTestAccess::fixture(worker,slot,camera);
 assert(worker.begin(3840,2160,false,1,false));
 until([&]{state=worker.poll(grid);return state!=GameGridState::Waiting;});
 assert(state==GameGridState::Preview&&worker.error==ErrorId::None&&worker.pickerOptions()[0].scenePose);
 worker.cancel();idle(worker);
 // Neither reader yields a usable grid: only then publish a failure.
 auto orientation=reinterpret_cast<float*>(*slot+0x3db0);float saved=orientation[0];orientation[0]=NAN;
 GameGridWorkerTestAccess::fixture(worker,slot,camera);assert(worker.begin(3840,2160,false,1));
 assert(worker.poll(grid)==GameGridState::Waiting);
 until([&]{state=worker.poll(grid);return state!=GameGridState::Waiting;});
 assert(state==GameGridState::Unavailable&&worker.error!=ErrorId::None&&worker.pickerOptions().empty());
 worker.cancel();idle(worker);orientation[0]=saved;
}
