#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hr::net {

// Little-endian binary encoding for network messages, the same on every
// platform.
class Writer {
public:
    void u8(uint8_t v) { bytes_.push_back(v); }
    void u16(uint16_t v);
    void u32(uint32_t v);
    void f32(float v);
    // Length-prefixed (one byte); longer strings are cut at `maxLength` (<= 255).
    void str(const std::string& s, size_t maxLength = 255);

    const std::vector<uint8_t>& bytes() const { return bytes_; }
    std::vector<uint8_t> take() { return std::move(bytes_); }

private:
    std::vector<uint8_t> bytes_;
};

// Reads what a Writer wrote. Reading past the end (a truncated or malformed
// packet) makes ok() false; every read after that returns zero / empty.
class Reader {
public:
    Reader(const uint8_t* data, size_t size) : data_(data), size_(size) {}

    uint8_t u8();
    uint16_t u16();
    uint32_t u32();
    // NaN and infinities read as 0, so a bad packet can't poison the game.
    float f32();
    std::string str();

    bool ok() const { return ok_; }
    bool atEnd() const { return pos_ == size_; }

private:
    bool need(size_t n);

    const uint8_t* data_;
    size_t size_;
    size_t pos_ = 0;
    bool ok_ = true;
};

} // namespace hr::net
