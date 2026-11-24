#pragma once

#include "net/Protocol.hpp"

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

namespace hr::net {

// Answers "any rooms here?" broadcasts for a hosted room.
class RoomBeacon {
public:
    RoomBeacon();
    ~RoomBeacon();
    RoomBeacon(const RoomBeacon&) = delete;
    RoomBeacon& operator=(const RoomBeacon&) = delete;

    // False if the port can't be opened: the room is then only joinable by address.
    bool open(uint16_t port);
    // Answers the queries that arrived since the last call.
    void update(const RoomAd& ad);

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

// Looks for rooms on the local network: broadcasts a query every second and
// lists whoever answers.
class RoomFinder {
public:
    struct Room {
        std::string address;  // "192.168.1.20" (port separate)
        RoomAd ad;
        float age = 0.0f;  // seconds since it last answered
    };

    explicit RoomFinder(uint16_t discoveryPort = kDiscoveryPort);
    ~RoomFinder();
    RoomFinder(const RoomFinder&) = delete;
    RoomFinder& operator=(const RoomFinder&) = delete;

    void update(float dt);
    const std::vector<Room>& rooms() const { return rooms_; }
    // False if no socket could be opened at all.
    bool working() const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
    std::vector<Room> rooms_;
    float sinceQuery_ = 1e9f;
};

// This computer's address on the local network ("192.168.1.20"), or empty.
std::string localAddress();
// True for addresses that only exist on local networks / VPNs (10.x, 192.168.x, 100.64.x ...).
bool isPrivateAddress(const std::string& ip);

} // namespace hr::net
