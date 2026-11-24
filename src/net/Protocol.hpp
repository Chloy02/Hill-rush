#pragma once

#include "sim/Defs.hpp"
#include "sim/Pose.hpp"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace hr::net {

// Bump when messages change shape: players on different versions can't race.
inline constexpr uint16_t kProtocolVersion = 1;
inline constexpr uint16_t kDefaultPort = 47821;    // race traffic (UDP)
inline constexpr uint16_t kDiscoveryPort = 47820;  // finding rooms on the local network (UDP broadcast)
inline constexpr int kMaxPlayers = 4;
inline constexpr size_t kMaxNameLength = 12;
inline constexpr float kCountdownSeconds = 3.0f;
// Once someone crosses the finish, everyone else has this long to get there.
inline constexpr float kFinishGraceSeconds = 30.0f;
// A race that somehow never ends is called after this long.
inline constexpr float kMaxRaceSeconds = 30.0f * 60.0f;

enum class Phase : uint8_t { Lobby, Countdown, Racing, Results };

// How one player's race is going.
enum class RacerState : uint8_t { Driving, Finished, Crashed, OutOfFuel, Left };

struct RoomSettings {
    std::string stage = "countryside";
    Difficulty difficulty = Difficulty::Easy;
    int goal = 1000;  // metres to the finish; 0 = whoever gets furthest wins

    bool operator==(const RoomSettings&) const = default;
};

struct Player {
    uint8_t id = 0;
    std::string name;
    std::string vehicle = "jeep";
    bool ready = false;
    bool host = false;
    uint8_t color = 0;  // index into the player colours (0..kMaxPlayers-1)
    uint16_t ack = 0;   // the newest PlayerUpdate (seq) the host has handled for this player

    // Reset when a race starts.
    RacerState state = RacerState::Driving;
    float distance = 0.0f;
    float finishTime = 0.0f;  // race seconds, when Finished
};

// Everything about a room, sent by the host whenever something changes.
struct RoomState {
    Phase phase = Phase::Lobby;
    uint32_t race = 0;       // increases with every race started
    float countdown = 0.0f;  // Countdown: seconds until GO
    float deadline = -1.0f;  // Racing: seconds until the race is called, or -1 when nobody has finished
    RoomSettings settings;
    std::vector<Player> players;
};

// One car at one moment, sent about 20 times a second.
struct CarSnapshot {
    uint8_t player = 0;
    uint32_t race = 0;  // which race: a late snapshot from the previous one is dropped
    float time = 0.0f;  // sender's race time (seconds since GO)
    float distance = 0.0f;
    RacerState state = RacerState::Driving;
    b2Vec2 velocity{0.0f, 0.0f};
    VehiclePose pose;
};

struct Hello {
    uint16_t protocol = kProtocolVersion;
    std::string version;  // game version, for the "different versions" message
    std::string name;
    std::string vehicle;
};

struct PlayerUpdate {
    std::string name;
    std::string vehicle;
    bool ready = false;
    uint16_t seq = 0;  // echoed back as Player::ack once the host has handled it
};

// A guest's race ended: finished, crashed or ran out of fuel.
struct RunReport {
    RacerState state = RacerState::Finished;
    float distance = 0.0f;
    float time = 0.0f;
};

// A room answering a discovery query.
struct RoomAd {
    uint32_t token = 0;  // tells rooms apart when one answers on several addresses
    uint16_t port = kDefaultPort;
    std::string name;
    uint8_t players = 0;
    uint8_t maxPlayers = kMaxPlayers;
    Phase phase = Phase::Lobby;
    std::string stage;
};

enum class MsgType : uint8_t {
    None = 0,
    Hello,         // guest -> host
    Welcome,       // host -> guest: your player id
    Reject,        // host -> guest: why you can't join
    Room,          // host -> everyone: RoomState
    PlayerUpdate,  // guest -> host
    Snapshot,      // anyone -> host -> everyone else (unreliable)
    RunReport,     // guest -> host
};

// Sent as ENet disconnect data, so the other side can say why.
enum class Farewell : uint32_t { None = 0, RoomClosed = 1, Rejected = 2, Left = 3 };

MsgType messageType(const uint8_t* data, size_t size);

std::vector<uint8_t> encode(const Hello& m);
std::vector<uint8_t> encodeWelcome(uint8_t playerId);
std::vector<uint8_t> encodeReject(const std::string& reason);
std::vector<uint8_t> encode(const RoomState& m);
std::vector<uint8_t> encode(const PlayerUpdate& m);
std::vector<uint8_t> encode(const CarSnapshot& m);
std::vector<uint8_t> encode(const RunReport& m);

// Each decoder checks the type byte and the whole length; false means the
// packet is malformed (or a different message) and must be ignored.
bool decode(const uint8_t* data, size_t size, Hello& out);
bool decodeWelcome(const uint8_t* data, size_t size, uint8_t& playerId);
bool decodeReject(const uint8_t* data, size_t size, std::string& reason);
bool decode(const uint8_t* data, size_t size, RoomState& out);
bool decode(const uint8_t* data, size_t size, PlayerUpdate& out);
bool decode(const uint8_t* data, size_t size, CarSnapshot& out);
bool decode(const uint8_t* data, size_t size, RunReport& out);

// Discovery packets (plain UDP, outside ENet).
std::vector<uint8_t> encodeDiscoveryQuery();
bool isDiscoveryQuery(const uint8_t* data, size_t size);
std::vector<uint8_t> encode(const RoomAd& m);
bool decode(const uint8_t* data, size_t size, RoomAd& out);

// Upper-case letters, digits, spaces and dashes, at most kMaxNameLength
// characters; "PLAYER" if nothing is left.
std::string sanitizeName(const std::string& name);

} // namespace hr::net
