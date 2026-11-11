#include "controller_probe.hpp"
#include "gamepad_bridge.hpp"
#include "version.hpp"
#include "imgui.h"
#include <array>
#include <algorithm>
namespace sketch::probe {
namespace {
Log logger=nullptr;
pad::HookReport hook{};
pad::CachedHook cached=pad::CachedHook::Pending;
ULONGLONG cacheDue=0;
struct Slot {XINPUT_STATE state{};DWORD result=ERROR_DEVICE_NOT_CONNECTED;ULONGLONG due=0;};
std::array<std::array<Slot,4>,8> slots{};
void emit(const char* text){if(logger)logger(text);}
}
void start(Log log){
 logger=log;hook=pad::install(GetModuleHandleW(nullptr));
 emit(hook.installed?"Controller preview 7 initialized; LT+RT entry; capture defaults OFF":"[SS-INPUT-002] Controller consumer hook unavailable; capture OFF");
}
void close(){if(pad::capturing()){pad::setCapturing(false);emit("Controller isolation OFF; release fences active");}}
void emergency(){pad::failOpen();emit("Controller isolation emergency pass-through");}
void maintain(){
 auto tick=GetTickCount64();
 if(hook.installed&&tick>=cacheDue&&(cached==pad::CachedHook::Pending||cached==pad::CachedHook::Attached)){
  auto previous=cached;cached=pad::attachCachedGameState(GetModuleHandleW(nullptr));cacheDue=tick+1000;
  if(cached!=previous){
   if(cached==pad::CachedHook::Attached)emit("Controller cache attached: profile=steam-26249670 original chain preserved");
   else if(cached==pad::CachedHook::Unsupported)emit("[SS-INPUT-006] Controller cache profile did not validate; import bridge only");
   else if(cached!=pad::CachedHook::Pending)emit("[SS-INPUT-004] Controller cache unavailable or changed; capture unavailable");
  }
 }
}
void panel(bool drawing){
 maintain();
 if(!drawing){close();return;}
 auto status=pad::stats();static bool missingLogged=false;if(!status.gameCalls&&!missingLogged){emit("[SS-INPUT-006] No game XInput queries observed; capture remains unavailable");missingLogged=true;}bool connected=false;auto now=GetTickCount64();
 for(unsigned e=0;e<status.endpoints&&e<slots.size();++e)for(DWORD s=0;s<4;++s){
  auto& slot=slots[e][s];
  if(now>=slot.due){slot.result=pad::sample(e,s,&slot.state);slot.due=now+(slot.result==ERROR_SUCCESS?33:1000);}
  connected=connected||slot.result==ERROR_SUCCESS;
 }
 auto size=ImGui::GetIO().DisplaySize;float width=std::max(100.f,std::min(420.f,size.x-20)),height=std::max(100.f,std::min(310.f,size.y-40));
 ImGui::SetNextWindowSize({width,height},ImGuiCond_Always);
 ImGui::SetNextWindowPos({std::max(10.f,std::min(360.f,size.x-width-10)),20},ImGuiCond_FirstUseEver);
 if(ImGui::Begin("手柄隔离测试")){
  ImGui::Text("Probe 4 | 基础内核 %s",CurrentVersion);
  ImGui::TextWrapped("本版仅验证输入隔离，不支持手柄绘图。");
  ImGui::Text("已观测游戏查询: %llu | 入口: %u",static_cast<unsigned long long>(status.gameCalls),status.endpoints);
  ImGui::Text("已过滤非零输入: %llu",static_cast<unsigned long long>(status.neutralized));
  if(!hook.installed)ImGui::TextWrapped("[SS-INPUT-002] 手柄查询入口不可用");
  else if(status.degraded)ImGui::TextWrapped("[SS-INPUT-004] 手柄查询入口不可用");
  else if(!status.gameCalls)ImGui::TextWrapped("[SS-INPUT-006] 尚未捕获游戏查询，请先连接手柄并返回游戏操作；仍为零时请反馈日志。");
  else if(!connected)ImGui::TextWrapped("请先连接并操作手柄。");
  bool active=pad::capturing();
  ImGui::BeginDisabled(!active&&(!hook.installed||status.degraded||!status.gameCalls||!connected));
  if(ImGui::Button(active?"停止隔离":"开始隔离")){
   pad::setCapturing(!active);emit(active?"Controller isolation OFF; release fences active":"Controller isolation ON; user requested");
  }
  ImGui::EndDisabled();ImGui::SameLine();
  if(ImGui::Button("立即恢复游戏输入"))emergency();
  ImGui::TextWrapped("关闭草稿纸后结束隔离，仍按住的控件松开或回中后恢复。");
  for(unsigned e=0;e<status.endpoints&&e<slots.size();++e)for(unsigned s=0;s<4;++s){
   auto& slot=slots[e][s];if(slot.result!=ERROR_SUCCESS)continue;auto&v=slot.state.Gamepad;
   ImGui::Text("E%u Slot%u 按钮 %04X 扳机 %u / %u",e,s,unsigned(v.wButtons),unsigned(v.bLeftTrigger),unsigned(v.bRightTrigger));
   ImGui::Text("摇杆 %d,%d / %d,%d",v.sThumbLX,v.sThumbLY,v.sThumbRX,v.sThumbRY);
  }
 }
 ImGui::End();
}
}
