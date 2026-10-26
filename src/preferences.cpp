#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "preferences.hpp"
#include <iomanip>
#include <locale>
#include <map>
#include <sstream>
#include <vector>
namespace sketch {namespace {
int keyCode(ImGuiKey key){if(key==ImGuiKey_None)return 0;if(key==ImGuiKey_MouseRight)return 2;if(key==ImGuiKey_MouseMiddle)return 4;for(auto entry:KeyboardKeys)if(entry.key==key)return entry.vk;return -1;}
ImGuiKey fromCode(int code){if(code==0)return ImGuiKey_None;if(code==2)return ImGuiKey_MouseRight;if(code==4)return ImGuiKey_MouseMiddle;for(auto entry:KeyboardKeys)if(entry.vk==code)return entry.key;return ImGuiKey_None;}
int modCode(ImGuiKeyChord mods){return (mods&ImGuiMod_Ctrl?1:0)|(mods&ImGuiMod_Alt?2:0)|(mods&ImGuiMod_Shift?4:0);}
ImGuiKeyChord fromMods(int mods){return (mods&1?ImGuiMod_Ctrl:0)|(mods&2?ImGuiMod_Alt:0)|(mods&4?ImGuiMod_Shift:0);}
template<class T> bool number(const std::string& text,T& value){std::istringstream in(text);in.imbue(std::locale::classic());in>>value;if(!in)return false;in>>std::ws;return in.eof();}
bool readFile(const std::wstring& path,std::string& text){
 HANDLE file=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 LARGE_INTEGER length{};bool ok=GetFileSizeEx(file,&length)&&length.QuadPart>0&&length.QuadPart<=16384;
 if(ok){text.resize(size_t(length.QuadPart));DWORD done=0;ok=ReadFile(file,text.data(),DWORD(text.size()),&done,nullptr)&&done==text.size();}CloseHandle(file);return ok;
}
}
std::string encodePreferences(const App& app){
 std::ostringstream out;out.imbue(std::locale::classic());out<<std::setprecision(9)<<"format=1\nauto_clear="<<app.autoClearOnLevel<<"\ngrid="<<app.gridEnabled<<"\ntint="<<app.tint<<"\ncolor="<<app.colorIndex<<"\ncustom1="<<app.colors[3]<<"\ncustom2="<<app.colors[4]<<"\npen_width="<<app.penWidth<<"\nerase_radius="<<app.eraseRadius<<"\npin_radius="<<app.pinRadius<<"\nnumber_radius="<<app.numberRadius<<"\nmark_opacity="<<app.markOpacity<<"\ntint_opacity="<<app.tintOpacity<<'\n';
 for(size_t i=0;i<app.gridDirections.size();++i)out<<"grid_direction."<<i<<'='<<app.gridDirections[i]<<'\n';
 for(int i=0;i<int(Action::Count);++i){auto b=app.bindings.get(static_cast<Action>(i));out<<"key."<<ActionNames[i]<<'='<<keyCode(b.key)<<','<<modCode(b.mods)<<','<<keyCode(b.companion)<<'\n';}return out.str();
}
bool decodePreferences(const std::string& text,App& app){
 if(text.size()>16384)return false;std::istringstream input(text);std::string line;std::map<std::string,std::string> entries;
 while(std::getline(input,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();if(entries.empty()&&line.compare(0,3,"\xef\xbb\xbf")==0)line.erase(0,3);if(line.empty()||line[0]=='#')continue;auto eq=line.find('=');if(eq==std::string::npos||!entries.emplace(line.substr(0,eq),line.substr(eq+1)).second)return false;}
 if(entries["format"]!="1")return false;
 App values; // Validate an independent set, then apply atomically without touching live session state.
 auto integer=[&](const char* name,int& field,int lo,int hi){auto it=entries.find(name);if(it==entries.end())return true;int v=0;if(!number(it->second,v)||v<lo||v>hi)return false;field=v;return true;};
 auto boolean=[&](const char* name,bool& field){int v=field?1:0;if(!integer(name,v,0,1))return false;field=v!=0;return true;};
 auto scalar=[&](const char* name,float& field,float lo,float hi){auto it=entries.find(name);if(it==entries.end())return true;float v=0;if(!number(it->second,v)||!std::isfinite(v)||v<lo||v>hi)return false;field=v;return true;};
 if(!boolean("auto_clear",values.autoClearOnLevel)||!boolean("grid",values.gridEnabled)||!boolean("tint",values.tint)||!integer("color",values.colorIndex,0,4)||!scalar("pen_width",values.penWidth,1,12)||!scalar("erase_radius",values.eraseRadius,5,60)||!scalar("pin_radius",values.pinRadius,16,48)||!scalar("number_radius",values.numberRadius,16,48)||!scalar("mark_opacity",values.markOpacity,.2f,.95f)||!scalar("tint_opacity",values.tintOpacity,.04f,.5f))return false;
 for(size_t i=0;i<values.gridDirections.size();++i){auto key="grid_direction."+std::to_string(i);if(!integer(key.c_str(),values.gridDirections[i],-1,3))return false;}
 for(int i=3;i<5;++i){auto it=entries.find(i==3?"custom1":"custom2");if(it!=entries.end()){uint64_t c=0;if(!number(it->second,c)||c>UINT32_MAX||(c>>24)!=255)return false;values.colors[i]=uint32_t(c);}}
 std::array<KeyBinding,size_t(Action::Count)> bindings{};
 for(int i=0;i<int(Action::Count);++i){bindings[i]=values.bindings.get(static_cast<Action>(i));auto it=entries.find(std::string("key.")+ActionNames[i]);if(it==entries.end())continue;std::string data=it->second;for(auto&c:data)if(c==',')c=' ';std::istringstream in(data);int key=0,mods=0,companion=0;if(!(in>>key>>mods>>companion)||mods<0||mods>7||keyCode(fromCode(key))!=key||keyCode(fromCode(companion))!=companion)return false;in>>std::ws;if(!in.eof())return false;bindings[i]={fromCode(key),fromMods(mods),fromCode(companion)};}
 if(!values.bindings.restore(bindings))return false;
 app.autoClearOnLevel=values.autoClearOnLevel;app.gridEnabled=values.gridEnabled;app.tint=values.tint;app.gridDirections=values.gridDirections;app.colorIndex=values.colorIndex;app.colors=values.colors;app.penWidth=values.penWidth;app.eraseRadius=values.eraseRadius;app.pinRadius=values.pinRadius;app.numberRadius=values.numberRadius;app.markOpacity=values.markOpacity;app.tintOpacity=values.tintOpacity;app.bindings=values.bindings;return true;
}
bool PreferenceStore::resolvePath(){
 if(!path.empty())return true;DWORD size=GetEnvironmentVariableW(L"LOCALAPPDATA",nullptr,0);if(!size||size>32700)return false;std::vector<wchar_t> buffer(size);if(!GetEnvironmentVariableW(L"LOCALAPPDATA",buffer.data(),size))return false;path=std::wstring(buffer.data())+L"\\StarSketch\\settings.ini";return true;
}
ErrorId PreferenceStore::load(App& app){
 ErrorId result=ErrorId::None;
 if(!resolvePath())result=ErrorId::ConfigReadFailure;
 else{DWORD attributes=GetFileAttributesW(path.c_str());if(attributes!=INVALID_FILE_ATTRIBUTES){std::string data;if(!readFile(path,data)||!decodePreferences(data,app)){invalidOriginal=true;result=ErrorId::ConfigReadFailure;}}else if(GetLastError()!=ERROR_FILE_NOT_FOUND&&GetLastError()!=ERROR_PATH_NOT_FOUND)result=ErrorId::ConfigReadFailure;}
 saved=lastSeen=encodePreferences(app);initialized=true;app.preferencesStatus=result==ErrorId::None?"设置与键位会自动保存，重启游戏后恢复":errorMessage(result);return result;
}
bool PreferenceStore::write(const std::string& data){
 if(!resolvePath())return false;auto slash=path.find_last_of(L"\\/");if(slash==std::wstring::npos)return false;auto directory=path.substr(0,slash);
 if(!CreateDirectoryW(directory.c_str(),nullptr)&&GetLastError()!=ERROR_ALREADY_EXISTS)return false;
 if(invalidOriginal){WIN32_FILE_ATTRIBUTE_DATA info{};if(!GetFileAttributesExW(path.c_str(),GetFileExInfoStandard,&info)||info.nFileSizeHigh||info.nFileSizeLow>16384)return false;auto backup=path+L".invalid-"+std::to_wstring(GetTickCount64());if(!CopyFileW(path.c_str(),backup.c_str(),TRUE))return false;invalidOriginal=false;}
 auto temporary=path+L".tmp-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());HANDLE file=CreateFileW(temporary.c_str(),GENERIC_WRITE,0,nullptr,CREATE_NEW,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return false;
 DWORD done=0;bool ok=WriteFile(file,data.data(),DWORD(data.size()),&done,nullptr)&&done==data.size()&&FlushFileBuffers(file);CloseHandle(file);
 if(ok)ok=MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=0;
 if(!ok)DeleteFileW(temporary.c_str());return ok;
}
ErrorId PreferenceStore::update(App& app,uint64_t now,bool flush){
 if(!initialized)return ErrorId::None;auto encoded=encodePreferences(app);if(encoded!=lastSeen){lastSeen=encoded;lastChange=now;retryAfter=0;}
 if(encoded==saved||(!flush&&(now-lastChange<500||now<retryAfter)))return ErrorId::None;
 if(!write(encoded)){retryAfter=now+5000;app.preferencesStatus=errorMessage(ErrorId::ConfigWriteFailure);return ErrorId::ConfigWriteFailure;}
 saved=encoded;app.preferencesStatus="设置已保存，重启游戏后恢复";return ErrorId::None;
}
}
