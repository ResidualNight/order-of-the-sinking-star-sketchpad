#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
namespace sketch::pad::cache {
// Evidence from Steam build 26249670. Every span must match before a write is
// permitted; a new executable/layout is rejected rather than guessing a slot.
inline constexpr uint32_t InitRva=0x1b00fa,CallRva=0x25f6d3,SlotRva=0xb69ff8;
inline constexpr uint8_t Init[]={
 0x48,0x8d,0x0d,0xbc,0x57,0x5a,0x00,0xe8,0x8a,0xdc,0x4a,0x00,0x48,0x85,0xc0,0x75,0x22,
 0x48,0x8d,0x0d,0xb9,0x57,0x5a,0x00,0xe8,0x79,0xdc,0x4a,0x00,0x48,0x85,0xc0,0x75,0x11,
 0x48,0x8d,0x0d,0xb6,0x57,0x5a,0x00,0xe8,0x68,0xdc,0x4a,0x00,0x48,0x85,0xc0,0x74,0x36,
 0x48,0x8d,0x15,0xb5,0x57,0x5a,0x00,0x48,0x89,0xc1,0xe8,0x74,0xdb,0x4a,0x00,0x48,0x89,0x05,0xb5,0x9e,0x9b,0x00};
inline constexpr uint8_t Call[]={0x66,0x0f,0x7f,0xb4,0x24,0xe0,0x02,0x00,0x00,0x89,0xf1,0x48,0x8d,0x94,0x24,0xe0,0x02,0x00,0x00,0xff,0x15,0x0c,0xa9,0x90,0x00,0x85,0xc0,0x0f,0x95,0xc1};
inline constexpr uint8_t Resolve[]={0xff,0x25,0x4a,0x15,0x16,0x00};
inline constexpr uint8_t Load[]={0xff,0x25,0x2a,0x15,0x16,0x00};
inline constexpr char Names[]="xinput1_4.dll\0xinput1_3.dll\0xinput1_9_0.dll\0XInputGetState";
// Reader accepts image-relative ranges. Runtime uses guarded process reads;
// offline tests map sections from the unmodified EXE without executing it.
template<class Read> uint32_t locate(Read read){
 IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
 if(!read(0,&dos,sizeof(dos))||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<0||dos.e_lfanew>1048576)return 0;
 if(!read(uint32_t(dos.e_lfanew),&nt,sizeof(nt))||nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt.FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64))return 0;
 auto size=nt.OptionalHeader.SizeOfImage;
 if(size<SlotRva+8||size>512*1024*1024||nt.FileHeader.NumberOfSections<1||nt.FileHeader.NumberOfSections>32)return 0;
 std::array<IMAGE_SECTION_HEADER,32> sections{};
 auto table=uint32_t(dos.e_lfanew)+24+nt.FileHeader.SizeOfOptionalHeader;
 if(!read(table,sections.data(),nt.FileHeader.NumberOfSections*sizeof(IMAGE_SECTION_HEADER)))return 0;
 auto range=[&](uint32_t rva,size_t bytes,DWORD required,DWORD forbidden){
  if(rva>size||bytes>size-rva)return false;
  unsigned matches=0;
  for(unsigned i=0;i<nt.FileHeader.NumberOfSections;++i){auto&s=sections[i];auto length=s.Misc.VirtualSize;
   if(s.VirtualAddress>size||length>size-s.VirtualAddress)return false;
   if(rva>=s.VirtualAddress&&rva-s.VirtualAddress<=length&&bytes<=length-(rva-s.VirtualAddress)){
    if((s.Characteristics&required)!=required||(s.Characteristics&forbidden))return false;
    ++matches;
   }
  }return matches==1;
 };
 constexpr DWORD R=IMAGE_SCN_MEM_READ,W=IMAGE_SCN_MEM_WRITE,X=IMAGE_SCN_MEM_EXECUTE;
 auto match=[&](uint32_t rva,const void*expected,size_t bytes,bool code){
  std::array<uint8_t,128> actual{};
  return bytes<=actual.size()&&range(rva,bytes,code?R|X:R,code?W:W|X)&&read(rva,actual.data(),bytes)&&std::memcmp(actual.data(),expected,bytes)==0;
 };
 if(!match(InitRva,Init,sizeof(Init),true)||!match(CallRva,Call,sizeof(Call),true)||!match(0x65dcb0,Resolve,sizeof(Resolve),true)||!match(0x65dd90,Load,sizeof(Load),true)||!match(0x7558bd,Names,sizeof(Names),false)||!range(SlotRva,8,R|W,X))return 0;
 // Confirm that both witnessed thunks really name the expected Win32 imports.
 auto dir=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
 if(dir.Size<sizeof(IMAGE_IMPORT_DESCRIPTOR)||dir.Size>65536||!range(dir.VirtualAddress,dir.Size,R,W|X))return 0;
 bool resolver=false,loader=false,terminated=false;
 for(size_t i=0;i<dir.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR);++i){
  IMAGE_IMPORT_DESCRIPTOR d{};if(!read(dir.VirtualAddress+uint32_t(i*sizeof(d)),&d,sizeof(d)))return 0;
  if(!d.Name){terminated=true;break;}
  char dll[14]{};if(!range(d.Name,13,R,W|X)||!read(d.Name,dll,13))return 0;
  if(_stricmp(dll,"KERNEL32.dll")!=0)continue;
  // This profile uses KERNEL32; refuse truncated or unrelated module names.
  if(!d.OriginalFirstThunk)continue;
  for(auto target:{uint32_t(0x7bf200),uint32_t(0x7bf2c0)}){
   if(target<d.FirstThunk||(target-d.FirstThunk)%8)continue;
   uint64_t name=0;uint32_t offset=target-d.FirstThunk;
   if(offset>32768||d.OriginalFirstThunk>size||offset>size-d.OriginalFirstThunk||!range(target,8,R,X)||!read(d.OriginalFirstThunk+offset,&name,8)||IMAGE_SNAP_BY_ORDINAL64(name)||name>size-16)continue;
   char symbol[16]{};if(!range(uint32_t(name)+2,15,R,W|X)||!read(uint32_t(name)+2,symbol,15))return 0;
   if(target==0x7bf200&&std::strcmp(symbol,"GetProcAddress")==0)resolver=true;
   if(target==0x7bf2c0&&std::strcmp(symbol,"LoadLibraryA")==0)loader=true;
  }
 }
 return terminated&&resolver&&loader?SlotRva:0;
}
}
