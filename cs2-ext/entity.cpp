#include "entity.h"
#include "mem.h"

namespace cs2 {

// Source 2 CConcreteEntityList:
//   base = *(dwEntityList)
//   chunk = *(base + 0x10 + 8 * (idx >> 9))
//   entity = *(chunk + 120 * (idx & 0x1FF))
// (The stride 120 = sizeof(EntityHandle+entry) has been stable for many patches,
//  but is the single value most worth re-confirming against cs2-dumper if entity
//  resolve starts returning garbage.)
constexpr uint32_t ENTLIST_CHUNK_BASE_OFF = 0x10;
constexpr uint32_t ENTLIST_CHUNK_STRIDE   = 120;

uintptr_t GetEntity(uintptr_t clientBase, int idx) {
    uintptr_t list = mem::ExtRead<uintptr_t>(clientBase + off::g.dwEntityList);
    if (!list) return 0;
    uintptr_t chunk = mem::ExtRead<uintptr_t>(list + ENTLIST_CHUNK_BASE_OFF + 8u * (uint32_t)(idx >> 9));
    if (!chunk) return 0;
    return mem::ExtRead<uintptr_t>(chunk + ENTLIST_CHUNK_STRIDE * (uint32_t)(idx & 0x1FF));
}

uintptr_t FromHandle(uintptr_t clientBase, uint32_t handle) {
    if (handle == 0 || handle == 0xFFFFFFFFu) return 0;
    return GetEntity(clientBase, (int)(handle & 0x7FFF));
}

Pawn LocalPawn(uintptr_t clientBase) {
    uintptr_t p = mem::ExtRead<uintptr_t>(clientBase + off::g.dwLocalPlayerPawn);
    return Pawn{ p };
}

std::vector<PlayerSlot> EnumPlayers(uintptr_t clientBase, int maxPlayers) {
    std::vector<PlayerSlot> out;
    out.reserve(maxPlayers);
    for (int i = 1; i <= maxPlayers; ++i) {
        uintptr_t ctrlA = GetEntity(clientBase, i);
        if (!ctrlA) continue;
        Controller c{ ctrlA };
        if (!c.PawnAlive()) continue;
        uintptr_t pawnA = FromHandle(clientBase, c.HPawn());
        if (!pawnA) continue;
        Pawn p{ pawnA };
        if (!p.Alive() || p.Dormant()) continue;
        out.push_back({ c, p, i });
    }
    return out;
}

} // namespace cs2
