#include "probe.h"
#include "sigs.h"
#include "game.h"
#include "mem.h"
#include "log.h"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace rbx::probe {

// --------- Low-level readers that do NOT depend on resolved offsets ---------

static std::string ReadStdStringAt(uintptr_t fieldAddr) {
    char inlineBuf[16]{};
    mem::ExtRPM(fieldAddr, inlineBuf, sizeof(inlineBuf));
    size_t len = mem::ExtRead<size_t>(fieldAddr + 0x10);
    size_t cap = mem::ExtRead<size_t>(fieldAddr + 0x18);
    if (len > 0x1000 || cap > 0x100000) return {};
    if (cap < 16) {
        inlineBuf[std::min<size_t>(len, 15)] = 0;
        return inlineBuf;
    }
    uintptr_t heap = *reinterpret_cast<uintptr_t*>(inlineBuf);
    if (!heap) return {};
    return mem::ReadCStr(heap, std::min<size_t>(len, 256));
}

struct VecBounds { uintptr_t begin; uintptr_t end; size_t count; };

// Vector layout: shared_ptr<T> slot = 2 pointers (object, control_block) = 16 bytes.
// field = shared_ptr<vector<shared_ptr<Instance>>>.
// vec is: begin, end, cap at offsets 0, 8, 16.
static bool ReadVecAt(uintptr_t fieldAddr, VecBounds& out, size_t minN, size_t maxN) {
    uintptr_t vecPtr = mem::ExtRead<uintptr_t>(fieldAddr);
    if (!vecPtr) return false;
    uintptr_t begin = mem::ExtRead<uintptr_t>(vecPtr);
    uintptr_t end   = mem::ExtRead<uintptr_t>(vecPtr + 8);
    if (!begin || end <= begin) return false;
    if ((end - begin) % 16 != 0) return false;
    size_t n = (end - begin) / 16;
    if (n < minN || n > maxN) return false;
    out = { begin, end, n };
    return true;
}

static std::vector<uintptr_t> ReadChildrenAt(uintptr_t inst, uint32_t childOff) {
    std::vector<uintptr_t> out;
    VecBounds vb;
    if (!ReadVecAt(inst + childOff, vb, 0, 8192)) return out;
    size_t lim = std::min(vb.count, (size_t)128);
    for (size_t i = 0; i < lim; ++i) {
        uintptr_t obj = mem::ExtRead<uintptr_t>(vb.begin + i * 16);
        if (obj) out.push_back(obj);
    }
    return out;
}

static bool IsOrthonormal3(const float r[9]) {
    for (int i = 0; i < 9; ++i) if (!(r[i] == r[i])) return false;
    auto len = [](float a, float b, float c) { return std::sqrt(a * a + b * b + c * c); };
    if (std::fabs(len(r[0], r[1], r[2]) - 1.f) > 0.02f) return false;
    if (std::fabs(len(r[3], r[4], r[5]) - 1.f) > 0.02f) return false;
    if (std::fabs(len(r[6], r[7], r[8]) - 1.f) > 0.02f) return false;
    auto dot = [](const float* a, const float* b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; };
    if (std::fabs(dot(r,     r + 3)) > 0.02f) return false;
    if (std::fabs(dot(r,     r + 6)) > 0.02f) return false;
    if (std::fabs(dot(r + 3, r + 6)) > 0.02f) return false;
    return true;
}

// --------- Probes ---------

bool InstanceLayout(uintptr_t dm) {
    static const uint32_t CHILD_CANDIDATES[] = {
        0x60, 0x68, 0x70, 0x78, 0x80, 0x88, 0x90, 0x98, 0xA0, 0xA8, 0xB0, 0xB8
    };
    static const uint32_t NAME_CANDIDATES[] = {
        0x28, 0x30, 0x38, 0x40, 0x48, 0x50, 0x58, 0x60, 0x68, 0x70, 0x78, 0x80
    };
    static const char* EXPECTED[] = {
        "Workspace", "Players", "Lighting", "ReplicatedStorage",
        "StarterGui", "RunService", "SoundService", "HttpService",
        "UserInputService", "ReplicatedFirst", "StarterPack", "StarterPlayer",
        "TweenService", "Teams"
    };

    for (uint32_t childOff : CHILD_CANDIDATES) {
        VecBounds vb;
        if (!ReadVecAt(dm + childOff, vb, 3, 500)) continue;

        for (uint32_t nameOff : NAME_CANDIDATES) {
            int hits = 0;
            size_t lim = std::min(vb.count, (size_t)64);
            for (size_t i = 0; i < lim; ++i) {
                uintptr_t child = mem::ExtRead<uintptr_t>(vb.begin + i * 16);
                if (!child) continue;
                std::string nm = ReadStdStringAt(child + nameOff);
                if (nm.empty()) continue;
                for (auto s : EXPECTED) if (nm == s) { ++hits; break; }
            }
            if (hits >= 3) {
                off::g.inst.Children = childOff;
                off::g.inst.Name     = nameOff;
                hilog::Info("probe: inst.Children=0x%X  inst.Name=0x%X  (%d expected-name hits)",
                          childOff, nameOff, hits);
                return true;
            }
        }
    }
    return false;
}

