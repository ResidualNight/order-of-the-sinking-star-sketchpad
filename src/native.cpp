#define WIN32_LEAN_AND_MEAN
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <imm.h>
#include <dxgi1_6.h>
#include <d3d12.h>
#include "imgui.h"
#include "backends/imgui_impl_dx12.h"
#include "backends/imgui_impl_win32.h"
#include "ui.hpp"
#include "ui_glyphs.hpp"
#include "game_grid.hpp"
#include <atomic>
#include <mutex>
#include <map>
#include <memory>
#include <stdexcept>
#include <cstdio>
#include <cstdarg>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND,UINT,WPARAM,LPARAM);
namespace {
template<class T>struct Com{T*p=nullptr;~Com(){reset();}Com()=default;Com(const Com&)=delete;Com&operator=(const Com&)=delete;Com(Com&&b)noexcept:p(b.p){b.p=nullptr;}Com&operator=(Com&&b)noexcept{reset();p=b.p;b.p=nullptr;return*this;}void reset(){if(p){p->Release();p=nullptr;}}T**put(){reset();return&p;}T*operator->()const{return p;}operator bool()const{return p!=nullptr;}};
HMODULE ownModule=nullptr,realDxgi=nullptr;std::recursive_mutex renderMutex;std::mutex hookMutex;std::map<void**,void*> originals;
ULONGLONG lastMouseMessage[3]{};std::atomic<bool> editing{false};HIMC savedImeContext=nullptr;bool imeDetached=false;sketch::App app;sketch::GameGridReader gameGrid;WNDPROC previousWndProc=nullptr;HWND gameWindow=nullptr;
std::wstring basePath(){wchar_t buffer[32768];DWORD n=GetModuleFileNameW(ownModule,buffer,32768);std::wstring path(buffer,n);return path.substr(0,path.find_last_of(L"\\/")+1);}
void log(const char*fmt,...){char line[2048];va_list args;va_start(args,fmt);vsnprintf(line,sizeof(line),fmt,args);va_end(args);std::wstring path=basePath()+L"StarSketch.log";HANDLE file=CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file!=INVALID_HANDLE_VALUE){DWORD done;WriteFile(file,line,(DWORD)strlen(line),&done,nullptr);WriteFile(file,"\r\n",2,&done,nullptr);CloseHandle(file);}}
void require(HRESULT hr,const char*what){if(FAILED(hr)){log("[SS-DX12-001] %s: HRESULT=0x%08lx",what,(unsigned long)hr);throw std::runtime_error(what);}}
void systemDxgi(){static std::once_flag once;std::call_once(once,[]{wchar_t path[MAX_PATH];GetSystemDirectoryW(path,MAX_PATH);wcscat_s(path,L"\\dxgi.dll");realDxgi=LoadLibraryW(path);log("StarSketch 0.1.4 | independent DX12 overlay | system DXGI loaded=%d",realDxgi!=nullptr);});}
template<class T>T real(const char*name){systemDxgi();return reinterpret_cast<T>(GetProcAddress(realDxgi,name));}
void patch(void**slot,void*replacement){DWORD old;if(!VirtualProtect(slot,sizeof(void*),PAGE_READWRITE,&old))throw std::runtime_error("VirtualProtect");InterlockedExchangePointer(slot,replacement);DWORD ignored;VirtualProtect(slot,sizeof(void*),old,&ignored);}
void hook(void*object,size_t index,void*replacement){void**slot=(*reinterpret_cast<void***>(object))+index;std::lock_guard<std::mutex>lock(hookMutex);if(originals.count(slot))return;originals[slot]=*slot;patch(slot,replacement);}
template<class T>T original(void*object,size_t index){std::lock_guard<std::mutex>lock(hookMutex);return reinterpret_cast<T>(originals.at((*reinterpret_cast<void***>(object))+index));}
void setEditing(bool enabled){
 if(editing.exchange(enabled)==enabled)return;
 app.setEditing(enabled);
 ClipCursor(nullptr);if(ImGui::GetCurrentContext())ImGui::GetIO().MouseDrawCursor=enabled;log("Drawing mode %s",enabled?"ON":"OFF");
}
void pollKeyboard(){
 static bool previous[256]{};bool foreground=GetForegroundWindow()==gameWindow;SHORT e=GetAsyncKeyState('E');bool eDown=(e&0x8000)!=0;bool ctrl=(GetAsyncKeyState(VK_CONTROL)&0x8000)!=0,alt=(GetAsyncKeyState(VK_MENU)&0x8000)!=0,shift=(GetAsyncKeyState(VK_SHIFT)&0x8000)!=0;
 if(foreground&&!ctrl&&!alt&&!previous['E']&&(eDown||(e&1)))setEditing(!editing.load());previous['E']=eDown;
 struct Key{int vk;ImGuiKey key;};static Key keys[]={{'B',ImGuiKey_B},{'A',ImGuiKey_A},{'P',ImGuiKey_P},{'G',ImGuiKey_G},{'T',ImGuiKey_T},{'C',ImGuiKey_C},{'Z',ImGuiKey_Z},{'Y',ImGuiKey_Y},{VK_ESCAPE,ImGuiKey_Escape}};
 auto&io=ImGui::GetIO();io.AddKeyEvent(ImGuiMod_Ctrl,foreground&&ctrl);io.AddKeyEvent(ImGuiMod_Alt,foreground&&alt);io.AddKeyEvent(ImGuiMod_Shift,foreground&&shift);
 for(auto k:keys){SHORT state=GetAsyncKeyState(k.vk);bool down=foreground&&(state&0x8000);if(foreground&&editing.load()&&!previous[k.vk]&&!down&&(state&1)){io.AddKeyEvent(k.key,true);io.AddKeyEvent(k.key,false);}else io.AddKeyEvent(k.key,down);previous[k.vk]=down;} static bool mousePrevious[3]{};int mouseKeys[]={VK_LBUTTON,VK_RBUTTON,VK_MBUTTON};for(int button=0;button<3;button++){SHORT state=GetAsyncKeyState(mouseKeys[button]);bool down=foreground&&(state&0x8000);if(GetTickCount64()-lastMouseMessage[button]<200){mousePrevious[button]=down;continue;}if(foreground&&editing.load()&&!mousePrevious[button]&&!down&&(state&1)){io.AddMouseButtonEvent(button,true);io.AddMouseButtonEvent(button,false);}else io.AddMouseButtonEvent(button,down);mousePrevious[button]=down;}
}
void saveDiagnosticCapture(const sketch::Image&image){wchar_t directory[32768];DWORD length=GetEnvironmentVariableW(L"STARSKETCH_CAPTURE_DIR",directory,32768);if(!length||length>=32768)return;std::wstring path=std::wstring(directory,length)+L"\\game-grid-capture.bmp";HANDLE file=CreateFileW(path.c_str(),GENERIC_WRITE,FILE_SHARE_READ,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);if(file==INVALID_HANDLE_VALUE)return;
 BITMAPFILEHEADER fh{};BITMAPINFOHEADER ih{};fh.bfType=0x4d42;fh.bfOffBits=sizeof(fh)+sizeof(ih);fh.bfSize=fh.bfOffBits+DWORD(image.rgba.size());ih.biSize=sizeof(ih);ih.biWidth=image.width;ih.biHeight=-image.height;ih.biPlanes=1;ih.biBitCount=32;ih.biCompression=BI_RGB;DWORD done;WriteFile(file,&fh,sizeof(fh),&done,nullptr);WriteFile(file,&ih,sizeof(ih),&done,nullptr);std::vector<uint8_t>row(image.width*4);for(int y=0;y<image.height;y++){for(int x=0;x<image.width;x++){auto p=&image.rgba[(y*image.width+x)*4];row[x*4]=p[2];row[x*4+1]=p[1];row[x*4+2]=p[0];row[x*4+3]=255;}WriteFile(file,row.data(),DWORD(row.size()),&done,nullptr);}CloseHandle(file);
}
SHORT WINAPI blockedAsync(int key){return editing.load()?0:GetAsyncKeyState(key);}SHORT WINAPI blockedKey(int key){return editing.load()?0:GetKeyState(key);}
BOOL WINAPI blockedKeyboard(PBYTE keys){if(editing.load()){memset(keys,0,256);return TRUE;}return GetKeyboardState(keys);}
BOOL WINAPI blockedClip(const RECT*r){return editing.load()?TRUE:ClipCursor(r);}BOOL WINAPI blockedPosition(int x,int y){return editing.load()?TRUE:SetCursorPos(x,y);}
UINT WINAPI blockedRaw(HRAWINPUT h,UINT command,LPVOID data,PUINT size,UINT header){if(editing.load()){if(size)*size=0;return 0;}return GetRawInputData(h,command,data,size,header);}
void hookGameInput(){auto module=(uint8_t*)GetModuleHandleW(nullptr);auto dos=(IMAGE_DOS_HEADER*)module;auto nt=(IMAGE_NT_HEADERS64*)(module+dos->e_lfanew);auto dir=nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];if(!dir.VirtualAddress)return;
 struct Entry{const char*name;void*fn;};Entry entries[]={{"GetAsyncKeyState",(void*)blockedAsync},{"GetKeyState",(void*)blockedKey},{"GetKeyboardState",(void*)blockedKeyboard},{"GetRawInputData",(void*)blockedRaw},{"ClipCursor",(void*)blockedClip},{"SetCursorPos",(void*)blockedPosition}};
 for(auto desc=(IMAGE_IMPORT_DESCRIPTOR*)(module+dir.VirtualAddress);desc->Name;++desc){if(_stricmp((char*)module+desc->Name,"USER32.dll")!=0||!desc->OriginalFirstThunk)continue;auto names=(IMAGE_THUNK_DATA64*)(module+desc->OriginalFirstThunk);auto slots=(IMAGE_THUNK_DATA64*)(module+desc->FirstThunk);for(;names->u1.AddressOfData;++names,++slots){if(IMAGE_SNAP_BY_ORDINAL64(names->u1.Ordinal))continue;const char*name=(char*)((IMAGE_IMPORT_BY_NAME*)(module+names->u1.AddressOfData))->Name;for(auto&e:entries)if(strcmp(name,e.name)==0){patch((void**)&slots->u1.Function,e.fn);log("Input interception: %s",name);}}}
}
bool polledKey(WPARAM key){switch(key){case 'E':case 'B':case 'A':case 'P':case 'G':case 'T':case 'C':case 'Z':case 'Y':case VK_ESCAPE:case VK_CONTROL:case VK_LCONTROL:case VK_RCONTROL:case VK_MENU:case VK_LMENU:case VK_RMENU:case VK_SHIFT:case VK_LSHIFT:case VK_RSHIFT:return true;default:return false;}}
bool polledButton(UINT message){return message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_LBUTTONDBLCLK||message==WM_RBUTTONDOWN||message==WM_RBUTTONUP||message==WM_RBUTTONDBLCLK||message==WM_MBUTTONDOWN||message==WM_MBUTTONUP||message==WM_MBUTTONDBLCLK;}
constexpr UINT DisableGameImeMessage=WM_APP+0x31a;
void disableGameIme(HWND window){if(imeDetached)return;HIMC context=ImmGetContext(window);if(context){ImmNotifyIME(context,NI_COMPOSITIONSTR,CPS_CANCEL,0);ImmReleaseContext(window,context);}savedImeContext=ImmAssociateContext(window,nullptr);imeDetached=true;log("IME disabled for game window only");}
LRESULT CALLBACK windowProc(HWND window,UINT message,WPARAM wparam,LPARAM lparam){
 if(message==DisableGameImeMessage){disableGameIme(window);return 0;}
 if(message==WM_NCDESTROY&&imeDetached){ImmAssociateContext(window,savedImeContext);imeDetached=false;}

 bool active=editing.load();
 if(message==WM_KILLFOCUS){std::lock_guard<std::recursive_mutex>lock(renderMutex);setEditing(false);}
 {std::lock_guard<std::recursive_mutex>lock(renderMutex);if(ImGui::GetCurrentContext()){bool keyboardMessage=message==WM_KEYDOWN||message==WM_KEYUP||message==WM_SYSKEYDOWN||message==WM_SYSKEYUP;if(polledButton(message)){int button=(message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||message==WM_LBUTTONDBLCLK)?0:((message==WM_RBUTTONDOWN||message==WM_RBUTTONUP||message==WM_RBUTTONDBLCLK)?1:2);lastMouseMessage[button]=GetTickCount64();}if(!(keyboardMessage&&polledKey(wparam))&&(!polledButton(message)||active))ImGui_ImplWin32_WndProcHandler(window,message,wparam,lparam);}}
 if(active){if(message==WM_INPUT)return DefWindowProcW(window,message,wparam,lparam);if((message>=WM_MOUSEFIRST&&message<=WM_MOUSELAST)||(message>=WM_KEYFIRST&&message<=WM_KEYLAST)||message==WM_CHAR)return 0;if(message==WM_SETCURSOR)return TRUE;}
 return CallWindowProcW(previousWndProc,window,message,wparam,lparam);
}
struct Frame{Com<ID3D12CommandAllocator> allocator;Com<ID3D12Resource> buffer;D3D12_CPU_DESCRIPTOR_HANDLE rtv{};UINT64 fenceValue=0;};
class Renderer{
public:
 IDXGISwapChain3*target=nullptr;Com<ID3D12CommandQueue>queue;Com<ID3D12Device>device;Com<ID3D12DescriptorHeap>rtvHeap,srvHeap;Com<ID3D12GraphicsCommandList>commands;Com<ID3D12Fence>fence;std::vector<Frame>frames;HANDLE fenceEvent=nullptr;UINT64 value=0;bool initialized=false,context=false,disabled=false;DXGI_FORMAT format{};UINT64 renderWidth=0;UINT renderHeight=0;
 void wait(UINT64 v){if(v&&fence->GetCompletedValue()<v){require(fence->SetEventOnCompletion(v,fenceEvent),"fence event");if(WaitForSingleObject(fenceEvent,5000)!=WAIT_OBJECT_0)throw std::runtime_error("GPU fence timeout");}}
 void flush(){if(queue&&fence){require(queue->Signal(fence.p,++value),"queue signal");wait(value);}}
 void releaseBuffers(){if(initialized){flush();ImGui_ImplDX12_Shutdown();initialized=false;}frames.clear();commands.reset();rtvHeap.reset();srvHeap.reset();}
 void bind(IDXGISwapChain*swap,IUnknown*unknown){Com<ID3D12CommandQueue>candidate;if(FAILED(unknown->QueryInterface(__uuidof(ID3D12CommandQueue),(void**)candidate.put())))return;Com<IDXGISwapChain3>chain;if(FAILED(swap->QueryInterface(__uuidof(IDXGISwapChain3),(void**)chain.put())))return;std::lock_guard<std::recursive_mutex>lock(renderMutex);if(target==chain.p)return;if(initialized)releaseBuffers();target=chain.p;queue=std::move(candidate);disabled=false;log("D3D12 swapchain attached");}
 bool init(IDXGISwapChain3*swap){DXGI_SWAP_CHAIN_DESC desc{};require(swap->GetDesc(&desc),"swapchain desc");if(desc.BufferCount<2||!desc.OutputWindow)return false;require(swap->GetDevice(__uuidof(ID3D12Device),(void**)device.put()),"D3D12 device");format=desc.BufferDesc.Format;
  if(!context){IMGUI_CHECKVERSION();ImGui::CreateContext();auto&io=ImGui::GetIO();io.IniFilename=nullptr;io.LogFilename=nullptr;std::wstring fontPath=basePath()+L"StarSketch-assets\\NotoSerifSC.ttf";int n=WideCharToMultiByte(CP_UTF8,0,fontPath.c_str(),-1,nullptr,0,nullptr,nullptr);std::string utf(n,'\0');WideCharToMultiByte(CP_UTF8,0,fontPath.c_str(),-1,utf.data(),n,nullptr,nullptr);static ImVector<ImWchar> glyphRanges;ImFontGlyphRangesBuilder glyphBuilder;glyphBuilder.AddRanges(io.Fonts->GetGlyphRangesChineseSimplifiedCommon());glyphBuilder.AddText(sketch::UiGlyphs);for(const auto& error:sketch::Errors)glyphBuilder.AddText(error.message);glyphBuilder.BuildRanges(&glyphRanges);if(!io.Fonts->AddFontFromFileTTF(utf.c_str(),32,nullptr,glyphRanges.Data))throw std::runtime_error("Chinese font missing");
   ImGui::StyleColorsDark();auto&style=ImGui::GetStyle();style.WindowRounding=7;style.FrameRounding=3;style.WindowBorderSize=1;style.Colors[ImGuiCol_WindowBg]={.035f,.055f,.085f,.96f};style.Colors[ImGuiCol_Border]={.64f,.51f,.27f,.9f};style.Colors[ImGuiCol_Button]={.18f,.20f,.23f,1};style.Colors[ImGuiCol_ButtonHovered]={.34f,.30f,.20f,1};style.Colors[ImGuiCol_FrameBg]={.12f,.16f,.21f,1};style.Colors[ImGuiCol_SliderGrab]={.76f,.61f,.32f,1};style.Colors[ImGuiCol_Text]={.87f,.84f,.73f,1};
   ImGui_ImplWin32_Init(desc.OutputWindow);gameWindow=desc.OutputWindow;previousWndProc=(WNDPROC)SetWindowLongPtrW(gameWindow,GWLP_WNDPROC,(LONG_PTR)windowProc);if(!previousWndProc)throw std::runtime_error("window subclass failed");hookGameInput();context=true;PostMessageW(gameWindow,DisableGameImeMessage,0,0);log("UI initialized; keyboard E toggles drawing");}
  D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_RTV;hd.NumDescriptors=desc.BufferCount;require(device->CreateDescriptorHeap(&hd,__uuidof(ID3D12DescriptorHeap),(void**)rtvHeap.put()),"RTV heap");hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=2;hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;require(device->CreateDescriptorHeap(&hd,__uuidof(ID3D12DescriptorHeap),(void**)srvHeap.put()),"SRV heap");frames.resize(desc.BufferCount);UINT step=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);auto handle=rtvHeap->GetCPUDescriptorHandleForHeapStart();
  for(UINT i=0;i<desc.BufferCount;i++){auto&f=frames[i];require(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,__uuidof(ID3D12CommandAllocator),(void**)f.allocator.put()),"allocator");require(swap->GetBuffer(i,__uuidof(ID3D12Resource),(void**)f.buffer.put()),"backbuffer");f.rtv=handle;device->CreateRenderTargetView(f.buffer.p,nullptr,handle);handle.ptr+=step;}
  require(device->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,frames[0].allocator.p,nullptr,__uuidof(ID3D12GraphicsCommandList),(void**)commands.put()),"command list");commands->Close();if(!fence){require(device->CreateFence(0,D3D12_FENCE_FLAG_NONE,__uuidof(ID3D12Fence),(void**)fence.put()),"fence");fenceEvent=CreateEventW(nullptr,FALSE,FALSE,nullptr);if(!fenceEvent)throw std::runtime_error("fence event create");}
  ImGui_ImplDX12_InitInfo info;info.Device=device.p;info.CommandQueue=queue.p;info.NumFramesInFlight=desc.BufferCount;info.RTVFormat=format;info.SrvDescriptorHeap=srvHeap.p;info.UserData=this;
  info.SrvDescriptorAllocFn=[](ImGui_ImplDX12_InitInfo*i,D3D12_CPU_DESCRIPTOR_HANDLE*c,D3D12_GPU_DESCRIPTOR_HANDLE*g){auto*r=(Renderer*)i->UserData;*c=r->srvHeap->GetCPUDescriptorHandleForHeapStart();*g=r->srvHeap->GetGPUDescriptorHandleForHeapStart();};info.SrvDescriptorFreeFn=[](ImGui_ImplDX12_InitInfo*,D3D12_CPU_DESCRIPTOR_HANDLE,D3D12_GPU_DESCRIPTOR_HANDLE){};
  if(!ImGui_ImplDX12_Init(&info))throw std::runtime_error("ImGui DX12 initialization");initialized=true;log("Render initialized: %ux%u, %u buffers, format %u",desc.BufferDesc.Width,desc.BufferDesc.Height,desc.BufferCount,(UINT)format);return true;
 }
 void barrier(ID3D12Resource*resource,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=resource;b.Transition.StateBefore=before;b.Transition.StateAfter=after;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;commands->ResourceBarrier(1,&b);}
 void capture(Frame&f){auto desc=f.buffer->GetDesc();D3D12_PLACED_SUBRESOURCE_FOOTPRINT layout{};UINT64 bytes;device->GetCopyableFootprints(&desc,0,1,0,&layout,nullptr,nullptr,&bytes);D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_READBACK;D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rd.Width=bytes;rd.Height=1;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.SampleDesc.Count=1;rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;Com<ID3D12Resource>readback;require(device->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_COPY_DEST,nullptr,__uuidof(ID3D12Resource),(void**)readback.put()),"grid readback");
  barrier(f.buffer.p,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_SOURCE);D3D12_TEXTURE_COPY_LOCATION src{};src.pResource=f.buffer.p;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;D3D12_TEXTURE_COPY_LOCATION dst{};dst.pResource=readback.p;dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=layout;commands->CopyTextureRegion(&dst,0,0,0,&src,nullptr);barrier(f.buffer.p,D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_PRESENT);require(commands->Close(),"capture close");ID3D12CommandList*list=commands.p;queue->ExecuteCommandLists(1,&list);require(queue->Signal(fence.p,++value),"capture signal");f.fenceValue=value;wait(value);
  sketch::Image image;image.width=(int)desc.Width;image.height=desc.Height;image.rgba.resize(size_t(image.width)*image.height*4);uint8_t*data=nullptr;D3D12_RANGE range{0,(SIZE_T)bytes};require(readback->Map(0,&range,(void**)&data),"capture map");bool supported=true;
  for(int y=0;y<image.height;y++)for(int x=0;x<image.width;x++){auto in=data+layout.Offset+y*layout.Footprint.RowPitch+x*4;auto out=&image.rgba[(y*image.width+x)*4];if(format==DXGI_FORMAT_R8G8B8A8_UNORM||format==DXGI_FORMAT_R8G8B8A8_UNORM_SRGB){memcpy(out,in,4);}else if(format==DXGI_FORMAT_B8G8R8A8_UNORM||format==DXGI_FORMAT_B8G8R8A8_UNORM_SRGB){out[0]=in[2];out[1]=in[1];out[2]=in[0];out[3]=255;}else if(format==DXGI_FORMAT_R10G10B10A2_UNORM){uint32_t q=*(uint32_t*)in;out[0]=(q&1023)*255/1023;out[1]=((q>>10)&1023)*255/1023;out[2]=((q>>20)&1023)*255/1023;out[3]=255;}else supported=false;}
  D3D12_RANGE written{0,0};readback->Unmap(0,&written);if(supported){saveDiagnosticCapture(image);auto result=sketch::detectGrid(image);app.acceptDetection(result);log("Grid detection: valid=%d confidence=%.3f %s status=%s",result.grid.usable(),result.grid.confidence,result.diagnostic.c_str(),app.status.c_str());}else{app.gridRefresh.finish();app.status=sketch::errorMessage(sketch::ErrorId::UnsupportedCaptureFormat);log("[SS-IMAGE-003] Grid capture unsupported format %u",(UINT)format);}
  require(f.allocator->Reset(),"capture allocator reset");require(commands->Reset(f.allocator.p,nullptr),"capture reset");
 }
 void render(IDXGISwapChain3*swap){
  if(disabled||swap!=target)return;
  std::lock_guard<std::recursive_mutex>lock(renderMutex);
  try{
   if(!initialized&&!init(swap))return;
   pollKeyboard();
   auto&f=frames.at(swap->GetCurrentBackBufferIndex());wait(f.fenceValue);
   require(f.allocator->Reset(),"frame allocator reset");require(commands->Reset(f.allocator.p,nullptr),"frame list reset");
   auto desc=f.buffer->GetDesc();
   if(renderWidth&&renderHeight&&(renderWidth!=desc.Width||renderHeight!=desc.Height)){
    app.requestGrid(true);
    log("Game resolution changed: %llux%u -> %llux%u; grid refresh scheduled",(unsigned long long)renderWidth,renderHeight,(unsigned long long)desc.Width,desc.Height);
   }
   renderWidth=desc.Width;renderHeight=desc.Height;
   ImGui_ImplDX12_NewFrame();ImGui_ImplWin32_NewFrame();ImGui::NewFrame();
   auto&io=ImGui::GetIO();app.resize(io.DisplaySize.x,io.DisplaySize.y);
   auto&refresh=app.gridRefresh;
   if(refresh.ready()){
    if(!refresh.pending){
     refresh.pending=gameGrid.begin(float(desc.Width),float(desc.Height),app.gridPlaneOverride,app.gridPlaneZ);
     log("Game grid localization started: %llux%u",(unsigned long long)desc.Width,desc.Height);
     if(!refresh.pending){
      app.rejectGameGrid(gameGrid.error);
      log("%s | Game grid unavailable: %s viewport=%llux%u",app.status.c_str(),gameGrid.diagnostic.c_str(),(unsigned long long)desc.Width,desc.Height);
     }
    }else{
     sketch::Grid projected;auto state=gameGrid.poll(projected);
     if(state==sketch::GameGridState::Ready){
      app.grid=projected;app.gridConfirmed=true;app.gridFromGame=true;app.gridPlaneZ=gameGrid.planeZ;refresh.finish();
      app.status=app.gridPlaneOverride?"已按游戏数据定位，使用指定高度层":"已按游戏数据定位，使用当前角色所在高度层";
      log("Game grid: %s",gameGrid.diagnostic.c_str());
     }else if(state==sketch::GameGridState::Unavailable){
      app.rejectGameGrid(gameGrid.error);
      log("%s | Game grid unavailable: %s viewport=%llux%u",app.status.c_str(),gameGrid.diagnostic.c_str(),(unsigned long long)desc.Width,desc.Height);
     }
    }
   }
   app.editing=editing.load();app.frame(io.DisplaySize.x,io.DisplaySize.y);
   if(app.wantExit){app.wantExit=false;setEditing(false);}ImGui::Render();
   barrier(f.buffer.p,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);commands->OMSetRenderTargets(1,&f.rtv,FALSE,nullptr);ID3D12DescriptorHeap*heap=srvHeap.p;commands->SetDescriptorHeaps(1,&heap);ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(),commands.p);barrier(f.buffer.p,D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);require(commands->Close(),"frame close");ID3D12CommandList*list=commands.p;queue->ExecuteCommandLists(1,&list);require(queue->Signal(fence.p,++value),"frame signal");f.fenceValue=value;
  }catch(const std::exception&e){disabled=true;editing.store(false);ClipCursor(nullptr);log("[SS-DX12-002] Overlay disabled after error: %s",e.what());}
 }
};
Renderer renderer;
HRESULT STDMETHODCALLTYPE present(IDXGISwapChain3*s,UINT sync,UINT flags){if(!(flags&DXGI_PRESENT_TEST))renderer.render(s);return original<HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*,UINT,UINT)>(s,8)(s,sync,flags);}
HRESULT STDMETHODCALLTYPE present1(IDXGISwapChain3*s,UINT sync,UINT flags,const DXGI_PRESENT_PARAMETERS*p){if(!(flags&DXGI_PRESENT_TEST))renderer.render(s);return original<HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*,UINT,UINT,const DXGI_PRESENT_PARAMETERS*)>(s,22)(s,sync,flags,p);}
HRESULT STDMETHODCALLTYPE resize(IDXGISwapChain3*s,UINT n,UINT w,UINT h,DXGI_FORMAT f,UINT flags){if(s==renderer.target){std::lock_guard<std::recursive_mutex>lock(renderMutex);try{renderer.releaseBuffers();}catch(...){renderer.disabled=true;}}return original<HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*,UINT,UINT,UINT,DXGI_FORMAT,UINT)>(s,13)(s,n,w,h,f,flags);}
HRESULT STDMETHODCALLTYPE resize1(IDXGISwapChain3*s,UINT n,UINT w,UINT h,DXGI_FORMAT f,UINT flags,const UINT*masks,IUnknown*const*queues){if(s==renderer.target){std::lock_guard<std::recursive_mutex>lock(renderMutex);try{renderer.releaseBuffers();}catch(...){renderer.disabled=true;}}return original<HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain3*,UINT,UINT,UINT,DXGI_FORMAT,UINT,const UINT*,IUnknown*const*)>(s,39)(s,n,w,h,f,flags,masks,queues);}
void attach(IDXGISwapChain*swap,IUnknown*queue){if(!swap||!queue)return;try{renderer.bind(swap,queue);Com<IDXGISwapChain3>s;if(SUCCEEDED(swap->QueryInterface(__uuidof(IDXGISwapChain3),(void**)s.put()))){hook(s.p,8,(void*)present);hook(s.p,13,(void*)resize);hook(s.p,22,(void*)present1);hook(s.p,39,(void*)resize1);}}catch(const std::exception&e){log("[SS-HOOK-001] Attach failed: %s",e.what());}}
HRESULT STDMETHODCALLTYPE createSwap(IDXGIFactory*factory,IUnknown*q,DXGI_SWAP_CHAIN_DESC*d,IDXGISwapChain**s){auto hr=original<HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory*,IUnknown*,DXGI_SWAP_CHAIN_DESC*,IDXGISwapChain**)>(factory,10)(factory,q,d,s);if(SUCCEEDED(hr))attach(*s,q);return hr;}
HRESULT STDMETHODCALLTYPE createHwnd(IDXGIFactory2*factory,IUnknown*q,HWND hwnd,const DXGI_SWAP_CHAIN_DESC1*d,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*f,IDXGIOutput*o,IDXGISwapChain1**s){auto hr=original<HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,HWND,const DXGI_SWAP_CHAIN_DESC1*,const DXGI_SWAP_CHAIN_FULLSCREEN_DESC*,IDXGIOutput*,IDXGISwapChain1**)>(factory,15)(factory,q,hwnd,d,f,o,s);if(SUCCEEDED(hr))attach(*s,q);return hr;}
HRESULT STDMETHODCALLTYPE createCore(IDXGIFactory2*factory,IUnknown*q,IUnknown*win,const DXGI_SWAP_CHAIN_DESC1*d,IDXGIOutput*o,IDXGISwapChain1**s){auto hr=original<HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,IUnknown*,const DXGI_SWAP_CHAIN_DESC1*,IDXGIOutput*,IDXGISwapChain1**)>(factory,16)(factory,q,win,d,o,s);if(SUCCEEDED(hr))attach(*s,q);return hr;}
HRESULT STDMETHODCALLTYPE createComposition(IDXGIFactory2*factory,IUnknown*q,const DXGI_SWAP_CHAIN_DESC1*d,IDXGIOutput*o,IDXGISwapChain1**s){auto hr=original<HRESULT(STDMETHODCALLTYPE*)(IDXGIFactory2*,IUnknown*,const DXGI_SWAP_CHAIN_DESC1*,IDXGIOutput*,IDXGISwapChain1**)>(factory,24)(factory,q,d,o,s);if(SUCCEEDED(hr))attach(*s,q);return hr;}
void hookFactory(void*object){if(!object)return;try{hook(object,10,(void*)createSwap);Com<IDXGIFactory2>f;if(SUCCEEDED(((IUnknown*)object)->QueryInterface(__uuidof(IDXGIFactory2),(void**)f.put()))){hook(f.p,10,(void*)createSwap);hook(f.p,15,(void*)createHwnd);hook(f.p,16,(void*)createCore);hook(f.p,24,(void*)createComposition);}}catch(const std::exception&e){log("[SS-HOOK-002] Factory hook failed: %s",e.what());}}
}
extern "C" HRESULT WINAPI ProxyCreateDXGIFactory(REFIID iid,void**out){auto fn=real<HRESULT(WINAPI*)(REFIID,void**)>("CreateDXGIFactory");if(!fn)return E_NOINTERFACE;auto hr=fn(iid,out);if(SUCCEEDED(hr))hookFactory(*out);return hr;}
extern "C" HRESULT WINAPI ProxyCreateDXGIFactory1(REFIID iid,void**out){auto fn=real<HRESULT(WINAPI*)(REFIID,void**)>("CreateDXGIFactory1");if(!fn)return E_NOINTERFACE;auto hr=fn(iid,out);if(SUCCEEDED(hr))hookFactory(*out);return hr;}
extern "C" HRESULT WINAPI ProxyCreateDXGIFactory2(UINT flags,REFIID iid,void**out){auto fn=real<HRESULT(WINAPI*)(UINT,REFIID,void**)>("CreateDXGIFactory2");if(!fn)return E_NOINTERFACE;auto hr=fn(flags,iid,out);if(SUCCEEDED(hr))hookFactory(*out);return hr;}
extern "C" HRESULT WINAPI ProxyDXGIGetDebugInterface1(UINT flags,REFIID iid,void**out){auto fn=real<HRESULT(WINAPI*)(UINT,REFIID,void**)>("DXGIGetDebugInterface1");return fn?fn(flags,iid,out):E_NOINTERFACE;}
extern "C" HRESULT WINAPI ProxyDXGIDeclareAdapterRemovalSupport(){auto fn=real<HRESULT(WINAPI*)()>("DXGIDeclareAdapterRemovalSupport");return fn?fn():S_OK;}
BOOL WINAPI DllMain(HINSTANCE module,DWORD reason,LPVOID){if(reason==DLL_PROCESS_ATTACH){ownModule=module;DisableThreadLibraryCalls(module);}return TRUE;}
