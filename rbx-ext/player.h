#pragma once
#include "game.h"
#include "camera.h"
#include "skeleton.h"
#include "config.h"
#include <vector>

namespace rbx {

struct BoxPx { float x0, y0, x1, y1; bool valid; };

struct ScreenPlayer {
    std::string              name;
    float                    health;
    float                    maxHealth;
    float                    dist;
    bool                     isEnemy;
    BoxPx                    box;
    Vec3                     rootWorld;
    Vec2                     rootScreen;
    Vec2                     headScreen;
    bool                     headVisible;
    RigType                  rig;
    std::vector<LimbPart>    limbs;          // world positions
};

// Reads DataModel/Camera/Players once, builds ScreenPlayer list.
std::vector<ScreenPlayer> Snapshot(const config::VisualCfg& cfg, CameraSnap& outCam);

} // namespace rbx
