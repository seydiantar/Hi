#include "config.h"
#include "log.h"
#include <windows.h>
#include <shlobj.h>
#include <fstream>

namespace config {

std::filesystem::path PathFor(const wchar_t* appName) {
    wchar_t appdata[MAX_PATH]{};
    if (SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, appdata) != S_OK) return {};
    auto dir = std::filesystem::path(appdata) / L"hi";
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    return dir / (std::wstring(appName) + L".json");
}

bool Load(const wchar_t* appName, AppCfg& out) {
    auto path = PathFor(appName);
    std::ifstream f(path);
    if (!f) return false;
    try {
        nlohmann::json j; f >> j;
        out = j.get<AppCfg>();
        hilog::Info("config loaded from %ls", path.c_str());
        return true;
    } catch (const std::exception& e) {
        hilog::Warn("config load failed: %s", e.what());
        return false;
    }
}

bool Save(const wchar_t* appName, const AppCfg& in) {
    auto path = PathFor(appName);
    nlohmann::json j = in;
    std::ofstream f(path);
    if (!f) { hilog::Err("config save open failed"); return false; }
    f << j.dump(2);
    hilog::Info("config saved to %ls", path.c_str());
    return true;
}

} // namespace config
