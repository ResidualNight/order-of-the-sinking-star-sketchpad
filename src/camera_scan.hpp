#pragma once
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <vector>
namespace sketch {
struct CameraScanStats {
 size_t scans=0,threads=0,stackRanges=0,bytes=0,unreadableRegions=0,readFailures=0;
 size_t viewportMatches=0,invalid=0,orientation=0,valid=0;
 size_t queryApiFailures=0,snapshotFailures=0,enumerationFailures=0,openThreadFailures=0,queryThreadFailures=0,tibReadFailures=0,badStackRanges=0;
 size_t virtualQueryFailures=0,guardRegions=0,uncommittedRegions=0,retryRecovered=0,retryFailures=0;
 DWORD queryApiError=0,snapshotError=0,enumerationError=0,openThreadError=0,tibReadError=0,virtualQueryError=0,readError=0;
 LONG threadQueryStatus=0;
 DWORD systemError=0;
};
// Read only committed readable pages. A guard page, a decommitted stack segment,
// or one failed read must not discard all the other frames in the thread stack.
template<class Visit>
void scanCameraStack(uintptr_t low,uintptr_t high,Visit visit,CameraScanStats& stats){
 constexpr size_t FrameBytes=960,ChunkBytes=64*1024,Overlap=FrameBytes-4;
 if(low<0x10000||high<=low)return;
 std::vector<unsigned char> buffer(ChunkBytes+Overlap);
 size_t carry=0;uintptr_t at=low,previousEnd=0;
 while(at<high){
  MEMORY_BASIC_INFORMATION info{};
  if(!VirtualQuery(reinterpret_cast<void*>(at),&info,sizeof(info))){stats.systemError=stats.virtualQueryError=GetLastError();++stats.virtualQueryFailures;break;}
  uintptr_t base=reinterpret_cast<uintptr_t>(info.BaseAddress);
  if(base>at||info.RegionSize>UINTPTR_MAX-base||base+info.RegionSize<=at)break;
  uintptr_t end=std::min(high,base+info.RegionSize);
  DWORD access=info.Protect&0xff;
  bool readable=info.State==MEM_COMMIT&&!(info.Protect&PAGE_GUARD)&&
   (access==PAGE_READONLY||access==PAGE_READWRITE||access==PAGE_WRITECOPY||
    access==PAGE_EXECUTE_READ||access==PAGE_EXECUTE_READWRITE||access==PAGE_EXECUTE_WRITECOPY);
  if(!readable){++stats.unreadableRegions;if(info.Protect&PAGE_GUARD)++stats.guardRegions;if(info.State!=MEM_COMMIT)++stats.uncommittedRegions;carry=0;at=end;continue;}
  while(at<end){
   if(previousEnd!=at)carry=0;
   size_t size=std::min(size_t(end-at),ChunkBytes);SIZE_T done=0;
   bool ok=ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),buffer.data()+carry,size,&done)&&done==size;
   if(!ok){
    ++stats.readFailures;stats.systemError=stats.readError=GetLastError();
    // Retry this page separately if a later page changed protection mid-scan.
    SYSTEM_INFO system{};GetSystemInfo(&system);size=std::min(size,size_t(system.dwPageSize-at%system.dwPageSize));done=0;
    ok=ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),buffer.data()+carry,size,&done)&&done==size;
    if(ok)++stats.retryRecovered;else{++stats.retryFailures;stats.systemError=stats.readError=GetLastError();}
   }
   if(ok){
    stats.bytes+=size;size_t total=carry+size;uintptr_t start=at-carry;
    size_t first=(4-start%4)%4;
    for(size_t offset=first;offset+FrameBytes<=total;offset+=4)visit(start+offset,buffer.data()+offset);
    carry=std::min(total,Overlap);std::memmove(buffer.data(),buffer.data()+total-carry,carry);previousEnd=at+size;
   }else carry=0;
   at+=size;
  }
 }
}
}
