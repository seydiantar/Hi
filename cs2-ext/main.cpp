#include "mem.h"
#include "scanner.h"
#include "overlay.h"
#include "menu.h"
#include "config.h"
#include "log.h"
#include "offsets.h"
#include "entity.h"
#include "player.h"
#include <windows.h>
#include <thread>
#include <chrono>

namespace cs2 {
void DrawESP(const std::vector<ScreenPlayer>&, const config::VisualCfg&);
void DrawSkeleton(const std::vector<ScreenPlayer>&, const float vm[16], const config::VisualCfg&);
void DrawGameTab();
}

int wmain() {
    hilog::Init(L"cs2-ext");
    hilog::Info("cs2-ext starting");

    config::AppCfg cfg;
    config::Load(L"cs2-ext", cfg);

    if (!overlay::Init()) { hilog::Err("overlay init failed"); return 3; }

    while (true) {
        if (!mem::WaitAttach(L"cs2.exe")) { hilog::Err("attach failed"); break; }
        if (!cs2::off::Load()) { hilog::Err("offsets load failed"); mem::Detach(); continue; }

        auto client = scan::GetModule(mem::g_pid, L"client.dll");
        for (int i = 0; !client && i < 30; ++i) {
            std::this_thread::sleep_for(std::chrono::seconds(1));
            client = scan::GetModule(mem::g_pid, L"client.dll");
        }
        if (!client) { hilog::Err("client.dll never mapped"); mem::Detach(); continue; }
        hilog::Info("client.dll base=0x%p size=0x%zX", (void*)client->base, client->size);
        cs2::off::SanityProbe();

        int topmostTick = 0;
        while (mem::IsAlive()) {
            overlay::BeginFrame();

            cs2::ViewState vs; vs.Read(client->base);
            int sw, sh; overlay::GetSize(sw, sh);
            auto players = cs2::Snapshot(client->base, vs.viewMatrix, sw, sh, cfg.visuals);
            cs2::DrawSkeleton(players, vs.viewMatrix, cfg.visuals);
            cs2::DrawESP(players, cfg.visuals);

            menu::Frame(cfg, { { "Game", &cs2::DrawGameTab } });

            overlay::EndFrame();
            if ((topmostTick++ & 31) == 0) overlay::AssertTopmost();
        }

        hilog::Info("cs2.exe gone, waiting for next launch...");
        mem::Detach();
    }

    overlay::Shutdown();
    hilog::Shutdown();
    return 0;
}
