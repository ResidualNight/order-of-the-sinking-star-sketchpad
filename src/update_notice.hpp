#pragma once
#include "version.hpp"
#include <array>
#include <cstdint>
#include <string>
namespace sketch {
struct ReleaseVersion {
 std::array<unsigned,3> parts{};
 bool operator>(const ReleaseVersion& other)const{return parts>other.parts;}
 uint64_t packed()const{return (uint64_t(parts[0])<<32)|(uint64_t(parts[1])<<16)|parts[2];}
 static ReleaseVersion unpack(uint64_t value){return {{{unsigned((value>>32)&65535),unsigned((value>>16)&65535),unsigned(value&65535)}}};}
 std::string text()const{return std::to_string(parts[0])+"."+std::to_string(parts[1])+"."+std::to_string(parts[2]);}
 static bool parse(const std::string& text,ReleaseVersion& result){
  ReleaseVersion parsed;size_t at=0;
  for(int i=0;i<3;++i){size_t start=at;unsigned value=0;
   while(at<text.size()&&text[at]>='0'&&text[at]<='9'){value=value*10+unsigned(text[at++]-'0');if(value>65535)return false;}
   if(at==start||(i<2&&(at==text.size()||text[at++]!='.')))return false;parsed.parts[i]=value;
  }
  if(at!=text.size())return false;result=parsed;return true;
 }
};
inline std::string updateNotice(uint64_t available){
 ReleaseVersion current;if(available>>48||!ReleaseVersion::parse(CurrentVersion,current))return {};
 auto latest=ReleaseVersion::unpack(available);if(!(latest>current))return {};
 return "发现新版本 "+latest.text()+"，请退出游戏后使用安装器更新";
}
struct UpdateCheckSchedule {
 static constexpr uint64_t IntervalMs=30*60*1000;
 bool started=false;uint64_t next=0;
 bool due(uint64_t now){if(started&&now<next)return false;started=true;next=now+IntervalMs;return true;}
};
}
