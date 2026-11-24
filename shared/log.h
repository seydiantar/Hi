#pragma once
#include <string>
#include <vector>
#include <mutex>

namespace hilog {

enum class Level { Dbg, Info, Warn, Err };

void Init(const wchar_t* appName);
void Shutdown();

void Write(Level lvl, const char* fmt, ...);
#define DEF_LOG(fn, lvl) template <class... A> inline void fn(const char* f, A... a) { Write(lvl, f, a...); }
DEF_LOG(Dbg,  Level::Dbg)
DEF_LOG(Info, Level::Info)
DEF_LOG(Warn, Level::Warn)
DEF_LOG(Err,  Level::Err)
#undef DEF_LOG

struct Entry { Level lvl; std::string text; };
const std::vector<Entry>& Ring();          // callers must lock
std::mutex&               RingLock();

} // namespace hilog
