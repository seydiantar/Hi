#include "scanner.h"
#include "mem.h"
#include <tlhelp32.h>
#include <psapi.h>
#include <algorithm>
#include <cstring>

namespace scan {

std::optional<ModuleInfo> GetModule(DWORD pid, const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, pid);
    if (snap == INVALID_HANDLE_VALUE) return std::nullopt;
    MODULEENTRY32W me{ sizeof(me) };
    for (BOOL ok = Module32FirstW(snap, &me); ok; ok = Module32NextW(snap, &me)) {
        if (_wcsicmp(me.szModule, name) == 0) {
            CloseHandle(snap);
            return ModuleInfo{ (uintptr_t)me.modBaseAddr, me.modBaseSize };
        }
    }
    CloseHandle(snap);
    return std::nullopt;
}

Pattern PatternToBytes(const std::string& ida) {
    Pattern p;
    for (size_t i = 0; i < ida.size();) {
        if (ida[i] == ' ') { ++i; continue; }
        if (ida[i] == '?') {
            p.bytes.push_back(0);
            p.mask.push_back(false);
            i += (i + 1 < ida.size() && ida[i + 1] == '?') ? 2 : 1;
        } else {
            p.bytes.push_back((uint8_t)std::stoul(ida.substr(i, 2), nullptr, 16));
            p.mask.push_back(true);
            i += 2;
        }
    }
    return p;
}

static uintptr_t FindOne(uintptr_t base, size_t size, const Pattern& p, int& hitCount, int stopAt) {
    const size_t plen = p.bytes.size();
    if (plen == 0 || size < plen) return 0;

    constexpr size_t CHUNK = 0x10000;
    std::vector<uint8_t> buf(CHUNK + plen);
    uintptr_t first = 0;
    for (uintptr_t off = 0; off < size; off += CHUNK) {
        const size_t want = std::min<size_t>(CHUNK + plen - 1, size - off);
        SIZE_T got = 0;
        if (!ReadProcessMemory(mem::g_hProc, (LPCVOID)(base + off), buf.data(), want, &got) || got < plen) continue;
        const size_t last = got - plen;
        for (size_t i = 0; i <= last; ++i) {
            bool hit = true;
            for (size_t j = 0; j < plen; ++j) {
                if (p.mask[j] && buf[i + j] != p.bytes[j]) { hit = false; break; }
            }
            if (hit) {
                if (!first) first = base + off + i;
                if (++hitCount >= stopAt) return first;
            }
        }
    }
    return first;
}

uintptr_t FindPattern(uintptr_t base, size_t size, const std::string& sig) {
    const Pattern p = PatternToBytes(sig);
    int hits = 0;
    return FindOne(base, size, p, hits, 1);
}

int CountPattern(uintptr_t base, size_t size, const std::string& sig, int stopAt) {
    const Pattern p = PatternToBytes(sig);
    int hits = 0;
    FindOne(base, size, p, hits, stopAt);
    return hits;
}

std::pair<uintptr_t, size_t> GetTextSection(uintptr_t base) {
    IMAGE_DOS_HEADER dos = mem::ExtRead<IMAGE_DOS_HEADER>(base);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return { 0, 0 };
    IMAGE_NT_HEADERS64 nt = mem::ExtRead<IMAGE_NT_HEADERS64>(base + dos.e_lfanew);
    if (nt.Signature != IMAGE_NT_SIGNATURE) return { 0, 0 };
    uintptr_t sh = base + dos.e_lfanew + sizeof(IMAGE_NT_HEADERS64);
    for (WORD i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER s = mem::ExtRead<IMAGE_SECTION_HEADER>(sh + i * sizeof(IMAGE_SECTION_HEADER));
        if (memcmp(s.Name, ".text", 5) == 0)
            return { base + s.VirtualAddress, s.Misc.VirtualSize };
    }
    return { 0, 0 };
}

template <typename T>
T RipRelative(uintptr_t instrAddr, int dispOffset, int instrSize) {
    int32_t disp = mem::ExtRead<int32_t>(instrAddr + dispOffset);
    return (T)(instrAddr + instrSize + disp);
}
template uintptr_t RipRelative<uintptr_t>(uintptr_t, int, int);

} // namespace scan
