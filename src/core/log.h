// TrailGuide.log next to the .asi; the previous session is kept as TrailGuide.log.old.
#pragma once
#include <string>

namespace tg::log {

enum class Level { Debug = 0, Info = 1, Warn = 2, Error = 3 };

void init(const std::string& path, Level minLevel);
void setLevel(Level minLevel);
void write(Level level, const char* fmt, ...);

}  // namespace tg::log

#define TG_DEBUG(...) ::tg::log::write(::tg::log::Level::Debug, __VA_ARGS__)
#define TG_INFO(...) ::tg::log::write(::tg::log::Level::Info, __VA_ARGS__)
#define TG_WARN(...) ::tg::log::write(::tg::log::Level::Warn, __VA_ARGS__)
#define TG_ERROR(...) ::tg::log::write(::tg::log::Level::Error, __VA_ARGS__)
