#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include "scene_camera.hpp"
#include "camera_picker.hpp"
#include "camera_fixture.hpp"
#include <cassert>
#include <fstream>
#include <iostream>
#include <memory>
using namespace sketch;
namespace {
bool readMemory(uintptr_t at,void* out,size_t n){SIZE_T done=0;return at>0x10000&&ReadProcessMemory(GetCurrentProcess(),reinterpret_cast<void*>(at),out,n,&done)&&done==n;}
template<class T>void put(std::vector<unsigned char>& bytes,size_t at,T value){assert(at+sizeof(value)<=bytes.size());std::memcpy(bytes.data()+at,&value,sizeof(value));}
void pattern(std::vector<unsigned char>& data,size_t at,std::initializer_list<int> p){for(int b:p)data[at++]=b<0?0:static_cast<unsigned char>(b);}
SceneCameraPose fixturePose(){
 auto c=gameCameraFixture();SceneCameraPose pose;for(int i=0;i<3;++i)pose.position[i]=c.f[208+i];
 // Independently recover a quaternion from the recorded rotation matrix.
 double r[3][3];for(int i=0;i<3;++i)for(int j=0;j<3;++j)r[i][j]=c.f[160+4*j+i];
 double trace=r[0][0]+r[1][1]+r[2][2];assert(trace>0);double s=std::sqrt(trace+1)*2;
 pose.orientation={float((r[2][1]-r[1][2])/s),float((r[0][2]-r[2][0])/s),float((r[1][0]-r[0][1])/s),float(.25*s)};
 pose.projection={float(2*std::atan(1/double(c.f[198]))),c.f[221],c.f[220]};return pose;
}
struct Fixture {
 static constexpr size_t Gate=0x1100,Normal=0x1200,Frozen=0x1400,Render=0x1600,Link=0x1800,Mode=0xb6a808;
 std::vector<unsigned char> image=std::vector<unsigned char>(0xc00000),manager=std::vector<unsigned char>(0x8000),entity=std::vector<unsigned char>(1072);
 uintptr_t base()const{return reinterpret_cast<uintptr_t>(image.data());}
 uintptr_t root()const{return reinterpret_cast<uintptr_t>(manager.data());}
 SceneCameraPose pose=fixturePose();
 Fixture(){
  IMAGE_DOS_HEADER dos{};dos.e_magic=IMAGE_DOS_SIGNATURE;dos.e_lfanew=0x80;put(image,0,dos);
  IMAGE_NT_HEADERS64 nt{};nt.Signature=IMAGE_NT_SIGNATURE;nt.FileHeader.Machine=IMAGE_FILE_MACHINE_AMD64;nt.FileHeader.NumberOfSections=2;
  nt.FileHeader.SizeOfOptionalHeader=sizeof(IMAGE_OPTIONAL_HEADER64);nt.OptionalHeader.Magic=IMAGE_NT_OPTIONAL_HDR64_MAGIC;nt.OptionalHeader.SizeOfImage=DWORD(image.size());put(image,0x80,nt);
  IMAGE_SECTION_HEADER text{},data{};std::memcpy(text.Name,".text",5);text.VirtualAddress=0x1000;text.Misc.VirtualSize=0x2000;text.Characteristics=IMAGE_SCN_MEM_EXECUTE|IMAGE_SCN_MEM_READ;
  std::memcpy(data.Name,".data",5);data.VirtualAddress=0xb00000;data.Misc.VirtualSize=0x100000;data.Characteristics=IMAGE_SCN_MEM_READ|IMAGE_SCN_MEM_WRITE;
  put(image,0x80+sizeof(nt),text);put(image,0x80+sizeof(nt)+sizeof(text),data);
  std::fill(image.begin()+0x1000,image.begin()+0x3000,0xcc);
  pattern(image,Gate,{0x49,0x8d,0xad,-1,-1,-1,-1,0x48,0x8b,0x05,-1,-1,-1,-1,0x48,0x85,0xc0,0x0f,0x84,-1,-1,-1,-1,0x48,0x83,0xf8,1,0x0f,0x85,-1,-1,-1,-1});
  put(image,Gate+3,uint32_t(0x3d98));put(image,Gate+10,int32_t(Mode-Gate-14));put(image,Gate+19,int32_t(Normal-Gate-23));put(image,Gate+29,int32_t(Frozen-Gate-33));
  pattern(image,Normal,{0x4c,0x89,0x74,0x24,0,0x49,0x83,0xbd,-1,-1,-1,-1,0,0x0f,0x8e,0,0,0,0});
  put(image,Normal+8,uint32_t(0x538));put(image,Normal+59,uint32_t(0x538));
  pattern(image,Normal-23,{0x50,0xc3,0xa8,1,0x0f,0x85,0,0,0,0,0xf3,0x0f,0x10,0x1d,0,0,0,0,0xe9,0,0,0,0});
  pattern(image,Normal+69,{0x49,0x8b,0x85,-1,-1,-1,-1,0x4c,0x8b,0x34,0xd8});put(image,Normal+72,uint32_t(0x540));
  pattern(image,Frozen-17,{0x41,0x0f,0x13,0x85,-1,-1,-1,-1,0xf3,0x41,0x0f,0x11,0x95,-1,-1,-1,-1,0x49,0x8b,0x45,0,0x48,0x83,0xc0,0xfb,0x48,0x83,0xf8,0x14,0x0f,0x87,0,0,0,0});
  put(image,Frozen-13,uint32_t(0x3d98));put(image,Frozen-4,uint32_t(0x3da0));
  pattern(image,Render-47,{0x48,0x8d,0x90,-1,-1,-1,-1,0x0f,0xb6,0x0d,0,0,0,0,0xc0,0xe9,4,0x80,0xe1,1,0x4c,0x8b,0x05,0,0,0,0,0xf2,0x41,0x0f,0x10,0x40,0x38,0x0f,0x5b,0xc0,0x0f,0x28,0xc8,0x0f,0xc6,0xc8,0x55,0xf3,0x0f,0x5e,0xc1});
  put(image,Render-44,uint32_t(0x3d98));
  pattern(image,Render,{0x4c,0x8d,0x80,-1,-1,-1,-1,0xf3,0x0f,0x10,0x88,-1,-1,-1,-1,0xf3,0x0f,0x10,0x98,-1,-1,-1,-1,0xf3,0x0f,0x10,0x90,-1,-1,-1,-1});
  put(image,Render+3,uint32_t(0x3db0));put(image,Render+11,uint32_t(0x3dc0));put(image,Render+19,uint32_t(0x3dc4));put(image,Render+27,uint32_t(0x3dc8));
  pattern(image,Link,{0x4c,0x8b,0x2d,-1,-1,-1,-1,0x49,0x8d,0x95,-1,-1,-1,-1,0x4d,0x8d,0x85,-1,-1,-1,-1,0x31,0xc9,0xe8,0,0,0,0});
  put(image,Link+3,int32_t(0xb69600-Link-7));put(image,Link+10,uint32_t(0x3d98));put(image,Link+17,uint32_t(0x3db0));put(image,0xb69600,root());
  struct Type{uint32_t kind,pad;int64_t size,count;uintptr_t name;};put(image,0xb00100,Type{7,0,1072,14,base()+0xb00200});std::memcpy(image.data()+0xb00200,"Camera_Control",14);
  put(manager,0x538,int64_t(1));put(manager,0x540,base()+0xb00300);put(image,0xb00300,reinterpret_cast<uintptr_t>(entity.data()));
  put(entity,240,root());put(entity,256,uint32_t(7));put(entity,264,base()+0xb00100);
  std::memcpy(manager.data()+0x3d98,pose.position.data(),12);std::memcpy(manager.data()+0x3db0,pose.orientation.data(),16);std::memcpy(manager.data()+0x3dc0,pose.projection.data(),12);
  // The real grid reader's Settings validation stays active in this fixture.
  struct Array{int64_t count;uintptr_t data;int64_t capacity;};struct Geometry{int64_t x,y,count;uint8_t wrapping,pad[3];int32_t corner[3];};
  put(manager,0xdd0,Array{1,base()+0xb00400,1});put(image,0xb00400,base()+0xb01000);put(image,0xb01000+240,root());put(image,0xb01000+256,uint32_t(8));
  put(image,0xb01000+264,base()+0xb00500);put(image,0xb00500,Type{7,0,1320,8,base()+0xb00600});std::memcpy(image.data()+0xb00600,"Settings",8);
  put(image,0xb01000+816,Geometry{8,7,56,0,{},{}});put(image,0xb01000+1296,int64_t(7));put(image,0xb01000+1304,base()+0xb00700);std::memcpy(image.data()+0xb00700,"fixture",7);
 }
};
void verifyFile(const char* filename){
 if(!filename)return;std::ifstream file(filename,std::ios::binary);assert(file);std::vector<unsigned char> bytes((std::istreambuf_iterator<char>(file)),{});
 auto dos=reinterpret_cast<const IMAGE_DOS_HEADER*>(bytes.data());assert(bytes.size()>sizeof(*dos)&&dos->e_magic==IMAGE_DOS_SIGNATURE);
 auto nt=reinterpret_cast<const IMAGE_NT_HEADERS64*>(bytes.data()+dos->e_lfanew);assert(nt->OptionalHeader.SizeOfImage<0x10000000);
 std::vector<unsigned char> mapped(nt->OptionalHeader.SizeOfImage);std::memcpy(mapped.data(),bytes.data(),nt->OptionalHeader.SizeOfHeaders);
 auto sections=IMAGE_FIRST_SECTION(nt);for(unsigned i=0;i<nt->FileHeader.NumberOfSections;++i){const auto& s=sections[i];assert(size_t(s.PointerToRawData)+s.SizeOfRawData<=bytes.size()&&size_t(s.VirtualAddress)+s.SizeOfRawData<=mapped.size());std::memcpy(mapped.data()+s.VirtualAddress,bytes.data()+s.PointerToRawData,s.SizeOfRawData);}
 SceneCameraReader reader;assert(reader.discover(readMemory,reinterpret_cast<uintptr_t>(mapped.data())));
 assert(reader.discovered().position==0x3d98&&reader.discovered().orientation==0x3db0&&reader.discovered().projection==0x3dc0);
 std::cout<<"PASS: real game PE signature/field cross-check (offline data only)\n";
}
}
void checkSceneCameraReaderFixture(uintptr_t image,CameraFrame* raw,const SceneCameraPose& pose);
void checkSceneCameraWorker(uintptr_t image,CameraFrame* raw);
void checkSceneCamera(const char* filename){
 auto pose=fixturePose();assert(pose.valid());auto camera=gameCameraFixture();
 // Compare every screen mapping against independent viewport arithmetic.
 CameraOption scene;scene.scenePose=true;scene.pose=pose;
 for(Point output:std::vector<Point>{{640,360},{1280,1024},{1600,1200},{1920,1080},{2560,1600},{2560,1080},{3440,1440},{5120,1440}}){
  for(int aspect=0;aspect<int(std::size(CameraAspects));++aspect){
   float ratio=aspect?CameraAspects[aspect]:output.x/output.y;
   auto reference=pose.grid(1080*ratio,1080,1);assert(reference.usable());
   for(auto fit:{CameraFit::Stretch,CameraFit::Contain,CameraFit::Cover}){
    float rw=output.x,rh=output.y;
    if(fit!=CameraFit::Stretch){float scale=fit==CameraFit::Contain?std::min(output.x/(1080*ratio),output.y/1080):std::max(output.x/(1080*ratio),output.y/1080);rw=1080*ratio*scale;rh=1080*scale;}
    for(int flip=0;flip<4;++flip){auto mapped=cameraOptionGrid(scene,output.x,output.y,1,flip,aspect,fit);assert(mapped.usable());
     for(int x=-2;x<=3;++x)for(int y=-2;y<=3;++y){auto p=reference.at(x,y);Point expected{p.x/(1080*ratio)*rw+(output.x-rw)*.5f,p.y/1080*rh+(output.y-rh)*.5f};
      if(flip&1)expected.x=output.x-expected.x;if(flip&2)expected.y=output.y-expected.y;
      assert(distance(mapped.at(x,y),expected)<.02f);assert(distance(mapped.cell(expected),{float(x),float(y)})<.002f);
     }
    }
   }
  }
 }
 CameraOption stack;stack.camera=camera;stack.camera.f[216]=1.f/2560;stack.camera.f[217]=1.f/1440;stack.aspectUnverified=true;
 auto fit=cameraOptionGrid(stack,2560,1600,1,0,0,CameraFit::Contain),reference=gridFromCamera(camera,2560,1440,1);
 assert(fit.usable());for(int x=-3;x<=3;++x)for(int y=-3;y<=3;++y)assert(distance(fit.at(x,y),reference.at(x,y)+Point{0,80})<.005f);
 assert(!cameraOptionGrid(stack,2560,1600,1,0,0,CameraFit(9)).usable());
 assert(!cameraOptionGrid(scene,2560,1600,1,0,99).usable());
 for(Point internal:std::vector<Point>{{2560,1440},{2560,1600},{1600,1200},{1280,1024},{2560,1080},{3440,1440},{5120,1440}})
  assert(cameraViewportPreviewMatches(1/internal.x,1/internal.y,2560,1600));
 assert(!cameraViewportPreviewMatches(1.f/1234,1.f/720,2560,1600));
 assert(!cameraViewportPreviewMatches(NAN,1.f/1440,2560,1600));
 // Recheck the upstream +X-forward / +Y-left / +Z-up convention with an
 // independent quaternion-vector cross-product oracle, including roll and
 // the upstream exact top-down quaternion (not just one recorded view).
 for(auto q:std::vector<std::array<float,4>>{{0,0,0,1},{0,0,.70710678f,.70710678f},{0,.70710678f,0,.70710678f},{-.5f,.5f,.5f,.5f},{1,2,3,4}}){
  double norm=0;for(auto v:q)norm+=double(v)*v;for(auto& v:q)v=float(v/std::sqrt(norm));
  SceneCameraPose probe;probe.position={5,-1,20};probe.orientation=q;probe.projection={1.5707963268f,.2f,200};
  auto rotate=[&](std::array<double,3> v){std::array<double,3> t{2*(q[1]*v[2]-q[2]*v[1]),2*(q[2]*v[0]-q[0]*v[2]),2*(q[0]*v[1]-q[1]*v[0])};
   return std::array<double,3>{v[0]+q[3]*t[0]+q[1]*t[2]-q[2]*t[1],v[1]+q[3]*t[1]+q[2]*t[0]-q[0]*t[2],v[2]+q[3]*t[2]+q[0]*t[1]-q[1]*t[0]};};
  for(Point output:std::vector<Point>{{1920,1080},{1920,1200},{3440,1440}}){std::array<float,16> matrix{};std::array<float,3> direction{};assert(probe.project(output.x,output.y,matrix,direction));
   for(auto local:std::vector<std::array<double,3>>{{10,0,0},{10,-1,0},{10,0,1}}){auto world=rotate(local);double clip[4]{};
    for(int r=0;r<4;++r){clip[r]=matrix[r*4+3];for(int c=0;c<3;++c)clip[r]+=matrix[r*4+c]*(world[c]+probe.position[c]);}
    assert(std::abs(clip[3]-10)<.00001);
    assert(std::abs(clip[0]/clip[3]+local[1]*output.y/output.x/10)<.000002);
    assert(std::abs(clip[1]/clip[3]-local[2]/10)<.000002);
   }
  }
 }
 for(float width:{1280.f,1920.f,3840.f}){float height=width*9/16;std::array<float,16> matrix{};std::array<float,3> direction{};assert(pose.project(width,height,matrix,direction));
  for(int i=0;i<16;++i)assert(std::abs(matrix[i]-camera.f[i])<.00003f);
  for(float z:{-2.f,1.f,3.f}){auto grid=pose.grid(width,height,z),reference=gridFromCamera(camera,width,height,z);assert(grid.usable()&&reference.usable());
   for(int y=-8;y<=8;++y)for(int x=-8;x<=8;++x)assert(length(grid.at(x,y)-reference.at(x,y))<.004f);}
 }
 assert(pose.category(3840,2160)==0&&sceneCameraEvidence(pose,camera,3840,2160));
 auto invalid=pose;invalid.orientation[0]=NAN;assert(!invalid.valid()&&!invalid.grid(3840,2160,1).usable());invalid=pose;invalid.projection[2]=invalid.projection[1];assert(!invalid.valid());
 camera.f[235]=1;assert(!sceneCameraEvidence(pose,camera,3840,2160));camera=gameCameraFixture();camera.f[208]+=1;assert(!sceneCameraEvidence(pose,camera,3840,2160));
 camera=gameCameraFixture();uint32_t reflection=5;std::memcpy(camera.f.data()+226,&reflection,4);assert(!sceneCameraEvidence(pose,camera,3840,2160));
 camera=gameCameraFixture();camera.f[0]=NAN;assert(sceneCameraEvidence(pose,camera,3840,2160));assert(!validCamera(camera,3840,2160)); // Not a relaxation of the stack path.
 auto f=std::make_unique<Fixture>();SceneCameraReader reader;SceneCameraPose result;
 assert(reader.discover(readMemory,f->base())&&reader.sample(readMemory,f->root(),result));assert(result.position==pose.position);
 // Re-read every byte of all six evidence spans; wildcard bytes are protected too.
 for(const auto& e:reader.discovered().evidence)for(size_t i=0;i<e.bytes.size();++i){auto at=e.address-f->base()+i;f->image[at]^=1;assert(!reader.sample(readMemory,f->root(),result));f->image[at]^=1;}
 put(f->image,Fixture::Mode,int64_t(2));assert(!reader.sample(readMemory,f->root(),result));put(f->image,Fixture::Mode,int64_t(0));
 put(f->entity,240,uintptr_t(0));assert(!reader.sample(readMemory,f->root(),result));put(f->entity,240,f->root());
 put(f->image,Fixture::Render+19,uint32_t(0x3dc8));assert(!reader.discover(readMemory,f->base()));put(f->image,Fixture::Render+19,uint32_t(0x3dc4));
 std::memcpy(f->image.data()+0x1a00,f->image.data()+Fixture::Link,28);assert(!reader.discover(readMemory,f->base()));std::fill(f->image.begin()+0x1a00,f->image.begin()+0x1a1c,0xcc);
 assert(reader.discover(readMemory,f->base()));
 camera=gameCameraFixture();checkSceneCameraReaderFixture(f->base(),&camera,pose);
 camera=gameCameraFixture();checkSceneCameraWorker(f->base(),&camera);
 verifyFile(filename);
 std::cout<<"PASS: scene pose projection vs captured matrix, source/map guards, six code proofs, layout ambiguity/conflicts and original fallback\n";
}
