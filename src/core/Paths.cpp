#include "core/Paths.hpp"

#include "core/Log.hpp"

#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <system_error>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <process.h>
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#endif

namespace hr::paths {

namespace {

std::string gDataDir;

std::string defaultDataDir()
{
#ifdef _WIN32
    const char* appData = std::getenv("APPDATA");
    if (appData && appData[0]) return std::string(appData) + "\\HillRush";
    return ".\\hillrush-data";
#endif
    const char* xdg = std::getenv("XDG_DATA_HOME");
    if (xdg && xdg[0] == '/') return std::string(xdg) + "/hillrush";
    const char* home = std::getenv("HOME");
    if (home && home[0]) return std::string(home) + "/.local/share/hillrush";
    return "./hillrush-data";
}

} // namespace

const std::string& dataDir()
{
    if (gDataDir.empty()) {
        gDataDir = defaultDataDir();
        ensureDir(gDataDir);
    }
    return gDataDir;
}

void setDataDir(const std::string& dir)
{
    gDataDir = dir;
    ensureDir(gDataDir);
}

bool ensureDir(const std::string& dir)
{
    std::error_code ec;
    std::filesystem::create_directories(dir, ec);
    if (ec) {
        LOG_WARN("Could not create directory %s: %s", dir.c_str(), ec.message().c_str());
        return false;
    }
    return true;
}

#ifdef _WIN32

bool writeFileAtomic(const std::string& path, const std::string& contents)
{
    const std::string tmp = path + ".tmp." + std::to_string(_getpid());
    {
        std::ofstream out(tmp, std::ios::binary | std::ios::trunc);
        out << contents;
        out.flush();
        if (!out) {
            LOG_WARN("Failed writing %s", tmp.c_str());
            std::remove(tmp.c_str());
            return false;
        }
    }
    // Write-through makes the replacement durable before we report success.
    if (!MoveFileExA(tmp.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        LOG_WARN("Could not replace %s (error %lu)", path.c_str(), static_cast<unsigned long>(GetLastError()));
        std::remove(tmp.c_str());
        return false;
    }
    return true;
}

#else

bool writeFileAtomic(const std::string& path, const std::string& contents)
{
    // Unique temp name so two running instances can't trample each other.
    const std::string tmp = path + ".tmp." + std::to_string(::getpid());
    const int fd = ::open(tmp.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
    if (fd < 0) {
        LOG_WARN("Could not open %s for writing: %s", tmp.c_str(), std::strerror(errno));
        return false;
    }
    size_t written = 0;
    while (written < contents.size()) {
        const ssize_t n = ::write(fd, contents.data() + written, contents.size() - written);
        if (n < 0) {
            if (errno == EINTR) continue;
            LOG_WARN("Failed writing %s: %s", tmp.c_str(), std::strerror(errno));
            ::close(fd);
            std::remove(tmp.c_str());
            return false;
        }
        written += static_cast<size_t>(n);
    }
    // Make sure the data is on disk before the rename makes it the real file,
    // so a power cut can't leave an empty save behind.
    ::fsync(fd);
    ::close(fd);

    if (::rename(tmp.c_str(), path.c_str()) != 0) {
        LOG_WARN("Could not replace %s: %s", path.c_str(), std::strerror(errno));
        std::remove(tmp.c_str());
        return false;
    }
    const std::string dir = std::filesystem::path(path).parent_path().string();
    const int dirFd = ::open(dir.empty() ? "." : dir.c_str(), O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (dirFd >= 0) {
        ::fsync(dirFd);
        ::close(dirFd);
    }
    return true;
}

#endif

bool readFile(const std::string& path, std::string& out)
{
    std::ifstream in(path, std::ios::binary);
    if (!in) return false;
    std::ostringstream ss;
    ss << in.rdbuf();
    out = ss.str();
    return true;
}

} // namespace hr::paths
