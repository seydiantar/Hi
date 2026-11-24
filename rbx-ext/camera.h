#pragma once
#include "math.h"
#include "game.h"

namespace rbx {

struct CameraSnap {
    CFrame cf;
    Vec2   viewport;
    float  fov;
    bool   valid;
};

// Reads DataModel -> Workspace -> Camera into a snapshot.
// Returns {valid=false} if any of the chain is null or offsets are stale.
CameraSnap ReadCamera();

// local = R_col * (world - pos)   using Roblox row-major CFrame convention.
Vec3  WorldToLocal(const CameraSnap& c, Vec3 world);

// Perspective-project a world point. Returns false if behind camera (z >= -near).
bool  WorldToScreen(const CameraSnap& c, Vec3 world, Vec2& screen);

// Same plus depth (positive = distance along -look).
bool  WorldToScreenWithDepth(const CameraSnap& c, Vec3 world, Vec2& screen, float& depth);

Vec3  EyePos(const CameraSnap& c);         // CFrame position
Vec3  Forward(const CameraSnap& c);        // -Z axis (what the camera looks at)
Vec3  Up(const CameraSnap& c);             // +Y axis
Vec3  Right(const CameraSnap& c);          // +X axis

// Distance from the view ray through `screen` to the world point. Used for closest-to-crosshair.
float RaylikeDistance(const CameraSnap& c, Vec3 world, Vec2 screen);

} // namespace rbx
