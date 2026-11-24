#include "player.h"
#include "overlay.h"
#include "config.h"
#include "wts.h"
#include <imgui.h>
#include <cmath>
#include <cstdio>

namespace cs2 {

static constexpr int BONE_LINKS[][2] = {
    { BONE_HEAD, BONE_NECK }, { BONE_NECK, BONE_CHEST }, { BONE_CHEST, BONE_PELVIS },
    { BONE_CHEST, 8  }, { 8, 9 },  { 9, 10 },     // left arm
    { BONE_CHEST, 11 }, { 11, 12 },{ 12, 13 },    // right arm
    { BONE_PELVIS, 22 }, { 22, 23 }, { 23, 24 }, { 24, 25 },  // left leg
    { BONE_PELVIS, 26 }, { 26, 27 }, { 27, 28 }, { 28, 29 },  // right leg
};

static ImU32 FromRGBA(const std::array<float, 4>& c) {
    return IM_COL32((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255), (int)(c[3] * 255));
}

static void Outline(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float thickness = 1.f) {
    const ImU32 shadow = IM_COL32(0, 0, 0, 180);
    dl->AddLine(a, b, shadow, thickness + 1.f);
    dl->AddLine(a, b, col,    thickness);
}

static void OutlineRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float t = 1.f) {
    dl->AddRect({ a.x - 1, a.y - 1 }, { b.x + 1, b.y + 1 }, IM_COL32(0, 0, 0, 180), 0, 0, t + 1);
    dl->AddRect(a, b, col, 0, 0, t);
}

void DrawESP(const std::vector<ScreenPlayer>& players, const config::VisualCfg& cfg) {
    auto* dl = ImGui::GetForegroundDrawList();
    const ImU32 cBox   = FromRGBA(cfg.color_box_rgba);
    const ImU32 cName  = FromRGBA(cfg.color_name_rgba);
    const ImU32 cHP    = FromRGBA(cfg.color_hp_rgba);
    const ImU32 cLow   = IM_COL32(220, 40, 40, 255);

    int sw, sh; overlay::GetSize(sw, sh);

    if (cfg.fov_circle) {
        dl->AddCircle({ sw * 0.5f, sh * 0.5f }, cfg.fov_radius, IM_COL32(255, 255, 255, 80), 64, 1.f);
    }

    for (const auto& p : players) {
        // Box
        if (cfg.box) {
            OutlineRect(dl, { p.box.x0, p.box.y0 }, { p.box.x1, p.box.y1 }, cBox, 1.f);
        }

        // Name
        if (cfg.name && !p.name.empty()) {
            ImVec2 sz = ImGui::CalcTextSize(p.name.c_str());
            float x = (p.box.x0 + p.box.x1) * 0.5f - sz.x * 0.5f;
            float y = p.box.y0 - sz.y - 2.f;
            dl->AddText({ x + 1, y + 1 }, IM_COL32(0, 0, 0, 220), p.name.c_str());
            dl->AddText({ x,     y     }, cName,                 p.name.c_str());
        }

        // Health bar (left edge)
        if (cfg.health) {
            float hpN = (float)p.health / 100.f;
            if (hpN < 0) hpN = 0; else if (hpN > 1) hpN = 1;
            float h = p.box.y1 - p.box.y0;
            ImVec2 a{ p.box.x0 - 6.f, p.box.y0 };
            ImVec2 b{ p.box.x0 - 2.f, p.box.y1 };
            dl->AddRectFilled({ a.x - 1, a.y - 1 }, { b.x + 1, b.y + 1 }, IM_COL32(0, 0, 0, 200));
            dl->AddRectFilled({ a.x, b.y - h * hpN }, b, hpN < 0.3f ? cLow : cHP);
        }

        // Distance under box
        if (cfg.distance) {
            char buf[32]; snprintf(buf, sizeof(buf), "%.0fm", p.dist / 39.37f);
            ImVec2 sz = ImGui::CalcTextSize(buf);
            float x = (p.box.x0 + p.box.x1) * 0.5f - sz.x * 0.5f;
            float y = p.box.y1 + 2.f;
            dl->AddText({ x + 1, y + 1 }, IM_COL32(0, 0, 0, 220), buf);
            dl->AddText({ x,     y     }, cName,                 buf);
        }

        // Snapline from screen center to foot
        if (cfg.snapline) {
            Outline(dl, { sw * 0.5f, (float)sh }, { p.feetScreen.x, p.feetScreen.y }, cBox, 1.f);
        }
    }
}

void DrawSkeleton(const std::vector<ScreenPlayer>& players,
                  const float vm[16], const config::VisualCfg& cfg) {
    if (!cfg.skeleton) return;
    auto* dl = ImGui::GetForegroundDrawList();
    const ImU32 col    = FromRGBA(cfg.color_box_rgba);
    const ImU32 shadow = IM_COL32(0, 0, 0, 180);

    int sw, sh; overlay::GetSize(sw, sh);

    for (const auto& p : players) {
        if (!p.pawnAddr) continue;
        Pawn pw{ p.pawnAddr };

        // project every bone we care about; -1 = not projected
        constexpr int MAX_BONE = 30;
        Vec2  screen[MAX_BONE + 1];
        bool  ok[MAX_BONE + 1]{};
        for (int b = 0; b <= MAX_BONE; ++b) {
            Vec3 w = pw.BonePos(b);
            if (w.x == 0 && w.y == 0 && w.z == 0) continue;
            if (wts::Source2(vm, w, sw, sh, screen[b])) ok[b] = true;
        }

        for (const auto& link : BONE_LINKS) {
            int a = link[0], b = link[1];
            if (a > MAX_BONE || b > MAX_BONE) continue;
            if (!ok[a] || !ok[b]) continue;
            dl->AddLine({ screen[a].x, screen[a].y }, { screen[b].x, screen[b].y }, shadow, 2.f);
            dl->AddLine({ screen[a].x, screen[a].y }, { screen[b].x, screen[b].y }, col,    1.f);
        }
        // joint dots on head + torso anchors
        if (ok[BONE_HEAD])   dl->AddCircleFilled({ screen[BONE_HEAD].x,   screen[BONE_HEAD].y   }, 2.f, col);
        if (ok[BONE_CHEST])  dl->AddCircleFilled({ screen[BONE_CHEST].x,  screen[BONE_CHEST].y  }, 1.5f, col);
        if (ok[BONE_PELVIS]) dl->AddCircleFilled({ screen[BONE_PELVIS].x, screen[BONE_PELVIS].y }, 1.5f, col);
    }
}

} // namespace cs2
