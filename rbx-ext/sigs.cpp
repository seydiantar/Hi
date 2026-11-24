#include "sigs.h"
#include "probe.h"
#include "mem.h"
#include "scanner.h"
#include "log.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>
#include <psapi.h>
#include <cstring>
#include <vector>

namespace rbx::off {

Offsets g;
using json = nlohmann::json;

// Starter sig families. These rot every few weeks; probe::All() self-verifies
// everything downstream, so only this sig matters for the whole resolver to work.
// Re-derive at PC with x64dbg: xref "DataModel" or "FakeDataModel" in .rdata,
// follow to the `mov rax, [rip+disp]` that loads the global pointer.
static const char* SIG_FAKE_DATAMODEL[] = {
    // mov rax, [rip+disp]; mov rcx, [rax+off]; test rcx,rcx;
    "48 8B 05 ? ? ? ? 48 8B 88 ? ? ? ? 48 85 C9",
    // sub rsp,X; mov rcx, [rip+disp]; test rcx,rcx; je X; mov rax,[rcx]
    "48 83 EC ? 48 8B 0D ? ? ? ? 48 85 C9 74 ? 48 8B 01",
    // mov rcx, [rip+disp]; call X; mov rbx, rax — common in GetService path
    "48 8B 0D ? ? ? ? E8 ? ? ? ? 48 8B D8 48 85 C0",
    // lea rcx, [rip+disp]; mov [rax+off], rcx — FakeDataModel installer
    "48 8D 0D ? ? ? ? 48 89 ? ? ? ? ? E8",
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
        hilog::Info("rbx sigs loaded from cache");
        return true;
    } catch (const std::exception& e) {
        hilog::Warn("cache parse: %s", e.what());
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
    hilog::Info("rbx sigs cached");
    return true;
}

static uintptr_t TrySigFamily(uintptr_t textBase, size_t textSize, const char* const sigs[]) {
    for (int i = 0; sigs[i]; ++i) {
        int hits = scan::CountPattern(textBase, textSize, sigs[i], 4);
        if (hits == 1) {
            uintptr_t at = scan::FindPattern(textBase, textSize, sigs[i]);
            uintptr_t resolved = scan::RipRelative<uintptr_t>(at, 3, 7);
            hilog::Info("sig '%s' hit=1 at=0x%p resolved=0x%p", sigs[i], (void*)at, (void*)resolved);
            return resolved;
        }
        hilog::Dbg("sig '%s' hits=%d", sigs[i], hits);
    }
    return 0;
}

// Quick pre-filter: does *inst* smell like an Instance? Looks for a sane
// shared_ptr<vector<shared_ptr<Instance>>> at any of the usual Children offsets.
static bool LooksLikeInstance(uintptr_t inst, size_t minChildren) {
    static const uint32_t CHILD_CANDS[] = {
        0x60, 0x68, 0x70, 0x78, 0x80, 0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8
    };
    for (uint32_t off : CHILD_CANDS) {
        uintptr_t vp = 0;
        if (!mem::ExtRPM(inst + off, &vp, 8)) continue;
        if (vp < 0x10000 || vp > 0x7FFFFFFFFFFFULL) continue;
        uintptr_t b = 0, e = 0;
        if (!mem::ExtRPM(vp,     &b, 8)) continue;
        if (!mem::ExtRPM(vp + 8, &e, 8)) continue;
        if (!b || e <= b) continue;
        if ((e - b) % 16 != 0) continue;
        size_t n = (e - b) / 16;
        if (n >= minChildren && n <= 2000) return true;
    }
    return false;
}

// No-sig resolver. Walks RobloxPlayerBeta.exe's .data / .rdata / .bss sections,
// 8-byte aligned, looking for any pointer whose dereference lands on an
// Instance-shaped object with >=5 children. First candidate that passes
// probe::InstanceLayout with 3+ expected service-name matches wins — that's
// FakeDataModel. Returns the static pointer address (write to g.FakeDataModelPtr)
// or 0.
static uintptr_t ScanDataSectionsForDataModel(uintptr_t modBase) {
    IMAGE_DOS_HEADER dos = mem::ExtRead<IMAGE_DOS_HEADER>(modBase);
    if (dos.e_magic != IMAGE_DOS_SIGNATURE) return 0;
    IMAGE_NT_HEADERS64 nt = mem::ExtRead<IMAGE_NT_HEADERS64>(modBase + dos.e_lfanew);
    if (nt.Signature != IMAGE_NT_SIGNATURE) return 0;
    uintptr_t sh = modBase + dos.e_lfanew + sizeof(IMAGE_NT_HEADERS64);

    int candidatesSeen = 0, preFilterPassed = 0;

    for (WORD i = 0; i < nt.FileHeader.NumberOfSections; ++i) {
        IMAGE_SECTION_HEADER s = mem::ExtRead<IMAGE_SECTION_HEADER>(sh + i * sizeof(IMAGE_SECTION_HEADER));
        char nm[9]{};
        std::memcpy(nm, s.Name, 8);

        bool isData = !std::strncmp(nm, ".data",  5)
                   || !std::strncmp(nm, ".rdata", 6)
                   || !std::strncmp(nm, ".bss",   4);
        if (!isData) continue;

        uintptr_t secBase = modBase + s.VirtualAddress;
        size_t    secSize = s.Misc.VirtualSize;
        hilog::Info("scan: %s base=0x%p size=0x%zX", nm, (void*)secBase, secSize);

        constexpr size_t CHUNK = 0x10000;
        std::vector<uint8_t> buf(CHUNK);
        uintptr_t off = 0;
        while (off < secSize) {
            size_t want = (secSize - off < CHUNK) ? (secSize - off) : CHUNK;
            SIZE_T got = 0;
            if (!ReadProcessMemory(mem::g_hProc, (LPCVOID)(secBase + off),
                                   buf.data(), want, &got) || got < 8) {
                off += CHUNK;
                continue;
            }
            for (size_t j = 0; j + 8 <= got; j += 8) {
                uintptr_t inst;
                std::memcpy(&inst, buf.data() + j, 8);
                if (inst < 0x10000 || inst > 0x7FFFFFFFFFFFULL) continue;
                ++candidatesSeen;
                if (!LooksLikeInstance(inst, 5)) continue;
                ++preFilterPassed;

                // Save inst offsets; probe::InstanceLayout will try to set them
                // against this candidate. Restore on fail so bad candidates don't
                // leave g.inst in a corrupted state.
                auto saved = g.inst;
                if (probe::InstanceLayout(inst)) {
                    uintptr_t global = secBase + off + j;
                    hilog::Info("scan: DataModel FOUND after %d candidates (%d pre-filtered)",
                                candidatesSeen, preFilterPassed);
                    hilog::Info("scan: FakeDataModelPtr=0x%p  *ptr=0x%p",
                                (void*)global, (void*)inst);
                    return global;
                }
                g.inst = saved;
            }
            off += got;
        }
    }
    hilog::Err("scan: no DataModel found (%d candidates, %d pre-filter hits)",
               candidatesSeen, preFilterPassed);
    return 0;
}

bool Resolve() {
    auto mod = scan::GetModule(mem::g_pid, L"RobloxPlayerBeta.exe");
    if (!mod) { hilog::Err("RobloxPlayerBeta.exe module not found"); return false; }

    auto key = KeyOf(*mod);
    if (TryLoadCache(key)) {
        if (!probe::All()) hilog::Warn("cache loaded but probe revalidate failed - will rescan");
        else return true;
    }

    auto [textBase, textSize] = scan::GetTextSection(mod->base);
    if (!textBase) { hilog::Err("could not parse .text"); return false; }
    hilog::Info("scanning .text base=0x%p size=0x%zX", (void*)textBase, textSize);

    g.FakeDataModelPtr = TrySigFamily(textBase, textSize, SIG_FAKE_DATAMODEL);

    if (!g.FakeDataModelPtr) {
        hilog::Warn("all sig families rot - falling back to data-section pointer scan");
        g.FakeDataModelPtr = ScanDataSectionsForDataModel(mod->base);
    }

    if (!g.FakeDataModelPtr) { hilog::Err("FakeDataModel: sig scan AND data scan failed"); return false; }

    if (!probe::All()) { hilog::Err("probe chain failed after FakeDataModel resolved"); return false; }

    SaveCache();
    return true;
}

bool SanityProbe() {
    uintptr_t dm = mem::ExtRead<uintptr_t>(g.FakeDataModelPtr);
    if (!dm) { hilog::Err("DataModel NULL (not in a place yet?)"); return true; }

    // Cheap consistency check: find Workspace via the just-resolved offsets.
    if (!g.inst.Children || !g.inst.Name) { hilog::Warn("sanity: inst offsets unresolved"); return false; }
    // Walk one level and count children.
    uintptr_t vecPtr = mem::ExtRead<uintptr_t>(dm + g.inst.Children);
    if (!vecPtr) { hilog::Err("sanity: DataModel children vec null"); return false; }
    uintptr_t b = mem::ExtRead<uintptr_t>(vecPtr);
    uintptr_t e = mem::ExtRead<uintptr_t>(vecPtr + 8);
    if (!b || e <= b) { hilog::Err("sanity: DataModel children empty"); return false; }
    size_t n = (e - b) / 16;
    hilog::Info("sanity ok: DataModel=0x%p children=%zu", (void*)dm, n);
    return true;
}

} // namespace rbx::off
