#include "camera.h"
#include "sigs.h"
#include "mem.h"
#include <cmath>

namespace rbx {

CameraSnap ReadCamera() {
    CameraSnap s{};
    DataModel dm = GetDataModel();
    if (!dm) return s;
    uintptr_t ws = dm.Service("Workspace");
    if (!ws) return s;
    uintptr_t cam = FindFirstChildByName(ws, "Camera");
    if (!cam) cam = FindFirstChildByName(ws, "CurrentCamera");
    if (!cam) cam = FindFirstChildByClass(ws, "Camera");
    if (!cam) return s;

    s.cf       = mem::ExtRead<CFrame>(cam + off::g.cam.CoordinateFrame);
    s.viewport = mem::ExtRead<Vec2>  (cam + off::g.cam.ViewportSize);
    s.fov      = mem::ExtRead<float> (cam + off::g.cam.FieldOfView);
    s.valid = (s.viewport.x > 1.f && s.viewport.y > 1.f
            && s.fov > 1.f && s.fov < 170.f
            && s.cf.R[0] == s.cf.R[0]);
    return s;
}

// Roblox CFrame rotation is stored row-major: R[0..2]=row0, R[3..5]=row1, R[6..8]=row2.
// The world-to-local transform uses R^T, so the columns of R become rows:
//   local.x = R[0]*dx + R[3]*dy + R[6]*dz  (column 0)
//   local.y = R[1]*dx + R[4]*dy + R[7]*dz  (column 1)
//   local.z = R[2]*dx + R[5]*dy + R[8]*dz  (column 2)
Vec3 WorldToLocal(const CameraSnap& c, Vec3 w) {
    float dx = w.x - c.cf.X, dy = w.y - c.cf.Y, dz = w.z - c.cf.Z;
    return {
        c.cf.R[0] * dx + c.cf.R[3] * dy + c.cf.R[6] * dz,
        c.cf.R[1] * dx + c.cf.R[4] * dy + c.cf.R[7] * dz,
        c.cf.R[2] * dx + c.cf.R[5] * dy + c.cf.R[8] * dz,
    };
}

bool WorldToScreen(const CameraSnap& c, Vec3 w, Vec2& out) {
    float d;
    return WorldToScreenWithDepth(c, w, out, d);
}

bool WorldToScreenWithDepth(const CameraSnap& c, Vec3 w, Vec2& out, float& depth) {
    if (!c.valid) return false;
    Vec3 local = WorldToLocal(c, w);
    // Roblox camera looks down -Z, so points in front satisfy local.z < 0.
    if (local.z >= -0.1f) return false;
    depth = -local.z;
    float aspect = c.viewport.x / c.viewport.y;
    float f = 1.0f / std::tan((c.fov * 0.017453292519943295f) * 0.5f);
    float ndcX = (local.x * f / aspect) / depth;
    float ndcY = (local.y * f)          / depth;
    out.x = (ndcX * 0.5f + 0.5f) * c.viewport.x;
    out.y = (1.0f - (ndcY * 0.5f + 0.5f)) * c.viewport.y;
    return true;
}

Vec3 EyePos (const CameraSnap& c) { return { c.cf.X, c.cf.Y, c.cf.Z }; }
Vec3 Right  (const CameraSnap& c) { return { c.cf.R[0], c.cf.R[3], c.cf.R[6] }; }
Vec3 Up     (const CameraSnap& c) { return { c.cf.R[1], c.cf.R[4], c.cf.R[7] }; }
Vec3 Forward(const CameraSnap& c) { return { -c.cf.R[2], -c.cf.R[5], -c.cf.R[8] }; }

float RaylikeDistance(const CameraSnap& c, Vec3 w, Vec2 screen) {
    Vec2 proj;
    if (!WorldToScreen(c, w, proj)) return 1e9f;
    float dx = proj.x - screen.x, dy = proj.y - screen.y;
    return std::sqrt(dx * dx + dy * dy);
}

} // namespace rbx
