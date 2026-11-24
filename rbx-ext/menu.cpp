#include "menu.h"
#include "sigs.h"
#include <imgui.h>

namespace rbx {

void DrawGameTab() {
    ImGui::TextUnformatted("Roblox target status");
    ImGui::Separator();
    ImGui::Text("FakeDataModelPtr     0x%p", (void*)off::g.FakeDataModelPtr);
    ImGui::Text("inst.Name            0x%X", off::g.inst.Name);
    ImGui::Text("inst.Children        0x%X", off::g.inst.Children);
    ImGui::Text("part.CFrame          0x%X", off::g.part.CFrame);
    ImGui::Text("cam.CoordinateFrame  0x%X", off::g.cam.CoordinateFrame);
    ImGui::Text("cam.ViewportSize     0x%X", off::g.cam.ViewportSize);
    ImGui::Text("cam.FieldOfView      0x%X", off::g.cam.FieldOfView);
    ImGui::Text("humanoid.Health      0x%X", off::g.humanoid.Health);
    ImGui::Separator();
    if (ImGui::Button("Re-scan sigs")) { off::Resolve(); off::SaveCache(); }
    ImGui::SameLine();
    if (ImGui::Button("Save cache"))   { off::SaveCache(); }
}

} // namespace rbx
