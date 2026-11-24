#include "player.h"
#include "wts.h"
#include "log.h"
#include <algorithm>

namespace rbx {

static uintptr_t LocalPlayerAddr(const DataModel& dm) {
    uintptr_t players = dm.Service("Players");
    if (!players) return 0;
    return mem::ExtRead<uintptr_t>(players + off::g.player.LocalPlayer);
}

static Camera CurrentCamera(const DataModel& dm) {
    uintptr_t ws = dm.Service("Workspace");
    if (!ws) return { 0 };
    uintptr_t cam = FindFirstChildByClass(ws, "Camera");
    if (!cam) cam = FindFirstChildByName(ws, "Camera");
    return { cam };
}

static BoxPx ComputeBox(const Character& ch, const CFrame& camCF, float fov, int sw, int sh) {
    BasePart root = ch.Root();
    if (!root.addr) return { 0,0,0,0,false };

    // 8 corners of root part box
    Vec3 pos = root.Position();
    Vec3 size = root.Size();
    size = { size.x * 0.5f, size.y * 0.5f, size.z * 0.5f };
    const Vec3 corners[8] = {
        { pos.x - size.x, pos.y - size.y, pos.z - size.z },
        { pos.x + size.x, pos.y - size.y, pos.z - size.z },
        { pos.x - size.x, pos.y + size.y, pos.z - size.z },
        { pos.x + size.x, pos.y + size.y, pos.z - size.z },
        { pos.x - size.x, pos.y - size.y, pos.z + size.z },
        { pos.x + size.x, pos.y - size.y, pos.z + size.z },
        { pos.x - size.x, pos.y + size.y, pos.z + size.z },
        { pos.x + size.x, pos.y + size.y, pos.z + size.z },
    };
    float minX =  1e9f, minY =  1e9f, maxX = -1e9f, maxY = -1e9f;
    int hits = 0;
    for (auto& c : corners) {
        Vec2 s;
        if (!wts::Roblox(camCF, c, fov, sw, sh, s)) continue;
        minX = std::min(minX, s.x); maxX = std::max(maxX, s.x);
        minY = std::min(minY, s.y); maxY = std::max(maxY, s.y);
        ++hits;
    }
    if (hits < 2) return { 0,0,0,0,false };
    return { minX, minY, maxX, maxY, true };
}

std::vector<ScreenPlayer> Snapshot(const config::VisualCfg& cfg) {
    std::vector<ScreenPlayer> out;
    DataModel dm = GetDataModel();
    if (!dm) return out;

    Camera cam = CurrentCamera(dm);
    if (!cam.addr) return out;

    CFrame camCF = cam.Frame();
    Vec2   vp    = cam.Viewport();
    float  fov   = cam.Fov();
    int sw = (int)vp.x, sh = (int)vp.y;
    if (sw <= 0 || sh <= 0) return out;

    uintptr_t playersSvc = dm.Service("Players");
    if (!playersSvc) return out;

    uintptr_t localAddr = mem::ExtRead<uintptr_t>(playersSvc + off::g.player.LocalPlayer);
    Character localChar = localAddr ? Player{ localAddr }.Char() : Character{};
    Vec3 localPos = localChar.addr ? localChar.Root().Position() : Vec3{ 0, 0, 0 };
    uintptr_t localTeam = localAddr ? Player{ localAddr }.Team() : 0;

    for (auto pAddr : Children(playersSvc)) {
        if (ReadClassName(pAddr) != "Player") continue;
        if (pAddr == localAddr) continue;
        Player p{ pAddr };
        Character ch = p.Char();
        if (!ch.addr) continue;
        BasePart root = ch.Root();
        if (!root.addr) continue;
        Vec3 pos = root.Position();
        float dist = Distance(localPos, pos);
        if (dist > cfg.max_dist && cfg.max_dist > 0) continue;

        uintptr_t team = p.Team();
        bool enemy = (team != localTeam);
        if (cfg.team_mode == 1 && !enemy) continue;
        if (cfg.team_mode == 2 &&  enemy) continue;

        BoxPx box = ComputeBox(ch, camCF, fov, sw, sh);
        if (!box.valid) continue;

        Humanoid h = ch.Humanoid();
        float hp = h.addr ? h.Health() : -1;
        float mx = h.addr ? h.MaxHealth() : 100.f;

        Vec2 rs{};
        wts::Roblox(camCF, pos, fov, sw, sh, rs);

        out.push_back({ p.Name(), hp, mx, dist, enemy, box, pos, rs });
    }
    return out;
}

} // namespace rbx
