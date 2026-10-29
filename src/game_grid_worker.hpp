#pragma once
#include "game_grid.hpp"
namespace sketch {
// Camera reads run independently of Present. Only completed, request-local
// results cross back to the render thread; the reader has one owning thread.
class GameGridWorker {
 friend struct GameGridWorkerTestAccess;
 struct Session;struct Service;
 std::shared_ptr<Service> service;
 std::shared_ptr<Session> active;
 GameGridReader levelReader;
 uint64_t serial=0;
 std::vector<CameraOption> options;
 std::vector<std::string> details;
 bool start(float w,float h,bool overrideHeight,float z,bool picking,bool preferScene=true);
 GameGridState result(bool picking,Grid* grid);
 static void run(const std::shared_ptr<Service>& service);
 static DWORD WINAPI worker(void* context);
 bool fail(ErrorId id);
 bool idle()const;
public:
 GameGridWorker();~GameGridWorker();
 GameGridWorker(const GameGridWorker&)=delete;
 GameGridWorker& operator=(const GameGridWorker&)=delete;
 ErrorId error=ErrorId::None;std::string diagnostic;float planeZ=1;uint8_t directionCategory=0;
 bool begin(float w,float h,bool overrideHeight,float z,bool preferScene=true){return start(w,h,overrideHeight,z,false,preferScene);}
 bool beginPicking(float w,float h,float z){return start(w,h,true,z,true);}
 GameGridState poll(Grid& grid){return result(false,&grid);}
 GameGridState pollPicking(){return result(true,nullptr);}
 const std::vector<CameraOption>& pickerOptions()const{return options;}
 bool readCurrentLevel(LevelIdentity& identity){return levelReader.readCurrentLevel(identity);}
 ErrorId buildWarning()const{return levelReader.buildWarning();}
 uint64_t requestId()const{return serial;}
 const std::vector<std::string>& failureDetails()const{return details;}
 void cancel();void cancelPicking();
};
}
