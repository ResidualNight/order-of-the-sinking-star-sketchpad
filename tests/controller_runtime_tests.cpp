#include "controller_probe.hpp"
#include "gamepad_bridge.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#include <cstdio>
#include <initializer_list>
#include <cstdlib>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"runtime probe FAIL %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main(){
 ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;
 unsigned char* pixels;int w,h;io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);
 sketch::probe::start(nullptr);
 CHECK(!sketch::pad::capturing());
 for(auto size:{ImVec2(1920,1080),ImVec2(640,360)}){
  io.DisplaySize=size;io.DeltaTime=1.f/60;
  ImGui::NewFrame();sketch::probe::panel(true);ImGui::Render();
  auto window=ImGui::FindWindowByName("手柄隔离测试");CHECK(window&&window->Active);
  CHECK(window->Size.x<=size.x&&window->Size.y<=size.y);
 }
 sketch::pad::setCapturing(true);CHECK(sketch::pad::capturing());
 sketch::probe::close();CHECK(!sketch::pad::capturing());
 sketch::pad::setCapturing(true);sketch::probe::panel(false);CHECK(!sketch::pad::capturing());
 sketch::pad::setCapturing(true);sketch::probe::emergency();CHECK(!sketch::pad::capturing());
 sketch::pad::remove();ImGui::DestroyContext();
 std::puts("runtime probe: default off, panel sizes, exit and emergency passed");
}
