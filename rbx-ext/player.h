#pragma once
#include "game.h"
#include "config.h"
#include <vector>

namespace rbx {

struct BoxPx { float x0, y0, x1, y1; bool valid; };

struct ScreenPlayer {
    std::string name;
    float       health;
    float       maxHealth;
    float       dist;
    bool        isEnemy;
    BoxPx       box;
    Vec3        rootWorld;
    Vec2        rootScreen;
};

std::vector<ScreenPlayer> Snapshot(const config::VisualCfg& cfg);

} // namespace rbx
