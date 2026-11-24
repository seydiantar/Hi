#include "skeleton.h"

namespace rbx {

static const std::vector<Link> R6_LINKS = {
    { "Head",  "Torso"     },
    { "Torso", "Left Arm"  }, { "Torso", "Right Arm" },
    { "Torso", "Left Leg"  }, { "Torso", "Right Leg" },
};

static const std::vector<Link> R15_LINKS = {
    { "Head", "UpperTorso" }, { "UpperTorso", "LowerTorso" },
    { "UpperTorso",    "LeftUpperArm"  }, { "LeftUpperArm",  "LeftLowerArm"  }, { "LeftLowerArm",  "LeftHand" },
    { "UpperTorso",    "RightUpperArm" }, { "RightUpperArm", "RightLowerArm" }, { "RightLowerArm", "RightHand"},
    { "LowerTorso",    "LeftUpperLeg"  }, { "LeftUpperLeg",  "LeftLowerLeg"  }, { "LeftLowerLeg",  "LeftFoot" },
    { "LowerTorso",    "RightUpperLeg" }, { "RightUpperLeg", "RightLowerLeg" }, { "RightLowerLeg", "RightFoot"},
};

RigType DetectRig(const Character& ch) {
    if (!ch.addr) return RigType::Unknown;
    for (auto c : Children(ch.addr)) {
        std::string n = ReadInstanceName(c);
        if (n == "UpperTorso") return RigType::R15;
        if (n == "Torso")      return RigType::R6;
    }
    return RigType::Unknown;
}

std::vector<LimbPart> GatherLimbs(const Character& ch) {
    std::vector<LimbPart> out;
    if (!ch.addr) return out;

    static const char* R6[]  = {
        "Head", "Torso", "Left Arm", "Right Arm", "Left Leg", "Right Leg"
    };
    static const char* R15[] = {
        "Head", "UpperTorso", "LowerTorso",
        "LeftUpperArm",  "LeftLowerArm",  "LeftHand",
        "RightUpperArm", "RightLowerArm", "RightHand",
        "LeftUpperLeg",  "LeftLowerLeg",  "LeftFoot",
        "RightUpperLeg", "RightLowerLeg", "RightFoot"
    };

    RigType t = DetectRig(ch);
    const char* const* names = (t == RigType::R15) ? R15 : R6;
    int n = (t == RigType::R15) ? 15 : 6;

    out.reserve(n);
    for (int i = 0; i < n; ++i) {
        uintptr_t part = FindFirstChildByName(ch.addr, names[i]);
        if (!part) continue;
        BasePart bp{ part };
        Vec3 p = bp.Position();
        if (!(p.x == p.x)) continue;
        out.push_back({ names[i], p });
    }
    return out;
}

const std::vector<Link>& LinksFor(RigType t) {
    return (t == RigType::R15) ? R15_LINKS : R6_LINKS;
}

} // namespace rbx
