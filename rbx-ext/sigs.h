#pragma once
#include <cstdint>
#include <string>

namespace rbx::off {

// Everything in this file is subject to Roblox's weekly bumps.
// Startup scan writes resolved values into Offsets::g; cache stores them per module timestamp.

struct Offsets {
    // Module-pointer globals (absolute, resolved from RIP-relative loads in .text).
    uintptr_t FakeDataModelPtr = 0;      // *(FakeDataModelPtr) -> DataModel

    // Instance layout (relative to Instance*).
    struct {
        uint32_t ClassDescriptor = 0x18;  // starting guess
        uint32_t Name            = 0x78;  // std::string (SSO)
        uint32_t Parent          = 0x50;
        uint32_t Children        = 0x80;  // std::shared_ptr<std::vector<std::shared_ptr<Instance>>>
    } inst;

    // ClassDescriptor layout.
    struct {
        uint32_t Name            = 0x08;  // const char* or std::string
    } cls;

    // BasePart / PVInstance layout.
    struct {
        uint32_t CFrame          = 0x140; // 12 float rotation+position
        uint32_t Size            = 0x170; // Vector3
    } part;

    // Camera.
    struct {
        uint32_t CoordinateFrame = 0x3E8;
        uint32_t ViewportSize    = 0x410;
        uint32_t FieldOfView     = 0x470;
    } cam;

    // Humanoid.
    struct {
        uint32_t Health          = 0x1A0;
        uint32_t MaxHealth       = 0x1A8;
    } humanoid;

    // Player / Players service.
    struct {
        uint32_t LocalPlayer     = 0x48;   // inside Players
        uint32_t Character       = 0x60;   // inside Player
        uint32_t Team            = 0x70;
    } player;
};

extern Offsets g;

// Scan cs2.exe-style anchor sigs against RobloxPlayerBeta.exe .text.
// Loads cache first; falls back to full scan.
// Returns false if the critical pointers can't be resolved.
bool Resolve();

// Writes the current offsets struct to %LOCALAPPDATA%\hi\rbx-sigs.json
// keyed by {module path, timestamp, size}.
bool SaveCache();

// Sanity probe: confirms we can walk DataModel -> Workspace, Workspace.Name == "Workspace".
bool SanityProbe();

} // namespace rbx::off
