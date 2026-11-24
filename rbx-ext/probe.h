#pragma once
#include <cstdint>

namespace rbx::probe {

// All() walks the full resolve chain:
//   DataModel -> Children (probe inst.Name + inst.Children)
//             -> Workspace (probe cls.ClassDescriptor + cls.Name)
//             -> Camera    (probe cam.CoordinateFrame + ViewportSize + FieldOfView)
//             -> Players   (probe player.LocalPlayer)
//             -> LocalPlayer -> Character (probe player.Character)
//                            -> Humanoid  (probe humanoid.Health + MaxHealth)
//                            -> HumanoidRootPart (probe part.CFrame authoritatively)
// Logs each resolved offset.
// Returns false only if the critical chain (DataModel + Instance layout + Camera) fails.
// Players/Character/Humanoid resolve best-effort — character may not exist yet.
bool All();

// Individual probes callers can re-run after character re-spawn etc.
bool InstanceLayout(uintptr_t dm);
bool ClassLayout(uintptr_t knownInst, const char* expectedClassName);
bool CameraLayout(uintptr_t camera);
bool PartLayout(uintptr_t part);
bool PlayerServiceLayout(uintptr_t playersSvc);
bool PlayerFields(uintptr_t player);
bool HumanoidLayout(uintptr_t humanoid);

} // namespace rbx::probe
