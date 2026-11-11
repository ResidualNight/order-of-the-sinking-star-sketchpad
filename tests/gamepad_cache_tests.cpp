#include "gamepad_bridge.hpp"
#include "gamepad_cache.hpp"
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <string>
using namespace sketch::pad;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"cache FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static XINPUT_STATE physical{};
static unsigned mappedCalls=0;
static DWORD WINAPI mapped(DWORD slot,XINPUT_STATE*s){++mappedCalls;if(!s||slot>3)return ERROR_BAD_ARGUMENTS;*s=physical;if(s->Gamepad.wButtons&XINPUT_GAMEPAD_A)s->Gamepad.wButtons=XINPUT_GAMEPAD_B;return 0;}
static DWORD WINAPI foreign(DWORD slot,XINPUT_STATE*s){return mapped(slot,s);}
static uint32_t locate(std::vector<uint8_t>&v){return cache::locate([&](uint32_t r,void*d,size_t n){if(r>v.size()||n>v.size()-r)return false;std::memcpy(d,v.data()+r,n);return true;});}
static std::vector<uint8_t> fixture(){
 std::vector<uint8_t> v(0xb6b000);auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(v.data());dos->e_magic=IMAGE_DOS_SIGNATURE;dos->e_lfanew=0x80;
 auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(v.data()+0x80);nt->Signature=IMAGE_NT_SIGNATURE;nt->FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;nt->FileHeader.NumberOfSections=3;nt->FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER64);nt->OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;nt->OptionalHeader.SizeOfImage=DWORD(v.size());
 auto sections=IMAGE_FIRST_SECTION(nt);
 sections[0].VirtualAddress=0x1000;sections[0].Misc.VirtualSize=0x65e000;sections[0].Characteristics=IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_EXECUTE;
 sections[1].VirtualAddress=0x65f000;sections[1].Misc.VirtualSize=0x1a2000;sections[1].Characteristics=IMAGE_SCN_MEM_READ;
 sections[2].VirtualAddress=0x801000;sections[2].Misc.VirtualSize=DWORD(v.size())-0x801000;sections[2].Characteristics=IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE;
 auto copy=[&](uint32_t r,const void*p,size_t n){std::memcpy(v.data()+r,p,n);};
 copy(cache::InitRva,cache::Init,sizeof(cache::Init));copy(cache::CallRva,cache::Call,sizeof(cache::Call));copy(0x65dcb0,cache::Resolve,sizeof(cache::Resolve));copy(0x65dd90,cache::Load,sizeof(cache::Load));copy(0x7558bd,cache::Names,sizeof(cache::Names));
 nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT]={0x700000,40};
 auto d=reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(v.data()+0x700000);d->Name=0x700100;d->OriginalFirstThunk=0x700200;d->FirstThunk=0x7bf200;
 copy(d->Name,"KERNEL32.dll",13);
 uint64_t n=0x700400;copy(d->OriginalFirstThunk,&n,8);n=0x700440;copy(d->OriginalFirstThunk+0xc0,&n,8);
 copy(0x700402,"GetProcAddress",15);copy(0x700442,"LoadLibraryA",13);
 return v;
}
static void offline(const wchar_t*path){
 HANDLE f=CreateFileW(path,GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);CHECK(f!=INVALID_HANDLE_VALUE);
 DWORD size=GetFileSize(f,nullptr),done=0;CHECK(size>1024&&size<128*1024*1024);std::vector<uint8_t> raw(size);CHECK(ReadFile(f,raw.data(),size,&done,nullptr)&&done==size);CloseHandle(f);
 auto dos=reinterpret_cast<IMAGE_DOS_HEADER*>(raw.data());CHECK(dos->e_lfanew>0&&size_t(dos->e_lfanew)+sizeof(IMAGE_NT_HEADERS64)<raw.size());auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(raw.data()+dos->e_lfanew);CHECK(nt->OptionalHeader.SizeOfImage<512*1024*1024);
 std::vector<uint8_t> mappedImage(nt->OptionalHeader.SizeOfImage);CHECK(nt->OptionalHeader.SizeOfHeaders<=raw.size()&&nt->OptionalHeader.SizeOfHeaders<=mappedImage.size());std::memcpy(mappedImage.data(),raw.data(),nt->OptionalHeader.SizeOfHeaders);
 auto sections=IMAGE_FIRST_SECTION(nt);CHECK(reinterpret_cast<uint8_t*>(sections+nt->FileHeader.NumberOfSections)<=raw.data()+raw.size());
 for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){auto&s=sections[i];CHECK(size_t(s.PointerToRawData)+s.SizeOfRawData<=raw.size()&&size_t(s.VirtualAddress)+s.SizeOfRawData<=mappedImage.size());std::memcpy(mappedImage.data()+s.VirtualAddress,raw.data()+s.PointerToRawData,s.SizeOfRawData);}
 CHECK(locate(mappedImage)==cache::SlotRva);std::puts("actual EXE offline: initializer, call site, strings, import identities and writable cache agree");
}
int wmain(int argc,wchar_t**argv){
 if(argc>=2)offline(argv[1]);
 auto v=fixture();CHECK(locate(v)==cache::SlotRva);
 struct Span{uint32_t r;size_t n;};
 for(auto span:{Span{cache::InitRva,sizeof(cache::Init)},Span{cache::CallRva,sizeof(cache::Call)},Span{0x65dcb0,sizeof(cache::Resolve)},Span{0x65dd90,sizeof(cache::Load)},Span{0x7558bd,sizeof(cache::Names)},Span{0x700402,15},Span{0x700442,13}}){for(size_t i=0;i<span.n;++i){v[span.r+i]^=1;CHECK(!locate(v));v[span.r+i]^=1;}}
 auto nt=reinterpret_cast<IMAGE_NT_HEADERS64*>(v.data()+0x80);auto sections=IMAGE_FIRST_SECTION(nt);
 sections[2].Characteristics|=IMAGE_SCN_MEM_EXECUTE;CHECK(!locate(v));sections[2].Characteristics&=~IMAGE_SCN_MEM_EXECUTE;
 sections[2].Characteristics&=~IMAGE_SCN_MEM_WRITE;CHECK(!locate(v));sections[2].Characteristics|=IMAGE_SCN_MEM_WRITE;
 auto saved=sections[2].Misc.VirtualSize;sections[2].Misc.VirtualSize=0xffffffff;CHECK(!locate(v));sections[2].Misc.VirtualSize=saved;
 CHECK(cache::locate([](uint32_t,void*,size_t){return false;})==0);
 auto image=VirtualAlloc(nullptr,v.size(),MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);CHECK(image);std::memcpy(image,v.data(),v.size());
 // Prevent the compiler/linker from dropping GetProcAddress from the test's IAT.
 CHECK(GetProcAddress(GetModuleHandleW(L"kernel32.dll"),"GetProcAddress"));
 CHECK(install(GetModuleHandleW(nullptr)).installed);
 auto consumer=static_cast<HMODULE>(image);auto slot=reinterpret_cast<GetState*>(static_cast<uint8_t*>(image)+cache::SlotRva);
 CHECK(attachCachedGameState(consumer)==CachedHook::Pending);*slot=&mapped;
 // The game has already cached its mapped chain before our attachment.
 physical.dwPacketNumber=7;physical.Gamepad.wButtons=XINPUT_GAMEPAD_A;
 XINPUT_STATE state{};CHECK((*slot)(0,&state)==0&&state.Gamepad.wButtons==XINPUT_GAMEPAD_B);CHECK(stats().gameCalls==0);
 CHECK(attachCachedGameState(consumer)==CachedHook::Attached);auto wrapped=*slot;CHECK(wrapped!=&mapped);CHECK(stats().gameCalls==0);
 CHECK(attachCachedGameState(consumer)==CachedHook::Attached&&*slot==wrapped);
 CHECK((*slot)(0,&state)==0&&state.Gamepad.wButtons==XINPUT_GAMEPAD_B);CHECK(stats().gameCalls==1);
 setCapturing(true);CHECK((*slot)(0,&state)==0&&state.Gamepad.wButtons==0&&stats().neutralized==1);
 CHECK(sample(0,0,&state)==0&&state.Gamepad.wButtons==XINPUT_GAMEPAD_B);
 setCapturing(false);CHECK((*slot)(0,&state)==0&&state.Gamepad.wButtons==0);
 physical.Gamepad={};CHECK((*slot)(0,&state)==0);physical.Gamepad.wButtons=XINPUT_GAMEPAD_A;CHECK((*slot)(0,&state)==0&&state.Gamepad.wButtons==XINPUT_GAMEPAD_B);
 if(argc==3){
  setCapturing(true);CHECK(remove()&&*slot==&mapped);CHECK(!capturing());CHECK(wrapped(0,&state)==0&&state.Gamepad.wButtons==XINPUT_GAMEPAD_B);
  VirtualFree(image,0,MEM_RELEASE);std::puts("cached bridge: clean removal restores original cache and retained callbacks pass through");return 0;
 }
 setCapturing(true);*slot=&foreign;CHECK(attachCachedGameState(consumer)==CachedHook::Conflict);CHECK(!capturing()&&stats().degraded);
 CHECK(wrapped(0,&state)==0&&state.Gamepad.wButtons==XINPUT_GAMEPAD_B);CHECK(!remove()&&*slot==&foreign);CHECK(mappedCalls>=8);
 VirtualFree(image,0,MEM_RELEASE);
 std::puts("cached bridge: evidence rejection, late attachment, mapped chain, counters, isolation/release and foreign ownership passed");
}
