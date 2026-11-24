#include "wts.h"
#include <cmath>

namespace wts {

bool Source2(const float m[16], Vec3 w, int sw, int sh, Vec2& out) {
    float x  = w.x * m[0]  + w.y * m[1]  + w.z * m[2]  + m[3];
    float y  = w.x * m[4]  + w.y * m[5]  + w.z * m[6]  + m[7];
    float wW = w.x * m[12] + w.y * m[13] + w.z * m[14] + m[15];
    if (wW < 0.01f) return false;
    float inv = 1.0f / wW;
    out.x = (sw * 0.5f) + (x * inv) * (sw * 0.5f);
    out.y = (sh * 0.5f) - (y * inv) * (sh * 0.5f);
    return true;
}

// Transform world point into Roblox camera-local space:
//   local = R_T * (world - camPos)   where R is the CFrame rotation stored row-major.
// Roblox cameras look down -Z (OpenGL convention).
bool Roblox(const CFrame& c, Vec3 w, float fovDeg, int sw, int sh, Vec2& out) {
    float dx = w.x - c.X, dy = w.y - c.Y, dz = w.z - c.Z;

    // Row-major 3x3, transpose implicit: columns are the axes right(R[0,3,6]), up(R[1,4,7]), -look(R[2,5,8]).
    float lx =  c.R[0] * dx + c.R[3] * dy + c.R[6] * dz;
    float ly =  c.R[1] * dx + c.R[4] * dy + c.R[7] * dz;
    float lz =  c.R[2] * dx + c.R[5] * dy + c.R[8] * dz;

    // Camera forward = -Z in Roblox CFrame; "in front of camera" = lz < 0.
    if (lz > -0.01f) return false;

    float aspect = (float)sw / (float)sh;
    float fovRad = fovDeg * 0.017453292519943295f;
    float f      = 1.0f / std::tan(fovRad * 0.5f);

    // Perspective-divide using -lz as positive depth.
    float ndcX =  (lx * f / aspect) / -lz;
    float ndcY =  (ly * f)          / -lz;

    out.x = (ndcX * 0.5f + 0.5f) * sw;
    out.y = (1.0f - (ndcY * 0.5f + 0.5f)) * sh;
    return true;
}

} // namespace wts
