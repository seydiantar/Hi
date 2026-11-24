#pragma once
#include "entity.h"
#include "config.h"
#include <vector>

namespace cs2 {

struct BoxPx { float x0, y0, x1, y1; bool valid; };

// Build the on-screen bounding box by projecting bones.
// Falls back to origin + fixed height if bone chain fails.
BoxPx ComputeBox(const Pawn& p, const float vm[16], int sw, int sh);

struct ScreenPlayer {
    int         idx;
    uintptr_t   pawnAddr;
    std::string name;
    int         health;
    int         team;
    float       dist;      // from local pawn, in units (CS2 = inches)
    bool        isEnemy;
    BoxPx       box;
    Vec3        originWorld;
    Vec2        feetScreen;
    Vec2        headScreen;
    bool        headValid;
};

std::vector<ScreenPlayer> Snapshot(uintptr_t clientBase,
                                   const float vm[16], int sw, int sh,
                                   const config::VisualCfg& cfg);

} // namespace cs2
