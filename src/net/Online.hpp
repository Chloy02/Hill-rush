#pragma once

#include "net/Protocol.hpp"
#include "net/Race.hpp"

#include <cstdint>
#include <deque>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <vector>

// ENet types, kept opaque here (see EnetSupport.hpp).
struct _ENetHost;
struct _ENetPeer;

namespace hr::net {

class RoomBeacon;

// This player's place in an online race room: either hosting it or joined
// to someone else's.
//
// The host owns the room: who's in, the settings, the race phase and the
// results; it relays every car's snapshots to the others. Each player drives
// only their own car (cars pass through each other), so nobody waits for
// anybody's physics. Everything runs on the game thread: call update() once
// per frame.
class Online {
public:
    struct Config {
        std::string name = "PLAYER";
        std::string vehicle = "jeep";
        std::string version;  // game version, quoted when versions don't match
        uint16_t port = kDefaultPort;
        uint16_t discoveryPort = kDiscoveryPort;  // host: 0 = don't answer discovery queries
        uint16_t protocol = kProtocolVersion;     // only tests change this
    };

    enum class Status { Connecting, InRoom, Closed };

    // Opens a room on this computer. Null (with `error`) if that fails.
    static std::unique_ptr<Online> host(const Config& config, std::string& error);
    // Starts joining a room: "192.168.1.20", "192.168.1.20:47821" or a host name.
    static std::unique_ptr<Online> join(const std::string& address, const Config& config, std::string& error);
    // Leaves the room (and closes it for everyone, if hosting).
    ~Online();
    Online(const Online&) = delete;
    Online& operator=(const Online&) = delete;

    void update(float dt);

    Status status() const { return status_; }
    // Why the room closed: the host left, the room was full, the connection dropped...
    const std::string& closeReason() const { return closeReason_; }
    bool isHost() const { return isHost_; }
    uint8_t myId() const { return myId_; }

    // Room -------------------------------------------------------------------
    const RoomState& room() const { return room_; }
    // The phase as this player sees it: Racing as soon as the local countdown ends.
    Phase phase() const;
    const Player* player(uint8_t id) const;
    const Player* me() const { return player(myId_); }
    std::string roomName() const;

    // Lobby choices (only while in the lobby).
    void setVehicle(const std::string& vehicleId);
    void setReady(bool ready);
    void setName(const std::string& name);

    // Host only; ignored for guests.
    void setSettings(const RoomSettings& settings);
    // At least two players and every guest ready.
    bool canStart() const;
    void startRace();
    void backToRoom();

    // Race -------------------------------------------------------------------
    // Seconds until GO, while counting down.
    float countdown() const { return countdown_; }
    // Seconds until the race is called, once someone has finished; -1 before that.
    float deadline() const { return deadline_; }
    // This player's car, about 20 times a second while racing.
    void sendSnapshot(const CarSnapshot& snapshot);
    // This player's race is over: Finished, Crashed or OutOfFuel.
    void reportRunEnded(RacerState state, float distance, float time);
    // Recent snapshots of another player's car (null if none arrived yet).
    const SnapshotBuffer* car(uint8_t playerId) const;
    // A player's distance, as fresh as this player knows it.
    float distanceOf(uint8_t playerId) const;
    // Player ids in race order, leader first.
    std::vector<uint8_t> raceOrder() const;

    // Connection --------------------------------------------------------------
    // Round trip to the host (guest), or to the slowest guest (host), in ms.
    int pingMs() const;
    // Host: the address friends can type to join ("192.168.1.20"), or empty.
    const std::string& shareAddress() const { return shareAddress_; }
    // Guest: the address being joined.
    const std::string& address() const { return address_; }
    // True when the connection is on a local network or VPN.
    bool onLan() const;

    // Things worth telling the player ("SAM JOINED"), oldest first.
    std::deque<std::string>& notices() { return notices_; }

private:
    // A connected guest, on the host. id 0 = hasn't said hello yet.
    struct Guest {
        uint8_t id = 0;
        float waiting = 0.0f;  // seconds without a hello
    };

    Online() = default;

    void onConnect(_ENetPeer* peer);
    void onDisconnect(_ENetPeer* peer, uint32_t data);
    void hostPacket(_ENetPeer* peer, const uint8_t* data, size_t size);
    void guestPacket(const uint8_t* data, size_t size);
    void reject(_ENetPeer* peer, const std::string& reason);
    void applyRoom(RoomState state);
    void applyRunEnded(uint8_t id, RacerState state, float distance, float time);
    void advanceClocks(float dt);
    void endRace();
    void broadcastRoom();
    void relaySnapshot(_ENetPeer* from, const CarSnapshot& snapshot);
    void changeMe(const PlayerUpdate& update);
    void close(const std::string& reason);
    Player* findPlayer(uint8_t id);
    uint8_t nextPlayerId();
    uint8_t freeColor() const;
    RoomAd advert() const;

    Config config_;
    Status status_ = Status::Connecting;
    std::string closeReason_;
    std::string rejectReason_;
    bool isHost_ = false;
    uint8_t myId_ = 0;
    std::string address_;
    std::string shareAddress_;

    _ENetHost* host_ = nullptr;
    _ENetPeer* server_ = nullptr;           // guest: connection to the host
    std::map<_ENetPeer*, Guest> guests_;    // host: connected guests
    std::unique_ptr<RoomBeacon> beacon_;
    uint32_t token_ = 0;
    uint8_t lastId_ = 1;

    RoomState room_;
    bool dirty_ = false;  // host: room changed, tell everyone
    std::optional<PlayerUpdate> pending_;  // guest: my last change, until the host has handled it
    uint16_t updateSeq_ = 0;
    float countdown_ = 0.0f;
    float deadline_ = -1.0f;
    float raceClock_ = 0.0f;
    float connectTime_ = 0.0f;
    std::map<uint8_t, SnapshotBuffer> cars_;
    std::deque<std::string> notices_;
};

} // namespace hr::net
