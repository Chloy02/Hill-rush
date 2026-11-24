#pragma once

#include <cstddef>

namespace hr::embedded {

// A file compiled into the executable (see cmake/EmbedFiles.cmake).
struct Blob {
    const char* name;
    const unsigned char* data;
    std::size_t size;
};

// Returns nullptr when no blob has that name.
const Blob* find(const char* name);

} // namespace hr::embedded
