#pragma once
#include <cmath>
#include <cstdint>

struct Vec2 { float x, y; };
struct Vec3 { float x, y, z;
    Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
    Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
    Vec3 operator*(float s)      const { return { x * s, y * s, z * s }; }
    float Dot(const Vec3& o)     const { return x * o.x + y * o.y + z * o.z; }
    float Length()               const { return std::sqrt(x * x + y * y + z * z); }
    Vec3  Normalized()           const { float l = Length(); return l > 1e-6f ? Vec3{ x / l, y / l, z / l } : Vec3{ 0,0,0 }; }
};
struct Vec4 { float x, y, z, w; };

// Source 2 view matrix: row-major 4x4 in memory.
struct Mat4 { float m[16]; };

// Roblox CFrame: 3x3 rotation + vec3 position, stored as 12 floats (R00..R22, X, Y, Z).
struct CFrame {
    float R[9];
    float X, Y, Z;
};

inline float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

// a - b in world space, length in studs (Roblox) or inches (Source 2 uses inches).
inline float Distance(const Vec3& a, const Vec3& b) {
    return (a - b).Length();
}
