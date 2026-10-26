#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <winhttp.h>
#include <xmllite.h>
#include <shlwapi.h>
#include "update_check.hpp"
#include <algorithm>
#include <new>
#include <vector>
namespace sketch { namespace {
struct UpdateWork{RuntimeUpdateChecker* checker;HMODULE module;};
template<class T>struct ComOwner{T* p=nullptr;~ComOwner(){if(p)p->Release();}};
struct HttpOwner{HINTERNET p=nullptr;explicit HttpOwner(HINTERNET value):p(value){}~HttpOwner(){if(p)WinHttpCloseHandle(p);}};
struct Release {ReleaseVersion version;std::wstring tag;bool stable=false;};
bool atomReleases(const std::string& body,std::vector<Release>& releases){
 if(body.empty()||body.size()>512*1024)return false;
 ComOwner<IStream> input;input.p=SHCreateMemStream(reinterpret_cast<const BYTE*>(body.data()),UINT(body.size()));
 ComOwner<IXmlReader> reader;
 if(!input.p||FAILED(CreateXmlReader(__uuidof(IXmlReader),reinterpret_cast<void**>(&reader.p),nullptr))||
    FAILED(reader.p->SetProperty(XmlReaderProperty_DtdProcessing,DtdProcessing_Prohibit))||
    FAILED(reader.p->SetProperty(XmlReaderProperty_MaxElementDepth,32))||FAILED(reader.p->SetInput(input.p)))return false;
 bool feed=false,entry=false;unsigned alternate=0;std::wstring link;
 auto finish=[&](){
  if(alternate!=1)return false;std::wstring prefix=std::wstring(ReleaseRoot)+L"/tag/";
  if(link.compare(0,prefix.size(),prefix)!=0)return false;
  std::wstring tag=link.substr(prefix.size());if(tag.empty()||tag[0]!=L'v')return true;
  std::wstring number=tag.substr(1);bool stable=true;
  if(number.size()>5&&number.compare(number.size()-5,5,L"-test")==0){number.resize(number.size()-5);stable=false;}
  std::string ascii;for(auto c:number){if(c>127)return true;ascii.push_back(char(c));}
  ReleaseVersion version;if(ReleaseVersion::parse(ascii,version))releases.push_back({version,tag,stable});
  return true;
 };
 XmlNodeType type;HRESULT hr;
 while((hr=reader.p->Read(&type))==S_OK){
  UINT depth=0;reader.p->GetDepth(&depth);const wchar_t* local=L"",*ns=L"";
  if(type==XmlNodeType_Element||type==XmlNodeType_EndElement){
   if(FAILED(reader.p->GetLocalName(&local,nullptr))||FAILED(reader.p->GetNamespaceUri(&ns,nullptr)))return false;
  }
  bool atom=std::wcscmp(ns,L"http://www.w3.org/2005/Atom")==0;
  if(type==XmlNodeType_Element){
   if(depth==0){if(feed||!atom||std::wcscmp(local,L"feed"))return false;feed=true;}
   if(depth==1&&atom&&std::wcscmp(local,L"entry")==0){
    if(reader.p->IsEmptyElement())return false;entry=true;alternate=0;link.clear();
   }
   if(depth==2&&entry&&atom&&std::wcscmp(local,L"link")==0){
    std::wstring rel,href;
    HRESULT attribute=reader.p->MoveToFirstAttribute();
    while(attribute==S_OK){const wchar_t* name=nullptr,*value=nullptr,*attrNs=nullptr;
     if(FAILED(reader.p->GetLocalName(&name,nullptr))||FAILED(reader.p->GetValue(&value,nullptr))||FAILED(reader.p->GetNamespaceUri(&attrNs,nullptr)))return false;
     if(!*attrNs){if(std::wcscmp(name,L"rel")==0)rel=value;else if(std::wcscmp(name,L"href")==0)href=value;}
     attribute=reader.p->MoveToNextAttribute();
    }
    if(FAILED(attribute)||FAILED(reader.p->MoveToElement()))return false;
    if(rel==L"alternate"){++alternate;link=href;}
   }
  }else if(type==XmlNodeType_EndElement&&depth==2&&entry&&atom&&std::wcscmp(local,L"entry")==0){
   if(!finish())return false;entry=false;
  }
 }
 return hr==S_FALSE&&feed&&!entry;
}
bool fetchUpdate(HINTERNET session,const std::wstring& original,bool head,size_t limit,std::string& body,uint64_t deadline){
 body.clear();std::wstring url=original;
 for(unsigned redirects=0;redirects<=5;++redirects){
  if(GetTickCount64()>=deadline||!allowedUpdateRedirect(url,original))return false;
  URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUrlPathLength=parts.dwExtraInfoLength=DWORD(-1);
  if(!WinHttpCrackUrl(url.c_str(),0,0,&parts))return false;
  std::wstring host(parts.lpszHostName,parts.dwHostNameLength),path(parts.lpszUrlPath,parts.dwUrlPathLength);
  if(parts.dwExtraInfoLength)path.append(parts.lpszExtraInfo,parts.dwExtraInfoLength);
  HttpOwner connection(WinHttpConnect(session,host.c_str(),INTERNET_DEFAULT_HTTPS_PORT,0));if(!connection.p)return false;
  HttpOwner request(WinHttpOpenRequest(connection.p,head?L"HEAD":L"GET",path.c_str(),nullptr,WINHTTP_NO_REFERER,WINHTTP_DEFAULT_ACCEPT_TYPES,WINHTTP_FLAG_SECURE));if(!request.p)return false;
  DWORD policy=WINHTTP_OPTION_REDIRECT_POLICY_NEVER,autologon=WINHTTP_AUTOLOGON_SECURITY_LEVEL_HIGH;
  if(!WinHttpSetOption(request.p,WINHTTP_OPTION_REDIRECT_POLICY,&policy,sizeof(policy))||
     !WinHttpSetOption(request.p,WINHTTP_OPTION_AUTOLOGON_POLICY,&autologon,sizeof(autologon))||
     !WinHttpSendRequest(request.p,L"Cache-Control: no-cache\r\n",DWORD(-1),WINHTTP_NO_REQUEST_DATA,0,0,0)||!WinHttpReceiveResponse(request.p,nullptr))return false;
  DWORD status=0,size=sizeof(status);
  if(!WinHttpQueryHeaders(request.p,WINHTTP_QUERY_STATUS_CODE|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&status,&size,WINHTTP_NO_HEADER_INDEX))return false;
  if(status==301||status==302||status==303||status==307||status==308){
   size=0;WinHttpQueryHeaders(request.p,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,nullptr,&size,WINHTTP_NO_HEADER_INDEX);
   if(!size||size>32768)return false;std::vector<wchar_t> location(size/sizeof(wchar_t)+1);
   if(!WinHttpQueryHeaders(request.p,WINHTTP_QUERY_LOCATION,WINHTTP_HEADER_NAME_BY_INDEX,location.data(),&size,WINHTTP_NO_HEADER_INDEX))return false;
   url=location.data();continue;
  }
  if(status!=200)return false;
  DWORD length=0;size=sizeof(length);
  bool known=WinHttpQueryHeaders(request.p,WINHTTP_QUERY_CONTENT_LENGTH|WINHTTP_QUERY_FLAG_NUMBER,WINHTTP_HEADER_NAME_BY_INDEX,&length,&size,WINHTTP_NO_HEADER_INDEX)!=FALSE;
  if((known&&(!length||length>limit))||(head&&!known))return false;
  if(head)return GetTickCount64()<deadline;
  char buffer[8192];DWORD read=0;
  do{
   if(GetTickCount64()>=deadline||!WinHttpReadData(request.p,buffer,sizeof(buffer),&read)||body.size()+read>limit)return false;
   body.append(buffer,read);
  }while(read);
  return !body.empty();
 }
 return false;
}
}
bool allowedUpdateRedirect(const std::wstring& url,const std::wstring& original){
 URL_COMPONENTS parts{};parts.dwStructSize=sizeof(parts);parts.dwHostNameLength=parts.dwUserNameLength=parts.dwPasswordLength=DWORD(-1);
 if(!WinHttpCrackUrl(url.c_str(),0,0,&parts)||parts.nScheme!=INTERNET_SCHEME_HTTPS||parts.nPort!=443||parts.dwUserNameLength||parts.dwPasswordLength||url.find(L'#')!=std::wstring::npos)return false;
 std::wstring host(parts.lpszHostName,parts.dwHostNameLength);
 if(host==L"github.com")return url==original;
 return host==L"release-assets.githubusercontent.com"||host==L"objects.githubusercontent.com";
}
uint64_t checkUpdate(const std::string& current,const UpdateRequest& request){
 ReleaseVersion installed;if(!ReleaseVersion::parse(current,installed))return 0;
 std::string body;std::vector<Release> releases;
 if(!request(std::wstring(ReleaseRoot)+L".atom",false,512*1024,body)||!atomReleases(body,releases))return 0;
 std::sort(releases.begin(),releases.end(),[](const Release&a,const Release&b){return a.version.parts==b.version.parts?a.stable>b.stable:a.version>b.version;});
 if(releases.empty()||!(releases.front().version>installed))return 0;
 const auto& latest=releases.front();std::wstring version;for(char c:latest.version.text())version.push_back(wchar_t(c));
 auto prefix=std::wstring(ReleaseRoot)+L"/download/"+latest.tag+L"/StarSketch-"+version;
 if(!request(prefix+L"-installer.zip",true,128*1024*1024,body)||!request(prefix+L"-installer-manifest.json",true,64*1024,body))return 0;
 return latest.version.packed();
}
uint64_t fetchAvailableUpdate(const std::string& current){
 uint64_t result=0;HRESULT com=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
 try{
  std::wstring agent=L"StarSketch-UpdateNotice/";for(const char* c=CurrentVersion;*c;++c)agent.push_back(wchar_t(*c));
  HttpOwner session(WinHttpOpen(agent.c_str(),WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,WINHTTP_NO_PROXY_NAME,WINHTTP_NO_PROXY_BYPASS,0));
  if(session.p&&WinHttpSetTimeouts(session.p,3000,4000,3000,4000)){
   DWORD tls=WINHTTP_FLAG_SECURE_PROTOCOL_TLS1_2;
   if(WinHttpSetOption(session.p,WINHTTP_OPTION_SECURE_PROTOCOLS,&tls,sizeof(tls))){
    uint64_t deadline=GetTickCount64()+30000;
    result=checkUpdate(current,[&](const std::wstring& url,bool head,size_t limit,std::string& body){return fetchUpdate(session.p,url,head,limit,body,deadline);});
   }
  }
 }catch(...){} // Background checks never affect drawing or display network errors.
 if(SUCCEEDED(com))CoUninitialize();return result;
}
unsigned long __stdcall RuntimeUpdateChecker::worker(void* context){
 auto work=static_cast<UpdateWork*>(context);auto self=work->checker;HMODULE module=work->module;delete work;
 self->available.store(fetchAvailableUpdate(CurrentVersion));self->running.store(false);
 FreeLibraryAndExitThread(module,0);return 0;
}
uint64_t RuntimeUpdateChecker::poll(uint64_t now){
 if(!running.load()&&schedule.due(now)){
  // Hold a DLL reference until the background thread exits; never wait in DllMain.
  HMODULE module=nullptr;
  if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&RuntimeUpdateChecker::worker),&module)){
   auto work=new(std::nothrow) UpdateWork{this,module};
   if(work){running.store(true);HANDLE thread=CreateThread(nullptr,0,&RuntimeUpdateChecker::worker,work,0,nullptr);
    if(thread)CloseHandle(thread);else{available.store(0);running.store(false);delete work;FreeLibrary(module);}
   }else{available.store(0);FreeLibrary(module);}
  }else available.store(0);
 }
 return available.load();
}
}
