#pragma once
#include "ui.hpp"
#include <cstdint>
#include <string>
namespace sketch {
// Only user preferences are serialized; documents, history and calibrated grids never are.
std::string encodePreferences(const App& app);
bool decodePreferences(const std::string& text,App& app);
class PreferenceStore {
 std::wstring path;std::string saved,lastSeen;uint64_t lastChange=0,retryAfter=0;bool invalidOriginal=false,initialized=false;
 bool resolvePath();bool write(const std::string& data);
public:
 explicit PreferenceStore(std::wstring file={}):path(std::move(file)){}
 ErrorId load(App& app);
 ErrorId update(App& app,uint64_t now,bool flush=false);
};
}
