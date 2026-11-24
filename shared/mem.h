#pragma once
#include <windows.h>
#include <cstdint>
#include <string>

namespace mem {

extern HANDLE g_hProc;
extern DWORD  g_pid;

DWORD FindPid(const wchar_t* exeName);
bool  Attach(const wchar_t* exeName);
bool  WaitAttach(const wchar_t* exeName, DWORD timeoutMs = 0);
void  Detach();
bool  IsAlive();

bool ExtRPM(uintptr_t addr, void* buf, size_t sz);

template <typename T>
inline T ExtRead(uintptr_t a) {
    T v{};
    ExtRPM(a, &v, sizeof(T));
    return v;
}

template <typename T>
inline bool ExtReadTo(uintptr_t a, T& out) {
    return ExtRPM(a, &out, sizeof(T));
}

std::string ReadCStr(uintptr_t addr, size_t max = 128);

} // namespace mem
