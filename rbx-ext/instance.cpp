#include "game.h"
#include "log.h"
#include <algorithm>

namespace rbx {

// std::string SSO: when length < 16, buf is inline; else buf is a heap pointer.
// Layout (MSVC x64): [ptr or inline 16 bytes][size_t len][size_t cap]
static std::string ReadStdString(uintptr_t fieldAddr) {
    char   inlineBuf[16]{};
    mem::ExtRPM(fieldAddr, inlineBuf, sizeof(inlineBuf));
    size_t len = mem::ExtRead<size_t>(fieldAddr + 0x10);
    size_t cap = mem::ExtRead<size_t>(fieldAddr + 0x18);
    if (len > 0x1000 || cap > 0x100000) return {};        // junk guard
    if (cap < 16) {
        inlineBuf[std::min<size_t>(len, 15)] = 0;
        return inlineBuf;
    }
    uintptr_t heap = *reinterpret_cast<uintptr_t*>(inlineBuf);
    if (!heap) return {};
    return mem::ReadCStr(heap, std::min<size_t>(len, 256));
}

std::string ReadInstanceName(uintptr_t inst) {
    if (!inst) return {};
    return ReadStdString(inst + off::g.inst.Name);
}

std::string ReadClassName(uintptr_t inst) {
    if (!inst) return {};
    uintptr_t cls = mem::ExtRead<uintptr_t>(inst + off::g.inst.ClassDescriptor);
    if (!cls) return {};
    // ClassDescriptor.Name is usually a pointer-to-string or a direct std::string.
    // Try pointer-to-cstr first, then std::string in place.
    uintptr_t ptr = mem::ExtRead<uintptr_t>(cls + off::g.cls.Name);
    std::string s = mem::ReadCStr(ptr, 64);
    if (!s.empty()) return s;
    return ReadStdString(cls + off::g.cls.Name);
}

std::vector<uintptr_t> Children(uintptr_t inst) {
    std::vector<uintptr_t> out;
    if (!inst) return out;
    // Instance.Children is shared_ptr<vector<shared_ptr<Instance>>>
    // control block layout: [ptr to vec][ptr to ctl]
    uintptr_t vecPtr = mem::ExtRead<uintptr_t>(inst + off::g.inst.Children);
    if (!vecPtr) return out;

    uintptr_t begin = mem::ExtRead<uintptr_t>(vecPtr + 0x0);
    uintptr_t end   = mem::ExtRead<uintptr_t>(vecPtr + 0x8);
    if (!begin || end <= begin) return out;

    size_t n = (end - begin) / sizeof(uintptr_t);
    if (n > 8192) return out;                             // sanity

    out.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        uintptr_t slot = begin + i * sizeof(uintptr_t) * 2;
        uintptr_t obj  = mem::ExtRead<uintptr_t>(slot);
        if (obj) out.push_back(obj);
    }
    return out;
}

uintptr_t FindFirstChildByName(uintptr_t inst, const std::string& name) {
    for (auto c : Children(inst)) {
        if (ReadInstanceName(c) == name) return c;
    }
    return 0;
}

uintptr_t FindFirstChildByClass(uintptr_t inst, const std::string& klass) {
    for (auto c : Children(inst)) {
        if (ReadClassName(c) == klass) return c;
    }
    return 0;
}

DataModel GetDataModel() {
    uintptr_t dm = mem::ExtRead<uintptr_t>(off::g.FakeDataModelPtr);
    return { dm };
}

} // namespace rbx
