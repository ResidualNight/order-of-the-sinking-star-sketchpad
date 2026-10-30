#pragma once
#include "errors.hpp"
#include <atomic>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
namespace sketch {
inline constexpr const char* FeedbackAddress="xlwpy@qq.com";
inline constexpr const char* DiagnosticsBuild="game build 26249670 compatibility 20261030 | automatic candidate snapping 20261030 | aspect preview mapping 20261030 | independent camera fallback 20261029 | diagnostic copy 20261029";
enum class FeedbackAction {OpenFolderAndCopyEmail,CopyDiagnostics};
struct ReportDiagnostics {
 uint32_t outputWidth=0,outputHeight=0,windowWidth=0,windowHeight=0;
 std::string graphics,windowMode,error,status,compatibility,cameraKey,level;
 uint64_t gridRequest=0;size_t cameraChoices=0;
 bool levelPaused=false,gridPinned=false,gridConfirmed=false;
};
struct ReportEnvironment {
 std::string utc,windows,architecture;uint32_t processors=0;uint64_t memoryMiB=0;
};
ReportEnvironment collectReportEnvironment();
std::string formatReportDiagnostics(const ReportDiagnostics& report,const ReportEnvironment& environment);
struct FeedbackResult {
 bool folderOpened=false,addressCopied=false;unsigned long folderError=0,clipboardError=0;
 FeedbackAction action=FeedbackAction::OpenFolderAndCopyEmail;bool diagnosticsCopied=false;
};
std::string feedbackResultMessage(const FeedbackResult& result);
FeedbackResult performFeedback(const std::wstring& folder,const std::function<unsigned long()>& copy,const std::function<unsigned long(const std::wstring&)>& open);
FeedbackResult performDiagnostics(const ReportDiagnostics& report,const ReportEnvironment& environment,const std::function<unsigned long(const std::string&)>& copy);
class FeedbackLauncher {
 struct Work;std::shared_ptr<Work> active;
 static unsigned long __stdcall worker(void* context);
 bool launch();
public:
 bool busy()const;
 bool start(const std::wstring& gameFolder,uintptr_t window);
 bool startDiagnostics(const ReportDiagnostics& report,uintptr_t window);
 bool takeResult(FeedbackResult& result);
};
}
