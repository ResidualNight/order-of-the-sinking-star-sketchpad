#include "gamepad_bridge.hpp"
#include "version.hpp"
#include <cstdio>
#include <cwchar>
#include <cstring>
using namespace sketch::pad;
static GetState game=nullptr;
static HWND button=nullptr,label=nullptr;
static unsigned nonzeroBlocked=0,leaks=0;
static bool foreground=true;
static bool neutral(const XINPUT_STATE&s){
 auto v=s.Gamepad;return !v.wButtons&&!v.bLeftTrigger&&!v.bRightTrigger&&!v.sThumbLX&&!v.sThumbLY&&!v.sThumbRX&&!v.sThumbRY;
}
static void refresh(){
 wchar_t text[4096];int n=swprintf(text,4096,
 L"Controller isolation prototype 2 (core %hs; separate process)\r\n"
 L"F8 / button: toggle capture. F9: emergency pass-through. Esc: close.\r\n"
 L"Capture: %s   Window active: %s\r\n\r\n",
 sketch::CurrentVersion,capturing()?L"ON":L"OFF",foreground?L"YES":L"NO");
 for(DWORD i=0;i<4;++i){
  XINPUT_STATE raw{},filtered{};DWORD a=sample(0,i,&raw),b=game(i,&filtered);
  if(capturing()&&a==0&&b==0&&!neutral(raw)){if(neutral(filtered))++nonzeroBlocked;else ++leaks;}
  n+=swprintf(text+n,4096-n,L"Slot %lu: raw=%lu game=%lu\r\n"
   L"  raw : buttons=%04x LT=%3u RT=%3u LX=%6d LY=%6d RX=%6d RY=%6d\r\n"
   L"  game: buttons=%04x LT=%3u RT=%3u LX=%6d LY=%6d RX=%6d RY=%6d packet=%lu\r\n",
   i,a,b,raw.Gamepad.wButtons,raw.Gamepad.bLeftTrigger,raw.Gamepad.bRightTrigger,raw.Gamepad.sThumbLX,raw.Gamepad.sThumbLY,raw.Gamepad.sThumbRX,raw.Gamepad.sThumbRY,
   filtered.Gamepad.wButtons,filtered.Gamepad.bLeftTrigger,filtered.Gamepad.bRightTrigger,filtered.Gamepad.sThumbLX,filtered.Gamepad.sThumbLY,filtered.Gamepad.sThumbRX,filtered.Gamepad.sThumbRY,filtered.dwPacketNumber);
 }
 swprintf(text+n,4096-n,L"\r\nActive raw samples blocked: %u   Unexpected non-neutral results: %u\r\n"
 L"Zero blocked samples is NOT a successful isolation test.\r\n"
 L"This validates this process only, NOT the real game or its Steam hook chain.\r\n"
 L"After exit, held controls stay blocked until released / centered.\r\n"
 L"No input is sent to the game; no files or Steam configuration are changed.\r\n%s",nonzeroBlocked,leaks,stats().degraded?L"[SS-INPUT-004] Hook endpoint limit or chain failure. Capture disabled.":L"");
 SetWindowTextW(label,text);
 SetWindowTextW(button,capturing()?L"Stop capture (F8)":L"Start capture (F8)");
}
static void toggle(){if(!foreground)return;setCapturing(!capturing());refresh();}
static LRESULT CALLBACK proc(HWND w,UINT m,WPARAM wp,LPARAM lp){
 switch(m){
 case WM_CREATE:
  button=CreateWindowW(L"BUTTON",L"Start capture (F8)",WS_CHILD|WS_VISIBLE|BS_PUSHBUTTON,15,15,220,32,w,(HMENU)1,nullptr,nullptr);
  label=CreateWindowW(L"STATIC",L"",WS_CHILD|WS_VISIBLE,15,60,920,610,w,nullptr,nullptr,nullptr);
  SendMessageW(label,WM_SETFONT,(WPARAM)GetStockObject(ANSI_FIXED_FONT),TRUE);SetTimer(w,1,33,nullptr);return 0;
 case WM_COMMAND:if(LOWORD(wp)==1)toggle();return 0;
 case WM_TIMER:refresh();return 0;
 case WM_KEYDOWN:
  if(wp==VK_F8)toggle();else if(wp==VK_F9){failOpen();refresh();}else if(wp==VK_ESCAPE)DestroyWindow(w);return 0;
 case WM_ACTIVATE:foreground=LOWORD(wp)!=WA_INACTIVE;if(!foreground)setCapturing(false);return 0;
 case WM_DESTROY:KillTimer(w,1);failOpen();remove();PostQuitMessage(0);return 0;
 }return DefWindowProcW(w,m,wp,lp);
}
int wmain(int argc,wchar_t**argv){
 bool snapshot=argc==2&&wcscmp(argv[1],L"--snapshot")==0;
 bool uiTest=argc==2&&wcscmp(argv[1],L"--ui-selftest")==0;
 auto report=install(GetModuleHandleW(nullptr));
 if(!report.installed){std::puts("[SS-INPUT-002] Cannot install prototype consumer hook.");return 2;}
 // System32 only: never resolve an arbitrary DLL beside the probe.
 auto module=LoadLibraryExW(L"xinput1_4.dll",nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32);
 if(!module){std::puts("[SS-INPUT-003] System XInput unavailable.");remove();return 3;}
 game=reinterpret_cast<GetState>(GetProcAddress(module,"XInputGetState"));
 if(!game||endpointCount()!=1){std::puts("[SS-INPUT-002] XInput endpoint was not wrapped.");remove();return 2;}
 if(snapshot){
  unsigned connected=0;
  for(DWORD i=0;i<4;++i){XINPUT_STATE state{};auto result=sample(0,i,&state);if(result==0)++connected;std::printf("slot=%lu connected=%d status=%lu\n",i,result==0,result);}
  std::printf("probe=2 core=%s probe_only=1 resolver_hooks=%u endpoints=%u connected=%u real_game_verified=0\n",sketch::CurrentVersion,report.resolvers,endpointCount(),connected);
  remove();FreeLibrary(module);return 0;
 }
 WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"StarSketchControllerProbe";wc.hCursor=LoadCursor(nullptr,IDC_ARROW);wc.hbrBackground=(HBRUSH)(COLOR_WINDOW+1);
 if(!RegisterClassW(&wc)){std::puts("[SS-INPUT-005] Probe window class creation failed.");remove();return 4;}
 auto window=CreateWindowW(wc.lpszClassName,L"StarSketch - Controller Isolation Prototype 2",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,980,760,nullptr,nullptr,wc.hInstance,nullptr);
 if(!window){std::puts("[SS-INPUT-005] Probe window creation failed.");remove();return 4;}
 if(uiTest){
  SendMessageW(window,WM_KEYDOWN,VK_F8,0);
  bool ok=capturing();wchar_t rendered[4096]{};GetWindowTextW(label,rendered,4096);
  ok=ok&&wcsstr(rendered,L"Slot 3:")&&wcsstr(rendered,L"NOT the real game")&&!wcsstr(rendered,L"\\r");
  SendMessageW(window,WM_ACTIVATE,WA_INACTIVE,0);ok=ok&&!capturing();
  SendMessageW(window,WM_ACTIVATE,WA_ACTIVE,0);SendMessageW(window,WM_KEYDOWN,VK_F8,0);ok=ok&&capturing();
  SendMessageW(window,WM_KEYDOWN,VK_F9,0);ok=ok&&!capturing();
  DestroyWindow(window);FreeLibrary(module);
  std::puts(ok?"probe UI: hidden window, text rendering, capture, focus loss and emergency reset passed":"[SS-INPUT-005] Probe UI self-test failed.");
  return ok?0:4;
 }
 ShowWindow(window,SW_SHOW);
 MSG msg;while(GetMessageW(&msg,nullptr,0,0)>0){
  // Buttons may own keyboard focus: process local shortcuts at the message loop.
  if(msg.message==WM_KEYDOWN&&(msg.wParam==VK_F8||msg.wParam==VK_F9||msg.wParam==VK_ESCAPE)){
   if(!(msg.lParam&(1LL<<30)))SendMessageW(window,msg.message,msg.wParam,msg.lParam);
   continue;
  }
  TranslateMessage(&msg);DispatchMessageW(&msg);
 }
 FreeLibrary(module);return 0;
}
