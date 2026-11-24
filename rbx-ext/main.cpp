#include "mem.h"
#include "scanner.h"
#include "overlay.h"
#include "menu.h"
#include "config.h"
#include "log.h"
#include "sigs.h"
#include "player.h"
#include <windows.h>
#include <thread>
#include <chrono>
#include <string>

namespace rbx {
void DrawESP(const std::vector<ScreenPlayer>&, const config::VisualCfg&);
void DrawGameTab();
}

static bool HasArg(int argc, wchar_t** argv, const wchar_t* needle) {
    for (int i = 1; i < argc; ++i) if (!_wcsicmp(argv[i], needle)) return true;
    return false;
}

int wmain(int argc, wchar_t** argv) {
    log::Init(L"rbx-ext");
    log::Info("rbx-ext starting");

    bool dumpOnly = HasArg(argc, argv, L"--dump-sigs");

    config::AppCfg cfg;
    config::Load(L"rbx-ext", cfg);

    if (!mem::WaitAttach(L"RobloxPlayerBeta.exe")) { log::Err("could not attach"); return 1; }

    if (!rbx::off::Resolve()) { log::Err("sig resolve failed"); return 2; }
    rbx::off::SanityProbe();

    if (dumpOnly) {
        rbx::off::SaveCache();
        log::Info("--dump-sigs done, exiting");
        log::Shutdown();
        return 0;
    }

    if (!overlay::Init()) { log::Err("overlay init failed"); return 3; }

    int topmostTick = 0;
    while (mem::IsAlive()) {
        overlay::BeginFrame();

        auto players = rbx::Snapshot(cfg.visuals);
        rbx::DrawESP(players, cfg.visuals);

        menu::Frame(cfg, { { "Game", &rbx::DrawGameTab } });

        overlay::EndFrame();
        if ((topmostTick++ & 31) == 0) overlay::AssertTopmost();
    }

    log::Info("Roblox gone, shutting down");
    overlay::Shutdown();
    mem::Detach();
    log::Shutdown();
    return 0;
}
