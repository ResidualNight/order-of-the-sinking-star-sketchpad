#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shellapi.h>
#include "feedback.hpp"
#include "version.hpp"
#include <cstdio>
#include <cstring>
#include <new>
#include <sstream>
#include <utility>
namespace sketch {
ReportEnvironment collectReportEnvironment(){
 ReportEnvironment result;SYSTEMTIME utc{};GetSystemTime(&utc);char text[96];
 std::snprintf(text,sizeof(text),"%04u-%02u-%02uT%02u:%02u:%02uZ",unsigned(utc.wYear),unsigned(utc.wMonth),unsigned(utc.wDay),unsigned(utc.wHour),unsigned(utc.wMinute),unsigned(utc.wSecond));result.utc=text;
 using VersionFunction=LONG(WINAPI*)(OSVERSIONINFOW*);auto ntdll=GetModuleHandleW(L"ntdll.dll");
 auto version=ntdll?reinterpret_cast<VersionFunction>(GetProcAddress(ntdll,"RtlGetVersion")):nullptr;OSVERSIONINFOW info{};info.dwOSVersionInfoSize=sizeof(info);
 if(version&&version(&info)==0){std::snprintf(text,sizeof(text),"Windows %lu.%lu (build %lu)",info.dwMajorVersion,info.dwMinorVersion,info.dwBuildNumber);result.windows=text;}
 SYSTEM_INFO system{};GetNativeSystemInfo(&system);result.processors=system.dwNumberOfProcessors;
 switch(system.wProcessorArchitecture){case PROCESSOR_ARCHITECTURE_AMD64:result.architecture="x64";break;case PROCESSOR_ARCHITECTURE_ARM64:result.architecture="ARM64 (x64 process)";break;default:result.architecture="unknown (x64 process)";}
 MEMORYSTATUSEX memory{};memory.dwLength=sizeof(memory);if(GlobalMemoryStatusEx(&memory))result.memoryMiB=memory.ullTotalPhys/(1024*1024);
 return result;
}
std::string formatReportDiagnostics(const ReportDiagnostics& r,const ReportEnvironment& e){
 auto value=[](const std::string& text)->std::string{return text.empty()?"未取得":text;};
 auto dimensions=[](uint32_t w,uint32_t h){return w&&h?std::to_string(w)+" x "+std::to_string(h):std::string("未取得");};
 std::ostringstream out;out<<"StarSketch 错误诊断\r\n采集时间 (UTC)："<<value(e.utc)<<"\r\n插件版本："<<CurrentVersion<<" ("<<DiagnosticsBuild<<")\r\n"
  <<"游戏输出分辨率："<<dimensions(r.outputWidth,r.outputHeight)<<"\r\n游戏窗口尺寸："<<dimensions(r.windowWidth,r.windowHeight)<<"\r\n窗口模式："<<value(r.windowMode)<<"\r\n"
  <<"系统："<<value(e.windows)<<"\r\n系统架构："<<value(e.architecture)<<"\r\n逻辑处理器："<<(e.processors?std::to_string(e.processors):"未取得")
  <<"\r\n物理内存 (MiB)："<<(e.memoryMiB?std::to_string(e.memoryMiB):"未取得")<<"\r\n图形环境：DirectX 12\r\n"<<(r.graphics.empty()?"显卡与驱动：未取得":r.graphics)<<"\r\n"
  <<"游戏版本兼容提示："<<value(r.compatibility)<<"\r\n关卡："<<value(r.level)<<"\r\n关卡暂不可用："<<(r.levelPaused?"是":"否")
  <<"\r\n相机检查按键："<<value(r.cameraKey)<<"\r\n定位请求："<<r.gridRequest<<"\r\n当前候选数："<<r.cameraChoices
  <<"\r\n手动网格已保留："<<(r.gridPinned?"是":"否")<<"\r\n网格已确认："<<(r.gridConfirmed?"是":"否")
  <<"\r\n\r\n汇报错误：\r\n"<<value(r.error)<<"\r\n当前状态：\r\n"<<value(r.status)
  <<"\r\n\r\n请补充触发操作，并附上游戏目录中的 StarSketch.log。\r\n";
 return out.str();
}
FeedbackResult performDiagnostics(const ReportDiagnostics& report,const ReportEnvironment& environment,const std::function<unsigned long(const std::string&)>& copy){
 FeedbackResult result;result.action=FeedbackAction::CopyDiagnostics;result.clipboardError=copy(formatReportDiagnostics(report,environment));result.diagnosticsCopied=result.clipboardError==0;return result;
}
FeedbackResult performFeedback(const std::wstring& folder,const std::function<unsigned long()>& copy,const std::function<unsigned long(const std::wstring&)>& open){
 FeedbackResult result;result.clipboardError=copy();result.addressCopied=result.clipboardError==0;
 result.folderError=open(folder);result.folderOpened=result.folderError==0;return result;
}
std::string feedbackResultMessage(const FeedbackResult& result){
 if(result.action==FeedbackAction::CopyDiagnostics)return result.diagnosticsCopied?"已复制诊断信息，请粘贴到邮件正文，并附上 StarSketch.log。":errorMessage(ErrorId::ReportDiagnosticsUnavailable);
 if(result.folderOpened&&result.addressCopied)return "已打开游戏目录并复制邮箱 xlwpy@qq.com；请将 StarSketch.log 作为邮件附件发送。";
 std::string message;
 if(!result.folderOpened)message=errorMessage(ErrorId::ReportFolderUnavailable)+"。";
 if(!result.addressCopied)message+=errorMessage(ErrorId::ReportClipboardUnavailable)+"。";
 if(result.folderOpened)message+="已打开游戏目录。";
 if(result.addressCopied)message+="已复制邮箱 xlwpy@qq.com。";
 return message;
}
struct FeedbackLauncher::Work {
 std::wstring gameFolder;ReportDiagnostics report;FeedbackAction action=FeedbackAction::OpenFolderAndCopyEmail;
 uintptr_t window=0;std::atomic<bool> done{false};FeedbackResult result;
};
namespace {
struct FeedbackTicket {std::shared_ptr<void> work;HMODULE module;};
DWORD errorOrFailure(){DWORD error=GetLastError();return error?error:ERROR_GEN_FAILURE;}
DWORD copyText(HWND window,const std::wstring& text){
 size_t bytes=(text.size()+1)*sizeof(wchar_t);HGLOBAL data=GlobalAlloc(GMEM_MOVEABLE,bytes);if(!data)return errorOrFailure();
 void* pointer=GlobalLock(data);if(!pointer){DWORD error=errorOrFailure();GlobalFree(data);return error;}
 std::memcpy(pointer,text.c_str(),bytes);GlobalUnlock(data);
 if(!OpenClipboard(window)){DWORD error=errorOrFailure();GlobalFree(data);return error;}
 bool ok=EmptyClipboard()&&SetClipboardData(CF_UNICODETEXT,data);DWORD error=ok?0:errorOrFailure();CloseClipboard();
 if(!ok)GlobalFree(data);return error;
}
DWORD copyUtf8(HWND window,const std::string& text){
 int count=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.c_str(),-1,nullptr,0);if(!count)return errorOrFailure();
 std::wstring wide(size_t(count),L'\0');if(!MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,text.c_str(),-1,wide.data(),count))return errorOrFailure();wide.pop_back();return copyText(window,wide);
}
DWORD openFolder(const std::wstring& folder){
 DWORD attributes=GetFileAttributesW(folder.c_str());if(attributes==INVALID_FILE_ATTRIBUTES)return errorOrFailure();if(!(attributes&FILE_ATTRIBUTE_DIRECTORY))return ERROR_DIRECTORY;
 // Pass the directory as a shell target, never as command-line text.
 auto result=reinterpret_cast<INT_PTR>(ShellExecuteW(nullptr,L"open",folder.c_str(),nullptr,nullptr,SW_SHOWNORMAL));return result>32?0:(result?DWORD(result):ERROR_GEN_FAILURE);
}
}
bool FeedbackLauncher::busy()const{return active&&!active->done.load(std::memory_order_acquire);}
bool FeedbackLauncher::start(const std::wstring& gameFolder,uintptr_t window){
 if(busy())return false;
 active=std::make_shared<Work>();active->gameFolder=gameFolder;active->window=window;
 return launch();
}
bool FeedbackLauncher::startDiagnostics(const ReportDiagnostics& report,uintptr_t window){
 if(busy())return false;
 active=std::make_shared<Work>();active->report=report;active->window=window;active->action=FeedbackAction::CopyDiagnostics;
 return launch();
}
bool FeedbackLauncher::launch(){
 active->result.action=active->action;
 HMODULE module=nullptr;DWORD error=0;
 if(GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS,reinterpret_cast<LPCWSTR>(&FeedbackLauncher::worker),&module)){
  auto ticket=new(std::nothrow) FeedbackTicket{active,module};
  if(ticket){HANDLE thread=CreateThread(nullptr,0,&FeedbackLauncher::worker,ticket,0,nullptr);if(thread){CloseHandle(thread);return true;}
   error=GetLastError();delete ticket;
  }else error=ERROR_NOT_ENOUGH_MEMORY;
  FreeLibrary(module);
 }else error=GetLastError();
 active->result.folderError=active->result.clipboardError=error;active->done.store(true,std::memory_order_release);return false;
}
bool FeedbackLauncher::takeResult(FeedbackResult& result){
 if(!active||!active->done.load(std::memory_order_acquire))return false;
 result=active->result;active.reset();return true;
}
unsigned long __stdcall FeedbackLauncher::worker(void* context){
 auto ticket=static_cast<FeedbackTicket*>(context);HMODULE module=ticket->module;
 {auto work=std::static_pointer_cast<Work>(ticket->work);delete ticket;
  HRESULT com=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
  try{
   // Copy first: Explorer may move focus away from the game. This action never
   // reads or uploads the log and never opens a mail client.
   HWND window=reinterpret_cast<HWND>(work->window);
   if(work->action==FeedbackAction::CopyDiagnostics)work->result=performDiagnostics(work->report,collectReportEnvironment(),[&](const std::string& text){return copyUtf8(window,text);});
   else work->result=performFeedback(work->gameFolder,[&]{return copyText(window,L"xlwpy@qq.com");},openFolder);
  }catch(...){if(!work->result.folderOpened)work->result.folderError=ERROR_GEN_FAILURE;if(!work->result.addressCopied)work->result.clipboardError=ERROR_GEN_FAILURE;}
  if(SUCCEEDED(com))CoUninitialize();work->done.store(true,std::memory_order_release);
 }
 FreeLibraryAndExitThread(module,0);return 0;
}
}
