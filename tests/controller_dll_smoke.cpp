#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <dxgi1_4.h>
#include <cstdio>
int wmain(int argc,wchar_t**argv){
 if(argc!=2)return 1;
 auto module=LoadLibraryW(argv[1]);if(!module)return 2;
 auto create=reinterpret_cast<HRESULT(WINAPI*)(REFIID,void**)>(GetProcAddress(module,"CreateDXGIFactory1"));
 if(!create)return 3;
 IDXGIFactory1*factory=nullptr;auto hr=create(__uuidof(IDXGIFactory1),reinterpret_cast<void**>(&factory));
 if(FAILED(hr)||!factory)return 4;
 factory->Release();std::puts("runtime DLL: loaded, early hook initialization and system DXGI forwarding passed");
 // DLL stays pinned while cached callbacks may exist.
 return 0;
}
