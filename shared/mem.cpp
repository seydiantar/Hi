#include "mem.h"
#include "log.h"
#include <tlhelp32.h>
#include <thread>
#include <chrono>

namespace mem {

HANDLE g_hProc = nullptr;
DWORD  g_pid   = 0;

DWORD FindPid(const wchar_t* exeName) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) };
    DWORD pid = 0;
    for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
        if (_wcsicmp(pe.szExeFile, exeName) == 0) { pid = pe.th32ProcessID; break; }
    }
    CloseHandle(snap);
    return pid;
}

bool Attach(const wchar_t* exeName) {
    DWORD pid = FindPid(exeName);
    if (!pid) return false;
    HANDLE h = OpenProcess(PROCESS_VM_READ, FALSE, pid);
    if (!h) { hilog::Err("OpenProcess pid=%lu gle=%lu", pid, GetLastError()); return false; }
    Detach();
    g_hProc = h;
    g_pid   = pid;
    hilog::Info("attached pid=%lu (%ls)", pid, exeName);
    return true;
}

bool WaitAttach(const wchar_t* exeName, DWORD timeoutMs) {
    const auto start = std::chrono::steady_clock::now();
    while (true) {
        if (Attach(exeName)) return true;
        if (timeoutMs) {
            auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::steady_clock::now() - start).count();
            if ((DWORD)elapsed >= timeoutMs) return false;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }
}

void Detach() {
    if (g_hProc) CloseHandle(g_hProc);
    g_hProc = nullptr;
    g_pid   = 0;
}

bool IsAlive() {
    if (!g_hProc) return false;
    DWORD code = 0;
    if (!GetExitCodeProcess(g_hProc, &code)) return false;
    return code == STILL_ACTIVE;
}

bool ExtRPM(uintptr_t addr, void* buf, size_t sz) {
    if (!g_hProc) return false;
    SIZE_T out = 0;
    return ReadProcessMemory(g_hProc, (LPCVOID)addr, buf, sz, &out) && out == sz;
}

std::string ReadCStr(uintptr_t addr, size_t max) {
    std::string s;
    s.resize(max);
    SIZE_T got = 0;
    if (!ReadProcessMemory(g_hProc, (LPCVOID)addr, s.data(), max, &got)) return {};
    s.resize(got);
    auto nul = s.find('\0');
    if (nul != std::string::npos) s.resize(nul);
    return s;
}

} // namespace mem
