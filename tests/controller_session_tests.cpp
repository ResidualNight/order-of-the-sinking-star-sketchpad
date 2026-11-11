#include "controller_runtime.hpp"
#include "controller_probe.hpp"
#include "gamepad_bridge.hpp"
#include "ui.hpp"
#include <cassert>
#include <cstdio>
using namespace sketch;
using Set=void(WINAPI*)(DWORD,const XINPUT_STATE*,DWORD);
int main(int argc,char**argv){
 assert(argc==2);ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;io.ConfigInputTrickleEventQueue=false;io.DisplaySize={1920,1080};io.DeltaTime=1.f/60;unsigned char*p;int w,h;io.Fonts->GetTexDataAsRGBA32(&p,&w,&h);io.AddMousePosEvent(-100,-100);
 auto module=LoadLibraryA(argv[1]);assert(module);auto setter=reinterpret_cast<Set>(GetProcAddress(module,"FixtureState"));assert(setter);probe::start(nullptr);auto game=reinterpret_cast<pad::GetState>(GetProcAddress(module,"XInputGetState"));assert(game);
 XINPUT_STATE raw{},out{};for(unsigned i=0;i<4;++i)setter(i,&raw,i==2?0:ERROR_DEVICE_NOT_CONNECTED);
 App app;auto frame=[&](bool foreground=true){controller::poll(app,foreground);ImGui::NewFrame();app.frame(1920,1080);ImGui::Render();if(app.wantExit){app.wantExit=false;app.setEditing(false);}controller::synchronize(app,foreground);};
 frame();assert(!app.editing);assert(game(2,&out)==0);
 raw.Gamepad.bLeftTrigger=raw.Gamepad.bRightTrigger=220;setter(2,&raw,0);assert(game(2,&out)==0&&pad::openRequest()==3);frame();assert(app.editing&&app.padLayout&&app.padPointer&&!app.padReady&&pad::capturing()&&!pad::openRequest());assert(app.document.marks.empty());
 raw.Gamepad={};setter(2,&raw,0);frame();assert(app.padReady);app.gridRefresh.cancel();raw.Gamepad.bRightTrigger=255;setter(2,&raw,0);frame();assert(app.document.marks.size()==1);raw.Gamepad={};setter(2,&raw,0);frame();
 // A second connected pad must not take over a disconnected session owner.
 setter(2,&raw,ERROR_DEVICE_NOT_CONNECTED);raw.Gamepad.bRightTrigger=255;setter(0,&raw,0);Sleep(1050);frame();assert(app.editing&&!app.padInput.connected&&app.document.marks.size()==1);
 raw.Gamepad={};setter(2,&raw,0);frame();assert(app.padInput.connected);raw.Gamepad.wButtons=XINPUT_GAMEPAD_A;setter(2,&raw,0);frame();raw.Gamepad={};setter(2,&raw,0);frame();assert(app.padReady);app.padQuick=false;
 raw.Gamepad.wButtons=XINPUT_GAMEPAD_B;setter(2,&raw,0);frame();assert(!app.editing&&!pad::capturing());assert(game(2,&out)==0&&out.Gamepad.wButtons==0);raw.Gamepad={};setter(2,&raw,0);assert(game(2,&out)==0);frame();raw.Gamepad.sThumbLX=20000;setter(2,&raw,0);assert(game(2,&out)==0&&out.Gamepad.sThumbLX==20000);
 raw.Gamepad={};setter(2,&raw,0);frame();assert(game(2,&out)==0);raw.Gamepad.bLeftTrigger=raw.Gamepad.bRightTrigger=220;setter(2,&raw,0);assert(game(2,&out)==0&&pad::openRequest());frame(false);assert(!app.editing&&!pad::capturing()&&!pad::openRequest());assert(app.document.marks.size()==1);
 raw.Gamepad={};setter(2,&raw,0);frame();assert(game(2,&out)==0);raw.Gamepad.bLeftTrigger=raw.Gamepad.bRightTrigger=220;setter(2,&raw,0);assert(game(2,&out)==0&&pad::openRequest());setter(2,&raw,ERROR_DEVICE_NOT_CONNECTED);frame();assert(!app.editing&&!pad::openRequest());
 controller::cancel(app);pad::remove();ImGui::DestroyContext();std::puts("controller session: slot-2 trigger entry, no opening paint, draw, B exit, release/movement, pending focus loss and disconnect passed");
}
