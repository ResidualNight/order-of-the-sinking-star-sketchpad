#include "game_grid_worker.hpp"
#include <atomic>
#include <mutex>
#include <new>
namespace sketch {
struct GameGridWorker::Session {
 GameGridReader reader;
 std::atomic<bool> cancelled{false},done{false};
 float width=0,height=0,z=1;bool overrideHeight=false,picking=false;
 LevelIdentity requestedLevel;uintptr_t requestedManager=0;
 GameGridState state=GameGridState::Waiting;Grid grid;
 std::vector<CameraOption> options;std::vector<std::string> details;
};
struct GameGridWorker::Service {
 std::mutex mutex;std::shared_ptr<Session> pending,current;bool running=false;
};
namespace {struct GridWork {std::shared_ptr<void> service;HMODULE module=nullptr;};}
GameGridWorker::GameGridWorker():service(std::make_shared<Service>()){}
GameGridWorker::~GameGridWorker(){
 // A global instance may be destroyed under the loader lock at process exit,
 // after Windows has stopped other threads. Never acquire their mutex here.
 if(active)active->cancelled.store(true);
}
bool GameGridWorker::idle()const{std::lock_guard<std::mutex> lock(service->mutex);return !service->running;}
bool GameGridWorker::fail(ErrorId id){error=id;diagnostic=errorInfo(id).diagnostic;return false;}
void GameGridWorker::cancel(){
 std::lock_guard<std::mutex> lock(service->mutex);
 if(service->pending)service->pending->cancelled.store(true);
 if(service->current)service->current->cancelled.store(true);
 service->pending.reset();active.reset();
}
void GameGridWorker::cancelPicking(){if(active&&active->picking)cancel();}
bool GameGridWorker::start(float w,float h,bool overrideHeight,float z,bool picking,bool preferScene){
 cancel();++serial;options.clear();details.clear();error=ErrorId::None;directionCategory=0;diagnostic="waiting_for_fresh_camera_frame";
 levelReader.verifyBuild();
 // Bind the queued request to the level the UI actually requested. These
 // guarded layout reads stay cheap; camera scans happen only in the worker.
 levelReader.width=w;levelReader.height=h;levelReader.heightOverride=overrideHeight;levelReader.forcedZ=z;
 levelReader.error=ErrorId::None;levelReader.started=GetTickCount64();levelReader.levelValidated=false;
 if(!levelReader.readLevel()||!levelReader.readCurrentLevel(levelReader.pickerLevel)||!levelReader.localizationContext()){
  if(levelReader.error==ErrorId::None)levelReader.fail(ErrorId::LevelChanged);
  error=levelReader.error;diagnostic=levelReader.diagnostic;details=levelReader.failureDetails();return false;
 }
 auto session=std::make_shared<Session>();session->width=w;session->height=h;session->z=z;session->overrideHeight=overrideHeight;session->picking=picking;
 session->requestedLevel=levelReader.pickerLevel;session->requestedManager=levelReader.manager;
 auto& reader=session->reader;reader.checked=levelReader.checked;reader.supported=levelReader.supported;reader.image=levelReader.image;
 reader.requestSerial=serial-1;reader.scanIntervalMs=50;reader.scanOwnThread=false;
 reader.preferScene=preferScene;
 reader.candidates=levelReader.candidates;
 std::lock_guard<std::mutex> lock(service->mutex);
 active=session;service->pending=session;
 if(service->running)return true;
 // Hold the module until all worker-owned objects have been destroyed. Never
 // join a thread from the render path or DllMain. A service has at most one worker.
 HMODULE module=nullptr;DWORD launchError=0;
 if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&GameGridWorker::worker),&module)){
  auto work=new(std::nothrow) GridWork{service,module};
  if(work){service->running=true;HANDLE thread=CreateThread(nullptr,0,&GameGridWorker::worker,work,0,nullptr);
   if(thread){CloseHandle(thread);return true;}
   launchError=GetLastError();service->running=false;delete work;
  }else launchError=ERROR_NOT_ENOUGH_MEMORY;
  FreeLibrary(module);
 }else launchError=GetLastError();
 service->pending.reset();active.reset();
 details.push_back("sampling=background launch_error="+std::to_string(launchError));return fail(ErrorId::CameraWorkerUnavailable);
}
void GameGridWorker::run(const std::shared_ptr<Service>& service){
 for(;;){
  std::shared_ptr<Session> session;
  {std::lock_guard<std::mutex> lock(service->mutex);
   session=std::move(service->pending);service->current=session;
   if(!session){service->running=false;return;}
  }
  auto& reader=session->reader;
  try{
   if(!session->cancelled.load()){
    bool began=session->picking?reader.beginPicking(session->width,session->height,session->z):reader.begin(session->width,session->height,session->overrideHeight,session->z);
    if(began&&(reader.manager!=session->requestedManager||reader.pickerLevel!=session->requestedLevel))began=reader.fail(ErrorId::LevelChanged);
    session->state=began?GameGridState::Waiting:GameGridState::Unavailable;
    size_t polls=0;
    while(session->state==GameGridState::Waiting&&!session->cancelled.load()){
     session->state=session->picking?reader.pollPicking():reader.poll(session->grid);
     // Vary the small delay so polling is not locked to a display refresh phase.
     if(session->state==GameGridState::Waiting)Sleep(DWORD(3+(polls++%3)));
    }
    if(session->state==GameGridState::Ready||session->state==GameGridState::Preview)session->options=reader.pickerOptions();
   }
  }catch(...){reader.fail(ErrorId::CameraWorkerUnavailable);session->state=GameGridState::Unavailable;}
  if(!session->cancelled.load()){
   try{session->details=reader.failureDetails();session->details.push_back("sampling=background scan_interval_ms=50 poll_delay_ms=3..5");}
   catch(...){reader.fail(ErrorId::CameraWorkerUnavailable);session->state=GameGridState::Unavailable;}
   // The worker never touches this reader after publication. Cancelled or
   // replaced sessions are never delivered to the UI.
   session->done.store(true,std::memory_order_release);
  }
 }
}
DWORD WINAPI GameGridWorker::worker(void* context){
 auto work=static_cast<GridWork*>(context);HMODULE module=work->module;
 {auto service=std::static_pointer_cast<Service>(work->service);delete work;run(service);}
 FreeLibraryAndExitThread(module,0);return 0;
}
GameGridState GameGridWorker::result(bool picking,Grid* grid){
 if(!active||active->picking!=picking){fail(picking?ErrorId::CameraChoiceExpired:ErrorId::FreshCameraFrameUnavailable);return GameGridState::Unavailable;}
 if(!active->done.load(std::memory_order_acquire))return GameGridState::Waiting;
 auto& reader=active->reader;error=reader.error;diagnostic=reader.diagnostic;details=active->details;
 if(active->state!=GameGridState::Ready&&active->state!=GameGridState::Preview)return active->state;
 // Check the level/manager again at delivery. Automatic captures keep the
 // original 250ms lifetime; displayed manual previews still adopt directly.
 if(!reader.localizationContext()){
  error=reader.error;diagnostic=reader.diagnostic;details=reader.failureDetails();return GameGridState::Unavailable;
 }
 if(!picking&&active->state==GameGridState::Ready&&GetTickCount64()-reader.capturedAt>GameGridReader::CaptureLifetimeMs){
  // The automatic lifetime is unchanged. A validated captured snapshot may
  // still be displayed for explicit manual adoption, like picker captures.
  if(!active->options.empty()){
   options=active->options;error=ErrorId::None;diagnostic="camera_preview automatic_capture_expired=1 manual_confirmation=1 sampling=background";
   return GameGridState::Preview;
  }
  fail(ErrorId::FreshCameraFrameUnavailable);details={"sampling=background delivery_capture_expired=1"};return GameGridState::Unavailable;
 }
 planeZ=reader.planeZ;directionCategory=reader.directionCategory;
 if(picking||active->state==GameGridState::Preview)options=active->options;else if(grid)*grid=active->grid;
 diagnostic+=" sampling=background";return active->state;
}
}