bool ClassLayout(uintptr_t knownInst, const char* expected) {
    static const uint32_t CLS_CANDIDATES[]     = { 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40 };
    static const uint32_t CLSNAME_CANDIDATES[] = { 0x00, 0x08, 0x10, 0x18, 0x20 };

    for (uint32_t clsOff : CLS_CANDIDATES) {
        uintptr_t cls = mem::ExtRead<uintptr_t>(knownInst + clsOff);
        if (!cls) continue;

        for (uint32_t nameOff : CLSNAME_CANDIDATES) {
            // Try: pointer-to-cstr
            uintptr_t strPtr = mem::ExtRead<uintptr_t>(cls + nameOff);
            if (strPtr) {
                std::string nm = mem::ReadCStr(strPtr, 48);
                if (nm == expected) {
                    off::g.inst.ClassDescriptor = clsOff;
                    off::g.cls.Name = nameOff;
                    hilog::Info("probe: inst.ClassDescriptor=0x%X  cls.Name=0x%X  (cstr '%s')",
                              clsOff, nameOff, nm.c_str());
                    return true;
                }
            }
            // Try: inline std::string
            std::string nm2 = ReadStdStringAt(cls + nameOff);
            if (nm2 == expected) {
                off::g.inst.ClassDescriptor = clsOff;
                off::g.cls.Name = nameOff;
                hilog::Info("probe: inst.ClassDescriptor=0x%X  cls.Name=0x%X  (SSO '%s')",
                          clsOff, nameOff, nm2.c_str());
                return true;
            }
        }
    }
    return false;
}

bool CameraLayout(uintptr_t cam) {
    bool cfOk = false, vpOk = false, fovOk = false;

    for (uint32_t off = 0x100; off < 0x900 && !cfOk; off += 4) {
        float r[9];
        if (!mem::ExtRPM(cam + off, r, sizeof(r))) continue;
        if (!IsOrthonormal3(r)) continue;
        Vec3 p = mem::ExtRead<Vec3>(cam + off + 36);
        if (!(p.x == p.x) || !(p.y == p.y) || !(p.z == p.z)) continue;
        if (std::fabs(p.x) > 1e7f || std::fabs(p.y) > 1e7f || std::fabs(p.z) > 1e7f) continue;
        off::g.cam.CoordinateFrame = off;
        cfOk = true;
        hilog::Info("probe: cam.CoordinateFrame=0x%X  (pos %.1f,%.1f,%.1f)", off, p.x, p.y, p.z);
    }

    for (uint32_t off = 0x100; off < 0xA00 && !vpOk; off += 4) {
        Vec2 v = mem::ExtRead<Vec2>(cam + off);
        if (!(v.x == v.x) || !(v.y == v.y)) continue;
        if (v.x < 400.f || v.x > 8192.f) continue;
        if (v.y < 300.f || v.y > 8192.f) continue;
        if (v.x < v.y) continue;
        off::g.cam.ViewportSize = off;
        vpOk = true;
        hilog::Info("probe: cam.ViewportSize=0x%X  (%.0fx%.0f)", off, v.x, v.y);
    }

    for (uint32_t off = 0x100; off < 0xA00 && !fovOk; off += 4) {
        float f = mem::ExtRead<float>(cam + off);
        if (!(f == f)) continue;
        if (f < 10.f || f > 120.f) continue;
        // Skip round-integer whole-pixel counts — a field holding 100.f could be opacity, not FOV
        float frac = f - (float)(int)f;
        if (frac == 0.f && f > 90.f) continue;
        off::g.cam.FieldOfView = off;
        fovOk = true;
        hilog::Info("probe: cam.FieldOfView=0x%X  (%.2f)", off, f);
    }
    return cfOk && vpOk && fovOk;
}

bool PartLayout(uintptr_t part) {
    for (uint32_t off = 0x100; off < 0x400; off += 4) {
        float r[9];
        if (!mem::ExtRPM(part + off, r, sizeof(r))) continue;
        if (!IsOrthonormal3(r)) continue;
        Vec3 p = mem::ExtRead<Vec3>(part + off + 36);
        if (!(p.x == p.x) || !(p.y == p.y) || !(p.z == p.z)) continue;
        if (std::fabs(p.x) > 1e7f) continue;
        off::g.part.CFrame = off;
        hilog::Info("probe: part.CFrame=0x%X  (pos %.1f,%.1f,%.1f)", off, p.x, p.y, p.z);
        return true;
    }
    return false;
}

