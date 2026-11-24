#pragma once
#include "game.h"
#include "math.h"
#include <string>
#include <vector>

namespace rbx {

enum class RigType { Unknown, R6, R15 };

struct LimbPart {
    std::string name;
    Vec3        pos;      // world position
};

struct Link { const char* a; const char* b; };

RigType                   DetectRig(const Character& ch);
std::vector<LimbPart>     GatherLimbs(const Character& ch);
const std::vector<Link>&  LinksFor(RigType t);

} // namespace rbx
