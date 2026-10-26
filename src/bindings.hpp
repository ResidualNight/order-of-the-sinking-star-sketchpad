#pragma once
#include "imgui.h"
#include <array>
#include <string>

namespace sketch {
enum class Action {Toggle,Pen,Arrow,Pin,Number,Grid,Tint,Calibrate,Clear,Undo,Redo,Erase,RestartNumber,Count};
inline constexpr const char* ActionLabels[]={"进入 / 退出绘画","自由画笔","箭头","星标","编号","网格与吸附","棋盘染色","临时选择相机","清空标记","撤销","重做","按住擦除","编号从 1 开始"};
inline constexpr const char* ActionNames[]={"toggle","pen","arrow","pin","number","grid","tint","calibrate","clear","undo","redo","erase","restart_number"};
struct KeyBinding {ImGuiKey key;ImGuiKeyChord mods=0;ImGuiKey companion=ImGuiKey_None;};
inline bool operator==(KeyBinding a,KeyBinding b){return a.mods==b.mods&&((a.key==b.key&&a.companion==b.companion)||(a.companion!=ImGuiKey_None&&a.key==b.companion&&a.companion==b.key));}
// Shared by native polling and the capture UI; unsupported/system keys cannot be bound.
struct KeyboardKey {int vk;ImGuiKey key;};
inline constexpr KeyboardKey KeyboardKeys[]={
 {0x30,ImGuiKey_0},{0x31,ImGuiKey_1},{0x32,ImGuiKey_2},{0x33,ImGuiKey_3},{0x34,ImGuiKey_4},{0x35,ImGuiKey_5},{0x36,ImGuiKey_6},{0x37,ImGuiKey_7},{0x38,ImGuiKey_8},{0x39,ImGuiKey_9},
 {0x41,ImGuiKey_A},{0x42,ImGuiKey_B},{0x43,ImGuiKey_C},{0x44,ImGuiKey_D},{0x45,ImGuiKey_E},{0x46,ImGuiKey_F},{0x47,ImGuiKey_G},{0x48,ImGuiKey_H},{0x49,ImGuiKey_I},{0x4a,ImGuiKey_J},{0x4b,ImGuiKey_K},{0x4c,ImGuiKey_L},{0x4d,ImGuiKey_M},{0x4e,ImGuiKey_N},{0x4f,ImGuiKey_O},{0x50,ImGuiKey_P},{0x51,ImGuiKey_Q},{0x52,ImGuiKey_R},{0x53,ImGuiKey_S},{0x54,ImGuiKey_T},{0x55,ImGuiKey_U},{0x56,ImGuiKey_V},{0x57,ImGuiKey_W},{0x58,ImGuiKey_X},{0x59,ImGuiKey_Y},{0x5a,ImGuiKey_Z},
 {0x70,ImGuiKey_F1},{0x71,ImGuiKey_F2},{0x72,ImGuiKey_F3},{0x73,ImGuiKey_F4},{0x74,ImGuiKey_F5},{0x75,ImGuiKey_F6},{0x76,ImGuiKey_F7},{0x77,ImGuiKey_F8},{0x78,ImGuiKey_F9},{0x79,ImGuiKey_F10},{0x7a,ImGuiKey_F11},{0x7b,ImGuiKey_F12},
 {0x09,ImGuiKey_Tab},{0x20,ImGuiKey_Space},{0x08,ImGuiKey_Backspace},{0x0d,ImGuiKey_Enter},{0x1b,ImGuiKey_Escape},
 {0x25,ImGuiKey_LeftArrow},{0x26,ImGuiKey_UpArrow},{0x27,ImGuiKey_RightArrow},{0x28,ImGuiKey_DownArrow},
 {0x21,ImGuiKey_PageUp},{0x22,ImGuiKey_PageDown},{0x23,ImGuiKey_End},{0x24,ImGuiKey_Home},{0x2d,ImGuiKey_Insert},{0x2e,ImGuiKey_Delete},
 {0xba,ImGuiKey_Semicolon},{0xbb,ImGuiKey_Equal},{0xbc,ImGuiKey_Comma},{0xbd,ImGuiKey_Minus},{0xbe,ImGuiKey_Period},{0xbf,ImGuiKey_Slash},{0xc0,ImGuiKey_GraveAccent},{0xdb,ImGuiKey_LeftBracket},{0xdc,ImGuiKey_Backslash},{0xdd,ImGuiKey_RightBracket},{0xde,ImGuiKey_Apostrophe}
};
class Bindings {
 std::array<KeyBinding,static_cast<size_t>(Action::Count)> values{};
 std::array<bool,static_cast<size_t>(Action::Count)> fired{},pending{};
 std::array<bool,ImGuiKey_NamedKey_COUNT> consumed{};
 static size_t index(ImGuiKey key){return size_t(key-ImGuiKey_NamedKey_BEGIN);}
 bool sharedPrefix(KeyBinding binding)const{for(auto other:values)if(other.companion!=ImGuiKey_None&&other.mods==binding.mods&&(other.key==binding.key||other.companion==binding.key))return true;return false;}
 void clearInput(){fired.fill(false);pending.fill(false);consumed.fill(false);}
public:
 Bindings(){reset();}
 void reset(){values={{{ImGuiKey_E},{ImGuiKey_B},{ImGuiKey_A},{ImGuiKey_P},{ImGuiKey_N},{ImGuiKey_G},{ImGuiKey_T},{ImGuiKey_G,ImGuiMod_Shift},{ImGuiKey_C},{ImGuiKey_Z,ImGuiMod_Ctrl},{ImGuiKey_Y,ImGuiMod_Ctrl},{ImGuiKey_MouseRight},{ImGuiKey_C,0,ImGuiKey_N}}};clearInput();}
 KeyBinding get(Action action)const{return values[static_cast<size_t>(action)];}
 static bool supported(Action action,KeyBinding binding){
  if(binding.mods&~(ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiMod_Shift))return false;
  auto keyboard=[](ImGuiKey candidate){if(candidate==ImGuiKey_Escape)return false;for(auto key:KeyboardKeys)if(key.key==candidate)return true;return false;};
  if(binding.companion!=ImGuiKey_None&&(!keyboard(binding.companion)||binding.companion==binding.key))return false;
  if(action==Action::Erase&&binding.companion==ImGuiKey_None&&(binding.key==ImGuiKey_MouseRight||binding.key==ImGuiKey_MouseMiddle))return true;
  return keyboard(binding.key);
 }
 int conflict(Action action,KeyBinding binding)const{
  for(size_t i=0;i<values.size();++i)if(i!=static_cast<size_t>(action)){
   auto other=values[i];if(other==binding)return int(i);
   // A held eraser cannot also serve as the prefix of a two-key action.
   if((action==Action::Erase||i==size_t(Action::Erase))&&other.mods==binding.mods){
    if(binding.companion!=ImGuiKey_None&&other.companion==ImGuiKey_None&&(binding.key==other.key||binding.companion==other.key))return int(i);
    if(other.companion!=ImGuiKey_None&&binding.companion==ImGuiKey_None&&(other.key==binding.key||other.companion==binding.key))return int(i);
   }
  }
  return -1;
 }
 bool assign(Action action,KeyBinding binding){if(!supported(action,binding)||conflict(action,binding)>=0)return false;values[static_cast<size_t>(action)]=binding;clearInput();return true;}
 bool restore(const std::array<KeyBinding,static_cast<size_t>(Action::Count)>& restored){
  auto old=values;values=restored;for(size_t i=0;i<values.size();++i)if(!supported(static_cast<Action>(i),values[i])||conflict(static_cast<Action>(i),values[i])>=0){values=old;return false;}clearInput();return true;
 }
 std::string label(Action action)const{
  auto b=get(action);std::string text;
  if(b.mods&ImGuiMod_Ctrl)text+="Ctrl+";if(b.mods&ImGuiMod_Alt)text+="Alt+";if(b.mods&ImGuiMod_Shift)text+="Shift+";
  if(b.companion!=ImGuiKey_None){text+=ImGui::GetKeyName(b.companion);text+="+";}
  text+=b.key==ImGuiKey_MouseRight?"右键":b.key==ImGuiKey_MouseMiddle?"中键":ImGui::GetKeyName(b.key);return text;
 }
 void update(){
  fired.fill(false);auto mods=ImGui::GetIO().KeyMods;
  for(size_t i=0;i<values.size();++i){auto b=values[i];if(b.companion!=ImGuiKey_None&&mods==b.mods&&ImGui::IsKeyDown(b.key)&&ImGui::IsKeyDown(b.companion)){
   fired[i]=ImGui::IsKeyPressed(b.key,false)||ImGui::IsKeyPressed(b.companion,false);consumed[index(b.key)]=consumed[index(b.companion)]=true;
  }}
  for(size_t i=0;i<values.size();++i){auto b=values[i];if(b.companion!=ImGuiKey_None)continue;
   if(sharedPrefix(b)){
    if(mods==b.mods&&ImGui::IsKeyPressed(b.key,false))pending[i]=true;
    if(ImGui::IsKeyReleased(b.key)){fired[i]=pending[i]&&!consumed[index(b.key)];pending[i]=false;}
   }else fired[i]=mods==b.mods&&ImGui::IsKeyPressed(b.key,false);
  }
  // Keep consumption through the release frame so releasing C after N+C never clears marks.
  for(int k=ImGuiKey_NamedKey_BEGIN;k<ImGuiKey_NamedKey_END;++k)if(!ImGui::IsKeyDown(static_cast<ImGuiKey>(k)))consumed[index(static_cast<ImGuiKey>(k))]=false;
 }
 bool active(Action action,bool held=false)const{
  if(!held)return fired[static_cast<size_t>(action)];auto b=get(action);
  return ImGui::GetIO().KeyMods==b.mods&&ImGui::IsKeyDown(b.key)&&(b.companion==ImGuiKey_None||ImGui::IsKeyDown(b.companion));
 }
};
}