bool PlayerServiceLayout(uintptr_t playersSvc) {
    if (!off::g.inst.Children) return false;
    auto kids = ReadChildrenAt(playersSvc, off::g.inst.Children);
    if (kids.empty()) return false;
    std::set<uintptr_t> kidSet(kids.begin(), kids.end());

    for (uint32_t off = 0x40; off < 0x240; off += 8) {
        uintptr_t cand = mem::ExtRead<uintptr_t>(playersSvc + off);
        if (!cand) continue;
        if (kidSet.count(cand)) {
            off::g.player.LocalPlayer = off;
            hilog::Info("probe: player.LocalPlayer=0x%X  -> 0x%p", off, (void*)cand);
            return true;
        }
    }
    return false;
}

bool PlayerFields(uintptr_t player) {
    if (!off::g.inst.Children || !off::g.inst.Name) return false;
    for (uint32_t off = 0x40; off < 0x240; off += 8) {
        uintptr_t cand = mem::ExtRead<uintptr_t>(player + off);
        if (!cand) continue;
        auto kids = ReadChildrenAt(cand, off::g.inst.Children);
        if (kids.size() < 3 || kids.size() > 500) continue;
        for (auto c : kids) {
            std::string n = ReadStdStringAt(c + off::g.inst.Name);
            if (n == "HumanoidRootPart" || n == "Head" || n == "Torso" || n == "UpperTorso") {
                off::g.player.Character = off;
                hilog::Info("probe: player.Character=0x%X", off);
                return true;
            }
        }
    }
    return false;
}

bool HumanoidLayout(uintptr_t humanoid) {
    for (uint32_t off = 0x100; off < 0x500; off += 4) {
        float h = mem::ExtRead<float>(humanoid + off);
        if (!(h == h) || h < 0.f || h > 1e5f) continue;

        // Try MaxHealth at +4 (adjacent) or +8 (skipping a tag field)
        for (int stride : { 4, 8 }) {
            float m = mem::ExtRead<float>(humanoid + off + stride);
            if (!(m == m) || m <= 0.f || m > 1e5f) continue;
            if (h > m + 0.1f) continue;
            off::g.humanoid.Health    = off;
            off::g.humanoid.MaxHealth = off + stride;
            hilog::Info("probe: humanoid.Health=0x%X  MaxHealth=0x%X  (%.1f/%.1f)",
                      off, off + stride, h, m);
            return true;
        }
    }
    return false;
}

// --------- Orchestration ---------

static uintptr_t FindChildNamed(uintptr_t parent, const char* name) {
    auto kids = ReadChildrenAt(parent, off::g.inst.Children);
    for (auto c : kids) {
        if (ReadStdStringAt(c + off::g.inst.Name) == name) return c;
    }
    return 0;
}

bool All() {
    if (!off::g.FakeDataModelPtr) { hilog::Err("probe: FakeDataModelPtr unresolved"); return false; }
    uintptr_t dm = mem::ExtRead<uintptr_t>(off::g.FakeDataModelPtr);
    if (!dm) { hilog::Err("probe: DataModel null (game not in a place yet?)"); return false; }

    if (!InstanceLayout(dm)) { hilog::Err("probe: InstanceLayout FAILED"); return false; }

    uintptr_t workspace = FindChildNamed(dm, "Workspace");
    if (!workspace) { hilog::Err("probe: no Workspace child"); return false; }

    ClassLayout(workspace, "Workspace");    // best-effort, defaults OK if it fails

    uintptr_t camera = FindChildNamed(workspace, "Camera");
    if (!camera) camera = FindChildNamed(workspace, "CurrentCamera");
    if (!camera) { hilog::Err("probe: no Camera under Workspace"); return false; }
    if (!CameraLayout(camera)) { hilog::Err("probe: CameraLayout FAILED"); return false; }

    // Seed part.CFrame from cam.CoordinateFrame; overwrite authoritatively below if a part is reachable.
    off::g.part.CFrame = off::g.cam.CoordinateFrame;

    uintptr_t players = FindChildNamed(dm, "Players");
    if (!players) { hilog::Warn("probe: no Players service (probably not in game yet)"); return true; }

    if (!PlayerServiceLayout(players)) {
        hilog::Warn("probe: PlayerServiceLayout failed, keeping defaults");
        return true;
    }

    uintptr_t localPlr = mem::ExtRead<uintptr_t>(players + off::g.player.LocalPlayer);
    if (!localPlr) { hilog::Warn("probe: LocalPlayer null"); return true; }

    if (!PlayerFields(localPlr)) {
        hilog::Warn("probe: Player.Character not located (character not spawned?)");
        return true;
    }

    uintptr_t charAddr = mem::ExtRead<uintptr_t>(localPlr + off::g.player.Character);
    if (!charAddr) { hilog::Warn("probe: local Character null"); return true; }

    uintptr_t humanoid = FindChildNamed(charAddr, "Humanoid");
    if (humanoid) HumanoidLayout(humanoid);

    uintptr_t hrp = FindChildNamed(charAddr, "HumanoidRootPart");
    if (!hrp) hrp = FindChildNamed(charAddr, "Head");
    if (hrp) PartLayout(hrp);

    return true;
}

} // namespace rbx::probe
