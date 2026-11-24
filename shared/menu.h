#pragma once
#include "config.h"
#include <functional>
#include <string>
#include <vector>

namespace menu {

struct Tab {
    std::string            name;
    std::function<void()>  draw;
};

// Call once per frame. Handles INSERT toggle, draws tabs if open, renders log console.
// `extraTabs` lets each project (cs2-ext / rbx-ext) splice in its own tab contents.
void Frame(config::AppCfg& cfg, const std::vector<Tab>& extraTabs);

// Common tab bodies callers can reuse.
void DrawVisualsTab(config::VisualCfg& v);
void DrawPlayerTab(config::PlayerCfg& p);
void DrawConfigTab(const wchar_t* appName, config::AppCfg& cfg);
void DrawAboutTab(const char* title);
void DrawLogConsole();

} // namespace menu
