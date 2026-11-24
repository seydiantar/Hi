#pragma once
#include "himath.h"
#include "mem.h"
#include "offsets.h"
#include <cstdint>
#include <string>

namespace cs2 {

constexpr int TEAM_SPECTATOR = 1;
constexpr int TEAM_T         = 2;
constexpr int TEAM_CT        = 3;

constexpr int BONE_HEAD   = 6;
constexpr int BONE_NECK   = 5;
constexpr int BONE_CHEST  = 4;
constexpr int BONE_PELVIS = 0;

constexpr int LIFE_ALIVE  = 256;

// Thin accessors; actual offsets come from cs2::off::g at runtime.

struct Pawn {
    uintptr_t addr = 0;
    explicit operator bool() const { return addr != 0; }

    int   Health()    const { return mem::ExtRead<int>  (addr + off::g.pawn.m_iHealth); }
    int   Team()      const { return mem::ExtRead<int>  (addr + off::g.pawn.m_iTeamNum); }
    int   LifeState() const { return mem::ExtRead<int>  (addr + off::g.pawn.m_lifeState); }
    bool  Alive()     const { return LifeState() == 0; }  // CS2: lifeState==0 is alive
    uint32_t Flags()  const { return mem::ExtRead<uint32_t>(addr + off::g.pawn.m_fFlags); }

    uintptr_t SceneNode() const { return mem::ExtRead<uintptr_t>(addr + off::g.pawn.m_pGameSceneNode); }

    Vec3  Origin() const {
        uintptr_t sn = SceneNode();
        if (!sn) return { 0,0,0 };
        return mem::ExtRead<Vec3>(sn + off::g.scene.m_vecAbsOrigin);
    }

    bool  Dormant() const {
        uintptr_t sn = SceneNode();
        if (!sn) return true;
        return mem::ExtRead<bool>(sn + off::g.pawn.m_bDormant);
    }

    // Bone matrix: scene_node -> m_modelState -> m_pChunk -> array of CTransform (32 bytes each).
    Vec3 BonePos(int boneIdx) const {
        uintptr_t sn = SceneNode();
        if (!sn) return { 0,0,0 };
        uintptr_t bones = mem::ExtRead<uintptr_t>(sn + off::g.scene.m_modelState + off::g.model_state.m_pChunk);
        if (!bones) return { 0,0,0 };
        return mem::ExtRead<Vec3>(bones + (uintptr_t)boneIdx * 32u);
    }

    uint32_t HController() const { return mem::ExtRead<uint32_t>(addr + off::g.pawn.m_hController); }
};

struct Controller {
    uintptr_t addr = 0;
    explicit operator bool() const { return addr != 0; }

    std::string Name() const {
        uintptr_t s = mem::ExtRead<uintptr_t>(addr + off::g.controller.m_iszPlayerName);
        return s ? mem::ReadCStr(s, 64) : std::string{};
    }
    uint32_t HPawn()      const { return mem::ExtRead<uint32_t>(addr + off::g.controller.m_hPawn); }
    int      PawnHealth() const { return mem::ExtRead<int>     (addr + off::g.controller.m_iPawnHealth); }
    bool     PawnAlive()  const { return mem::ExtRead<bool>    (addr + off::g.controller.m_bPawnIsAlive); }
};

struct ViewState {
    float viewMatrix[16]{};
    void Read(uintptr_t clientBase) {
        mem::ExtRPM(clientBase + off::g.dwViewMatrix, viewMatrix, sizeof(viewMatrix));
    }
};

} // namespace cs2
