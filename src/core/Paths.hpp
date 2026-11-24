#pragma once

#include <string>

namespace hr::paths {

// Directory for save data and screenshots. Defaults to $XDG_DATA_HOME/hillrush
// (usually ~/.local/share/hillrush), or %APPDATA%\HillRush on Windows.
// Created on first use.
const std::string& dataDir();

// Overrides the data directory (used by tests and the --data-dir flag).
void setDataDir(const std::string& dir);

// Creates `dir` and its parents if needed. Returns false on failure.
bool ensureDir(const std::string& dir);

// Writes `contents` to `path` atomically (temp file + rename), so a crash
// mid-write never leaves a truncated save file behind.
bool writeFileAtomic(const std::string& path, const std::string& contents);

// Reads a whole file. Returns false if it doesn't exist or can't be read.
bool readFile(const std::string& path, std::string& out);

} // namespace hr::paths
