#pragma once
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <utility>

namespace sketch {
// Stable Settings values, not allocation addresses or just the number of cells.
struct LevelIdentity {
 uint32_t settingsId=0;int columns=0,rows=0;std::array<int32_t,3> corner{};std::string name;
 bool valid()const{return settingsId&&columns>0&&rows>0&&columns<=2048&&rows<=2048&&!name.empty()&&name.size()<=128;}
};
inline bool operator==(const LevelIdentity&a,const LevelIdentity&b){return a.settingsId==b.settingsId&&a.columns==b.columns&&a.rows==b.rows&&a.corner==b.corner&&a.name==b.name;}
inline bool operator!=(const LevelIdentity&a,const LevelIdentity&b){return !(a==b);}

// These r67271 offsets are attempted on unverified builds as well. The caller
// reports the fingerprint mismatch; all runtime layout/stability checks apply.
// The injected reader also lets tests exercise the exact runtime validation without game data.
template<class Read> bool readValidatedLevel(Read read,uintptr_t image,LevelIdentity& output){
 auto get=[&](uintptr_t address,auto& value){return address>0x10000&&read(address,&value,sizeof(value));};
 struct Array{int64_t count;uintptr_t data;int64_t capacity;};
 struct Type{uint32_t tag,pad;int64_t size,count;uintptr_t data;};
 struct Geometry{int64_t x,y,count;uint8_t wrapping,pad[3];int32_t corner[3];};
 struct Text{int64_t count;uintptr_t data;};
 static_assert(sizeof(Array)==24&&sizeof(Type)==32&&sizeof(Geometry)==40&&sizeof(Text)==16);
 uintptr_t manager=0;if(!get(image+0xb69600,manager)||manager<=0x10000)return false;
 Array array{};if(!get(manager+0xdd0,array)||array.count<1||array.count>4||array.capacity<array.count||array.capacity>100000||array.data<=0x10000)return false;
 std::array<uintptr_t,4> settings{};if(!read(array.data,settings.data(),size_t(array.count)*sizeof(uintptr_t)))return false;
 for(int64_t i=0;i<array.count;++i){
  uintptr_t entity=settings[size_t(i)],owner=0,type=0;uint32_t id=0;uint8_t destroyed=1;Type info{};char typeName[8]{};
  if(entity<=0x10000||!get(entity+240,owner)||owner!=manager||!get(entity+264,type)||!get(type,info)||info.tag!=7||info.size!=1320||info.count!=8||info.data<=0x10000||!read(info.data,typeName,8)||std::memcmp(typeName,"Settings",8)!=0)continue;
  if(!get(entity+256,id)||!id||!get(entity+280,destroyed)||destroyed)return false;
  Geometry grid{};if(!get(entity+816,grid)||grid.x<1||grid.x>2048||grid.y<1||grid.y>2048||grid.count!=grid.x*grid.y)return false;
  Text name{};if(!get(entity+1296,name)||name.count<1||name.count>128||name.data<=0x10000)return false;
  std::string value(size_t(name.count),'\0');if(!read(name.data,value.data(),value.size()))return false;
  for(unsigned char c:value)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'||c=='/'))return false;
  // Reject mixed snapshots while loading, including manager reuse and an in-place Settings edit.
  uintptr_t endManager=0,endEntity=0,endOwner=0,endType=0;Array endArray{};Geometry endGrid{};Text endName{};uint32_t endId=0;uint8_t endDestroyed=1;
  if(!get(image+0xb69600,endManager)||endManager!=manager||!get(manager+0xdd0,endArray)||endArray.count!=array.count||endArray.data!=array.data||endArray.capacity!=array.capacity||!get(array.data+size_t(i)*sizeof(uintptr_t),endEntity)||endEntity!=entity||!get(entity+240,endOwner)||endOwner!=owner||!get(entity+264,endType)||endType!=type||!get(entity+256,endId)||endId!=id||!get(entity+280,endDestroyed)||endDestroyed||!get(entity+816,endGrid)||std::memcmp(&grid,&endGrid,sizeof(grid))||!get(entity+1296,endName)||endName.count!=name.count||endName.data!=name.data)return false;
  std::string again(value.size(),'\0');if(!read(name.data,again.data(),again.size())||again!=value)return false;
  output={id,int(grid.x),int(grid.y),{grid.corner[0],grid.corner[1],grid.corner[2]},std::move(value)};return true;
 }
 return false;
}

class LevelWatch {
 LevelIdentity candidate;bool polling=false,pending=false;unsigned samples=0;uint64_t lastPoll=0,since=0,lastSample=0;
public:
 static constexpr uint64_t PollMs=200,SettleMs=500,MaxGapMs=1000;
 LevelIdentity current;bool known=false,available=false;
 bool due(uint64_t now){if(polling&&now-lastPoll<PollMs)return false;polling=true;lastPoll=now;return true;}
 bool transitioning()const{return known&&(!available||pending);}
 bool observe(const LevelIdentity* value,uint64_t now){
  available=value&&value->valid();if(!available){pending=false;samples=0;return false;}
  if(known&&*value==current){pending=false;samples=0;lastSample=now;return false;}
  if(!pending||*value!=candidate||now-lastSample>MaxGapMs){candidate=*value;since=now;samples=0;pending=true;}
  ++samples;lastSample=now;if(samples<3||now-since<SettleMs)return false;
  bool changed=known;current=candidate;known=true;pending=false;samples=0;return changed;
 }
};
}
