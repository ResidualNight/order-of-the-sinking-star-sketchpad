#include "gamepad_bridge.hpp"
#include "gamepad_capture.hpp"
#include "gamepad_cache.hpp"
#include "gamepad_entry.hpp"
#include <atomic>
#include <cstring>
#include <cwchar>
#include <array>

namespace sketch::pad {
namespace {
constexpr unsigned MaxEndpoints=8,MaxPatches=16;
using Resolver=FARPROC(WINAPI*)(HMODULE,LPCSTR);
struct Endpoint {
 GetState next=nullptr;
 SRWLOCK lock=SRWLOCK_INIT;
 CaptureFilter filter;
 std::array<TriggerEntry,4> entry{};uint64_t entryGeneration=0;
 uint64_t epoch=0;
 std::array<uint64_t,4> issued{},completed{};
};
struct Patch {void**slot=nullptr;void*old=nullptr;void*replacement=nullptr;};
struct Bridge {
 SRWLOCK registry=SRWLOCK_INIT;
 SRWLOCK entryControl=SRWLOCK_INIT;
 std::array<Endpoint,MaxEndpoints> endpoints{};
 std::array<Patch,MaxPatches> patches{};
 unsigned count=0,patchCount=0;
 Resolver resolver=nullptr;
 std::atomic<bool> enabled{false},capture{false},degraded{false},bypass{true};
 std::atomic<bool> entryEnabled{false};std::atomic<uint64_t> entryGeneration{0};std::atomic<unsigned> open{0};
 std::atomic<uint64_t> gameCalls{0},rawCalls{0},recursions{0},neutralized{0};
};
// Process lifetime: cached game pointers must never point to a freed trampoline.
Bridge& get(){static auto* b=new Bridge;return *b;}
thread_local bool inDownstream=false;
bool xinputName(const wchar_t* name) {
 for(auto allowed:{L"xinput1_4.dll",L"xinput1_3.dll",L"xinput9_1_0.dll",L"xinput1_2.dll",L"xinput1_1.dll"})
  if(_wcsicmp(name,allowed)==0)return true;
 return false;
}
bool xinputModule(HMODULE module) {
 wchar_t path[32768];DWORD n=GetModuleFileNameW(module,path,32768);
 if(!n||n>=32768)return false;
 auto leaf=wcsrchr(path,L'\\');return xinputName(leaf?leaf+1:path);
}
DWORD downstream(GetState fn,DWORD slot,XINPUT_STATE*state) {
 if(!fn)return ERROR_DEVICE_NOT_CONNECTED;
 if(inDownstream){get().recursions.fetch_add(1);return ERROR_GEN_FAILURE;}
 inDownstream=true;DWORD result=fn(slot,state);inDownstream=false;return result;
}
struct Query {uint64_t epoch,sequence;};
Query beginQuery(Endpoint&e,DWORD slot) {
 AcquireSRWLockExclusive(&e.lock);
 Query q{e.epoch,slot<4?++e.issued[slot]:0};
 ReleaseSRWLockExclusive(&e.lock);return q;
}
// Called under the endpoint lock. A later completion supersedes an older
// observation, including disconnects; a capture transition invalidates both.
bool acceptQuery(Endpoint&e,DWORD slot,Query q) {
 if(slot>=4||q.epoch!=e.epoch||q.sequence<=e.completed[slot])return false;
 e.completed[slot]=q.sequence;return true;
}
template<unsigned N> DWORD WINAPI gameState(DWORD slot,XINPUT_STATE*state) {
 auto&b=get();auto&e=b.endpoints[N];b.gameCalls.fetch_add(1);
 // next is immutable after publishing the wrapper. Call the preserved chain
 // outside all our locks, so Steam and other consumers can run independently.
 auto query=beginQuery(e,slot);
 DWORD result=downstream(e.next,slot,state);
 if(!state)return result;
 AcquireSRWLockExclusive(&e.lock);
 bool entryEnabled=b.entryEnabled.load();bool pending=b.open.load()!=0;
 if(!b.bypass.load()||entryEnabled||pending) {
  XINPUT_GAMEPAD v{};if(result==ERROR_SUCCESS)v=state->Gamepad;bool active=result==ERROR_SUCCESS&&(v.wButtons||v.bLeftTrigger||v.bRightTrigger||v.sThumbLX||v.sThumbLY||v.sThumbRX||v.sThumbRY);
  bool fresh=acceptQuery(e,slot,query);
  auto generation=b.entryGeneration.load();if(e.entryGeneration!=generation){for(auto&item:e.entry)item.reset();e.entryGeneration=generation;}
  if(slot<4&&fresh&&result==ERROR_DEVICE_NOT_CONNECTED)e.entry[slot].reset();
  if(slot<4&&fresh&&result==ERROR_SUCCESS&&entryEnabled&&!b.capture.load()&&!pending){
   if(e.entry[slot].filter(state->Gamepad.bLeftTrigger,state->Gamepad.bRightTrigger,GetTickCount64())){
    AcquireSRWLockExclusive(&b.entryControl);
    if(b.entryEnabled.load()&&b.entryGeneration.load()==generation&&!b.capture.load()){unsigned expected=0;b.open.compare_exchange_strong(expected,N*4+slot+1);}
    ReleaseSRWLockExclusive(&b.entryControl);
   }
  }
  bool capture=b.capture.load()||b.open.load()!=0;
  if(capture!=e.filter.isCapturing()){++e.epoch;e.filter.setCapturing(capture);}
  bool transformed=result==ERROR_SUCCESS&&(v.bLeftTrigger!=state->Gamepad.bLeftTrigger||v.bRightTrigger!=state->Gamepad.bRightTrigger);
  result=e.filter.filter(slot,result,*state,fresh,query.epoch==e.epoch,transformed);
  if(active&&e.filter.isCapturing())b.neutralized.fetch_add(1);
 }
 ReleaseSRWLockExclusive(&e.lock);return result;
}
constexpr GetState wrappers[]={gameState<0>,gameState<1>,gameState<2>,gameState<3>,gameState<4>,gameState<5>,gameState<6>,gameState<7>};
GetState bind(GetState next) {
 for(auto wrapper:wrappers)if(next==wrapper)return nullptr;
 HMODULE owner=nullptr;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,reinterpret_cast<LPCWSTR>(next),&owner))return nullptr;
 auto&b=get();AcquireSRWLockExclusive(&b.registry);
 for(unsigned i=0;i<b.count;++i)if(b.endpoints[i].next==next){ReleaseSRWLockExclusive(&b.registry);return wrappers[i];}
 if(b.count==MaxEndpoints){ReleaseSRWLockExclusive(&b.registry);return nullptr;}
 auto i=b.count++;b.endpoints[i].next=next;b.endpoints[i].filter.setCapturing(b.capture.load());
 ReleaseSRWLockExclusive(&b.registry);return wrappers[i];
}
FARPROC WINAPI resolve(HMODULE module,LPCSTR name) {
 auto&b=get();auto next=b.resolver;FARPROC result=next(module,name);
 // Low pointer values denote ordinals. We deliberately leave these unchanged.
 if(!b.enabled.load()||!result||reinterpret_cast<uintptr_t>(name)<=0xffff||strcmp(name,"XInputGetState")!=0||!xinputModule(module))return result;
 auto wrapped=bind(reinterpret_cast<GetState>(result));
 if(!wrapped){b.degraded=true;failOpen();}
 return wrapped?reinterpret_cast<FARPROC>(wrapped):result;
}
bool patch(void** slot,void* expected,void* replacement) {
 auto&b=get();
 if(b.patchCount==MaxPatches)return false;
 DWORD protection;
 if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&protection))return false;
 auto old=InterlockedCompareExchangePointer(slot,replacement,expected);
 DWORD unused;bool restored=VirtualProtect(slot,sizeof(void*),protection,&unused)!=0;
 if(old!=expected)return false; // Preserve a hook installed after discovery.
 b.patches[b.patchCount++]={slot,old,replacement};
 // Even a protection-restore failure must retain the patch for cleanup.
 return restored;
}
struct Image {
 uint8_t*base;size_t size;
 template<class T>T* at(size_t rva,size_t count=1)const{
  if(rva>size||count>(size-rva)/sizeof(T))return nullptr;
  return reinterpret_cast<T*>(base+rva);
 }
 const char* string(size_t rva)const{
  auto p=at<char>(rva);return p&&memchr(p,0,size-rva)?p:nullptr;
 }
};
}
HookReport install(HMODULE consumer) {
 HookReport report;auto&b=get();
 if(!consumer||b.patchCount)return report; // caller installs once, outside loader lock
 auto base=reinterpret_cast<uint8_t*>(consumer);
 auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(base);
 if(dos->e_magic!=IMAGE_DOS_SIGNATURE||dos->e_lfanew<0||dos->e_lfanew>1048576)return report;
 auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(base+dos->e_lfanew);
 if(nt->Signature!=IMAGE_NT_SIGNATURE||nt->OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC)return report;
 Image image{base,nt->OptionalHeader.SizeOfImage};
 auto directory=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
 if(!directory.VirtualAddress||directory.Size<sizeof(IMAGE_IMPORT_DESCRIPTOR))return report;
 auto descriptors=image.at<IMAGE_IMPORT_DESCRIPTOR>(directory.VirtualAddress,directory.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR));
 if(!descriptors)return report;
 // Cached callbacks require the code module to remain resident.
 HMODULE owner=nullptr;
 if(!GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS|GET_MODULE_HANDLE_EX_FLAG_PIN,
    reinterpret_cast<LPCWSTR>(&install),&owner))return report;
 b.enabled=true;
 bool ok=true;
 for(unsigned d=0;d<directory.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR)&&descriptors[d].Name;++d) {
  auto&desc=descriptors[d];auto dll=image.string(desc.Name);
  if(!dll||!desc.OriginalFirstThunk)continue;
  for(size_t j=0;;++j) {
   auto name=image.at<IMAGE_THUNK_DATA64>(size_t(desc.OriginalFirstThunk)+j*sizeof(IMAGE_THUNK_DATA64));
   auto address=image.at<IMAGE_THUNK_DATA64>(size_t(desc.FirstThunk)+j*sizeof(IMAGE_THUNK_DATA64));
   if(!name||!address){ok=false;break;}
   if(!name->u1.AddressOfData)break;
   if(IMAGE_SNAP_BY_ORDINAL64(name->u1.Ordinal))continue;
   auto symbol=image.string(size_t(name->u1.AddressOfData)+offsetof(IMAGE_IMPORT_BY_NAME,Name));
   if(!symbol){ok=false;break;}
   auto slot=reinterpret_cast<void**>(&address->u1.Function);
   if(strcmp(symbol,"GetProcAddress")==0&&(_stricmp(dll,"KERNEL32.dll")==0||_stricmp(dll,"KERNELBASE.dll")==0)) {
    // Do not merge distinct resolver chains.
    auto current=reinterpret_cast<Resolver>(*slot);
    if(b.resolver&&b.resolver!=current){ok=false;break;}
    b.resolver=current;
    if(!patch(slot,reinterpret_cast<void*>(current),reinterpret_cast<void*>(&resolve))){ok=false;break;}++report.resolvers;
   } else if(strcmp(symbol,"XInputGetState")==0) {
    wchar_t wide[64]{};
    if(strlen(dll)>=64)continue;
    for(size_t k=0;dll[k];++k)wide[k]=wchar_t(dll[k]);
    if(!xinputName(wide))continue;
    auto current=*slot;
    auto wrapped=bind(reinterpret_cast<GetState>(current));
    if(!wrapped||!patch(slot,current,reinterpret_cast<void*>(wrapped))){ok=false;break;}++report.directStates;
   }
  }
  if(!ok)break;
 }
 report.installed=ok&&(report.resolvers||report.directStates);
 if(!report.installed)remove();
 return report;
}
CachedHook attachCachedGameState(HMODULE consumer) {
 auto&b=get();
 if(!consumer||!b.enabled.load()||b.degraded.load())return CachedHook::Unavailable;
 auto base=reinterpret_cast<uintptr_t>(consumer);
 auto rva=cache::locate([&](uint32_t offset,void*data,size_t bytes){
  SIZE_T done=0;return offset<=UINTPTR_MAX-base&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(base+offset),data,bytes,&done)&&done==bytes;
 });
 if(!rva)return CachedHook::Unsupported;
 auto slot=reinterpret_cast<void**>(base+rva);void*current=nullptr;SIZE_T done=0;
 if(!ReadProcessMemory(GetCurrentProcess(),slot,&current,sizeof(current),&done)||done!=sizeof(current))return CachedHook::Unavailable;
 for(unsigned i=0;i<b.patchCount;++i)if(b.patches[i].slot==slot){
  if(current==b.patches[i].replacement)return CachedHook::Attached;
  b.degraded=true;failOpen();return CachedHook::Conflict;
 }
 if(!current)return CachedHook::Pending;
 for(auto wrapper:wrappers)if(current==reinterpret_cast<void*>(wrapper))return CachedHook::Attached;
 MEMORY_BASIC_INFORMATION info{};
 if(!VirtualQuery(current,&info,sizeof(info))||info.State!=MEM_COMMIT||(info.Protect&(PAGE_GUARD|PAGE_NOACCESS))||!(info.Protect&(PAGE_EXECUTE|PAGE_EXECUTE_READ|PAGE_EXECUTE_READWRITE|PAGE_EXECUTE_WRITECOPY)))return CachedHook::Unavailable;
 // Preserve the exact cached chain, including Steam's mapping wrapper. Do not
 // substitute a freshly resolved system export or patch any executable bytes.
 auto wrapped=bind(reinterpret_cast<GetState>(current));
 if(!wrapped||!patch(slot,current,reinterpret_cast<void*>(wrapped))){b.degraded=true;failOpen();return CachedHook::Unavailable;}
 return CachedHook::Attached;
}
void enableEntry(bool enabled){
 auto&b=get();enabled=enabled&&b.enabled.load()&&!b.degraded.load();
 AcquireSRWLockExclusive(&b.entryControl);
 if(b.entryEnabled.exchange(enabled)!=enabled)b.entryGeneration.fetch_add(1);
 if(!enabled)b.open=0;
 ReleaseSRWLockExclusive(&b.entryControl);
}
unsigned openRequest(){return get().open.load();}
void acknowledgeOpen(){get().open=0;}
void setCapturing(bool value) {
 auto&b=get();AcquireSRWLockExclusive(&b.registry);
 value=value&&b.enabled.load()&&!b.degraded.load();b.capture=value;
 if(value)b.bypass=false;
 for(unsigned i=0;i<b.count;++i){auto&e=b.endpoints[i];AcquireSRWLockExclusive(&e.lock);++e.epoch;e.filter.setCapturing(value);ReleaseSRWLockExclusive(&e.lock);}
 ReleaseSRWLockExclusive(&b.registry);
}
void failOpen() {
 auto&b=get();enableEntry(false);b.open=0;AcquireSRWLockExclusive(&b.registry);b.capture=false;b.bypass=true;
 for(unsigned i=0;i<b.count;++i){auto&e=b.endpoints[i];AcquireSRWLockExclusive(&e.lock);++e.epoch;e.filter.failOpen();ReleaseSRWLockExclusive(&e.lock);}
 ReleaseSRWLockExclusive(&b.registry);
}
bool capturing(){return get().capture.load();}
unsigned endpointCount(){auto&b=get();AcquireSRWLockShared(&b.registry);auto n=b.count;ReleaseSRWLockShared(&b.registry);return n;}
DWORD sample(unsigned endpoint,DWORD slot,XINPUT_STATE*state) {
 auto&b=get();AcquireSRWLockShared(&b.registry);
 auto fn=endpoint<b.count?b.endpoints[endpoint].next:nullptr;
 ReleaseSRWLockShared(&b.registry);
 if(!fn||!state)return ERROR_BAD_ARGUMENTS;
 b.rawCalls.fetch_add(1);
 auto&e=b.endpoints[endpoint];auto query=beginQuery(e,slot);
 DWORD result=downstream(fn,slot,state);
 AcquireSRWLockExclusive(&e.lock);
 if(!b.bypass.load()) {
  if(acceptQuery(e,slot,query))e.filter.observe(slot,result,*state);
  else if(result==ERROR_SUCCESS){*state={};result=ERROR_RETRY;}
 }
 ReleaseSRWLockExclusive(&e.lock);return result;
}
BridgeStats stats(){auto&b=get();return {b.gameCalls.load(),b.rawCalls.load(),b.recursions.load(),endpointCount(),b.degraded.load(),b.neutralized.load()};}
bool remove() {
 auto&b=get();b.enabled=false;failOpen();bool ok=true;
 for(unsigned i=b.patchCount;i>0;--i) {
  auto&p=b.patches[i-1];DWORD protection;
  if(!VirtualProtect(p.slot,sizeof(void*),PAGE_READWRITE,&protection)){ok=false;continue;}
  auto previous=InterlockedCompareExchangePointer(p.slot,p.old,p.replacement);
  if(previous!=p.replacement)ok=false;
  DWORD unused;if(!VirtualProtect(p.slot,sizeof(void*),protection,&unused))ok=false;
 }
 // Keep immutable endpoints and resolver: another hook or a caller may have cached them.
 return ok;
}
}
