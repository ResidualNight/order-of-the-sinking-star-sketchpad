#include "camera_scan.hpp"
#include "camera_grid.hpp"
#include "camera_fixture.hpp"
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
 static void finishCollection(GameGridReader& reader){reader.started=GetTickCount64()-GameGridReader::PickCollectMs;}
};
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
 }
}
int main(){
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
 };
 scanCameraStack(reinterpret_cast<uintptr_t>(memory),reinterpret_cast<uintptr_t>(memory)+Reserve,visit,stats);
 assert(found==expected&&visits>0&&stats.unreadableRegions>=2&&stats.readFailures==0);
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
}
