#pragma once
#include <windows.h>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <new>
namespace sketch::pad::cache {
// Evidence from Steam builds 26249670, 26466613 and 26787118. Each span must
// match before the bridge can attach to the cache.
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
inline constexpr uint8_t NewInit[]={
 0x48,0x8d,0x0d,0x38,0x04,0x5b,0x00,0xe8,0x5b,0x7d,0x4b,0x00,0x48,0x85,0xc0,0x75,0x22,
 0x48,0x8d,0x0d,0x35,0x04,0x5b,0x00,0xe8,0x4a,0x7d,0x4b,0x00,0x48,0x85,0xc0,0x75,0x11,
 0x48,0x8d,0x0d,0x32,0x04,0x5b,0x00,0xe8,0x39,0x7d,0x4b,0x00,0x48,0x85,0xc0,0x74,0x36,
 0x48,0x8d,0x15,0x31,0x04,0x5b,0x00,0x48,0x89,0xc1,0xe8,0x45,0x7c,0x4b,0x00,0x48,0x89,0x05,0x76,0x47,0x9c,0x00};
inline constexpr uint8_t NewCall[]={0x66,0x0f,0x7f,0xb4,0x24,0x30,0x02,0x00,0x00,0x89,0xf1,0x48,0x8d,0x94,0x24,0x30,0x02,0x00,0x00,0xff,0x15,0xcc,0xeb,0x90,0x00,0x85,0xc0,0x0f,0x95,0xc1};
inline constexpr uint8_t NewResolve[]={0xff,0x25,0x1a,0x21,0x16,0x00},NewLoad[]={0xff,0x25,0xfa,0x20,0x16,0x00};
inline constexpr uint8_t Steam26787118Init[]={
 0x48,0x8d,0x0d,0x52,0x95,0x5b,0x00,0xe8,0x3b,0x05,0x4c,0x00,0x48,0x85,0xc0,0x75,0x22,
 0x48,0x8d,0x0d,0x4f,0x95,0x5b,0x00,0xe8,0x2a,0x05,0x4c,0x00,0x48,0x85,0xc0,0x75,0x11,
 0x48,0x8d,0x0d,0x4c,0x95,0x5b,0x00,0xe8,0x19,0x05,0x4c,0x00,0x48,0x85,0xc0,0x74,0x36,
 0x48,0x8d,0x15,0x4b,0x95,0x5b,0x00,0x48,0x89,0xc1,0xe8,0x25,0x04,0x4c,0x00,0x48,0x89,0x05,0xa6,0xe0,0x9c,0x00};
inline constexpr uint8_t Steam26787118Call[]={0x66,0x0f,0x7f,0xb4,0x24,0xd0,0x01,0x00,0x00,0x89,0xf1,0x48,0x8d,0x94,0x24,0xd0,0x01,0x00,0x00,0xff,0x15,0x4c,0x21,0x91,0x00,0x85,0xc0,0x0f,0x95,0xc1};
inline constexpr uint8_t Steam26787118Resolve[]={0xff,0x25,0x2a,0x2a,0x16,0x00},Steam26787118Load[]={0xff,0x25,0x0a,0x2a,0x16,0x00};
struct Profile {uint32_t initRva,callRva,slotRva,resolveRva,loadRva,namesRva,resolveIat,loadIat;const uint8_t*init,*call,*resolve,*load;};
inline constexpr Profile Profiles[]={
 {InitRva,CallRva,SlotRva,0x65dcb0,0x65dd90,0x7558bd,0x7bf200,0x7bf2c0,Init,Call,Resolve,Load},
 {0x1b1859,0x267433,0xb76018,0x6694e0,0x6695c0,0x761c98,0x7cb600,0x7cb6c0,NewInit,NewCall,NewResolve,NewLoad},
 {0x1aefe9,0x26af73,0xb7d0d8,0x66f450,0x66f530,0x768542,0x7d1e80,0x7d1f40,Steam26787118Init,Steam26787118Call,Steam26787118Resolve,Steam26787118Load}};
static_assert(sizeof(Init)==sizeof(NewInit)&&sizeof(Call)==sizeof(NewCall)&&sizeof(Resolve)==sizeof(NewResolve)&&sizeof(Load)==sizeof(NewLoad));
static_assert(sizeof(Init)==sizeof(Steam26787118Init)&&sizeof(Call)==sizeof(Steam26787118Call)&&
              sizeof(Resolve)==sizeof(Steam26787118Resolve)&&sizeof(Load)==sizeof(Steam26787118Load));
// Readers use image-relative addresses. In the game every read is guarded;
// offline tests can supply an image mapped from an unmodified game executable.
constexpr DWORD R=IMAGE_SCN_MEM_READ,W=IMAGE_SCN_MEM_WRITE,X=IMAGE_SCN_MEM_EXECUTE;
struct Image {
 uint32_t size=0;
 unsigned count=0;
 IMAGE_DATA_DIRECTORY imports{};
 std::array<IMAGE_SECTION_HEADER,32> sections{};
 bool range(uint32_t rva,size_t bytes,DWORD required,DWORD forbidden)const{
  if(rva>size||bytes>size-rva)return false;
  unsigned matches=0;
  for(unsigned i=0;i<count;++i){const auto& s=sections[i];auto length=s.Misc.VirtualSize;
   if(rva>=s.VirtualAddress&&rva-s.VirtualAddress<=length&&bytes<=length-(rva-s.VirtualAddress)){
    if((s.Characteristics&required)!=required||(s.Characteristics&forbidden))return false;
    ++matches;
   }
  }
  return matches==1;
 }
};
template<class Read> bool readImage(Read read,Image& image){
 IMAGE_DOS_HEADER dos{};IMAGE_NT_HEADERS64 nt{};
 if(!read(0,&dos,sizeof(dos))||dos.e_magic!=IMAGE_DOS_SIGNATURE||dos.e_lfanew<64||dos.e_lfanew>1048576)return false;
 if(!read(uint32_t(dos.e_lfanew),&nt,sizeof(nt))||nt.Signature!=IMAGE_NT_SIGNATURE||nt.FileHeader.Machine!=IMAGE_FILE_MACHINE_AMD64||
    nt.OptionalHeader.Magic!=IMAGE_NT_OPTIONAL_HDR64_MAGIC||nt.FileHeader.SizeOfOptionalHeader!=sizeof(IMAGE_OPTIONAL_HEADER64)||
    nt.OptionalHeader.NumberOfRvaAndSizes<=IMAGE_DIRECTORY_ENTRY_IMPORT)return false;
 image.size=nt.OptionalHeader.SizeOfImage;image.count=nt.FileHeader.NumberOfSections;
 if(image.size<0x1000||image.size>512*1024*1024||!image.count||image.count>image.sections.size())return false;
 uint32_t table=uint32_t(dos.e_lfanew)+24+nt.FileHeader.SizeOfOptionalHeader;
 const auto tableSize=image.count*sizeof(IMAGE_SECTION_HEADER);
 if(table>image.size||tableSize>image.size-table||!read(table,image.sections.data(),tableSize))return false;
 for(unsigned i=0;i<image.count;++i){const auto& s=image.sections[i];
  if(s.VirtualAddress>image.size||s.Misc.VirtualSize>image.size-s.VirtualAddress)return false;
 }
 image.imports=nt.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
 return true;
}
template<size_t Size,class Read> bool matchBytes(Read read,const Image& image,uint32_t rva,const void* expected,DWORD required,DWORD forbidden){
 std::array<uint8_t,Size> actual{};
 return image.range(rva,Size,required,forbidden)&&read(rva,actual.data(),Size)&&std::memcmp(actual.data(),expected,Size)==0;
}
template<class Read> bool verifyImports(Read read,const Image& image,uint32_t resolveIat,uint32_t loadIat){
 const auto dir=image.imports;
 if(resolveIat==loadIat||dir.Size<sizeof(IMAGE_IMPORT_DESCRIPTOR)||dir.Size>65536||!image.range(dir.VirtualAddress,dir.Size,R,W|X))return false;
 bool resolver=false,loader=false,terminated=false;
 for(size_t i=0;i<dir.Size/sizeof(IMAGE_IMPORT_DESCRIPTOR);++i){
  IMAGE_IMPORT_DESCRIPTOR d{};if(!read(dir.VirtualAddress+uint32_t(i*sizeof(d)),&d,sizeof(d)))return false;
  if(!d.Name){terminated=true;break;}
  char dll[14]{};if(!image.range(d.Name,13,R,W|X)||!read(d.Name,dll,13))return false;
  if(_stricmp(dll,"KERNEL32.dll")!=0||!d.OriginalFirstThunk)continue;
  for(auto target:{resolveIat,loadIat}){
   if(target<d.FirstThunk||(target-d.FirstThunk)%8)continue;
   uint64_t name=0;uint32_t offset=target-d.FirstThunk;
   if(offset>32768||d.OriginalFirstThunk>image.size||offset>image.size-d.OriginalFirstThunk||!image.range(target,8,R,X)||
      !image.range(d.OriginalFirstThunk+offset,8,R,W|X)||!read(d.OriginalFirstThunk+offset,&name,8)||IMAGE_SNAP_BY_ORDINAL64(name)||name>image.size-17)continue;
   char symbol[16]{};if(!image.range(uint32_t(name)+2,15,R,W|X)||!read(uint32_t(name)+2,symbol,15))return false;
   if(target==resolveIat&&std::strcmp(symbol,"GetProcAddress")==0)resolver=true;
   if(target==loadIat&&std::strcmp(symbol,"LoadLibraryA")==0)loader=true;
  }
 }
 return terminated&&resolver&&loader;
}
template<class Read> bool verifyProfile(Read read,const Image& image,const Profile& p){
 return matchBytes<sizeof(Init)>(read,image,p.initRva,p.init,R|X,W)&&matchBytes<sizeof(Call)>(read,image,p.callRva,p.call,R|X,W)&&
        matchBytes<sizeof(Resolve)>(read,image,p.resolveRva,p.resolve,R|X,W)&&matchBytes<sizeof(Load)>(read,image,p.loadRva,p.load,R|X,W)&&
        matchBytes<sizeof(Names)>(read,image,p.namesRva,Names,R,W|X)&&image.range(p.slotRva,8,R|W,X)&&verifyImports(read,image,p.resolveIat,p.loadIat);
}
template<class Read> uint32_t locateProfile(Read read,const Profile& p){
 Image image;return readImage(read,image)&&verifyProfile(read,image,p)?p.slotRva:0;
}
struct Match {
 uint32_t slotRva=0,initRva=0,callRva=0;
 const Profile* profile=nullptr;
};
// Only displacement fields vary across the three observed builds. Decode and
// cross-check them before accepting an initializer or caller.
inline constexpr std::array<size_t,9> InitOperands={3,8,20,25,37,42,54,62,69};
inline constexpr std::array<size_t,3> CallOperands={5,15,21};
template<size_t Size,size_t Count> bool matchInstructions(const uint8_t* bytes,const uint8_t (&pattern)[Size],const std::array<size_t,Count>& operands){
 size_t start=0;
 for(size_t offset:operands){
  if(std::memcmp(bytes+start,pattern+start,offset-start)!=0)return false;
  start=offset+4;
 }
 return std::memcmp(bytes+start,pattern+start,Size-start)==0;
}
inline int32_t displacement(const uint8_t* bytes,size_t offset){int32_t value=0;std::memcpy(&value,bytes+offset,4);return value;}
inline uint32_t relativeTarget(uint32_t rva,const uint8_t* bytes,size_t offset){
 int64_t target=int64_t(rva)+int64_t(offset)+4+displacement(bytes,offset);
 return target>0&&target<=UINT32_MAX?uint32_t(target):0;
}
template<class Read> uint32_t verifyInitializer(Read read,const Image& image,uint32_t rva,const uint8_t* bytes){
 if(!matchInstructions(bytes,Init,InitOperands)||!image.range(rva,sizeof(Init),R|X,W))return 0;
 const auto names=relativeTarget(rva,bytes,3),load=relativeTarget(rva,bytes,8),resolve=relativeTarget(rva,bytes,62);
 const auto slot=relativeTarget(rva,bytes,69);
 if(!names||!load||!resolve||!slot||(slot%8)||
    !matchBytes<sizeof(Names)>(read,image,names,Names,R,W|X)||
    relativeTarget(rva,bytes,20)!=names+14||relativeTarget(rva,bytes,37)!=names+28||relativeTarget(rva,bytes,54)!=names+44||
    relativeTarget(rva,bytes,25)!=load||relativeTarget(rva,bytes,42)!=load||!image.range(slot,8,R|W,X))return 0;
 auto importTarget=[&](uint32_t thunk){
  uint8_t instruction[6]{};
  if(!image.range(thunk,sizeof(instruction),R|X,W)||!read(thunk,instruction,sizeof(instruction))||instruction[0]!=0xff||instruction[1]!=0x25)return uint32_t(0);
  return relativeTarget(thunk,instruction,2);
 };
 const auto resolveIat=importTarget(resolve),loadIat=importTarget(load);
 return resolveIat&&loadIat&&slot!=resolveIat&&slot!=loadIat&&verifyImports(read,image,resolveIat,loadIat)?slot:0;
}
inline bool verifyCaller(uint32_t callRva,const uint8_t* bytes,uint32_t slot){
 if(!matchInstructions(bytes,Call,CallOperands))return false;
 const auto state=displacement(bytes,5);
 return state>=0&&state%16==0&&state==displacement(bytes,15)&&relativeTarget(callRva,bytes,21)==slot;
}
template<class Read> Match discoverAdaptive(Read read,const Image& image){
 const IMAGE_SECTION_HEADER* text=nullptr;
 for(unsigned i=0;i<image.count;++i){const auto& section=image.sections[i];
  if(std::memcmp(section.Name,".text\0\0\0",8)!=0)continue;
  if(text)return {};
  text=&section;
 }
 if(!text||text->Misc.VirtualSize<sizeof(Init)||text->Misc.VirtualSize>16*1024*1024||!image.range(text->VirtualAddress,text->Misc.VirtualSize,R|X,W))return {};
 const auto start=text->VirtualAddress,size=text->Misc.VirtualSize;
 std::unique_ptr<uint8_t[]> code(new(std::nothrow) uint8_t[size]);
 if(!code||!read(start,code.get(),size))return {};
 Match found;
 for(uint32_t i=0;i<=size-sizeof(Init);++i){
  if(code[i]!=Init[0]||code[i+1]!=Init[1]||code[i+2]!=Init[2])continue;
  const auto slot=verifyInitializer(read,image,start+i,code.get()+i);
  if(!slot)continue;
  if(found.slotRva)return {};
  found.slotRva=slot;found.initRva=start+i;
 }
 if(!found.slotRva)return {};
 for(uint32_t i=0;i<=size-sizeof(Call);++i){
  if(code[i]!=Call[0]||code[i+1]!=Call[1]||code[i+2]!=Call[2]||!verifyCaller(start+i,code.get()+i,found.slotRva))continue;
  if(found.callRva)return {};
  found.callRva=start+i;
 }
 return found.callRva?found:Match{};
}
template<class Read> Match discover(Read read,bool allowAdaptive=true){
 Image image;if(!readImage(read,image))return {};
 Match found;
 for(const auto& profile:Profiles){
  if(!verifyProfile(read,image,profile))continue;
  if(found.slotRva)return {};
  found={profile.slotRva,profile.initRva,profile.callRva,&profile};
 }
 if(found.slotRva||!allowAdaptive)return found;
 return discoverAdaptive(read,image);
}
template<class Read> bool revalidate(Read read,const Match& match){
 Image image;if(!match.slotRva||!readImage(read,image))return false;
 if(match.profile)return verifyProfile(read,image,*match.profile);
 uint8_t init[sizeof(Init)]{},call[sizeof(Call)]{};
 return image.range(match.initRva,sizeof(init),R|X,W)&&read(match.initRva,init,sizeof(init))&&
        verifyInitializer(read,image,match.initRva,init)==match.slotRva&&
        image.range(match.callRva,sizeof(call),R|X,W)&&read(match.callRva,call,sizeof(call))&&verifyCaller(match.callRva,call,match.slotRva);
}
}
