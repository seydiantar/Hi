#include "log.h"
#include <windows.h>
#include <shlobj.h>
#include <cstdio>
#include <cstdarg>
#include <ctime>
#include <filesystem>

namespace log {

static FILE*            g_fp    = nullptr;
static std::mutex       g_lock;
static std::vector<Entry> g_ring;
static constexpr size_t   RING_MAX = 512;

static const char* LevelTag(Level l) {
    switch (l) { case Level::Dbg: return "DBG"; case Level::Info: return "INF";
                 case Level::Warn: return "WRN"; case Level::Err:  return "ERR"; }
    return "???";
}

void Init(const wchar_t* appName) {
    wchar_t appdata[MAX_PATH];
    if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata) != S_OK) return;
    std::filesystem::path dir = std::filesystem::path(appdata) / L"hi" / L"log";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);

    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_s(&tm, &t);
    wchar_t stamp[64];
    swprintf_s(stamp, L"%ls-%04d%02d%02d-%02d%02d%02d.log",
               appName, tm.tm_year + 1900, tm.tm_mon + 1, tm.tm_mday,
               tm.tm_hour, tm.tm_min, tm.tm_sec);
    auto path = dir / stamp;
    _wfopen_s(&g_fp, path.wstring().c_str(), L"w");
}

void Shutdown() { if (g_fp) { fclose(g_fp); g_fp = nullptr; } }

void Write(Level lvl, const char* fmt, ...) {
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);

    {
        std::lock_guard<std::mutex> lk(g_lock);
        if (g_fp) {
            std::time_t t = std::time(nullptr);
            std::tm tm{};
            localtime_s(&tm, &t);
            fprintf(g_fp, "[%02d:%02d:%02d] %s %s\n",
                    tm.tm_hour, tm.tm_min, tm.tm_sec, LevelTag(lvl), buf);
            fflush(g_fp);
        }
        g_ring.push_back({ lvl, buf });
        if (g_ring.size() > RING_MAX) g_ring.erase(g_ring.begin());
    }
    OutputDebugStringA(buf);
    OutputDebugStringA("\n");
}

const std::vector<Entry>& Ring() { return g_ring; }
std::mutex&               RingLock() { return g_lock; }

} // namespace log
