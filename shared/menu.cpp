#include "menu.h"
#include "overlay.h"
#include "log.h"
#include <imgui.h>
#include <windows.h>

namespace menu {

static bool  g_prev_key_down = false;

static bool KeyPressedEdge(int vk) {
    bool now = (GetAsyncKeyState(vk) & 0x8000) != 0;
    bool edge = now && !g_prev_key_down;
    g_prev_key_down = now;
    return edge;
}

void Frame(config::AppCfg& cfg, const std::vector<Tab>& extraTabs) {
    if (KeyPressedEdge(cfg.player.toggle_key)) {
        overlay::SetMenuOpen(!overlay::MenuOpen());
    }
    if (!overlay::MenuOpen()) return;

    ImGui::SetNextWindowSize({ 600, 420 }, ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos({ 80, 80 }, ImGuiCond_FirstUseEver);
    if (!ImGui::Begin("hi", nullptr, ImGuiWindowFlags_NoCollapse)) { ImGui::End(); return; }

    if (ImGui::BeginTabBar("##tabs")) {
        if (ImGui::BeginTabItem("Visuals")) { DrawVisualsTab(cfg.visuals); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Player"))  { DrawPlayerTab(cfg.player);   ImGui::EndTabItem(); }
        for (auto& t : extraTabs) {
            if (ImGui::BeginTabItem(t.name.c_str())) { t.draw(); ImGui::EndTabItem(); }
        }
        if (ImGui::BeginTabItem("Log"))     { DrawLogConsole(); ImGui::EndTabItem(); }
        if (ImGui::BeginTabItem("Config"))  { DrawConfigTab(L"app", cfg);  ImGui::EndTabItem(); }
        ImGui::EndTabBar();
    }
    ImGui::End();
}

void DrawVisualsTab(config::VisualCfg& v) {
    ImGui::Checkbox("Box",       &v.box);
    ImGui::Checkbox("Skeleton",  &v.skeleton);
    ImGui::Checkbox("Name",      &v.name);
    ImGui::Checkbox("Health",    &v.health);
    ImGui::Checkbox("Distance",  &v.distance);
    ImGui::Checkbox("Snapline",  &v.snapline);
    ImGui::Checkbox("FOV circle",&v.fov_circle);
    ImGui::SliderFloat("FOV radius", &v.fov_radius, 20.f, 500.f, "%.0f px");
    ImGui::SliderFloat("Max dist",   &v.max_dist,   50.f, 2000.f, "%.0f");
    const char* teams[] = { "All", "Enemies only", "Allies only" };
    ImGui::Combo("Team filter", &v.team_mode, teams, 3);
    ImGui::Separator();
    ImGui::ColorEdit4("Box color",  v.color_box_rgba.data());
    ImGui::ColorEdit4("HP color",   v.color_hp_rgba.data());
    ImGui::ColorEdit4("Name color", v.color_name_rgba.data());
}

void DrawPlayerTab(config::PlayerCfg& p) {
    char buf[8]; snprintf(buf, sizeof(buf), "0x%02X", p.toggle_key);
    ImGui::Text("Menu toggle key: %s  (INSERT=0x2D)", buf);
    ImGui::Text("Change: edit value in config JSON, press F5.");
}

void DrawConfigTab(const wchar_t* appName, config::AppCfg& cfg) {
    static char status[128] = "";
    if (ImGui::Button("Save")) {
        if (config::Save(appName, cfg)) snprintf(status, sizeof(status), "saved");
        else                            snprintf(status, sizeof(status), "save failed");
    }
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        if (config::Load(appName, cfg)) snprintf(status, sizeof(status), "reloaded");
        else                            snprintf(status, sizeof(status), "reload failed");
    }
    ImGui::SameLine();
    ImGui::TextUnformatted(status);
}

void DrawAboutTab(const char* title) {
    ImGui::TextUnformatted(title);
    ImGui::Separator();
    ImGui::Text("INSERT: toggle menu");
    ImGui::Text("ESP: Visuals tab");
    ImGui::Text("Config: %ls", config::PathFor(L"app").wstring().c_str());
}

void DrawLogConsole() {
    std::lock_guard<std::mutex> lk(log::RingLock());
    const auto& ring = log::Ring();
    if (ImGui::BeginChild("##log", { 0, 0 }, true)) {
        for (auto& e : ring) {
            ImVec4 c{ 0.9f, 0.9f, 0.9f, 1.f };
            switch (e.lvl) {
                case log::Level::Warn: c = { 1.f, 0.85f, 0.3f, 1.f }; break;
                case log::Level::Err:  c = { 1.f, 0.4f,  0.4f, 1.f }; break;
                case log::Level::Dbg:  c = { 0.6f, 0.6f, 0.6f, 1.f }; break;
                default: break;
            }
            ImGui::TextColored(c, "%s", e.text.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 2)
            ImGui::SetScrollHereY(1.f);
    }
    ImGui::EndChild();
}

} // namespace menu
