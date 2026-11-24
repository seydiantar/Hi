#include "menu.h"
#include "config.h"
#include "offsets.h"
#include <imgui.h>

namespace cs2 {

void DrawGameTab() {
    ImGui::TextUnformatted("CS2 target status");
    ImGui::Separator();
    ImGui::Text("dwLocalPlayerPawn   0x%08X", off::g.dwLocalPlayerPawn);
    ImGui::Text("dwEntityList        0x%08X", off::g.dwEntityList);
    ImGui::Text("dwViewMatrix        0x%08X", off::g.dwViewMatrix);
    ImGui::Text("C_BaseEntity::m_iHealth     0x%X", off::g.pawn.m_iHealth);
    ImGui::Text("CGameSceneNode::m_vecAbsOrigin 0x%X", off::g.scene.m_vecAbsOrigin);
    ImGui::Separator();
    if (ImGui::Button("Reload offsets")) { off::Load(); }
}

} // namespace cs2
