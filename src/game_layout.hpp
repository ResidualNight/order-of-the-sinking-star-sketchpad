#pragma once
#include <cstdint>
#include <string_view>
namespace sketch {
struct GameLayout {const char* sha256;uintptr_t managerRva;const char* name;};
inline constexpr GameLayout GameLayouts[]={
 {"0ae10a513dae25286fe93d0db4fa8a2df9a473bc07e1734e12bc1ad30166db1c",0xb69600,"r67271"},
 {"d802098b7cb85eadfc733b88d1554042fad950c572e916febea3f669a474a549",0xb68618,"steam-26249670"}
};
inline const GameLayout* knownGameLayout(std::string_view hash){for(const auto& layout:GameLayouts)if(hash==layout.sha256)return &layout;return nullptr;}
}
