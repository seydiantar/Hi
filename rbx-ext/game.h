#pragma once
#include "math.h"
#include "mem.h"
#include "sigs.h"
#include <cstdint>
#include <string>
#include <vector>

namespace rbx {

std::string ReadInstanceName(uintptr_t inst);
std::string ReadClassName(uintptr_t inst);
std::vector<uintptr_t> Children(uintptr_t inst);
uintptr_t FindFirstChildByName(uintptr_t inst, const std::string& name);
uintptr_t FindFirstChildByClass(uintptr_t inst, const std::string& klass);

struct DataModel {
    uintptr_t addr = 0;
    explicit operator bool() const { return addr != 0; }
    uintptr_t Service(const std::string& name) const { return FindFirstChildByName(addr, name); }
};

DataModel GetDataModel();

struct Camera {
    uintptr_t addr = 0;
    CFrame Frame() const { return mem::ExtRead<CFrame>(addr + off::g.cam.CoordinateFrame); }
    Vec2   Viewport() const { return mem::ExtRead<Vec2>(addr + off::g.cam.ViewportSize); }
    float  Fov()      const { return mem::ExtRead<float>(addr + off::g.cam.FieldOfView); }
};

struct BasePart {
    uintptr_t addr = 0;
    CFrame Frame() const { return mem::ExtRead<CFrame>(addr + off::g.part.CFrame); }
    Vec3   Position() const { auto c = Frame(); return { c.X, c.Y, c.Z }; }
    Vec3   Size() const { return mem::ExtRead<Vec3>(addr + off::g.part.Size); }
};

struct Humanoid {
    uintptr_t addr = 0;
    float Health()    const { return mem::ExtRead<float>(addr + off::g.humanoid.Health); }
    float MaxHealth() const { return mem::ExtRead<float>(addr + off::g.humanoid.MaxHealth); }
};

struct Character {
    uintptr_t addr = 0;
    BasePart Root() const {
        uintptr_t hrp = FindFirstChildByName(addr, "HumanoidRootPart");
        if (!hrp) hrp = FindFirstChildByName(addr, "Torso");
        if (!hrp) hrp = FindFirstChildByName(addr, "Head");
        return { hrp };
    }
    BasePart Head() const  { return { FindFirstChildByName(addr, "Head") }; }
    Humanoid Humanoid() const { return { FindFirstChildByClass(addr, "Humanoid") }; }
};

struct Player {
    uintptr_t addr = 0;
    std::string Name() const { return ReadInstanceName(addr); }
    Character   Char() const {
        uintptr_t c = mem::ExtRead<uintptr_t>(addr + off::g.player.Character);
        return { c };
    }
    uintptr_t Team() const { return mem::ExtRead<uintptr_t>(addr + off::g.player.Team); }
};

} // namespace rbx
