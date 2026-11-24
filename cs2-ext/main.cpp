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
void DrawGameTab();
}

int main() {
    log::Init(L"cs2-ext");
    log::Info("cs2-ext starting");

    config::AppCfg cfg;
    config::Load(L"cs2-ext", cfg);

    if (!mem::WaitAttach(L"cs2.exe")) { log::Err("could not attach cs2.exe"); return 1; }
    if (!cs2::off::Load())            { log::Err("offsets failed to load"); return 2; }

    // resolve client.dll once; its base can change between game launches but not within a session
    auto client = scan::GetModule(mem::g_pid, L"client.dll");
    while (!client) {
        log::Info("waiting for client.dll to map...");
        std::this_thread::sleep_for(std::chrono::seconds(1));
        client = scan::GetModule(mem::g_pid, L"client.dll");
    }
    log::Info("client.dll base=0x%p size=0x%zX", (void*)client->base, client->size);
    cs2::off::SanityProbe();

    if (!overlay::Init()) { log::Err("overlay init failed"); return 3; }

    int topmostTick = 0;
    while (mem::IsAlive()) {
        overlay::BeginFrame();

        cs2::ViewState vs; vs.Read(client->base);
        int sw, sh; overlay::GetSize(sw, sh);
        auto players = cs2::Snapshot(client->base, vs.viewMatrix, sw, sh, cfg.visuals);
        cs2::DrawESP(players, cfg.visuals);

        menu::Frame(cfg, { { "Game", &cs2::DrawGameTab } });

        overlay::EndFrame();
        if ((topmostTick++ & 31) == 0) overlay::AssertTopmost();
    }

    log::Info("cs2.exe gone, shutting down");
    overlay::Shutdown();
    mem::Detach();
    log::Shutdown();
    return 0;
}
