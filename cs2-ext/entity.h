#pragma once
#include "game.h"
#include <cstdint>
#include <vector>

namespace cs2 {

struct PlayerSlot {
    Controller ctrl;
    Pawn       pawn;
    int        idx = -1;
};

// Resolve a pawn (or controller) address from the entity-list for index idx in [0,64).
// Returns 0 if missing.
uintptr_t GetEntity(uintptr_t clientBase, int idx);

// Read local player's pawn through dwLocalPlayerPawn (handle-based).
Pawn LocalPawn(uintptr_t clientBase);

// Iterate up to maxPlayers slots and collect valid controller+pawn pairs.
std::vector<PlayerSlot> EnumPlayers(uintptr_t clientBase, int maxPlayers = 64);

// Resolve C_BaseEntity from a 32-bit handle (lower 15 bits = index).
uintptr_t FromHandle(uintptr_t clientBase, uint32_t handle);

} // namespace cs2
