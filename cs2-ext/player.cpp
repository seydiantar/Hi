#include "player.h"
#include "wts.h"
#include <algorithm>
#include <cmath>

namespace cs2 {

BoxPx ComputeBox(const Pawn& p, const float vm[16], int sw, int sh) {
    // Try the bone-chain box first.
    constexpr int bones[] = { BONE_HEAD, BONE_NECK, BONE_CHEST, BONE_PELVIS, 10, 13, 25, 29 };
    float minX =  1e9f, minY =  1e9f, maxX = -1e9f, maxY = -1e9f;
    int hits = 0;
    for (int b : bones) {
        Vec3 w = p.BonePos(b);
        if (w.x == 0 && w.y == 0 && w.z == 0) continue;
        Vec2 s;
        if (!wts::Source2(vm, w, sw, sh, s)) continue;
        minX = std::min(minX, s.x); maxX = std::max(maxX, s.x);
        minY = std::min(minY, s.y); maxY = std::max(maxY, s.y);
        ++hits;
    }
    if (hits >= 2 && maxX > minX && maxY > minY) {
        // widen the box a little so it wraps the model, not just the bones
        float pad = (maxX - minX) * 0.1f;
        return { minX - pad, minY - 6.f, maxX + pad, maxY + 4.f, true };
    }

    // Fallback: origin + head (origin + ~72 inch standing height).
    Vec3 foot = p.Origin();
    Vec3 head = { foot.x, foot.y, foot.z + 72.f };
    Vec2 sf, sh2;
    if (!wts::Source2(vm, foot, sw, sh, sf)) return { 0,0,0,0,false };
    if (!wts::Source2(vm, head, sw, sh, sh2)) return { 0,0,0,0,false };
    float height = std::abs(sh2.y - sf.y);
    float width  = height * 0.45f;
    return { sf.x - width * 0.5f, sh2.y, sf.x + width * 0.5f, sf.y, true };
}

std::vector<ScreenPlayer> Snapshot(uintptr_t clientBase, const float vm[16], int sw, int sh,
                                   const config::VisualCfg& cfg) {
    std::vector<ScreenPlayer> out;
    Pawn local = LocalPawn(clientBase);
    int   localTeam = local ? local.Team() : 0;
    Vec3  localPos  = local ? local.Origin() : Vec3{ 0,0,0 };

    for (auto& s : EnumPlayers(clientBase)) {
        if (!local || s.pawn.addr == local.addr) continue;

        int    team   = s.pawn.Team();
        bool   enemy  = (team != localTeam);
        if (cfg.team_mode == 1 && !enemy) continue;
        if (cfg.team_mode == 2 &&  enemy) continue;

        float dist = Distance(localPos, s.pawn.Origin());
        if (dist > cfg.max_dist && cfg.max_dist > 0.f) continue;

        BoxPx box = ComputeBox(s.pawn, vm, sw, sh);
        if (!box.valid) continue;

        Vec3 foot = s.pawn.Origin();
        Vec3 head = s.pawn.BonePos(BONE_HEAD);
        Vec2 fs{}, hs{};
        bool fv = wts::Source2(vm, foot, sw, sh, fs);
        bool hv = wts::Source2(vm, head, sw, sh, hs);
        if (!fv) continue;

        out.push_back({
            s.idx, s.pawn.addr, s.ctrl.Name(), s.pawn.Health(), team, dist, enemy,
            box, foot, fs, hs, hv
        });
    }
    return out;
}

} // namespace cs2
