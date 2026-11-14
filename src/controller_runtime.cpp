#include "controller_runtime.hpp"
#include "controller_probe.hpp"
#include "gamepad_bridge.hpp"
#include "ui.hpp"
#include <cstdio>
namespace sketch::controller {
namespace {unsigned endpoint=0,slot=0;bool selected=false;ULONGLONG retryAt=0;PadInput last{};}
void cancel(App&app){pad::enableEntry(false);pad::acknowledgeOpen();pad::setCapturing(false);app.resetPadGesture();last={};}
void synchronize(App&app,bool foreground){
 if(!foreground){app.setEditing(false);cancel(app);return;}
 if(pad::capturing()!=app.editing)pad::setCapturing(app.editing);
 pad::enableEntry(app.padEntryEnabled&&!app.editing);
}
void poll(App&app,bool foreground){
 probe::maintain();auto now=GetTickCount64();auto request=pad::openRequest();
 if(!foreground){app.setEditing(false);cancel(app);app.padPrepare({});return;}
 if(request){
  endpoint=(request-1)/4;slot=(request-1)%4;selected=true;

 }
 XINPUT_STATE state{};DWORD result=selected?pad::sample(endpoint,slot,&state):ERROR_DEVICE_NOT_CONNECTED;
 if(!request&&(!app.editing||!selected)&&result!=ERROR_SUCCESS&&result!=ERROR_RETRY&&now>=retryAt){
  retryAt=now+1000;
  for(unsigned e=0;e<pad::endpointCount();++e){for(unsigned s=0;s<4;++s){
   XINPUT_STATE candidate{};if(pad::sample(e,s,&candidate)==ERROR_SUCCESS){endpoint=e;slot=s;selected=true;state=candidate;result=ERROR_SUCCESS;break;}
  }if(result==ERROR_SUCCESS)break;}
 }
 if(request){if(result==ERROR_SUCCESS&&app.padEntryEnabled&&!app.editing){pad::setCapturing(true);app.openFromPad();}pad::acknowledgeOpen();}
 PadInput input{};
 if(result==ERROR_SUCCESS){auto&v=state.Gamepad;input={true,v.wButtons,v.sThumbLX/32768.f,v.sThumbLY/32768.f,v.sThumbRX/32768.f,v.sThumbRY/32768.f,v.bLeftTrigger,v.bRightTrigger};last=input;}
 else if(result==ERROR_RETRY)input=last;
 else{last={};if(!app.editing)selected=false;}
 app.padPrepare(input);
 auto status=pad::stats();char text[256];std::snprintf(text,sizeof(text),"Controller preview 8.3 | slot %u | game queries %llu | endpoints %u | filtered %llu%s",slot,static_cast<unsigned long long>(status.gameCalls),status.endpoints,static_cast<unsigned long long>(status.neutralized),status.degraded?" | [SS-INPUT-004] unavailable":"");app.padStatus=text;
 if(status.degraded&&app.padPointer){app.setEditing(false);app.padPointer=false;app.status="[SS-INPUT-004] 手柄输入不可用，请使用键鼠操作";}
 synchronize(app,foreground);
}
}
