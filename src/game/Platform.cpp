// Kept apart from the rest of the game: <windows.h> clashes with raylib names.
#include "game/Platform.hpp"

#include <cstdio>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#endif

namespace hr::platform {

void showFatalError(const char* title, const char* message)
{
    std::fprintf(stderr, "%s: %s\n", title, message);
#ifdef _WIN32
    MessageBoxA(nullptr, message, title, MB_OK | MB_ICONERROR);
#endif
}

} // namespace hr::platform
