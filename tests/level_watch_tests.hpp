#pragma once
#include "level_watch.hpp"
#include "game_layout.hpp"
#include <cassert>
#include <map>
#include <vector>
namespace sketch {
struct LevelMemoryFixture {
  std::map<uintptr_t,unsigned char> bytes;int roots=0;bool drift=false;uintptr_t managerRva=0xb69600;
  void zero(uintptr_t at,size_t size){for(size_t i=0;i<size;++i)bytes[at+i]=0;}
  template<class T> void put(uintptr_t at,const T& value){auto p=reinterpret_cast<const unsigned char*>(&value);for(size_t i=0;i<sizeof(T);++i)bytes[at+i]=p[i];}
  bool read(uintptr_t at,void* data,size_t size){if(at==0x100000+managerRva&&drift&&++roots==2){uintptr_t other=0x500000;std::memcpy(data,&other,size);return true;}auto out=static_cast<unsigned char*>(data);for(size_t i=0;i<size;++i){auto p=bytes.find(at+i);if(p==bytes.end())return false;out[i]=p->second;}return true;}
};
inline void checkLevelWatch(){
 using Memory=LevelMemoryFixture;
 assert(!knownGameLayout("")&&!knownGameLayout("d802098b"));
 LevelIdentity a;
 for(const auto& layout:GameLayouts){assert(knownGameLayout(layout.sha256)==&layout);
 auto fixture=[&](){Memory m;m.managerRva=layout.managerRva;uintptr_t manager=0x200000,entity=0x300000,type=0x400000,list=0x210000,typenameAt=0x410000,nameAt=0x420000;
  m.zero(manager+0xdd0,24);m.zero(entity,1320);m.zero(type,32);m.put(uintptr_t(0x100000+m.managerRva),manager);
  m.put(manager+0xdd0,int64_t(1));m.put(manager+0xdd8,list);m.put(manager+0xde0,int64_t(1));m.put(list,entity);
  m.put(entity+240,manager);m.put(entity+256,uint32_t(51380225));m.put(entity+264,type);
  m.put(type,uint32_t(7));m.put(type+8,int64_t(1320));m.put(type+16,int64_t(8));m.put(type+24,typenameAt);for(size_t i=0;i<8;++i)m.put(typenameAt+i,"Settings"[i]);
  m.put(entity+816,int64_t(12));m.put(entity+824,int64_t(9));m.put(entity+832,int64_t(108));m.put(entity+844,int32_t(-1));m.put(entity+848,int32_t(2));m.put(entity+852,int32_t(-1));
  std::string name="torc_blocked";m.put(entity+1296,int64_t(name.size()));m.put(entity+1304,nameAt);for(size_t i=0;i<name.size();++i)m.put(nameAt+i,name[i]);return m;
 };
 auto read=[](Memory& m,LevelIdentity& result){return readValidatedLevel([&](uintptr_t p,void* b,size_t n){return m.read(p,b,n);},0x100000,result,m.managerRva);};
 auto memory=fixture();assert(read(memory,a)&&a.valid()&&a.columns==12&&a.rows==9&&a.settingsId==51380225&&a.name=="torc_blocked"&&a.corner[0]==-1);
 for(int kind=0;kind<13;++kind){auto bad=fixture();switch(kind){
  case 0:bad.put(0x300000+240,uintptr_t(0x500000));break;
  case 1:bad.put(0x400000,uint32_t(6));break;
  case 2:bad.put(0x400000+8,int64_t(1321));break;
  case 3:bad.put(0x300000+256,uint32_t(0));break;
  case 4:bad.put(0x300000+280,uint8_t(1));break;
  case 5:bad.put(0x300000+832,int64_t(109));break;
  case 6:bad.put(0x300000+816,int64_t(2049));break;
  case 7:bad.put(0x300000+1296,int64_t(0));break;
  case 8:bad.put(0x300000+1296,int64_t(129));break;
  case 9:bad.put(0x420000,char('\0'));break;
  case 10:bad.put(0x200000+0xde0,int64_t(0));break;
  case 11:bad.bytes.erase(0x420000+2);break;
  case 12:bad.drift=true;break;
 }LevelIdentity untouched=a;assert(!read(bad,untouched)&&untouched==a);}
 }
 LevelWatch watch;assert(watch.due(0)&&!watch.due(199)&&watch.due(200));
 assert(!watch.observe(&a,0)&&!watch.observe(&a,200)&&!watch.observe(&a,400)&&!watch.known);
 assert(!watch.observe(&a,600)&&watch.known&&watch.current==a);
 auto b=a;b.name="another_level"; // Same dimensions must still distinguish different levels.
 assert(!watch.observe(&b,800)&&watch.transitioning());assert(!watch.observe(&a,1000)&&!watch.transitioning());
 assert(!watch.observe(nullptr,1200)&&watch.current==a);assert(!watch.observe(&a,1400)&&!watch.transitioning());
 assert(!watch.observe(&b,1600)&&!watch.observe(&b,1800));assert(!watch.observe(nullptr,2000));
 assert(!watch.observe(&b,2200)&&!watch.observe(&b,2400)&&!watch.observe(&b,2600));assert(watch.observe(&b,2800)&&watch.current==b);
 for(uint64_t t=3000;t<4000;t+=200)assert(!watch.observe(&b,t));
 auto c=b;++c.settingsId;assert(!watch.observe(&c,4000)&&!watch.observe(&c,4200));
 assert(!watch.observe(&c,7000)&&!watch.observe(&c,7200)&&!watch.observe(&c,7400));assert(watch.observe(&c,7600));
 LevelIdentity invalid=c;invalid.name.clear();assert(!watch.observe(&invalid,7800)&&watch.current==c);
}
}
