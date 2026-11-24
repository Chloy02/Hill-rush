#include "net/Online.hpp"

#include "core/Log.hpp"
#include "net/Discovery.hpp"
#include "net/EnetSupport.hpp"

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <random>

namespace hr::net {

namespace {

constexpr uint8_t kRoomChannel = 0;  // reliable: room state, joins, results
constexpr uint8_t kCarChannel = 1;   // unreliable: car snapshots (old ones are dropped)
constexpr size_t kChannels = 2;
constexpr size_t kPeerSlots = kMaxPlayers + 4;  // spares, so a full room can still say "full"
constexpr float kConnectTimeout = 8.0f;
constexpr float kHelloTimeout = 5.0f;  // a connection that never introduces itself is dropped
constexpr uint8_t kHostId = 1;
// Room messages are small; nothing legitimate comes close to these.
constexpr size_t kMaxPacketSize = 64 * 1024;
constexpr size_t kMaxWaitingData = 256 * 1024;

void limitPackets(ENetHost* host)
{
    host->maximumPacketSize = kMaxPacketSize;
    host->maximumWaitingData = kMaxWaitingData;
}

void sendBytes(ENetPeer* peer, uint8_t channel, const std::vector<uint8_t>& bytes, bool reliable)
{
    ENetPacket* packet = enet_packet_create(bytes.data(), bytes.size(), reliable ? ENET_PACKET_FLAG_RELIABLE : 0);
    if (packet && enet_peer_send(peer, channel, packet) != 0) enet_packet_destroy(packet);
}

std::string validVehicle(const std::string& id) { return findVehicle(id) ? id : "jeep"; }

// Peers that stop answering are dropped after a few seconds instead of ENet's default 30.
void quickTimeouts(ENetPeer* peer) { enet_peer_timeout(peer, 0, 3000, 8000); }

uint32_t randomToken()
{
    const auto now = static_cast<uint64_t>(std::chrono::steady_clock::now().time_since_epoch().count());
    return static_cast<uint32_t>(std::random_device{}()) ^ static_cast<uint32_t>(now ^ (now >> 32));
}

} // namespace

std::unique_ptr<Online> Online::host(const Config& config, std::string& error)
{
    if (!initEnet()) {
        error = "Networking isn't available on this computer.";
        return nullptr;
    }
    std::unique_ptr<Online> o(new Online());
    o->config_ = config;
    o->isHost_ = true;
    ENetAddress address{};
    address.host = ENET_HOST_ANY;
    address.port = config.port;
    o->host_ = enet_host_create(&address, kPeerSlots, kChannels, 0, 0);
    if (!o->host_) {
        error = "Couldn't open port " + std::to_string(config.port) +
                ". Is another Hill Rush already hosting on this computer?";
        return nullptr;
    }
    limitPackets(o->host_);
    o->myId_ = kHostId;
    Player me;
    me.id = kHostId;
    me.name = sanitizeName(config.name);
    me.vehicle = validVehicle(config.vehicle);
    me.ready = true;
    me.host = true;
    o->room_.players.push_back(me);
    o->status_ = Status::InRoom;
    o->token_ = randomToken();
    o->shareAddress_ = localAddress();
    if (!o->shareAddress_.empty() && config.port != kDefaultPort) o->shareAddress_ += ":" + std::to_string(config.port);
    if (config.discoveryPort != 0) {
        o->beacon_ = std::make_unique<RoomBeacon>();
        o->beacon_->open(config.discoveryPort);
    }
    LOG_INFO("Hosting an online room on port %u (%s)", config.port,
             o->shareAddress_.empty() ? "no network address" : o->shareAddress_.c_str());
    return o;
}

std::unique_ptr<Online> Online::join(const std::string& addressText, const Config& config, std::string& error)
{
    if (!initEnet()) {
        error = "Networking isn't available on this computer.";
        return nullptr;
    }
    std::string hostPart = addressText;
    hostPart.erase(0, hostPart.find_first_not_of(" \t"));
    hostPart.erase(hostPart.find_last_not_of(" \t") + 1);
    uint16_t port = config.port;
    const size_t colon = hostPart.rfind(':');
    if (colon != std::string::npos) {
        const long p = std::strtol(hostPart.c_str() + colon + 1, nullptr, 10);
        if (p <= 0 || p > 65535) {
            error = "That port number doesn't look right.";
            return nullptr;
        }
        port = static_cast<uint16_t>(p);
        hostPart.resize(colon);
    }
    if (hostPart.empty()) {
        error = "Type the host's address first (it's shown in their room).";
        return nullptr;
    }
    ENetAddress address{};
    address.port = port;
    if (enet_address_set_host(&address, hostPart.c_str()) != 0) {
        error = "Couldn't find \"" + hostPart + "\" on the network.";
        return nullptr;
    }

    std::unique_ptr<Online> o(new Online());
    o->config_ = config;
    o->address_ = port == kDefaultPort ? hostPart : hostPart + ":" + std::to_string(port);
    o->host_ = enet_host_create(nullptr, 1, kChannels, 0, 0);
    if (!o->host_) {
        error = "Networking isn't available on this computer.";
        return nullptr;
    }
    limitPackets(o->host_);
    o->server_ = enet_host_connect(o->host_, &address, kChannels, config.protocol);
    if (!o->server_) {
        error = "Couldn't start connecting.";
        return nullptr;
    }
    quickTimeouts(o->server_);
    LOG_INFO("Joining the room at %s", o->address_.c_str());
    return o;
}

Online::~Online()
{
    if (!host_) return;
    if (isHost_) {
        for (const auto& [peer, guest] : guests_) enet_peer_disconnect(peer, static_cast<uint32_t>(Farewell::RoomClosed));
    } else if (server_ && status_ != Status::Closed) {
        enet_peer_disconnect(server_, static_cast<uint32_t>(Farewell::Left));
    }
    enet_host_flush(host_);
    // Give the goodbyes a moment to be acknowledged, without hanging the game.
    const auto start = std::chrono::steady_clock::now();
    ENetEvent event;
    while (std::chrono::steady_clock::now() - start < std::chrono::milliseconds(200) &&
           enet_host_service(host_, &event, 10) > 0) {
        if (event.type == ENET_EVENT_TYPE_RECEIVE) enet_packet_destroy(event.packet);
    }
    enet_host_destroy(host_);
}

void Online::update(float dt)
{
    if (!host_ || status_ == Status::Closed) return;
    ENetEvent event;
    while (status_ != Status::Closed && enet_host_service(host_, &event, 0) > 0) {
        switch (event.type) {
        case ENET_EVENT_TYPE_CONNECT:
            onConnect(event.peer);
            break;
        case ENET_EVENT_TYPE_RECEIVE:
            if (isHost_) hostPacket(event.peer, event.packet->data, event.packet->dataLength);
            else guestPacket(event.packet->data, event.packet->dataLength);
            enet_packet_destroy(event.packet);
            break;
        case ENET_EVENT_TYPE_DISCONNECT:
            onDisconnect(event.peer, event.data);
            break;
        default:
            break;
        }
    }
    if (status_ == Status::Closed) return;

    if (!isHost_ && status_ == Status::Connecting) {
        connectTime_ += dt;
        if (connectTime_ > kConnectTimeout) {
            enet_peer_reset(server_);
            close("Couldn't reach a room at " + address_ +
                  ". Check the address, and that the host's firewall lets Hill Rush through.");
            return;
        }
    }
    advanceClocks(dt);
    if (isHost_) {
        for (auto it = guests_.begin(); it != guests_.end();) {
            if (it->second.id == 0 && (it->second.waiting += dt) > kHelloTimeout) {
                enet_peer_disconnect_now(it->first, static_cast<uint32_t>(Farewell::Rejected));
                it = guests_.erase(it);
            } else {
                ++it;
            }
        }
        if (beacon_) beacon_->update(advert());
        if (dirty_) broadcastRoom();
    }
    enet_host_flush(host_);
}

void Online::onConnect(ENetPeer* peer)
{
    quickTimeouts(peer);
    if (isHost_) {
        guests_[peer] = Guest{};  // waits for Hello
        return;
    }
    Hello hello;
    hello.protocol = config_.protocol;
    hello.version = config_.version;
    hello.name = sanitizeName(config_.name);
    hello.vehicle = validVehicle(config_.vehicle);
    sendBytes(peer, kRoomChannel, encode(hello), true);
}

void Online::onDisconnect(ENetPeer* peer, uint32_t data)
{
    if (!isHost_) {
        if (!rejectReason_.empty()) close(rejectReason_);
        else if (data == static_cast<uint32_t>(Farewell::RoomClosed)) close("The host closed the room.");
        else if (status_ == Status::Connecting) close("Couldn't reach a room at " + address_ + ".");
        else close("Lost the connection to the host.");
        return;
    }
    const auto it = guests_.find(peer);
    if (it == guests_.end()) return;
    const uint8_t id = it->second.id;
    guests_.erase(it);
    Player* p = findPlayer(id);
    if (!p) return;
    notices_.push_back(p->name + " LEFT");
    LOG_INFO("%s left the room", p->name.c_str());
    if (room_.phase == Phase::Lobby) {
        room_.players.erase(std::remove_if(room_.players.begin(), room_.players.end(),
                                           [id](const Player& x) { return x.id == id; }),
                            room_.players.end());
    } else if ((room_.phase == Phase::Countdown || room_.phase == Phase::Racing) && p->state == RacerState::Driving) {
        p->state = RacerState::Left;  // (after the race, their result stands)
    }
    cars_.erase(id);
    dirty_ = true;
}

void Online::reject(ENetPeer* peer, const std::string& reason)
{
    sendBytes(peer, kRoomChannel, encodeReject(reason), true);
    // "Later": after the reason has been delivered.
    enet_peer_disconnect_later(peer, static_cast<uint32_t>(Farewell::Rejected));
    guests_.erase(peer);
    LOG_INFO("Turned a player away: %s", reason.c_str());
}

void Online::hostPacket(ENetPeer* peer, const uint8_t* data, size_t size)
{
    const auto it = guests_.find(peer);
    if (it == guests_.end()) return;
    const uint8_t id = it->second.id;
    switch (messageType(data, size)) {
    case MsgType::Hello: {
        Hello hello;
        if (id != 0 || !decode(data, size, hello)) return;
        if (hello.protocol != config_.protocol) {
            reject(peer, "Different versions of Hill Rush (theirs " + hello.version + ", the host's " +
                             config_.version + "). Use the same version on both computers.");
            return;
        }
        if (room_.phase == Phase::Results) {
            reject(peer, "The room is looking at race results. Try again in a moment.");
            return;
        }
        if (room_.phase != Phase::Lobby) {
            reject(peer, "A race is already under way in this room. Try again when it's over.");
            return;
        }
        if (room_.players.size() >= static_cast<size_t>(kMaxPlayers)) {
            reject(peer, "This room is full.");
            return;
        }
        Player p;
        p.id = nextPlayerId();
        p.name = sanitizeName(hello.name);
        p.vehicle = validVehicle(hello.vehicle);
        p.color = freeColor();
        room_.players.push_back(p);
        it->second.id = p.id;
        sendBytes(peer, kRoomChannel, encodeWelcome(p.id), true);
        notices_.push_back(p.name + " JOINED");
        LOG_INFO("%s joined the room from %s", p.name.c_str(), ipString(peer->address).c_str());
        dirty_ = true;
        break;
    }
    case MsgType::PlayerUpdate: {
        PlayerUpdate u;
        Player* p = findPlayer(id);
        if (!p || !decode(data, size, u)) return;
        p->ack = u.seq;  // handled, even when the room is past changing cars
        p->name = sanitizeName(u.name);
        if (room_.phase == Phase::Lobby) {
            p->vehicle = validVehicle(u.vehicle);
            p->ready = u.ready;
        }
        dirty_ = true;
        break;
    }
    case MsgType::Snapshot: {
        CarSnapshot s;
        Player* p = findPlayer(id);
        if (!p || (room_.phase != Phase::Racing && room_.phase != Phase::Countdown) || !decode(data, size, s)) return;
        // Late ones from an earlier race, or times that can't be right yet, would poison the buffers.
        if (s.race != room_.race) return;
        if (room_.phase == Phase::Countdown ? s.time > 0.5f : s.time > raceClock_ + 5.0f) return;
        s.player = id;  // whatever the sender claimed
        cars_[id].push(s);
        if (p->state == RacerState::Driving) p->distance = std::max(p->distance, s.distance);
        relaySnapshot(peer, s);
        break;
    }
    case MsgType::RunReport: {
        RunReport r;
        if (id != 0 && decode(data, size, r)) applyRunEnded(id, r.state, r.distance, r.time);
        break;
    }
    default:
        break;
    }
}

void Online::guestPacket(const uint8_t* data, size_t size)
{
    switch (messageType(data, size)) {
    case MsgType::Welcome: {
        uint8_t id = 0;
        if (decodeWelcome(data, size, id) && id != 0) {
            myId_ = id;
            status_ = Status::InRoom;
            LOG_INFO("Joined the room as player %u", id);
        }
        break;
    }
    case MsgType::Reject: {
        std::string reason;
        if (decodeReject(data, size, reason)) rejectReason_ = reason;  // the disconnect follows
        break;
    }
    case MsgType::Room: {
        RoomState state;
        if (decode(data, size, state)) applyRoom(std::move(state));
        break;
    }
    case MsgType::Snapshot: {
        CarSnapshot s;
        if (decode(data, size, s) && s.player != myId_ && s.race == room_.race) cars_[s.player].push(s);
        break;
    }
    default:
        break;
    }
}

void Online::applyRoom(RoomState state)
{
    const bool newRace = state.phase == Phase::Countdown && (state.race != room_.race || room_.phase != Phase::Countdown);
    // My own race progress is known best here: keep it while the host catches up.
    const Player* old = me();
    const std::optional<Player> mine = old ? std::optional<Player>(*old) : std::nullopt;
    const bool sameRace = state.race == room_.race;
    room_ = std::move(state);

    Player* p = findPlayer(myId_);
    if (p && mine && sameRace && (room_.phase == Phase::Racing || room_.phase == Phase::Countdown)) {
        p->distance = std::max(p->distance, mine->distance);
        if (p->state == RacerState::Driving && mine->state != RacerState::Driving) {
            p->state = mine->state;
            p->finishTime = mine->finishTime;
        }
    }
    if (p && pending_) {
        // Once the host has handled my change, its word is final (it ignores
        // car changes after the start, for instance); until then, keep mine.
        if (static_cast<int16_t>(p->ack - pending_->seq) >= 0) {
            pending_.reset();
        } else {
            p->name = pending_->name;
            p->vehicle = pending_->vehicle;
            p->ready = pending_->ready;
        }
    }
    if (newRace) {
        countdown_ = room_.countdown;
        cars_.clear();
    }
    if (room_.phase != Phase::Countdown) countdown_ = 0.0f;
    deadline_ = room_.deadline;
    if (room_.phase == Phase::Lobby) cars_.clear();
}

void Online::applyRunEnded(uint8_t id, RacerState state, float distance, float time)
{
    Player* p = findPlayer(id);
    if (!p || p->state != RacerState::Driving || state == RacerState::Driving || state == RacerState::Left) return;
    if (room_.phase != Phase::Racing) return;
    // A player's race clock trails the host's by their latency: a time far from
    // the host's own clock is wrong (or a fib), so it's pulled back into range.
    time = std::clamp(time, std::max(0.0f, raceClock_ - 5.0f), raceClock_ + 1.0f);
    if (state == RacerState::Finished) {
        const int goal = room_.settings.goal;
        if (goal <= 0 || distance < static_cast<float>(goal) - 1.0f) return;  // can't have finished
        p->finishTime = time;
        if (deadline_ < 0.0f) deadline_ = kFinishGraceSeconds;
        notices_.push_back(p->name + " FINISHED!");
    }
    p->state = state;
    p->distance = std::max(p->distance, distance);
    dirty_ = true;
}

void Online::advanceClocks(float dt)
{
    if (room_.phase == Phase::Countdown) {
        countdown_ = std::max(0.0f, countdown_ - dt);
        if (isHost_ && countdown_ <= 0.0f) {
            room_.phase = Phase::Racing;
            raceClock_ = 0.0f;
            dirty_ = true;
        }
    }
    if (room_.phase != Phase::Racing) return;
    if (deadline_ >= 0.0f) deadline_ = std::max(0.0f, deadline_ - dt);
    if (!isHost_) return;
    raceClock_ += dt;
    const bool timeUp = deadline_ == 0.0f || raceClock_ > kMaxRaceSeconds;
    if (timeUp || nobodyDriving(room_.players)) endRace();
}

void Online::endRace()
{
    room_.phase = Phase::Results;
    deadline_ = -1.0f;
    for (Player& p : room_.players) p.distance = distanceOf(p.id);
    dirty_ = true;
    LOG_INFO("Race over");
}

void Online::broadcastRoom()
{
    dirty_ = false;
    room_.countdown = countdown_;
    room_.deadline = deadline_;
    const std::vector<uint8_t> bytes = encode(room_);
    for (const auto& [peer, guest] : guests_) {
        if (guest.id != 0) sendBytes(peer, kRoomChannel, bytes, true);
    }
}

void Online::relaySnapshot(ENetPeer* from, const CarSnapshot& snapshot)
{
    const std::vector<uint8_t> bytes = encode(snapshot);
    for (const auto& [peer, guest] : guests_) {
        if (peer != from && guest.id != 0) sendBytes(peer, kCarChannel, bytes, false);
    }
}

Phase Online::phase() const
{
    if (room_.phase == Phase::Countdown && countdown_ <= 0.0f) return Phase::Racing;
    return room_.phase;
}

const Player* Online::player(uint8_t id) const
{
    for (const Player& p : room_.players) {
        if (p.id == id) return &p;
    }
    return nullptr;
}

Player* Online::findPlayer(uint8_t id) { return const_cast<Player*>(player(id)); }

std::string Online::roomName() const
{
    for (const Player& p : room_.players) {
        if (p.host) return p.name + "'S ROOM";
    }
    return "ROOM";
}

void Online::changeMe(const PlayerUpdate& update)
{
    Player* p = findPlayer(myId_);
    if (!p) return;
    p->name = update.name;
    p->vehicle = update.vehicle;
    p->ready = update.ready;
    if (isHost_) {
        p->ready = true;  // the host is always ready
        dirty_ = true;
        return;
    }
    pending_ = update;
    pending_->seq = ++updateSeq_;
    if (server_) sendBytes(server_, kRoomChannel, encode(*pending_), true);
}

void Online::setVehicle(const std::string& vehicleId)
{
    const Player* p = me();
    if (!p || room_.phase != Phase::Lobby) return;
    changeMe({p->name, validVehicle(vehicleId), p->ready});
}

void Online::setReady(bool ready)
{
    const Player* p = me();
    if (!p || room_.phase != Phase::Lobby) return;
    changeMe({p->name, p->vehicle, ready});
}

void Online::setName(const std::string& name)
{
    const Player* p = me();
    if (!p) return;
    config_.name = sanitizeName(name);
    changeMe({config_.name, p->vehicle, p->ready});
}

void Online::setSettings(const RoomSettings& settings)
{
    if (!isHost_ || room_.phase != Phase::Lobby) return;
    RoomSettings s = settings;
    if (!findStage(s.stage)) s.stage = "countryside";
    s.goal = std::clamp(s.goal, 0, 60000);
    if (s == room_.settings) return;
    room_.settings = s;
    dirty_ = true;
}

bool Online::canStart() const
{
    if (!isHost_ || room_.phase != Phase::Lobby || room_.players.size() < 2) return false;
    return std::all_of(room_.players.begin(), room_.players.end(), [](const Player& p) { return p.host || p.ready; });
}

void Online::startRace()
{
    if (!canStart()) return;
    for (Player& p : room_.players) {
        p.state = RacerState::Driving;
        p.distance = 0.0f;
        p.finishTime = 0.0f;
    }
    room_.race++;
    room_.phase = Phase::Countdown;
    countdown_ = kCountdownSeconds;
    deadline_ = -1.0f;
    raceClock_ = 0.0f;
    cars_.clear();
    dirty_ = true;
    LOG_INFO("Race %u starting with %zu players", room_.race, room_.players.size());
}

void Online::backToRoom()
{
    if (!isHost_ || room_.phase != Phase::Results) return;
    // Players who left during the race go now.
    room_.players.erase(std::remove_if(room_.players.begin(), room_.players.end(),
                                       [this](const Player& p) {
                                           if (p.host) return false;
                                           return std::none_of(guests_.begin(), guests_.end(),
                                                               [&](const auto& g) { return g.second.id == p.id; });
                                       }),
                        room_.players.end());
    for (Player& p : room_.players) {
        p.state = RacerState::Driving;
        p.distance = 0.0f;
        p.finishTime = 0.0f;
    }
    room_.phase = Phase::Lobby;
    cars_.clear();
    dirty_ = true;
}

void Online::sendSnapshot(const CarSnapshot& snapshot)
{
    Player* p = findPlayer(myId_);
    if (!p || status_ != Status::InRoom) return;
    CarSnapshot s = snapshot;
    s.player = myId_;
    s.race = room_.race;
    if (p->state == RacerState::Driving) p->distance = std::max(p->distance, s.distance);
    if (isHost_) relaySnapshot(nullptr, s);
    else if (server_) sendBytes(server_, kCarChannel, encode(s), false);
}

void Online::reportRunEnded(RacerState state, float distance, float time)
{
    if (isHost_) {
        applyRunEnded(myId_, state, distance, time);
        return;
    }
    Player* p = findPlayer(myId_);
    if (!p || p->state != RacerState::Driving) return;
    p->state = state;
    p->distance = std::max(p->distance, distance);
    if (state == RacerState::Finished) p->finishTime = time;
    if (server_) sendBytes(server_, kRoomChannel, encode(RunReport{state, distance, time}), true);
}

const SnapshotBuffer* Online::car(uint8_t playerId) const
{
    const auto it = cars_.find(playerId);
    return it == cars_.end() || it->second.empty() ? nullptr : &it->second;
}

float Online::distanceOf(uint8_t playerId) const
{
    const Player* p = player(playerId);
    float d = p ? p->distance : 0.0f;
    if (p && p->state == RacerState::Driving) {
        if (const SnapshotBuffer* b = car(playerId)) d = std::max(d, b->latest().distance);
    }
    return d;
}

std::vector<uint8_t> Online::raceOrder() const
{
    std::vector<Player> live = room_.players;
    for (Player& p : live) p.distance = distanceOf(p.id);
    std::vector<uint8_t> ids;
    for (size_t i : standings(live)) ids.push_back(live[i].id);
    return ids;
}

int Online::pingMs() const
{
    if (!isHost_) return server_ && status_ == Status::InRoom ? static_cast<int>(server_->roundTripTime) : 0;
    int worst = 0;
    for (const auto& [peer, guest] : guests_) {
        if (guest.id != 0) worst = std::max(worst, static_cast<int>(peer->roundTripTime));
    }
    return worst;
}

bool Online::onLan() const
{
    if (isHost_) return true;
    return server_ && isPrivateAddress(ipString(server_->address));
}

void Online::close(const std::string& reason)
{
    if (status_ == Status::Closed) return;
    status_ = Status::Closed;
    closeReason_ = reason;
    LOG_INFO("Online room closed: %s", reason.c_str());
}

uint8_t Online::nextPlayerId()
{
    // Fresh ids for newcomers, so a stale snapshot can never land on the wrong car.
    for (int tries = 0; tries < 255; ++tries) {
        lastId_ = static_cast<uint8_t>(lastId_ >= 250 ? 2 : lastId_ + 1);
        if (!player(lastId_)) return lastId_;
    }
    return 0;
}

uint8_t Online::freeColor() const
{
    for (uint8_t c = 0; c < kMaxPlayers; ++c) {
        if (std::none_of(room_.players.begin(), room_.players.end(), [c](const Player& p) { return p.color == c; })) return c;
    }
    return 0;
}

RoomAd Online::advert() const
{
    RoomAd ad;
    ad.token = token_;
    ad.port = config_.port;
    ad.name = roomName();
    ad.players = static_cast<uint8_t>(room_.players.size());
    ad.maxPlayers = kMaxPlayers;
    ad.phase = room_.phase;
    ad.stage = room_.settings.stage;
    return ad;
}

} // namespace hr::net
