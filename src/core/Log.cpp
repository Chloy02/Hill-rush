#include "core/Log.hpp"

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <mutex>

namespace hr::log {

namespace {

Level gLevel = Level::Info;
std::FILE* gFile = nullptr;
std::mutex gMutex;
const auto gStart = std::chrono::steady_clock::now();

const char* levelName(Level level)
{
    switch (level) {
    case Level::Debug: return "DEBUG";
    case Level::Info: return "INFO";
    case Level::Warn: return "WARN";
    case Level::Error: return "ERROR";
    }
    return "?";
}

} // namespace

void setLevel(Level level) { gLevel = level; }

Level level() { return gLevel; }

void setFile(const char* path)
{
    std::lock_guard<std::mutex> lock(gMutex);
    if (gFile) {
        std::fclose(gFile);
        gFile = nullptr;
    }
    if (path) gFile = std::fopen(path, "w");
}

void write(Level level, const char* fmt, ...)
{
    if (level < gLevel) return;

    char message[1024];
    va_list args;
    va_start(args, fmt);
    std::vsnprintf(message, sizeof(message), fmt, args);
    va_end(args);

    const double seconds =
        std::chrono::duration<double>(std::chrono::steady_clock::now() - gStart).count();

    std::lock_guard<std::mutex> lock(gMutex);
    std::fprintf(stderr, "[%8.3f] %-5s %s\n", seconds, levelName(level), message);
    if (gFile) {
        std::fprintf(gFile, "[%8.3f] %-5s %s\n", seconds, levelName(level), message);
        std::fflush(gFile);
    }
}

} // namespace hr::log
