#pragma once
#include "camera_grid.hpp"
#include <vector>
namespace sketch {
// Runtime pose fields discovered from the active scene, never serialized.
// Discovery evidence is adapted from qiekn/ootss-cmd cf432e9; see third-party notices.
struct SceneCameraPose {
 std::array<float,3> position{};
 std::array<float,4> orientation{};
 std::array<float,3> projection{}; // vertical FOV (radians), near, far
 bool valid()const;
 bool project(float width,float height,std::array<float,16>& matrix,std::array<float,3>& direction)const;
 Grid grid(float width,float height,float z)const;
 uint8_t category(float width,float height)const;
};
bool sceneCameraEvidence(const SceneCameraPose&,const CameraFrame&,float width,float height);
class SceneCameraReader {
public:
 using Read=bool(*)(uintptr_t,void*,size_t);
 struct Evidence {uintptr_t address=0;std::vector<unsigned char> bytes;};
 struct Layout {uintptr_t image=0,size=0,managerSlot=0,mode=0;uint32_t position=0,orientation=0,projection=0,cameras=0;std::vector<Evidence> evidence;};
private:
 Layout layout;
 bool current(Read)const;
public:
 const char* status="not_attempted";
 bool discover(Read,uintptr_t image,uintptr_t managerRva=0xb69600);
 bool sample(Read,uintptr_t manager,SceneCameraPose& output);
 bool available()const{return layout.managerSlot!=0;}
 const Layout& discovered()const{return layout;}
};
}
