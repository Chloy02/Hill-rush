#include "net/Discovery.hpp"

#include "core/Log.hpp"
#include "net/EnetSupport.hpp"

#include <algorithm>
#include <cstdio>

namespace hr::net {

namespace {

constexpr float kQueryInterval = 1.0f;
constexpr float kForgetAfter = 3.5f;  // a room that stops answering drops off the list

ENetSocket openUdp(uint16_t port, bool broadcast)
{
    ENetSocket s = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (s == ENET_SOCKET_NULL) return s;
    enet_socket_set_option(s, ENET_SOCKOPT_NONBLOCK, 1);
    enet_socket_set_option(s, ENET_SOCKOPT_REUSEADDR, 1);
    if (broadcast) enet_socket_set_option(s, ENET_SOCKOPT_BROADCAST, 1);
    ENetAddress bindAddress{};
    bindAddress.host = ENET_HOST_ANY;
    bindAddress.port = port;
    if (enet_socket_bind(s, &bindAddress) != 0) {
        enet_socket_destroy(s);
        return ENET_SOCKET_NULL;
    }
    return s;
}

void sendTo(ENetSocket s, const ENetAddress& to, const std::vector<uint8_t>& bytes)
{
    ENetBuffer buffer;
    buffer.data = const_cast<uint8_t*>(bytes.data());
    buffer.dataLength = bytes.size();
    enet_socket_send(s, &to, &buffer, 1);  // best effort: discovery just tries again
}

// Reads one datagram; returns its size, or 0 when nothing is waiting.
size_t receiveFrom(ENetSocket s, ENetAddress& from, uint8_t* data, size_t capacity)
{
    ENetBuffer buffer;
    buffer.data = data;
    buffer.dataLength = capacity;
    const int n = enet_socket_receive(s, &from, &buffer, 1);
    return n > 0 ? static_cast<size_t>(n) : 0;
}

} // namespace

struct RoomBeacon::Impl {
    ENetSocket socket = ENET_SOCKET_NULL;
};

RoomBeacon::RoomBeacon() : impl_(std::make_unique<Impl>()) {}

RoomBeacon::~RoomBeacon()
{
    if (impl_->socket != ENET_SOCKET_NULL) enet_socket_destroy(impl_->socket);
}

bool RoomBeacon::open(uint16_t port)
{
    if (!initEnet()) return false;
    impl_->socket = openUdp(port, false);
    if (impl_->socket == ENET_SOCKET_NULL) {
        LOG_WARN("Discovery port %u is busy: friends will have to join this room by address", port);
        return false;
    }
    return true;
}

void RoomBeacon::update(const RoomAd& ad)
{
    if (impl_->socket == ENET_SOCKET_NULL) return;
    uint8_t data[512];
    ENetAddress from{};
    std::vector<uint8_t> answer;
    for (int i = 0; i < 32; ++i) {
        const size_t n = receiveFrom(impl_->socket, from, data, sizeof(data));
        if (n == 0) break;
        if (!isDiscoveryQuery(data, n)) continue;
        if (answer.empty()) answer = encode(ad);
        sendTo(impl_->socket, from, answer);
    }
}

// ---------------------------------------------------------------------------

struct RoomFinder::Impl {
    ENetSocket socket = ENET_SOCKET_NULL;
    uint16_t discoveryPort = kDiscoveryPort;
};

RoomFinder::RoomFinder(uint16_t discoveryPort) : impl_(std::make_unique<Impl>())
{
    impl_->discoveryPort = discoveryPort;
    if (initEnet()) impl_->socket = openUdp(ENET_PORT_ANY, true);
    if (impl_->socket == ENET_SOCKET_NULL) LOG_WARN("Could not open a socket to look for rooms");
}

RoomFinder::~RoomFinder()
{
    if (impl_->socket != ENET_SOCKET_NULL) enet_socket_destroy(impl_->socket);
}

bool RoomFinder::working() const { return impl_->socket != ENET_SOCKET_NULL; }

void RoomFinder::update(float dt)
{
    if (impl_->socket == ENET_SOCKET_NULL) return;
    for (Room& r : rooms_) r.age += dt;
    rooms_.erase(std::remove_if(rooms_.begin(), rooms_.end(), [](const Room& r) { return r.age > kForgetAfter; }),
                 rooms_.end());

    sinceQuery_ += dt;
    if (sinceQuery_ >= kQueryInterval) {
        sinceQuery_ = 0.0f;
        const std::vector<uint8_t> query = encodeDiscoveryQuery();
        // Everyone on the network, rooms on this computer, and the usual home
        // /24 network directly (some systems only broadcast on one adapter).
        std::vector<std::string> targets = {"255.255.255.255", "127.0.0.1"};
        const std::string mine = localAddress();
        const size_t dot = mine.rfind('.');
        if (dot != std::string::npos) targets.push_back(mine.substr(0, dot) + ".255");
        for (const std::string& ip : targets) {
            ENetAddress to{};
            if (enet_address_set_host_ip(&to, ip.c_str()) != 0) continue;
            to.port = impl_->discoveryPort;
            sendTo(impl_->socket, to, query);
        }
    }

    uint8_t data[512];
    ENetAddress from{};
    for (int i = 0; i < 64; ++i) {
        const size_t n = receiveFrom(impl_->socket, from, data, sizeof(data));
        if (n == 0) break;
        RoomAd ad;
        if (!decode(data, n, ad)) continue;
        const std::string ip = ipString(from);
        // The same room can answer on several addresses (loopback and the
        // LAN one, when it's on this computer): keep one entry per room.
        auto it = std::find_if(rooms_.begin(), rooms_.end(), [&](const Room& r) { return r.ad.token == ad.token; });
        if (it == rooms_.end()) {
            rooms_.push_back({ip, ad, 0.0f});
        } else {
            it->ad = ad;
            it->age = 0.0f;
            if (ip == "127.0.0.1") it->address = ip;  // the most reliable way to reach a room on this computer
        }
    }
}

std::string localAddress()
{
    if (!initEnet()) return {};
    // "Connecting" a UDP socket sends nothing, but makes the system pick the
    // network adapter (and so the address) it would use to reach the internet.
    ENetSocket s = enet_socket_create(ENET_SOCKET_TYPE_DATAGRAM);
    if (s == ENET_SOCKET_NULL) return {};
    std::string result;
    ENetAddress remote{};
    enet_address_set_host_ip(&remote, "8.8.8.8");
    remote.port = 53;
    if (enet_socket_connect(s, &remote) == 0) {
        ENetAddress local{};
        if (enet_socket_get_address(s, &local) == 0 && local.host != ENET_HOST_ANY) result = ipString(local);
    }
    enet_socket_destroy(s);
    return result;
}

bool isPrivateAddress(const std::string& ip)
{
    unsigned a = 0, b = 0, c = 0, d = 0;
    if (std::sscanf(ip.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) != 4) return false;
    return a == 10 || a == 127 || (a == 192 && b == 168) || (a == 172 && b >= 16 && b <= 31) ||
           (a == 169 && b == 254) || (a == 100 && b >= 64 && b <= 127);
}

} // namespace hr::net
