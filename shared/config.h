#pragma once
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <array>

namespace config {

struct VisualCfg {
    bool  box          = true;
    bool  skeleton     = true;
    bool  name         = true;
    bool  health       = true;
    bool  distance     = true;
    bool  snapline     = false;
    bool  fov_circle   = false;
    float fov_radius   = 150.f;
    float max_dist     = 500.f;
    int   team_mode    = 0;    // 0=all, 1=enemies, 2=allies
    std::array<float, 4> color_box_rgba  = { 0.95f, 0.95f, 0.95f, 1.f };
    std::array<float, 4> color_hp_rgba   = { 0.20f, 0.90f, 0.20f, 1.f };
    std::array<float, 4> color_name_rgba = { 0.95f, 0.95f, 0.95f, 1.f };
};

struct PlayerCfg {
    int   toggle_key   = 0x2D;  // VK_INSERT
};

struct AppCfg {
    VisualCfg visuals;
    PlayerCfg player;
};

bool Load(const wchar_t* appName, AppCfg& out);
bool Save(const wchar_t* appName, const AppCfg& in);

std::filesystem::path PathFor(const wchar_t* appName);

NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(VisualCfg, box, skeleton, name, health, distance, snapline,
                                    fov_circle, fov_radius, max_dist, team_mode,
                                    color_box_rgba, color_hp_rgba, color_name_rgba)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(PlayerCfg, toggle_key)
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AppCfg, visuals, player)

} // namespace config
