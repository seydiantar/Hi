#pragma once
#include <cstdint>
#include <string>
#include <unordered_map>

namespace cs2::off {

struct Offsets {
    // Module base pointers (relative to the owning module).
    //   dwLocalPlayerPawn       - client.dll
    //   dwEntityList            - client.dll
    //   dwViewMatrix            - client.dll
    //   dwLocalPlayerController - client.dll
    //   dwGlobalVars            - client.dll
    //   dwGameEntitySystem      - client.dll
    //   dwPlantedC4             - client.dll
    uint32_t dwLocalPlayerPawn       = 0;
    uint32_t dwEntityList            = 0;
    uint32_t dwViewMatrix            = 0;
    uint32_t dwLocalPlayerController = 0;
    uint32_t dwGlobalVars            = 0;
    uint32_t dwGameEntitySystem      = 0;
    uint32_t dwPlantedC4             = 0;

    // Class member offsets. Named per cs2-dumper client_dll.json keys.
    struct {
        uint32_t m_iHealth              = 0;
        uint32_t m_iTeamNum             = 0;
        uint32_t m_lifeState            = 0;
        uint32_t m_bDormant             = 0;
        uint32_t m_pGameSceneNode       = 0;
        uint32_t m_vOldOrigin           = 0;
        uint32_t m_hController          = 0;
        uint32_t m_pClippingWeapon      = 0;
        uint32_t m_iShotsFired          = 0;
        uint32_t m_bIsScoped            = 0;
        uint32_t m_angEyeAngles         = 0;
        uint32_t m_fFlags               = 0;
    } pawn;

    struct {
        uint32_t m_iszPlayerName        = 0;
        uint32_t m_hPawn                = 0;
        uint32_t m_iPawnHealth          = 0;
        uint32_t m_iPawnLifetimeStart   = 0;
        uint32_t m_bPawnIsAlive         = 0;
    } controller;

    struct {
        uint32_t m_vecAbsOrigin         = 0;
        uint32_t m_modelState           = 0;
    } scene;

    struct {
        uint32_t m_pChunk               = 0;      // float4 bone transforms
    } model_state;

    // Loaded from JSON, resolved at runtime.
    uint64_t    source_sha_offsets  = 0;
    uint64_t    source_sha_members  = 0;
    std::string source_url;
};

extern Offsets g;

// Load from <exe-dir>/offsets.json + client_dll.json, then try a2x/cs2-dumper main branch.
// Writes resolved copy to %LOCALAPPDATA%\hi\cs2\offsets.json.
bool Load();

// Runtime sanity probe: dereferences enough pointers to confirm no key offset is stale.
// Call after Attach + Load. Aborts with Err() on any failure.
bool SanityProbe();

} // namespace cs2::off
