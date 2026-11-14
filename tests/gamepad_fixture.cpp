#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define XInputGetState FixtureHeaderGetState
#include <xinput.h>
#undef XInputGetState
#include <array>
static SRWLOCK lock=SRWLOCK_INIT;
static std::array<XINPUT_STATE,4> states{};
static std::array<DWORD,4> results{};
extern "C" __declspec(dllexport) void WINAPI FixtureState(DWORD slot,const XINPUT_STATE*state,DWORD result){
 if(slot>=4)return;AcquireSRWLockExclusive(&lock);states[slot]=*state;results[slot]=result;ReleaseSRWLockExclusive(&lock);
}
extern "C" __declspec(dllexport) DWORD WINAPI XInputGetState(DWORD slot,XINPUT_STATE*state){
 if(slot>=4||!state)return ERROR_BAD_ARGUMENTS;
 AcquireSRWLockShared(&lock);auto result=results[slot];if(result==0)*state=states[slot];ReleaseSRWLockShared(&lock);return result;
}
