#pragma once
#include "himath.h"

namespace wts {

// Source 2 (CS2) world-to-screen.
// viewMatrix: 16 floats, row-major. sw/sh = viewport size. out = pixel pos.
// returns false if behind camera (w < 0.01).
bool Source2(const float viewMatrix[16], Vec3 world, int sw, int sh, Vec2& out);

// Roblox world-to-screen.
// camCF: Camera.CFrame (3x3 rot + pos). fovDegrees: Camera.FieldOfView.
// sw/sh: Camera.ViewportSize.
// returns false if behind camera.
bool Roblox(const CFrame& camCF, Vec3 world, float fovDegrees, int sw, int sh, Vec2& out);

} // namespace wts
