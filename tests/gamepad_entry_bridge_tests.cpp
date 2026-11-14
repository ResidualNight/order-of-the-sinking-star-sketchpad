#include "gamepad_bridge.hpp"
#include <cassert>
#include <cstdio>
using namespace sketch::pad;
using Set=void(WINAPI*)(DWORD,const XINPUT_STATE*,DWORD);
int main(int argc,char**argv){
 assert(argc==2);auto module=LoadLibraryA(argv[1]);assert(module);auto setter=reinterpret_cast<Set>(GetProcAddress(module,"FixtureState"));assert(setter);assert(install(GetModuleHandleW(nullptr)).installed);auto game=reinterpret_cast<GetState>(GetProcAddress(module,"XInputGetState"));assert(game);
 XINPUT_STATE raw{},out{};raw.dwPacketNumber=17;for(unsigned i=0;i<4;++i)setter(i,&raw,0);
 enableEntry(true);setter(3,&raw,ERROR_DEVICE_NOT_CONNECTED);assert(game(3,&out)==ERROR_DEVICE_NOT_CONNECTED);raw.Gamepad.wButtons=XINPUT_GAMEPAD_A;raw.Gamepad.sThumbLX=20000;setter(3,&raw,0);assert(game(3,&out)==0&&out.Gamepad.wButtons==XINPUT_GAMEPAD_A&&out.Gamepad.sThumbLX==20000);raw.Gamepad={};assert(game(2,&out)==0);raw.Gamepad.bLeftTrigger=220;setter(2,&raw,0);assert(game(2,&out)==0&&out.Gamepad.bLeftTrigger==0&&!openRequest());
 XINPUT_STATE sampled{};assert(sample(0,2,&sampled)==0&&sampled.Gamepad.bLeftTrigger==220);assert(!openRequest());
 raw.Gamepad.bRightTrigger=220;raw.Gamepad.wButtons=XINPUT_GAMEPAD_A;setter(2,&raw,0);assert(game(2,&out)==0&&out.Gamepad.wButtons==0&&out.Gamepad.bRightTrigger==0);assert(openRequest()==3);
 setter(1,&raw,0);assert(game(1,&out)==0&&out.Gamepad.wButtons==0&&out.Gamepad.bRightTrigger==0);assert(sample(0,2,&sampled)==0&&sampled.Gamepad.wButtons==XINPUT_GAMEPAD_A);
 setCapturing(true);acknowledgeOpen();enableEntry(false);assert(capturing()&&!openRequest());assert(game(2,&out)==0&&out.Gamepad.wButtons==0);
 setCapturing(false);assert(game(2,&out)==0&&out.Gamepad.wButtons==0&&out.Gamepad.bLeftTrigger==0);raw.Gamepad={};setter(2,&raw,0);assert(game(2,&out)==0);raw.Gamepad.wButtons=XINPUT_GAMEPAD_A;setter(2,&raw,0);assert(game(2,&out)==0&&out.Gamepad.wButtons==XINPUT_GAMEPAD_A);
 failOpen();enableEntry(true);raw.Gamepad={};setter(0,&raw,0);assert(game(0,&out)==0);raw.Gamepad.bRightTrigger=255;setter(0,&raw,0);assert(game(0,&out)==0&&out.Gamepad.bRightTrigger==0);raw.Gamepad={};setter(0,&raw,0);assert(game(0,&out)==0&&out.Gamepad.bRightTrigger==0);Sleep(190);assert(sample(0,0,&sampled)==0&&sampled.Gamepad.bRightTrigger==0);assert(game(0,&out)==0&&out.Gamepad.bRightTrigger==255);auto packet=out.dwPacketNumber;assert(game(0,&out)==0&&out.Gamepad.bRightTrigger==0&&packet!=out.dwPacketNumber);
 enableEntry(false);raw.Gamepad.bLeftTrigger=raw.Gamepad.bRightTrigger=255;setter(0,&raw,0);assert(game(0,&out)==0&&!openRequest());failOpen();assert(remove());std::puts("trigger bridge: actual-query entry, raw poll independence, immediate all-slot isolation, release fences and delayed tap packet transitions passed");
}
