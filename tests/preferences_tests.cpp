#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include "preferences.hpp"
#include <cassert>
#include <filesystem>
#include <fstream>
using namespace sketch;
namespace {
std::string read(const std::filesystem::path& path){std::ifstream file(path,std::ios::binary);return {std::istreambuf_iterator<char>(file),std::istreambuf_iterator<char>()};}
void put(const std::filesystem::path& path,const std::string& text){std::ofstream file(path,std::ios::binary);file<<text;assert(file.good());}
}
void checkPreferences(){
 App edited;assert(!edited.autoClearOnLevel);edited.autoClearOnLevel=true;edited.gridEnabled=edited.tint=true;edited.colorIndex=4;edited.colors[3]=IM_COL32(20,40,70,255);edited.colors[4]=IM_COL32(140,120,100,255);edited.penWidth=6.5f;edited.eraseRadius=23;edited.pinRadius=40;edited.numberRadius=33;edited.markOpacity=.65f;edited.tintOpacity=.25f;
 edited.gridDirections[2]=1;edited.gridDirections[4]=3;edited.gridDirections[6]=2;
 assert(edited.bindings.assign(Action::Pen,{ImGuiKey_X}));assert(edited.bindings.assign(Action::Arrow,{ImGuiKey_B}));assert(edited.bindings.assign(Action::Pen,{ImGuiKey_A}));
 assert(edited.bindings.assign(Action::RestartNumber,{ImGuiKey_V,ImGuiMod_Shift,ImGuiKey_R}));
 Mark mark;mark.kind=Kind::Number;mark.points={{12345,6789}};edited.document.add(mark);edited.grid.valid=true;edited.grid.origin={12345,6789};
 edited.gridPinned=edited.pickerPreviousPinned=true;edited.pickerPreviousGrid=edited.grid;edited.cameraPreviewAdoptions=17;
 auto encoded=encodePreferences(edited);assert(encoded.find("12345")==std::string::npos&&encoded.find("6789")==std::string::npos);
 App restored;restored.document.add(mark);assert(decodePreferences(encoded,restored));assert(encodePreferences(restored)==encoded&&restored.document.marks.size()==1&&!restored.grid.valid);
 assert(restored.gridDirections==edited.gridDirections&&!restored.gameGridBase.valid&&!restored.gridDirectionPending);
 assert(!restored.gridPinned&&!restored.pickerPreviousPinned&&!restored.pickerPreviousGrid.valid&&restored.cameraPreviewAdoptions==0);
 auto reject=[&](std::string text){auto before=encodePreferences(restored);assert(!decodePreferences(text,restored)&&encodePreferences(restored)==before&&restored.document.marks.size()==1);};
 reject(encoded+"auto_clear=0\n");reject("format=2\n");reject("format=1\nauto_clear=2\n");reject("format=1\nmark_opacity=nan\n");reject("format=1\npen_width=1000\n");reject("format=1\ncustom1=123\n");reject("format=1\nkey.pen=999,0,0\n");reject("format=1\nkey.pen=65,0,0\nkey.arrow=65,0,0\n");reject("format=1\nkey.erase=78,0,0\n");reject(std::string(17000,'x'));
 App defaults;assert(decodePreferences("format=1\nfuture_setting=ignored\n",defaults)&&!defaults.autoClearOnLevel);
 assert(defaults.gridDirections[0]==0&&defaults.gridDirections[6]==-1);
 reject("format=1\ngrid_direction.6=4\n");reject("format=1\ngrid_direction.6=-2\n");reject("format=1\ngrid_direction.6=nan\n");
 wchar_t exe[32768];assert(GetModuleFileNameW(nullptr,exe,32768));auto root=std::filesystem::path(exe).parent_path().parent_path()/L"work"/L"preferences-tests"/(std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64()));std::filesystem::create_directories(root);
 auto path=root/L"中文 设置"/L"settings.ini";PreferenceStore store(path.wstring());App app;assert(store.load(app)==ErrorId::None);
 assert(decodePreferences(encoded,app));assert(store.update(app,0)==ErrorId::None&&!std::filesystem::exists(path));assert(store.update(app,499)==ErrorId::None&&!std::filesystem::exists(path));assert(store.update(app,500)==ErrorId::None&&read(path)==encoded);
 PreferenceStore reopened(path.wstring());App next;assert(reopened.load(next)==ErrorId::None&&encodePreferences(next)==encoded&&next.document.marks.empty()&&!next.grid.valid);
 auto original=read(path);assert(SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_READONLY));next.penWidth=8;
 assert(reopened.update(next,1000,true)==ErrorId::ConfigWriteFailure&&read(path)==original);
 assert(SetFileAttributesW(path.c_str(),FILE_ATTRIBUTE_NORMAL));assert(reopened.update(next,7000,true)==ErrorId::None&&read(path)==encodePreferences(next));
 auto bad=root/L"bad.ini";std::string broken="format=1\nauto_clear=oops\n";put(bad,broken);PreferenceStore recovery(bad.wstring());App safe;
 assert(recovery.load(safe)==ErrorId::ConfigReadFailure&&!safe.autoClearOnLevel);assert(recovery.update(safe,0,true)==ErrorId::None&&read(bad)==broken);
 safe.penWidth=5;assert(recovery.update(safe,1,true)==ErrorId::None);bool backup=false;
 for(auto entry:std::filesystem::directory_iterator(root))if(entry.path().filename().wstring().find(L"bad.ini.invalid-")==0){assert(read(entry.path())==broken);backup=true;}assert(backup&&read(bad)==encodePreferences(safe));
}
