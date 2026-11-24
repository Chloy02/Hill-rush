#pragma once

namespace hr::log {

enum class Level { Debug = 0, Info = 1, Warn = 2, Error = 3 };

void setLevel(Level level);
Level level();

// Mirrors every message into a file in addition to stderr. Pass nullptr to stop.
void setFile(const char* path);

#ifdef _WIN32
// MinGW: check formats against the C99 printf we build with (__USE_MINGW_ANSI_STDIO).
void write(Level level, const char* fmt, ...) __attribute__((format(gnu_printf, 2, 3)));
#else
void write(Level level, const char* fmt, ...) __attribute__((format(printf, 2, 3)));
#endif

} // namespace hr::log

#define LOG_DEBUG(...) ::hr::log::write(::hr::log::Level::Debug, __VA_ARGS__)
#define LOG_INFO(...) ::hr::log::write(::hr::log::Level::Info, __VA_ARGS__)
#define LOG_WARN(...) ::hr::log::write(::hr::log::Level::Warn, __VA_ARGS__)
#define LOG_ERROR(...) ::hr::log::write(::hr::log::Level::Error, __VA_ARGS__)
