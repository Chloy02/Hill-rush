#pragma once

namespace hr::platform {

// Tells the player about a fatal start-up problem: a message box on Windows
// (where there is no console), stderr elsewhere.
void showFatalError(const char* title, const char* message);

} // namespace hr::platform
