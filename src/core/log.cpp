#include "core/log.h"

#include <cstdarg>
#include <cstdio>
#include <ctime>

#include "core/fs.h"

namespace tg::log {

static std::string g_path;
static Level g_min = Level::Info;

void init(const std::string& path, Level minLevel) {
    g_path = path;
    g_min = minLevel;
    if (fs::exists(path)) {
        fs::remove(path + ".old");
        fs::copy(path, path + ".old");
        fs::remove(path);
    }
}

void setLevel(Level minLevel) { g_min = minLevel; }

void write(Level level, const char* fmt, ...) {
    if (level < g_min || g_path.empty()) return;
    char msg[2048];
    va_list ap;
    va_start(ap, fmt);
    std::vsnprintf(msg, sizeof msg, fmt, ap);
    va_end(ap);
    std::time_t t = std::time(nullptr);
    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
    FILE* f = _wfopen(fs::widen(g_path).c_str(), L"ab");
#else
    localtime_r(&t, &tm);
    FILE* f = std::fopen(g_path.c_str(), "ab");
#endif
    if (!f) return;
    static const char* names[] = {"debug", "info", "warn", "ERROR"};
    std::fprintf(f, "%02d:%02d:%02d [%s] %s\r\n", tm.tm_hour, tm.tm_min, tm.tm_sec, names[int(level)], msg);
    std::fclose(f);
}

}  // namespace tg::log
