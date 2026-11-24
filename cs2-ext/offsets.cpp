#include "offsets.h"
#include "mem.h"
#include "scanner.h"
#include "log.h"
#include <nlohmann/json.hpp>
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <fstream>

namespace cs2::off {

Offsets g;

using json = nlohmann::json;

static std::filesystem::path ExeDir() {
    wchar_t buf[MAX_PATH];
    GetModuleFileNameW(nullptr, buf, MAX_PATH);
    return std::filesystem::path(buf).parent_path();
}

static std::filesystem::path CacheDir() {
    wchar_t ad[MAX_PATH]{};
    SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, ad);
    auto p = std::filesystem::path(ad) / L"hi" / L"cs2";
    std::error_code ec; std::filesystem::create_directories(p, ec);
    return p;
}

static bool TryRead(const std::filesystem::path& p, json& out) {
    std::ifstream f(p);
    if (!f) return false;
    try { f >> out; } catch (const std::exception& e) {
        log::Warn("offsets parse %ls: %s", p.c_str(), e.what());
        return false;
    }
    return true;
}

// cs2-dumper offsets.json shape:  { "client.dll": { "dwLocalPlayerPawn": 15000000, ... }, ... }
static uint32_t M(const json& j, const char* module, const char* key, uint32_t fallback = 0) {
    if (!j.contains(module)) return fallback;
    const auto& m = j[module];
    if (!m.contains(key)) return fallback;
    const auto& v = m[key];
    if (v.is_number_unsigned()) return (uint32_t)v.get<uint64_t>();
    if (v.is_number_integer())  return (uint32_t)v.get<int64_t>();
    return fallback;
}

// cs2-dumper client_dll.json shape:
//   { "client.dll": { "classes": { "C_CSPlayerPawn": { "fields": { "m_iHealth": 808 } } } } }
static uint32_t F(const json& j, const char* module, const char* klass, const char* field, uint32_t fallback = 0) {
    try {
        const auto& v = j.at(module).at("classes").at(klass).at("fields").at(field);
        if (v.is_number_unsigned()) return (uint32_t)v.get<uint64_t>();
        if (v.is_number_integer())  return (uint32_t)v.get<int64_t>();
    } catch (...) {}
    return fallback;
}

static void FillFromJson(const json& offs, const json& cdll) {
    g.dwLocalPlayerPawn       = M(offs, "client.dll", "dwLocalPlayerPawn");
    g.dwEntityList            = M(offs, "client.dll", "dwEntityList");
    g.dwViewMatrix            = M(offs, "client.dll", "dwViewMatrix");
    g.dwLocalPlayerController = M(offs, "client.dll", "dwLocalPlayerController");
    g.dwGlobalVars            = M(offs, "client.dll", "dwGlobalVars");
    g.dwGameEntitySystem      = M(offs, "client.dll", "dwGameEntitySystem");
    g.dwPlantedC4             = M(offs, "client.dll", "dwPlantedC4");

    g.pawn.m_iHealth          = F(cdll, "client.dll", "C_BaseEntity",       "m_iHealth");
    g.pawn.m_iTeamNum         = F(cdll, "client.dll", "C_BaseEntity",       "m_iTeamNum");
    g.pawn.m_lifeState        = F(cdll, "client.dll", "C_BaseEntity",       "m_lifeState");
    g.pawn.m_bDormant         = F(cdll, "client.dll", "CGameSceneNode",     "m_bDormant");
    g.pawn.m_pGameSceneNode   = F(cdll, "client.dll", "C_BaseEntity",       "m_pGameSceneNode");
    g.pawn.m_vOldOrigin       = F(cdll, "client.dll", "C_BasePlayerPawn",   "m_vOldOrigin");
    g.pawn.m_hController      = F(cdll, "client.dll", "C_CSPlayerPawnBase", "m_hController");
    g.pawn.m_pClippingWeapon  = F(cdll, "client.dll", "C_CSPlayerPawnBase", "m_pClippingWeapon");
    g.pawn.m_iShotsFired      = F(cdll, "client.dll", "C_CSPlayerPawnBase", "m_iShotsFired");
    g.pawn.m_bIsScoped        = F(cdll, "client.dll", "C_CSPlayerPawnBase", "m_bIsScoped");
    g.pawn.m_angEyeAngles     = F(cdll, "client.dll", "C_CSPlayerPawnBase", "m_angEyeAngles");
    g.pawn.m_fFlags           = F(cdll, "client.dll", "C_BaseEntity",       "m_fFlags");

    g.controller.m_iszPlayerName      = F(cdll, "client.dll", "CBasePlayerController", "m_iszPlayerName");
    g.controller.m_hPawn              = F(cdll, "client.dll", "CBasePlayerController", "m_hPawn");
    g.controller.m_iPawnHealth        = F(cdll, "client.dll", "CCSPlayerController",   "m_iPawnHealth");
    g.controller.m_iPawnLifetimeStart = F(cdll, "client.dll", "CCSPlayerController",   "m_iPawnLifetimeStart");
    g.controller.m_bPawnIsAlive       = F(cdll, "client.dll", "CCSPlayerController",   "m_bPawnIsAlive");

    g.scene.m_vecAbsOrigin    = F(cdll, "client.dll", "CGameSceneNode", "m_vecAbsOrigin");
    g.scene.m_modelState      = F(cdll, "client.dll", "CSkeletonInstance", "m_modelState");
    g.model_state.m_pChunk    = 0x80;  // historically stable within CModelState; JSON key varies
}

