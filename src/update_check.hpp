#pragma once
#include "update_notice.hpp"
#include <atomic>
#include <functional>
namespace sketch {
inline constexpr const wchar_t* ReleaseRoot=L"https://github.com/ResidualNight/order-of-the-sinking-star-sketchpad/releases";
using UpdateRequest=std::function<bool(const std::wstring& url,bool head,size_t limit,std::string& body)>;
// Returns a numeric version only after both official installer assets exist.
uint64_t checkUpdate(const std::string& current,const UpdateRequest& request);
uint64_t fetchAvailableUpdate(const std::string& current);
bool allowedUpdateRedirect(const std::wstring& url,const std::wstring& original);
class RuntimeUpdateChecker {
 std::atomic<bool> running{false};std::atomic<uint64_t> available{0};UpdateCheckSchedule schedule;
 static unsigned long __stdcall worker(void* context);
public:
 uint64_t poll(uint64_t now);
};
}
