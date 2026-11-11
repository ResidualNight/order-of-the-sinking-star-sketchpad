#include "gamepad_bridge.hpp"
#include <atomic>
#include <thread>
#include <cstdio>
#include <cstdlib>
#include <cstring>
using namespace sketch::pad;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
using Set=void(WINAPI*)(DWORD,const XINPUT_STATE*,DWORD);
using Resolver=FARPROC(WINAPI*)(HMODULE,LPCSTR);
static Resolver originalResolver=nullptr;
static GetState fixtureGet=nullptr,recursiveTarget=nullptr;
static bool recursiveMode=false;
static std::atomic<bool> holdNext{false};
static HANDLE queryEntered=nullptr,queryRelease=nullptr;
static Resolver laterNext=nullptr;
static FARPROC WINAPI laterResolver(HMODULE h,LPCSTR name){return laterNext(h,name);}
static std::atomic<unsigned> mappedCalls{0},resolveCalls{0};
// Models a pre-existing mapping chain, not a real Steam compatibility test.
static DWORD WINAPI mapped(DWORD slot,XINPUT_STATE*s){++mappedCalls;if(recursiveMode)return recursiveTarget(slot,s);auto r=fixtureGet(slot,s);if(holdNext.exchange(false)){SetEvent(queryEntered);CHECK(WaitForSingleObject(queryRelease,5000)==WAIT_OBJECT_0);}if(!r&&s->Gamepad.wButtons==XINPUT_GAMEPAD_A)s->Gamepad.wButtons=XINPUT_GAMEPAD_B;return r;}
template<int N> static DWORD WINAPI variant(DWORD slot,XINPUT_STATE*s){auto r=mapped(slot,s);if(!r)s->dwPacketNumber+=N;return r;}
static GetState variants[]={variant<1>,variant<2>,variant<3>,variant<4>,variant<5>,variant<6>,variant<7>,variant<8>};
static unsigned mappingVariant=0;
static FARPROC WINAPI priorResolver(HMODULE h,LPCSTR name){
 ++resolveCalls;auto p=originalResolver(h,name);
 if(reinterpret_cast<uintptr_t>(name)>65535&&strcmp(name,"XInputGetState")==0&&p==reinterpret_cast<FARPROC>(fixtureGet))return reinterpret_cast<FARPROC>(mappingVariant?variants[mappingVariant-1]:&mapped);
 return p;
}
static void** resolverSlot(){
 auto base=reinterpret_cast<BYTE*>(GetModuleHandleW(nullptr));
 auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
 auto desc=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(base+nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress);
 for(;desc->Name;++desc){if(!desc->OriginalFirstThunk)continue;
  auto names=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+desc->OriginalFirstThunk),slots=reinterpret_cast<IMAGE_THUNK_DATA64*>(base+desc->FirstThunk);
  for(;names->u1.AddressOfData;++names,++slots)if(!IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal)){
   auto n=reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(base+names->u1.AddressOfData);
   if(strcmp(reinterpret_cast<char*>(n->Name),"GetProcAddress")==0)return reinterpret_cast<void**>(&slots->u1.Function);
  }
 }return nullptr;
}
static void* exchange(void**slot,void*value){DWORD p,u;CHECK(VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&p));auto old=InterlockedExchangePointer(slot,value);CHECK(VirtualProtect(slot,sizeof(void*),p,&u));return old;}
int main(int argc,char**argv){
 CHECK(argc==2||argc==3);bool capacity=argc==3&&strcmp(argv[2],"--capacity")==0;bool foreign=argc==3&&!capacity;auto module=LoadLibraryA(argv[1]);CHECK(module);
 fixtureGet=reinterpret_cast<GetState>(GetProcAddress(module,"XInputGetState"));CHECK(fixtureGet);
 auto set=reinterpret_cast<Set>(GetProcAddress(module,"FixtureState"));CHECK(set);
 auto slot=resolverSlot();CHECK(slot);originalResolver=reinterpret_cast<Resolver>(exchange(slot,reinterpret_cast<void*>(&priorResolver)));
 auto cachedBefore=reinterpret_cast<GetState>(GetProcAddress(module,"XInputGetState"));
 auto report=install(GetModuleHandleW(nullptr));CHECK(report.installed&&report.resolvers==1);
 auto game=reinterpret_cast<GetState>(GetProcAddress(module,"XInputGetState"));CHECK(game&&game!=cachedBefore);
 CHECK(GetProcAddress(module,"FixtureState")==reinterpret_cast<FARPROC>(set));
 CHECK(GetProcAddress(module,"MissingExport")==nullptr);
 // Ordinal lookup is deliberately untouched.
 CHECK(GetProcAddress(module,MAKEINTRESOURCEA(1))==originalResolver(module,MAKEINTRESOURCEA(1)));
 CHECK(endpointCount()>=1);
 XINPUT_STATE raw{};raw.dwPacketNumber=9;raw.Gamepad.wButtons=XINPUT_GAMEPAD_A;raw.Gamepad.bRightTrigger=255;
 for(DWORD i=0;i<4;++i)set(i,&raw,0);
 XINPUT_STATE result{};CHECK(game(2,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B);DWORD before=result.dwPacketNumber;
  // A disconnect before ever entering capture must remain transparent.
 set(3,&raw,ERROR_DEVICE_NOT_CONNECTED);CHECK(game(3,&result)==ERROR_DEVICE_NOT_CONNECTED);
 set(3,&raw,0);CHECK(game(3,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B);
 setCapturing(true);CHECK(game(2,&result)==0&&result.Gamepad.wButtons==0&&result.Gamepad.bRightTrigger==0&&result.dwPacketNumber!=before);
#ifdef PROBE_DIRECT_IMPORT
 CHECK(report.directStates==1);CHECK(XInputGetState(0,&result)==0&&result.Gamepad.wButtons==0);
#endif
 // Locate the mapped endpoint (a directly imported fixture can be endpoint 0).
 unsigned mappedEndpoint=0;
 for(unsigned i=0;i<endpointCount();++i){CHECK(sample(i,2,&result)==0);if(result.Gamepad.wButtons==XINPUT_GAMEPAD_B)mappedEndpoint=i;}
 CHECK(sample(mappedEndpoint,2,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B&&result.Gamepad.bRightTrigger==255);
 CHECK(cachedBefore(2,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B); // late install does not cover old cache!
 CHECK(game(9,&result)==ERROR_BAD_ARGUMENTS);CHECK(game(0,nullptr)==ERROR_BAD_ARGUMENTS);
 set(1,&raw,ERROR_DEVICE_NOT_CONNECTED);CHECK(game(1,&result)==ERROR_DEVICE_NOT_CONNECTED);
 setCapturing(false);CHECK(game(2,&result)==0&&result.Gamepad.wButtons==0&&result.Gamepad.bRightTrigger==0);
 XINPUT_STATE zero{};set(2,&zero,0);CHECK(sample(mappedEndpoint,2,&result)==0);set(2,&raw,0);
 CHECK(game(2,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B&&result.Gamepad.bRightTrigger==255);
 setCapturing(true);std::atomic<bool> ok{true};
 std::thread gameThread([&]{for(int i=0;i<10000;++i){XINPUT_STATE s{};if(game(0,&s)||s.Gamepad.wButtons||s.Gamepad.bRightTrigger)ok=false;}});
 std::thread rawThread([&]{for(int i=0;i<10000;++i){XINPUT_STATE s{};auto r=sample(mappedEndpoint,0,&s);if(r!=ERROR_RETRY&&(r||s.Gamepad.wButtons!=XINPUT_GAMEPAD_B))ok=false;}});
 gameThread.join();rawThread.join();CHECK(ok.load());CHECK(mappedCalls>20000&&resolveCalls>0&&stats().recursions==0);
 queryEntered=CreateEventW(nullptr,FALSE,FALSE,nullptr);queryRelease=CreateEventW(nullptr,FALSE,FALSE,nullptr);
 CHECK(queryEntered&&queryRelease);
 for(int sameEpoch=0;sameEpoch<2;++sameEpoch){
  setCapturing(true);
  if(sameEpoch){setCapturing(false);CHECK(game(0,&result)==0&&result.Gamepad.wButtons==0);}
  set(0,&zero,0);holdNext=true;DWORD delayedResult=0;
  std::thread delayed([&]{XINPUT_STATE s{};delayedResult=sample(mappedEndpoint,0,&s);});
  CHECK(WaitForSingleObject(queryEntered,5000)==WAIT_OBJECT_0);
  if(!sameEpoch)setCapturing(false);
  set(0,&raw,0);CHECK(game(0,&result)==0&&result.Gamepad.wButtons==0);
  SetEvent(queryRelease);delayed.join();
  CHECK(delayedResult==ERROR_RETRY);
  CHECK(game(0,&result)==0&&result.Gamepad.wButtons==0&&result.Gamepad.bRightTrigger==0);
  set(0,&zero,0);CHECK(sample(mappedEndpoint,0,&result)==0);
  set(0,&raw,0);CHECK(game(0,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B);
 }
 // An older game query must reuse the latest game-visible state, not fabricate a release.
 set(0,&zero,0);holdNext=true;XINPUT_STATE delayedGame{};DWORD delayedGameResult=0;
 std::thread delayedGameThread([&]{delayedGameResult=game(0,&delayedGame);});
 CHECK(WaitForSingleObject(queryEntered,5000)==WAIT_OBJECT_0);
 set(0,&raw,0);CHECK(game(0,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B);
 SetEvent(queryRelease);delayedGameThread.join();
 CHECK(delayedGameResult==0&&delayedGame.Gamepad.wButtons==XINPUT_GAMEPAD_B&&delayedGame.dwPacketNumber==result.dwPacketNumber);
 CloseHandle(queryEntered);CloseHandle(queryRelease);
 setCapturing(true);
 recursiveMode=true;recursiveTarget=game;CHECK(game(0,&result)==ERROR_GEN_FAILURE&&stats().recursions==1);recursiveMode=false;
 failOpen();CHECK(game(0,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B);
 set(3,&raw,ERROR_DEVICE_NOT_CONNECTED);CHECK(game(3,&result)==ERROR_DEVICE_NOT_CONNECTED);
 set(3,&raw,0);CHECK(game(3,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B&&result.dwPacketNumber==raw.dwPacketNumber);
 if(capacity){
  setCapturing(true);
  for(mappingVariant=1;mappingVariant<=8;++mappingVariant)CHECK(GetProcAddress(module,"XInputGetState"));
  mappingVariant=0;CHECK(stats().degraded&&!capturing()&&endpointCount()==8);setCapturing(true);CHECK(!capturing());
  CHECK(game(0,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B);
 }
 if(foreign){laterNext=reinterpret_cast<Resolver>(exchange(slot,reinterpret_cast<void*>(&laterResolver)));CHECK(!remove());CHECK(*slot==reinterpret_cast<void*>(&laterResolver));}
 else {CHECK(remove());CHECK(*slot==reinterpret_cast<void*>(&priorResolver));}
#ifdef PROBE_DIRECT_IMPORT
 CHECK(XInputGetState(0,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_A);
#endif
 CHECK(game(0,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B); // cached wrappers remain callable
 set(3,&raw,ERROR_DEVICE_NOT_CONNECTED);CHECK(game(3,&result)==ERROR_DEVICE_NOT_CONNECTED);
 set(3,&raw,0);CHECK(game(3,&result)==0&&result.Gamepad.wButtons==XINPUT_GAMEPAD_B&&result.dwPacketNumber==raw.dwPacketNumber);
 CHECK(GetProcAddress(module,"XInputGetState")==reinterpret_cast<FARPROC>(&mapped));
 exchange(slot,reinterpret_cast<void*>(originalResolver));
 std::puts("gamepad bridge: mapping chain, dynamic cache, 4 slots, fences, errors, concurrency, recursion and cleanup passed");
 FreeLibrary(module);
}
