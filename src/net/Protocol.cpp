#include "net/Protocol.hpp"

#include "net/Wire.hpp"

#include <cctype>
#include <cmath>
#include <cstring>

namespace hr::net {

namespace {

constexpr size_t kMaxIdLength = 24;  // stage / vehicle ids
constexpr uint8_t kMaxWheels = 8;
const char kQueryMagic[4] = {'H', 'R', 'U', 'Q'};
const char kAdMagic[4] = {'H', 'R', 'U', 'A'};

Writer start(MsgType type)
{
    Writer w;
    w.u8(static_cast<uint8_t>(type));
    return w;
}

// Positions on the wire are metres; a car is never further than this from
// the start, so anything bigger is garbage.
constexpr float kMaxCoordinate = 1.0e6f;

float sane(float v) { return v > kMaxCoordinate ? kMaxCoordinate : v < -kMaxCoordinate ? -kMaxCoordinate : v; }

// Race clocks, countdowns and the like, in seconds: whatever a peer sends,
// they stay small enough to be counted down and shown.
float seconds(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }
constexpr float kLongestTime = kMaxRaceSeconds + 60.0f;

// Angles go through the exact trig functions: Box2D's fast approximations
// lose about a tenth of a degree on a round trip.
void writeTransform(Writer& w, const b2Transform& t)
{
    w.f32(t.p.x);
    w.f32(t.p.y);
    w.f32(std::atan2(t.q.s, t.q.c));
}

b2Transform readTransform(Reader& r)
{
    const float x = sane(r.f32());
    const float y = sane(r.f32());
    const float angle = r.f32();
    return {{x, y}, {std::cos(angle), std::sin(angle)}};
}

bool validState(uint8_t s) { return s <= static_cast<uint8_t>(RacerState::Left); }

// Opens a message of the expected type for reading.
bool open(const uint8_t* data, size_t size, MsgType type, Reader& r)
{
    return messageType(data, size) == type && (r.u8(), r.ok());
}

bool finish(const Reader& r) { return r.ok() && r.atEnd(); }

} // namespace

MsgType messageType(const uint8_t* data, size_t size)
{
    if (!data || size == 0) return MsgType::None;
    const uint8_t t = data[0];
    if (t < static_cast<uint8_t>(MsgType::Hello) || t > static_cast<uint8_t>(MsgType::RunReport)) return MsgType::None;
    return static_cast<MsgType>(t);
}

std::vector<uint8_t> encode(const Hello& m)
{
    Writer w = start(MsgType::Hello);
    w.u16(m.protocol);
    w.str(m.version, 32);
    w.str(m.name, kMaxNameLength);
    w.str(m.vehicle, kMaxIdLength);
    return w.take();
}

bool decode(const uint8_t* data, size_t size, Hello& out)
{
    Reader r(data, size);
    if (!open(data, size, MsgType::Hello, r)) return false;
    out.protocol = r.u16();
    out.version = r.str();
    out.name = r.str();
    out.vehicle = r.str();
    return finish(r);
}

std::vector<uint8_t> encodeWelcome(uint8_t playerId)
{
    Writer w = start(MsgType::Welcome);
    w.u8(playerId);
    return w.take();
}

bool decodeWelcome(const uint8_t* data, size_t size, uint8_t& playerId)
{
    Reader r(data, size);
    if (!open(data, size, MsgType::Welcome, r)) return false;
    playerId = r.u8();
    return finish(r);
}

std::vector<uint8_t> encodeReject(const std::string& reason)
{
    Writer w = start(MsgType::Reject);
    w.str(reason, 120);
    return w.take();
}

bool decodeReject(const uint8_t* data, size_t size, std::string& reason)
{
    Reader r(data, size);
    if (!open(data, size, MsgType::Reject, r)) return false;
    reason = r.str();
    return finish(r);
}

std::vector<uint8_t> encode(const RoomState& m)
{
    Writer w = start(MsgType::Room);
    w.u8(static_cast<uint8_t>(m.phase));
    w.u32(m.race);
    w.f32(m.countdown);
    w.f32(m.deadline);
    w.str(m.settings.stage, kMaxIdLength);
    w.u8(static_cast<uint8_t>(m.settings.difficulty));
    w.u16(static_cast<uint16_t>(m.settings.goal < 0 ? 0 : m.settings.goal > 65535 ? 65535 : m.settings.goal));
    w.u8(static_cast<uint8_t>(m.players.size()));
    for (const Player& p : m.players) {
        w.u8(p.id);
        w.str(p.name, kMaxNameLength);
        w.str(p.vehicle, kMaxIdLength);
        w.u8(static_cast<uint8_t>((p.ready ? 1 : 0) | (p.host ? 2 : 0)));
        w.u8(p.color);
        w.u16(p.ack);
        w.u8(static_cast<uint8_t>(p.state));
        w.f32(p.distance);
        w.f32(p.finishTime);
    }
    return w.take();
}

bool decode(const uint8_t* data, size_t size, RoomState& out)
{
    Reader r(data, size);
    if (!open(data, size, MsgType::Room, r)) return false;
    RoomState m;
    const uint8_t phase = r.u8();
    if (phase > static_cast<uint8_t>(Phase::Results)) return false;
    m.phase = static_cast<Phase>(phase);
    m.race = r.u32();
    m.countdown = seconds(r.f32(), 0.0f, kCountdownSeconds);
    m.deadline = seconds(r.f32(), -1.0f, kFinishGraceSeconds);
    m.settings.stage = r.str();
    const uint8_t difficulty = r.u8();
    if (difficulty > static_cast<uint8_t>(Difficulty::Hard)) return false;
    m.settings.difficulty = static_cast<Difficulty>(difficulty);
    m.settings.goal = r.u16();
    const uint8_t count = r.u8();
    if (count > kMaxPlayers) return false;
    for (uint8_t i = 0; i < count; ++i) {
        Player p;
        p.id = r.u8();
        p.name = r.str();
        p.vehicle = r.str();
        const uint8_t flags = r.u8();
        p.ready = (flags & 1) != 0;
        p.host = (flags & 2) != 0;
        p.color = r.u8();
        p.ack = r.u16();
        const uint8_t state = r.u8();
        if (!validState(state)) return false;
        p.state = static_cast<RacerState>(state);
        p.distance = sane(r.f32());
        p.finishTime = seconds(r.f32(), 0.0f, kLongestTime);
        m.players.push_back(std::move(p));
    }
    if (!finish(r)) return false;
    out = std::move(m);
    return true;
}

std::vector<uint8_t> encode(const PlayerUpdate& m)
{
    Writer w = start(MsgType::PlayerUpdate);
    w.u16(m.seq);
    w.str(m.name, kMaxNameLength);
    w.str(m.vehicle, kMaxIdLength);
    w.u8(m.ready ? 1 : 0);
    return w.take();
}

bool decode(const uint8_t* data, size_t size, PlayerUpdate& out)
{
    Reader r(data, size);
    if (!open(data, size, MsgType::PlayerUpdate, r)) return false;
    out.seq = r.u16();
    out.name = r.str();
    out.vehicle = r.str();
    out.ready = r.u8() != 0;
    return finish(r);
}

std::vector<uint8_t> encode(const CarSnapshot& m)
{
    Writer w = start(MsgType::Snapshot);
    w.u8(m.player);
    w.u32(m.race);
    w.f32(m.time);
    w.f32(m.distance);
    w.u8(static_cast<uint8_t>(m.state) | (m.pose.crashed ? 0x80 : 0));
    w.f32(m.velocity.x);
    w.f32(m.velocity.y);
    writeTransform(w, m.pose.chassis);
    writeTransform(w, m.pose.head);
    const size_t wheels = m.pose.wheels.size() < kMaxWheels ? m.pose.wheels.size() : kMaxWheels;
    w.u8(static_cast<uint8_t>(wheels));
    for (size_t i = 0; i < wheels; ++i) writeTransform(w, m.pose.wheels[i]);
    return w.take();
}

bool decode(const uint8_t* data, size_t size, CarSnapshot& out)
{
    Reader r(data, size);
    if (!open(data, size, MsgType::Snapshot, r)) return false;
    CarSnapshot m;
    m.player = r.u8();
    m.race = r.u32();
    m.time = seconds(r.f32(), 0.0f, kLongestTime);
    m.distance = sane(r.f32());
    const uint8_t state = r.u8();
    if (!validState(state & 0x7F)) return false;
    m.state = static_cast<RacerState>(state & 0x7F);
    m.pose.crashed = (state & 0x80) != 0;
    m.velocity = {sane(r.f32()), sane(r.f32())};
    m.pose.chassis = readTransform(r);
    m.pose.head = readTransform(r);
    const uint8_t wheels = r.u8();
    if (wheels > kMaxWheels) return false;
    for (uint8_t i = 0; i < wheels; ++i) m.pose.wheels.push_back(readTransform(r));
    if (!finish(r)) return false;
    out = std::move(m);
    return true;
}

std::vector<uint8_t> encode(const RunReport& m)
{
    Writer w = start(MsgType::RunReport);
    w.u8(static_cast<uint8_t>(m.state));
    w.f32(m.distance);
    w.f32(m.time);
    return w.take();
}

bool decode(const uint8_t* data, size_t size, RunReport& out)
{
    Reader r(data, size);
    if (!open(data, size, MsgType::RunReport, r)) return false;
    const uint8_t state = r.u8();
    if (!validState(state)) return false;
    out.state = static_cast<RacerState>(state);
    out.distance = sane(r.f32());
    out.time = seconds(r.f32(), 0.0f, kLongestTime);
    return finish(r);
}

std::vector<uint8_t> encodeDiscoveryQuery()
{
    Writer w;
    for (char c : kQueryMagic) w.u8(static_cast<uint8_t>(c));
    w.u16(kProtocolVersion);
    return w.take();
}

bool isDiscoveryQuery(const uint8_t* data, size_t size)
{
    return size >= 6 && std::memcmp(data, kQueryMagic, 4) == 0;
}

std::vector<uint8_t> encode(const RoomAd& m)
{
    Writer w;
    for (char c : kAdMagic) w.u8(static_cast<uint8_t>(c));
    w.u16(kProtocolVersion);
    w.u32(m.token);
    w.u16(m.port);
    w.str(m.name, 40);
    w.u8(m.players);
    w.u8(m.maxPlayers);
    w.u8(static_cast<uint8_t>(m.phase));
    w.str(m.stage, kMaxIdLength);
    return w.take();
}

bool decode(const uint8_t* data, size_t size, RoomAd& out)
{
    if (size < 6 || std::memcmp(data, kAdMagic, 4) != 0) return false;
    Reader r(data + 4, size - 4);
    // Rooms from other game versions are skipped: you couldn't join them anyway.
    if (r.u16() != kProtocolVersion) return false;
    RoomAd m;
    m.token = r.u32();
    m.port = r.u16();
    m.name = r.str();
    m.players = r.u8();
    m.maxPlayers = r.u8();
    const uint8_t phase = r.u8();
    if (phase > static_cast<uint8_t>(Phase::Results)) return false;
    m.phase = static_cast<Phase>(phase);
    m.stage = r.str();
    if (!finish(r) || m.port == 0) return false;
    out = std::move(m);
    return true;
}

std::string sanitizeName(const std::string& name)
{
    std::string out;
    for (char c : name) {
        const unsigned char u = static_cast<unsigned char>(c);
        if (std::isalnum(u)) out += static_cast<char>(std::toupper(u));
        else if ((c == ' ' || c == '-') && !out.empty() && out.back() != ' ') out += c;
        if (out.size() >= kMaxNameLength) break;
    }
    while (!out.empty() && (out.back() == ' ' || out.back() == '-')) out.pop_back();
    return out.empty() ? "PLAYER" : out;
}

} // namespace hr::net