static bool FetchLatest(json& offs, json& cdll) {
    // FILL: implement WinHTTP GET for
    //   https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/offsets.json
    //   https://raw.githubusercontent.com/a2x/cs2-dumper/main/output/client_dll.json
    // Write to CacheDir() if both parse. Returning false here skips to bundled copy.
    (void)offs; (void)cdll;
    return false;
}

bool Load() {
    json offs, cdll;

    // 1. Cache (last successful fetch)
    auto cache = CacheDir();
    bool have = TryRead(cache / L"offsets.json", offs) && TryRead(cache / L"client_dll.json", cdll);

    // 2. Live fetch — overwrites cache on success
    if (FetchLatest(offs, cdll)) {
        std::ofstream(cache / L"offsets.json")    << offs.dump(2);
        std::ofstream(cache / L"client_dll.json") << cdll.dump(2);
        have = true;
    }

    // 3. Bundled snapshot (ships next to exe)
    if (!have) {
        auto dir = ExeDir();
        have = TryRead(dir / L"offsets.json", offs) && TryRead(dir / L"client_dll.json", cdll);
    }

    if (!have) { log::Err("no offsets source available (cache, net, bundled all failed)"); return false; }

    FillFromJson(offs, cdll);

    log::Info("offsets loaded: pawn=0x%X entity=0x%X view=0x%X",
              g.dwLocalPlayerPawn, g.dwEntityList, g.dwViewMatrix);
    return g.dwLocalPlayerPawn && g.dwEntityList && g.dwViewMatrix
        && g.pawn.m_iHealth && g.pawn.m_pGameSceneNode;
}

bool SanityProbe() {
    auto client = scan::GetModule(mem::g_pid, L"client.dll");
    if (!client) { log::Err("client.dll not mapped"); return false; }

    uintptr_t local = mem::ExtRead<uintptr_t>(client->base + g.dwLocalPlayerPawn);
    if (!local) { log::Warn("LocalPlayerPawn NULL (not in a match yet?)"); return true; }

    int hp = mem::ExtRead<int>(local + g.pawn.m_iHealth);
    if (hp < -10 || hp > 1000) { log::Err("sanity: m_iHealth=%d (out of range)", hp); return false; }

    float vm[16];
    mem::ExtRPM(client->base + g.dwViewMatrix, vm, sizeof(vm));
    bool nanFree = true;
    for (float f : vm) if (!(f == f)) { nanFree = false; break; }
    if (!nanFree) { log::Err("sanity: view matrix has NaN"); return false; }

    log::Info("sanity ok: localHP=%d", hp);
    return true;
}

} // namespace cs2::off
