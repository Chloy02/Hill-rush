#include "net/Wire.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace hr::net {

void Writer::u16(uint16_t v)
{
    u8(static_cast<uint8_t>(v & 0xFF));
    u8(static_cast<uint8_t>(v >> 8));
}

void Writer::u32(uint32_t v)
{
    u16(static_cast<uint16_t>(v & 0xFFFF));
    u16(static_cast<uint16_t>(v >> 16));
}

void Writer::f32(float v)
{
    uint32_t bits = 0;
    std::memcpy(&bits, &v, sizeof(bits));
    u32(bits);
}

void Writer::str(const std::string& s, size_t maxLength)
{
    const size_t n = std::min({s.size(), maxLength, size_t{255}});
    u8(static_cast<uint8_t>(n));
    bytes_.insert(bytes_.end(), s.begin(), s.begin() + static_cast<std::ptrdiff_t>(n));
}

bool Reader::need(size_t n)
{
    if (!ok_ || size_ - pos_ < n) {
        ok_ = false;
        return false;
    }
    return true;
}

uint8_t Reader::u8()
{
    if (!need(1)) return 0;
    return data_[pos_++];
}

uint16_t Reader::u16()
{
    if (!need(2)) return 0;
    const uint16_t v = static_cast<uint16_t>(data_[pos_] | (data_[pos_ + 1] << 8));
    pos_ += 2;
    return v;
}

uint32_t Reader::u32()
{
    if (!need(4)) return 0;
    const uint32_t lo = u16();
    const uint32_t hi = u16();
    return lo | (hi << 16);
}

float Reader::f32()
{
    const uint32_t bits = u32();
    float v = 0.0f;
    std::memcpy(&v, &bits, sizeof(v));
    return std::isfinite(v) ? v : 0.0f;
}

std::string Reader::str()
{
    const size_t n = u8();
    if (!need(n)) return {};
    std::string s(reinterpret_cast<const char*>(data_ + pos_), n);
    pos_ += n;
    return s;
}

} // namespace hr::net
