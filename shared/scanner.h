#pragma once
#include <windows.h>
#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace scan {

struct ModuleInfo { uintptr_t base; size_t size; };
struct Pattern    { std::vector<uint8_t> bytes; std::vector<bool> mask; };

std::optional<ModuleInfo>     GetModule(DWORD pid, const wchar_t* name);
std::pair<uintptr_t, size_t>  GetTextSection(uintptr_t base);
Pattern                       PatternToBytes(const std::string& ida);
uintptr_t                     FindPattern(uintptr_t base, size_t size, const std::string& sig);
int                           CountPattern(uintptr_t base, size_t size, const std::string& sig, int stopAt = 4);

template <typename T = uintptr_t>
T RipRelative(uintptr_t instrAddr, int dispOffset = 3, int instrSize = 7);

} // namespace scan
