#include "player.h"
#include "overlay.h"
#include "config.h"
#include <imgui.h>
#include <cstdio>

namespace rbx {

static ImU32 FromRGBA(const std::array<float, 4>& c) {
    return IM_COL32((int)(c[0] * 255), (int)(c[1] * 255), (int)(c[2] * 255), (int)(c[3] * 255));
}

static void OutlineRect(ImDrawList* dl, ImVec2 a, ImVec2 b, ImU32 col, float t = 1.f) {
    dl->AddRect({ a.x - 1, a.y - 1 }, { b.x + 1, b.y + 1 }, IM_COL32(0, 0, 0, 180), 0, 0, t + 1);
    dl->AddRect(a, b, col, 0, 0, t);
}

void DrawESP(const std::vector<ScreenPlayer>& players, const config::VisualCfg& cfg) {
    auto* dl = ImGui::GetForegroundDrawList();
    const ImU32 cBox  = FromRGBA(cfg.color_box_rgba);
    const ImU32 cName = FromRGBA(cfg.color_name_rgba);
    const ImU32 cHP   = FromRGBA(cfg.color_hp_rgba);
    const ImU32 cLow  = IM_COL32(220, 40, 40, 255);

    int sw, sh; overlay::GetSize(sw, sh);

    if (cfg.fov_circle) {
        dl->AddCircle({ sw * 0.5f, sh * 0.5f }, cfg.fov_radius, IM_COL32(255, 255, 255, 80), 64, 1.f);
    }

    for (const auto& p : players) {
        if (cfg.box) OutlineRect(dl, { p.box.x0, p.box.y0 }, { p.box.x1, p.box.y1 }, cBox, 1.f);

        if (cfg.name && !p.name.empty()) {
            ImVec2 sz = ImGui::CalcTextSize(p.name.c_str());
            float x = (p.box.x0 + p.box.x1) * 0.5f - sz.x * 0.5f;
            float y = p.box.y0 - sz.y - 2.f;
            dl->AddText({ x + 1, y + 1 }, IM_COL32(0, 0, 0, 220), p.name.c_str());
            dl->AddText({ x,     y     }, cName,                 p.name.c_str());
        }

        if (cfg.health && p.maxHealth > 0.f) {
            float hpN = p.health / p.maxHealth;
            if (hpN < 0) hpN = 0; else if (hpN > 1) hpN = 1;
            float h = p.box.y1 - p.box.y0;
            ImVec2 a{ p.box.x0 - 6.f, p.box.y0 };
            ImVec2 b{ p.box.x0 - 2.f, p.box.y1 };
            dl->AddRectFilled({ a.x - 1, a.y - 1 }, { b.x + 1, b.y + 1 }, IM_COL32(0, 0, 0, 200));
            dl->AddRectFilled({ a.x, b.y - h * hpN }, b, hpN < 0.3f ? cLow : cHP);
        }

        if (cfg.distance) {
            char buf[32]; snprintf(buf, sizeof(buf), "%.0fs", p.dist);
            ImVec2 sz = ImGui::CalcTextSize(buf);
            float x = (p.box.x0 + p.box.x1) * 0.5f - sz.x * 0.5f;
            float y = p.box.y1 + 2.f;
            dl->AddText({ x + 1, y + 1 }, IM_COL32(0, 0, 0, 220), buf);
            dl->AddText({ x,     y     }, cName,                 buf);
        }

        if (cfg.snapline) {
            dl->AddLine({ sw * 0.5f, (float)sh }, { p.rootScreen.x, p.rootScreen.y }, IM_COL32(0, 0, 0, 180), 2.f);
            dl->AddLine({ sw * 0.5f, (float)sh }, { p.rootScreen.x, p.rootScreen.y }, cBox, 1.f);
        }
    }
}

} // namespace rbx
