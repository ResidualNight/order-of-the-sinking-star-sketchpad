#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include <array>
#include <cstdint>

namespace sketch::pad {
// Serialized by the caller. No OS calls, allocation, or ImGui dependencies.
class CaptureFilter {
 struct Slot {
  bool fence=false, left=false, right=false, lt=false, rt=false;
  WORD buttons=0;
  bool haveOutput=false, managed=false, guardReconnect=false;
  DWORD packet=0;
  XINPUT_GAMEPAD output{};
 };
 std::array<Slot,4> slots{};
 bool capturing=false;
 static bool moved(SHORT x,SHORT y,int deadzone) {
  return int64_t(x)*x+int64_t(y)*y>int64_t(deadzone)*deadzone;
 }
 static bool equal(const XINPUT_GAMEPAD&a,const XINPUT_GAMEPAD&b) {
  return a.wButtons==b.wButtons&&a.bLeftTrigger==b.bLeftTrigger&&a.bRightTrigger==b.bRightTrigger
   &&a.sThumbLX==b.sThumbLX&&a.sThumbLY==b.sThumbLY&&a.sThumbRX==b.sThumbRX&&a.sThumbRY==b.sThumbRY;
 }
 void updateFence(Slot&s,const XINPUT_GAMEPAD&v) {
  bool left=moved(v.sThumbLX,v.sThumbLY,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);
  bool right=moved(v.sThumbRX,v.sThumbRY,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);
  bool lt=v.bLeftTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
  bool rt=v.bRightTrigger>XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
  if(s.fence) {
   s.buttons=v.wButtons;s.left=left;s.right=right;s.lt=lt;s.rt=rt;s.fence=false;
  } else {
   s.buttons&=v.wButtons;s.left=s.left&&left;s.right=s.right&&right;s.lt=s.lt&&lt;s.rt=s.rt&&rt;
  }
 }
public:
 void setCapturing(bool value) {
  if(capturing==value)return;
  capturing=value;
  for(auto&s:slots) {
   s.buttons=0;s.left=s.right=s.lt=s.rt=false;
   // Arm on the first fresh sample after exit, including an unpolled slot.
   s.fence=!value;s.managed=true;s.guardReconnect=true;
  }
 }
 bool isCapturing()const{return capturing;}
 // Failure recovery explicitly abandons capture and fences.
 void failOpen() {capturing=false;for(auto&s:slots){s.fence=false;s.guardReconnect=false;s.buttons=0;s.left=s.right=s.lt=s.rt=false;}}
 void observe(DWORD index,DWORD result,const XINPUT_STATE&raw) {
  if(index>=slots.size())return;
  auto&s=slots[index];
  if(result==ERROR_DEVICE_NOT_CONNECTED) {
   // Reconnect protection belongs to capture, not entry packet tracking.
   bool guard=s.guardReconnect||capturing; s=Slot{};s.fence=guard;s.managed=guard;s.guardReconnect=guard;return;
  }
  if(result!=ERROR_SUCCESS)return;
  if(!capturing)updateFence(s,raw.Gamepad);
 }
 DWORD filter(DWORD index,DWORD result,XINPUT_STATE&state,bool fresh=true,bool sameMode=true,bool transformed=false) {
  if(index>=slots.size())return result;
  if(fresh)observe(index,result,state);
  if(result!=ERROR_SUCCESS)return result; // Preserve downstream error and output.
  auto&s=slots[index];if(transformed)s.managed=true;auto value=state.Gamepad;
  if(capturing)value={};
  else if(!fresh)value=sameMode&&s.haveOutput?s.output:XINPUT_GAMEPAD{}; // Never replay an obsolete physical sample.
  else {
   value.wButtons&=WORD(~s.buttons);
   if(s.left)value.sThumbLX=value.sThumbLY=0;
   if(s.right)value.sThumbRX=value.sThumbRY=0;
   if(s.lt)value.bLeftTrigger=0;
   if(s.rt)value.bRightTrigger=0;
  }
  if(!s.haveOutput){s.packet=state.dwPacketNumber+((transformed||(s.managed&&!equal(value,state.Gamepad)))?1u:0u);s.haveOutput=true;}
  else if(!s.managed)s.packet=state.dwPacketNumber;
  else if(!equal(value,s.output))++s.packet;
  s.output=value;state.Gamepad=value;state.dwPacketNumber=s.packet;
  return result;
 }
};
}
