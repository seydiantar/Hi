#include "sigs.h"
#include "mem.h"
#include "scanner.h"
#include "log.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <psapi.h>

namespace rbx::off {

Offsets g;
using json = nlohmann::json;

// Starter sig families. These rot weekly; re-derive at PC via x64dbg xref on
// the strings "DataModel" / "FakeDataModel" / "print" and on the mov/lea into
// a global pointer or vtable that lives there.
static const char* SIG_FAKE_DATAMODEL[] = {
    "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9",
    "48 83 EC ? 48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01",
    nullptr
};

static std::filesystem::path CachePath() {
    wchar_t ad[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, ad);
    auto p = std::filesystem::path(ad) / L"hi";
    std::error_code ec; std::filesystem::create_directories(p, ec);
    return p / L"rbx-sigs.json";
}

struct ModuleKey {
    std::string path;
    uint64_t    size;
    uint64_t    timestamp;
};

static ModuleKey KeyOf(const scan::ModuleInfo& m) {
    wchar_t buf[MAX_PATH]{};
    HMODULE h = (HMODULE)m.base;
    GetModuleFileNameExW(mem::g_hProc, h, buf, MAX_PATH);
    HANDLE f = CreateFileW(buf, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    FILETIME ft{};
    if (f != INVALID_HANDLE_VALUE) { GetFileTime(f, nullptr, nullptr, &ft); CloseHandle(f); }
    uint64_t ts = ((uint64_t)ft.dwHighDateTime << 32) | ft.dwLowDateTime;
    char utf[MAX_PATH * 2]{};
    WideCharToMultiByte(CP_UTF8, 0, buf, -1, utf, sizeof(utf), nullptr, nullptr);
    return { utf, m.size, ts };
}

static bool TryLoadCache(const ModuleKey& key) {
    std::ifstream f(CachePath());
    if (!f) return false;
    try {
        json j; f >> j;
        if (j["key"]["path"] != key.path)      return false;
        if (j["key"]["size"] != key.size)      return false;
        if (j["key"]["timestamp"] != key.timestamp) return false;

        g.FakeDataModelPtr        = j["offsets"]["FakeDataModelPtr"].get<uint64_t>();
        g.inst.ClassDescriptor    = j["offsets"]["inst.ClassDescriptor"].get<uint32_t>();
        g.inst.Name               = j["offsets"]["inst.Name"].get<uint32_t>();
        g.inst.Parent             = j["offsets"]["inst.Parent"].get<uint32_t>();
        g.inst.Children           = j["offsets"]["inst.Children"].get<uint32_t>();
        g.cls.Name                = j["offsets"]["cls.Name"].get<uint32_t>();
        g.part.CFrame             = j["offsets"]["part.CFrame"].get<uint32_t>();
        g.cam.CoordinateFrame     = j["offsets"]["cam.CoordinateFrame"].get<uint32_t>();
        g.cam.ViewportSize        = j["offsets"]["cam.ViewportSize"].get<uint32_t>();
        g.cam.FieldOfView         = j["offsets"]["cam.FieldOfView"].get<uint32_t>();
        g.humanoid.Health         = j["offsets"]["humanoid.Health"].get<uint32_t>();
        g.humanoid.MaxHealth      = j["offsets"]["humanoid.MaxHealth"].get<uint32_t>();
        g.player.LocalPlayer      = j["offsets"]["player.LocalPlayer"].get<uint32_t>();
        g.player.Character        = j["offsets"]["player.Character"].get<uint32_t>();
        g.player.Team             = j["offsets"]["player.Team"].get<uint32_t>();
        log::Info("rbx sigs loaded from cache");
        return true;
    } catch (const std::exception& e) {
        log::Warn("cache parse: %s", e.what());
        return false;
    }
}

bool SaveCache() {
    auto mod = scan::GetModule(mem::g_pid, L"RobloxPlayerBeta.exe");
    if (!mod) return false;
    auto key = KeyOf(*mod);

    json j;
    j["key"]["path"]      = key.path;
    j["key"]["size"]      = key.size;
    j["key"]["timestamp"] = key.timestamp;

    j["offsets"]["FakeDataModelPtr"]     = (uint64_t)g.FakeDataModelPtr;
    j["offsets"]["inst.ClassDescriptor"] = g.inst.ClassDescriptor;
    j["offsets"]["inst.Name"]            = g.inst.Name;
    j["offsets"]["inst.Parent"]          = g.inst.Parent;
    j["offsets"]["inst.Children"]        = g.inst.Children;
    j["offsets"]["cls.Name"]             = g.cls.Name;
    j["offsets"]["part.CFrame"]          = g.part.CFrame;
    j["offsets"]["cam.CoordinateFrame"]  = g.cam.CoordinateFrame;
    j["offsets"]["cam.ViewportSize"]     = g.cam.ViewportSize;
    j["offsets"]["cam.FieldOfView"]      = g.cam.FieldOfView;
    j["offsets"]["humanoid.Health"]      = g.humanoid.Health;
    j["offsets"]["humanoid.MaxHealth"]   = g.humanoid.MaxHealth;
    j["offsets"]["player.LocalPlayer"]   = g.player.LocalPlayer;
    j["offsets"]["player.Character"]     = g.player.Character;
    j["offsets"]["player.Team"]          = g.player.Team;

    std::ofstream f(CachePath());
    if (!f) return false;
    f << j.dump(2);
    log::Info("rbx sigs cached");
    return true;
}

static uintptr_t TrySigFamily(uintptr_t textBase, size_t textSize, const char* const sigs[]) {
    for (int i = 0; sigs[i]; ++i) {
        int hits = scan::CountPattern(textBase, textSize, sigs[i], 4);
        if (hits == 1) {
            uintptr_t at = scan::FindPattern(textBase, textSize, sigs[i]);
            uintptr_t resolved = scan::RipRelative<uintptr_t>(at, 3, 7);
            log::Info("sig '%s' hit=1 at=0x%p resolved=0x%p", sigs[i], (void*)at, (void*)resolved);
            return resolved;
        }
        log::Dbg("sig '%s' hits=%d", sigs[i], hits);
    }
    return 0;
}

bool Resolve() {
    auto mod = scan::GetModule(mem::g_pid, L"RobloxPlayerBeta.exe");
    if (!mod) { log::Err("RobloxPlayerBeta.exe module not found"); return false; }

    auto key = KeyOf(*mod);
    if (TryLoadCache(key)) return true;

    auto [textBase, textSize] = scan::GetTextSection(mod->base);
    if (!textBase) { log::Err("could not parse .text"); return false; }
    log::Info("scanning .text base=0x%p size=0x%zX", (void*)textBase, textSize);

    g.FakeDataModelPtr = TrySigFamily(textBase, textSize, SIG_FAKE_DATAMODEL);
    if (!g.FakeDataModelPtr) { log::Err("FakeDataModel sig rot"); return false; }

    // FILL: additional sig families for instance.Name offset, child vector offset,
    // CFrame offset. Starter values come from the struct defaults; verify them via
    // probe below.
    SaveCache();
    return true;
}

bool SanityProbe() {
    uintptr_t dm = mem::ExtRead<uintptr_t>(g.FakeDataModelPtr);
    if (!dm) { log::Err("DataModel NULL (not in a place yet?)"); return true; /* soft */ }

    // walk to a known-named child and confirm Name
    // FILL: wire this up once Resolve() populates inst.* reliably
    log::Info("DataModel=0x%p", (void*)dm);
    return true;
}

} // namespace rbx::off
