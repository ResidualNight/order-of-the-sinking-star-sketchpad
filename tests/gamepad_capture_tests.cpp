#include "gamepad_capture.hpp"
#include <cstdio>
#include <cstdlib>
using namespace sketch::pad;
#define CHECK(x) do{if(!(x)){std::fprintf(stderr,"FAIL line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
static XINPUT_STATE state(WORD buttons=0,DWORD packet=7) {XINPUT_STATE s{};s.dwPacketNumber=packet;s.Gamepad.wButtons=buttons;return s;}
static bool neutral(const XINPUT_STATE&s){auto v=s.Gamepad;return !v.wButtons&&!v.bLeftTrigger&&!v.bRightTrigger&&!v.sThumbLX&&!v.sThumbLY&&!v.sThumbRX&&!v.sThumbRY;}
int main(){
 {CaptureFilter f;auto s=state(XINPUT_GAMEPAD_A,123);CHECK(f.filter(0,0,s)==0);CHECK(s.Gamepad.wButtons==XINPUT_GAMEPAD_A&&s.dwPacketNumber==123);
  auto t=state(XINPUT_GAMEPAD_A,456);f.filter(0,0,t);CHECK(t.dwPacketNumber==456);
  f.setCapturing(true);s=state(XINPUT_GAMEPAD_A,456);f.filter(0,0,s);CHECK(neutral(s)&&s.dwPacketNumber!=456);
  DWORD n=s.dwPacketNumber;s=state(XINPUT_GAMEPAD_B,999);f.filter(0,0,s);CHECK(neutral(s)&&s.dwPacketNumber==n);
  f.setCapturing(false);s=state(XINPUT_GAMEPAD_B,999);f.filter(0,0,s);CHECK(neutral(s)&&s.dwPacketNumber==n);
  s=state(XINPUT_GAMEPAD_A|XINPUT_GAMEPAD_B,999);f.filter(0,0,s);CHECK(s.Gamepad.wButtons==XINPUT_GAMEPAD_A&&s.dwPacketNumber!=n);
  s=state();f.filter(0,0,s);s=state(XINPUT_GAMEPAD_B);f.filter(0,0,s);CHECK(s.Gamepad.wButtons==XINPUT_GAMEPAD_B);
 }
 {CaptureFilter f;f.setCapturing(true);f.setCapturing(false); // no intermediate query
  for(unsigned i=0;i<4;++i){auto s=state(XINPUT_GAMEPAD_X);s.Gamepad.bLeftTrigger=255;s.Gamepad.bRightTrigger=250;
   s.Gamepad.sThumbLX=-32768;s.Gamepad.sThumbRY=32767;f.filter(i,0,s);CHECK(neutral(s));
   auto zero=state();f.observe(i,0,zero);s=state(XINPUT_GAMEPAD_X);s.Gamepad.bLeftTrigger=255;s.Gamepad.sThumbLX=-32768;
   f.filter(i,0,s);CHECK(s.Gamepad.wButtons==XINPUT_GAMEPAD_X&&s.Gamepad.sThumbLX==-32768&&s.Gamepad.bLeftTrigger==255);
  }
 }
 {CaptureFilter f;f.setCapturing(true);f.setCapturing(false);auto s=state();s.Gamepad.sThumbLX=7000;s.Gamepad.sThumbLY=7000;
  s.Gamepad.bRightTrigger=31;f.filter(2,0,s);CHECK(neutral(s)); // circular, not square deadzone
  s=state();s.Gamepad.sThumbLX=100;s.Gamepad.bRightTrigger=30;f.observe(2,0,s);
  s=state();s.Gamepad.sThumbLX=16000;s.Gamepad.bRightTrigger=80;f.filter(2,0,s);CHECK(s.Gamepad.sThumbLX==16000&&s.Gamepad.bRightTrigger==80);
 }
 {CaptureFilter f;f.setCapturing(true);f.setCapturing(false);auto s=state(XINPUT_GAMEPAD_A);f.filter(0,0,s);auto before=s;
  CHECK(f.filter(0,ERROR_DEVICE_NOT_CONNECTED,s)==ERROR_DEVICE_NOT_CONNECTED);CHECK(s.Gamepad.wButtons==before.Gamepad.wButtons);
  s=state(XINPUT_GAMEPAD_A);f.filter(0,0,s);CHECK(neutral(s));s=state();f.filter(0,0,s);
  s=state(XINPUT_GAMEPAD_A);f.filter(0,0,s);CHECK(s.Gamepad.wButtons==XINPUT_GAMEPAD_A);
  f.setCapturing(true);s=state(XINPUT_GAMEPAD_B);CHECK(f.filter(0,ERROR_GEN_FAILURE,s)==ERROR_GEN_FAILURE&&s.Gamepad.wButtons==XINPUT_GAMEPAD_B);
  CHECK(f.filter(99,ERROR_BAD_ARGUMENTS,s)==ERROR_BAD_ARGUMENTS);
  f.failOpen();s=state(XINPUT_GAMEPAD_B);f.filter(0,0,s);CHECK(s.Gamepad.wButtons==XINPUT_GAMEPAD_B);
 }
 {CaptureFilter f;auto s=state(XINPUT_GAMEPAD_A,0xffffffff);f.filter(0,0,s);f.setCapturing(true);
  s=state(XINPUT_GAMEPAD_A,0xffffffff);f.filter(0,0,s);CHECK(s.dwPacketNumber==0&&neutral(s));
 }
 {CaptureFilter f;f.setCapturing(true);auto s=state(XINPUT_GAMEPAD_A,41);f.filter(0,0,s);CHECK(neutral(s)&&s.dwPacketNumber==42);
  s=state(XINPUT_GAMEPAD_A,41);f.filter(0,0,s);CHECK(s.dwPacketNumber==42);
 }
 {CaptureFilter f;auto s=state();f.filter(0,0,s,true,true,true);f.filter(0,ERROR_DEVICE_NOT_CONNECTED,s);
  s=state(XINPUT_GAMEPAD_A);s.Gamepad.sThumbLX=20000;f.filter(0,0,s);CHECK(s.Gamepad.wButtons==XINPUT_GAMEPAD_A&&s.Gamepad.sThumbLX==20000);
  f.setCapturing(true);f.failOpen();f.filter(0,ERROR_DEVICE_NOT_CONNECTED,s);s=state(XINPUT_GAMEPAD_B);f.filter(0,0,s);CHECK(s.Gamepad.wButtons==XINPUT_GAMEPAD_B);
 }
 std::puts("gamepad capture: 7 behavior groups passed");
}
