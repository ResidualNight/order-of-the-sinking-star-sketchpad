#include "update_check.hpp"
#include <windows.h>
#include <cassert>
#include <iostream>
using namespace sketch;
namespace {
uint64_t number(const std::string& text){ReleaseVersion version;assert(ReleaseVersion::parse(text,version));return version.packed();}
std::string entry(const std::string& tag){return "<entry><link rel='alternate' href='https://github.com/ResidualNight/order-of-the-sinking-star-sketchpad/releases/tag/"+tag+"'/></entry>";}
std::string feed(const std::string& entries){return "<?xml version='1.0' encoding='UTF-8'?><feed xmlns='http://www.w3.org/2005/Atom'>"+entries+"</feed>";}
struct Mock {
 std::string xml;bool offline=false,missingZip=false,missingReceipt=false;int gets=0,heads=0;
 uint64_t check(const std::string& current="0.2.1"){
  return checkUpdate(current,[&](const std::wstring& url,bool head,size_t limit,std::string& body){
   if(offline)return false;
   if(!head){++gets;assert(url==std::wstring(ReleaseRoot)+L".atom"&&limit==512*1024);body=xml;return true;}
   ++heads;assert(url.find(std::wstring(ReleaseRoot)+L"/download/")==0);
   if(url.find(L"-installer.zip")!=std::wstring::npos){assert(limit==128*1024*1024);return !missingZip;}
   assert(url.find(L"-installer-manifest.json")!=std::wstring::npos&&limit==64*1024);return !missingReceipt;
  });
 }
};
}
int main(int argc,char** argv){
 if(argc==3&&std::string(argv[1])=="--live"){
  auto result=fetchAvailableUpdate(argv[2]);std::cout<<(result?ReleaseVersion::unpack(result).text():"no notice (no newer version or network unavailable)")<<"\n";return 0;
 }
 CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 Mock newer{feed(entry("v0.2.2-test"))};assert(newer.check()==number("0.2.2")&&newer.gets==1&&newer.heads==2);
 Mock sorted{feed(entry("v0.2.9-test")+entry("v0.2.10-test")+entry("v0.2.8"))};assert(sorted.check()==number("0.2.10"));
 Mock stable{feed(entry("v0.2.2-test")+entry("v0.2.2"))};
 assert(checkUpdate("0.2.1",[&](const std::wstring& url,bool head,size_t,std::string& body){if(!head){body=stable.xml;return true;}assert(url.find(L"/v0.2.2/")!=std::wstring::npos);return true;})==number("0.2.2"));
 for(auto tag:{"v0.2.0-test","v0.2.1-test","v0.2.1"}){Mock same{feed(entry(tag))};assert(same.check()==0&&same.heads==0);}
 Mock offline{newer.xml};offline.offline=true;assert(offline.check()==0);
 Mock zip{newer.xml};zip.missingZip=true;assert(zip.check()==0&&zip.heads==1);
 Mock receipt{newer.xml};receipt.missingReceipt=true;assert(receipt.check()==0&&receipt.heads==2);
 for(auto invalid:{"v0.2.2-beta","v0.2.2-test/../evil","v0.2.2-test?x=1","v0.2.65536-test","v0.2.2.3","v99999999999.0.0","V0.2.2"}){
  Mock bad{feed(entry(invalid))};assert(bad.check()==0&&bad.heads==0);
 }
 for(auto invalid:{std::string("<html>not a feed</html>"),feed(entry("v0.2.2-test"))+"<",feed("<entry/>"),
   std::string("<!DOCTYPE feed [<!ENTITY xxe SYSTEM 'file:///C:/Windows/win.ini'>]>")+feed(entry("v0.2.2-test")),
   std::string("<feed>")+entry("v0.2.2-test")+"</feed>",
   feed("<entry><link rel='alternate' href='https://evil.example/v0.2.2'/></entry>"),
   feed("<entry><link rel='alternate' href='https://github.com/ResidualNight/order-of-the-sinking-star-sketchpad/releases/tag/v0.2.2'/><link rel='alternate' href='https://evil.example'/></entry>")}){
  Mock bad{invalid};assert(bad.check()==0&&bad.heads==0);
 }
 Mock huge{std::string(512*1024+1,'x')};assert(huge.check()==0);
 assert(newer.check("invalid")==0);
 ReleaseVersion unchanged;unchanged.parts={1,2,3};assert(!ReleaseVersion::parse("1.2.x",unchanged)&&unchanged.text()=="1.2.3");
 assert(updateNotice(0).empty()&&updateNotice(number("0.2.0")).empty()&&updateNotice(number("0.2.1")).empty());
 auto current=number(CurrentVersion);assert(updateNotice(current).empty());
 auto nextVersion=ReleaseVersion::unpack(current);++nextVersion.parts[2];
 assert(updateNotice(nextVersion.packed())=="发现新版本 "+nextVersion.text()+"，请退出游戏后使用安装器更新");
 assert(updateNotice(uint64_t(1)<<63).empty());
 UpdateCheckSchedule schedule;assert(schedule.due(100)&&!schedule.due(101)&&!schedule.due(100+UpdateCheckSchedule::IntervalMs-1));
 assert(schedule.due(100+UpdateCheckSchedule::IntervalMs));
 std::wstring original=std::wstring(ReleaseRoot)+L".atom";
 assert(allowedUpdateRedirect(original,original));
 for(auto url:{L"https://release-assets.githubusercontent.com/assets?q=x",L"https://objects.githubusercontent.com/assets"})assert(allowedUpdateRedirect(url,original));
 for(auto url:{L"http://github.com/",L"https://evil.example/",L"https://github.com.evil.example/",L"https://github.com/other",L"https://user:pass@release-assets.githubusercontent.com/x",L"https://release-assets.githubusercontent.com:444/x",L"https://release-assets.githubusercontent.com/x#fragment"})assert(!allowedUpdateRedirect(url,original));
 CoUninitialize();
 std::cout<<"PASS: numeric versions, silent offline/same/older releases, published installer assets, Atom validation/DTD rejection, official HTTPS redirects and 30-minute schedule\n";
}
