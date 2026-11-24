#include "mem.h"
#include "scanner.h"
#include "overlay.h"
#include "menu.h"
#include "config.h"
#include "log.h"
#include "sigs.h"
#include "probe.h"
#include "player.h"
#include "camera.h"
#include <windows.h>
#include <thread>
#include <chrono>
#include <string>

namespace rbx {
void DrawESP(const std::vector<ScreenPlayer>&, const CameraSnap&, const config::VisualCfg&);
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
    bool verbose  = HasArg(argc, argv, L"--verbose");
    (void)verbose;

    config::AppCfg cfg;
    config::Load(L"rbx-ext", cfg);

    if (!mem::WaitAttach(L"RobloxPlayerBeta.exe")) { log::Err("could not attach"); return 1; }

    // sigs.cpp::Resolve runs sig-scan + probe::All internally
    if (!rbx::off::Resolve()) {
        log::Err("offset resolve failed");
        // Last-ditch: if probe is the only thing that failed (character not spawned),
        // keep probing in a loop until the game is in a place.
        for (int i = 0; i < 60 && !rbx::probe::All(); ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }
        rbx::off::SaveCache();
    }
    rbx::off::SanityProbe();

    if (dumpOnly) {
        rbx::off::SaveCache();
        log::Info("--dump-sigs done, exiting");
        log::Shutdown();
        return 0;
    }

    if (!overlay::Init()) { log::Err("overlay init failed"); return 3; }

    int topmostTick = 0;
    int probeTick   = 0;
    while (mem::IsAlive()) {
        overlay::BeginFrame();

        rbx::CameraSnap cam;
        auto players = rbx::Snapshot(cfg.visuals, cam);
        rbx::DrawESP(players, cam, cfg.visuals);

        menu::Frame(cfg, { { "Game", &rbx::DrawGameTab } });

        overlay::EndFrame();
        if ((topmostTick++ & 31) == 0) overlay::AssertTopmost();

        // Every ~10s, re-probe Humanoid + Part if they weren't resolved yet
        // (can happen if the character spawned after startup).
        if ((probeTick++ % 600) == 0) {
            if (rbx::off::g.player.LocalPlayer && (!rbx::off::g.humanoid.Health || !rbx::off::g.part.CFrame)) {
                rbx::probe::All();
                rbx::off::SaveCache();
            }
        }
    }

    log::Info("Roblox gone, shutting down");
    overlay::Shutdown();
    mem::Detach();
    log::Shutdown();
    return 0;
}
