#include "player.h"
#include "camera.h"
#include "log.h"
#include <algorithm>

namespace rbx {

static BoxPx ComputeBox(const Character& ch, const CameraSnap& cam) {
    BasePart root = ch.Root();
    if (!root.addr) return { 0,0,0,0,false };

    Vec3 pos  = root.Position();
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
        if (!WorldToScreen(cam, c, s)) continue;
        minX = std::min(minX, s.x); maxX = std::max(maxX, s.x);
        minY = std::min(minY, s.y); maxY = std::max(maxY, s.y);
        ++hits;
    }
    if (hits < 2) return { 0,0,0,0,false };
    // widen vertically to wrap full humanoid, not just HRP
    float h = maxY - minY;
    return { minX, minY - h * 1.3f, maxX, maxY + h * 0.3f, true };
}

std::vector<ScreenPlayer> Snapshot(const config::VisualCfg& cfg, CameraSnap& outCam) {
    std::vector<ScreenPlayer> out;

    outCam = ReadCamera();
    if (!outCam.valid) return out;

    DataModel dm = GetDataModel();
    if (!dm) return out;

    uintptr_t players = dm.Service("Players");
    if (!players) return out;

    uintptr_t localAddr = off::g.player.LocalPlayer
        ? mem::ExtRead<uintptr_t>(players + off::g.player.LocalPlayer)
        : 0;

    Character localChar = localAddr ? Player{ localAddr }.Char() : Character{};
    Vec3 localPos = localChar.addr ? localChar.Root().Position() : Vec3{ 0, 0, 0 };
    uintptr_t localTeam = localAddr ? Player{ localAddr }.Team() : 0;

    for (auto pAddr : Children(players)) {
        if (pAddr == localAddr) continue;
        if (ReadClassName(pAddr) != "Player") continue;

        Player p{ pAddr };
        Character ch = p.Char();
        if (!ch.addr) continue;

        BasePart root = ch.Root();
        if (!root.addr) continue;
        Vec3 rootPos = root.Position();

        float dist = Distance(localPos, rootPos);
        if (cfg.max_dist > 0 && dist > cfg.max_dist) continue;

        uintptr_t team = p.Team();
        bool enemy = (team != localTeam);
        if (cfg.team_mode == 1 && !enemy) continue;
        if (cfg.team_mode == 2 &&  enemy) continue;

        BoxPx box = ComputeBox(ch, outCam);
        if (!box.valid) continue;

        Humanoid h = ch.Humanoid();
        float hp = h.addr ? h.Health()    : -1.f;
        float mx = h.addr ? h.MaxHealth() : 100.f;

        Vec2 rs{};
        WorldToScreen(outCam, rootPos, rs);

        Vec2 hs{};
        bool headValid = false;
        BasePart head = ch.Head();
        if (head.addr) {
            headValid = WorldToScreen(outCam, head.Position(), hs);
        }

        RigType rig = DetectRig(ch);
        std::vector<LimbPart> limbs;
        if (cfg.skeleton) limbs = GatherLimbs(ch);

        out.push_back({
            p.Name(), hp, mx, dist, enemy,
            box, rootPos, rs, hs, headValid, rig, std::move(limbs)
        });
    }
    return out;
}

} // namespace rbx
