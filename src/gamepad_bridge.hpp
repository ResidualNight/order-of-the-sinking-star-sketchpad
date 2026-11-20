#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <xinput.h>
#include <cstdint>
namespace sketch::pad {
using GetState=DWORD(WINAPI*)(DWORD,XINPUT_STATE*);
struct HookReport {unsigned resolvers=0,directStates=0;bool installed=false;};
struct BridgeStats {uint64_t gameCalls=0,rawCalls=0,recursions=0;unsigned endpoints=0;bool degraded=false;uint64_t neutralized=0;};
// Import bridge plus the validated XInput cache in the game executable.
HookReport install(HMODULE consumer);
enum class CachedHook { Unsupported, Pending, Attached, Conflict, Unavailable };
// Called on the initialization/render owner thread, never from query callbacks.
CachedHook attachCachedGameState(HMODULE consumer);
void setCapturing(bool enabled);
void enableEntry(bool enabled);
// Encoded endpoint*4+slot+1; request holds all game inputs neutral until acknowledged.
unsigned openRequest();
void acknowledgeOpen();
void failOpen();
bool capturing();
unsigned endpointCount();
// An out-of-date successful query returns ERROR_RETRY and a zeroed output.
DWORD sample(unsigned endpoint,DWORD slot,XINPUT_STATE* state);
BridgeStats stats();
// Returns false if another hook owns one of our slots; never overwrites it.
// Cached wrappers remain valid after removal and become pass-through.
bool remove();
}
