#include "TestFramework.hpp"

#include "net/Discovery.hpp"
#include "net/Online.hpp"
#include "net/Protocol.hpp"
#include "net/Race.hpp"
#include "net/Wire.hpp"

#include <chrono>
#include <thread>

using namespace hr;
using namespace hr::net;

namespace {

// Away from the game's own ports, one per test so they never collide.
constexpr uint16_t kTestDiscoveryPort = 47930;
uint16_t testPort(int n) { return static_cast<uint16_t>(47931 + n); }

Online::Config config(const char* name, uint16_t port, const char* vehicle = "jeep")
{
    Online::Config c;
    c.name = name;
    c.vehicle = vehicle;
    c.version = "test";
    c.port = port;
    c.discoveryPort = 0;
    return c;
}

// Services every connection (as the game does each frame) until `done`
// says so, or about two seconds pass.
template <typename Done>
bool pump(const std::vector<Online*>& all, Done done, float dt = 1.0f / 60.0f)
{
    for (int i = 0; i < 2000; ++i) {
        for (Online* o : all) o->update(dt);
        if (done()) return true;
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    return done();
}

// Lets `seconds` of game time pass for every connection.
void advance(const std::vector<Online*>& all, float seconds, float dt = 0.5f)
{
    for (float t = 0.0f; t < seconds; t += dt) {
        for (Online* o : all) o->update(dt);
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
}

CarSnapshot snapshotAt(float time, float x)
{
    CarSnapshot s;
    s.time = time;
    s.distance = x;
    s.velocity = {8.0f, 0.5f};
    s.pose.chassis = {{x, 1.0f}, b2MakeRot(0.1f)};
    s.pose.head = {{x - 0.2f, 2.0f}, b2MakeRot(0.1f)};
    s.pose.wheels = {{{x - 1.2f, 0.4f}, b2MakeRot(time)}, {{x + 1.2f, 0.4f}, b2MakeRot(time)}};
    return s;
}

// A host plus guests, all joined.
struct Room {
    std::unique_ptr<Online> host;
    std::vector<std::unique_ptr<Online>> guests;

    std::vector<Online*> all() const
    {
        std::vector<Online*> v{host.get()};
        for (const auto& g : guests) v.push_back(g.get());
        return v;
    }
};

Room makeRoom(uint16_t port, std::vector<const char*> guestNames)
{
    Room room;
    std::string error;
    room.host = Online::host(config("ANNA", port), error);
    CHECK(room.host != nullptr);
    if (!room.host) return room;
    for (const char* name : guestNames) {
        room.guests.push_back(Online::join("127.0.0.1:" + std::to_string(port), config(name, port, "bike"), error));
        CHECK(room.guests.back() != nullptr);
    }
    const size_t expected = guestNames.size() + 1;
    const bool joined = pump(room.all(), [&] {
        if (room.host->room().players.size() != expected) return false;
        for (const auto& g : room.guests) {
            if (g->status() != Online::Status::InRoom || g->room().players.size() != expected) return false;
        }
        return true;
    });
    CHECK(joined);
    return room;
}

} // namespace

TEST("net: wire format round-trips and rejects truncated data")
{
    Writer w;
    w.u8(200);
    w.u16(65000);
    w.u32(4000000000u);
    w.f32(-3.25f);
    w.str("hello");
    const std::vector<uint8_t> bytes = w.bytes();
    Reader r(bytes.data(), bytes.size());
    CHECK(r.u8() == 200);
    CHECK(r.u16() == 65000);
    CHECK(r.u32() == 4000000000u);
    CHECK(r.f32() == -3.25f);
    CHECK(r.str() == "hello");
    CHECK(r.ok() && r.atEnd());

    Reader cut(bytes.data(), bytes.size() - 2);
    cut.u8();
    cut.u16();
    cut.u32();
    cut.f32();
    CHECK(cut.str().empty());
    CHECK(!cut.ok());
}

TEST("net: messages survive encoding")
{
    RoomState room;
    room.phase = Phase::Racing;
    room.race = 7;
    room.countdown = 1.5f;
    room.deadline = 12.0f;
    room.settings = {"desert", Difficulty::Hard, 2000};
    Player a;
    a.id = 1;
    a.name = "ANNA";
    a.vehicle = "racecar";
    a.ready = true;
    a.host = true;
    a.color = 0;
    a.state = RacerState::Finished;
    a.distance = 2001.5f;
    a.finishTime = 61.25f;
    Player b;
    b.id = 4;
    b.name = "BEN";
    b.vehicle = "bike";
    b.color = 3;
    b.state = RacerState::Crashed;
    b.distance = 812.0f;
    room.players = {a, b};
    const std::vector<uint8_t> bytes = encode(room);
    RoomState back;
    CHECK(messageType(bytes.data(), bytes.size()) == MsgType::Room);
    CHECK(decode(bytes.data(), bytes.size(), back));
    CHECK(back.phase == Phase::Racing && back.race == 7 && back.deadline == 12.0f);
    CHECK(back.settings == room.settings);
    CHECK(back.players.size() == 2);
    CHECK(back.players[0].name == "ANNA" && back.players[0].host && back.players[0].ready);
    CHECK(back.players[0].state == RacerState::Finished && back.players[0].finishTime == 61.25f);
    CHECK(back.players[1].id == 4 && back.players[1].vehicle == "bike" && back.players[1].color == 3);
    CHECK(back.players[1].state == RacerState::Crashed && !back.players[1].ready);

    CarSnapshot s = snapshotAt(3.5f, 120.0f);
    s.player = 4;
    s.pose.crashed = true;
    s.state = RacerState::Crashed;
    const std::vector<uint8_t> sb = encode(s);
    CarSnapshot sback;
    CHECK(decode(sb.data(), sb.size(), sback));
    CHECK(sback.player == 4 && sback.time == 3.5f && sback.distance == 120.0f);
    CHECK(sback.state == RacerState::Crashed && sback.pose.crashed);
    CHECK(sback.pose.wheels.size() == 2);
    CHECK_NEAR(sback.pose.chassis.p.x, 120.0f, 1e-4);
    CHECK_NEAR(sback.pose.chassis.q.c, s.pose.chassis.q.c, 1e-5);
    CHECK_NEAR(sback.pose.chassis.q.s, s.pose.chassis.q.s, 1e-5);
    CHECK_NEAR(sback.pose.wheels[0].q.s, s.pose.wheels[0].q.s, 1e-5);
    CHECK_NEAR(sback.pose.wheels[1].p.x, 121.2f, 1e-4);

    Hello hello{kProtocolVersion, "1.1.0", "ANNA", "jeep"};
    const std::vector<uint8_t> hb = encode(hello);
    Hello hback;
    CHECK(decode(hb.data(), hb.size(), hback) && hback.name == "ANNA" && hback.version == "1.1.0");

    RunReport report{RacerState::Finished, 1000.5f, 58.0f};
    const std::vector<uint8_t> rb = encode(report);
    RunReport rback;
    CHECK(decode(rb.data(), rb.size(), rback) && rback.state == RacerState::Finished && rback.time == 58.0f);

    RoomAd ad;
    ad.token = 0xDEADBEEF;
    ad.port = 47821;
    ad.name = "ANNA'S ROOM";
    ad.players = 3;
    ad.stage = "arctic";
    const std::vector<uint8_t> ab = encode(ad);
    RoomAd aback;
    CHECK(decode(ab.data(), ab.size(), aback) && aback.token == 0xDEADBEEF && aback.name == "ANNA'S ROOM");
    CHECK(aback.players == 3 && aback.stage == "arctic");
    const std::vector<uint8_t> query = encodeDiscoveryQuery();
    CHECK(isDiscoveryQuery(query.data(), query.size()));
    CHECK(!isDiscoveryQuery(ab.data(), ab.size()));
}

TEST("net: malformed packets are ignored")
{
    RoomState room;
    room.players.resize(2);
    std::vector<uint8_t> bytes = encode(room);
    RoomState out;
    // Truncated, padded, wrong type, garbage.
    CHECK(!decode(bytes.data(), bytes.size() - 1, out));
    bytes.push_back(0);
    CHECK(!decode(bytes.data(), bytes.size(), out));
    const std::vector<uint8_t> other = encodeWelcome(3);
    CHECK(!decode(other.data(), other.size(), out));
    const uint8_t junk[] = {static_cast<uint8_t>(MsgType::Snapshot), 1, 2, 3};
    CarSnapshot s;
    CHECK(!decode(junk, sizeof(junk), s));
    CHECK(messageType(nullptr, 0) == MsgType::None);
    const uint8_t unknown[] = {200, 0};
    CHECK(messageType(unknown, sizeof(unknown)) == MsgType::None);
    // Too many players for a room.
    room.players.resize(kMaxPlayers + 1);
    const std::vector<uint8_t> crowded = encode(room);
    CHECK(!decode(crowded.data(), crowded.size(), out));
    // NaN positions arrive as zero rather than breaking the drawing.
    CarSnapshot nan = snapshotAt(1.0f, 10.0f);
    nan.pose.chassis.p.x = std::nanf("");
    const std::vector<uint8_t> nb = encode(nan);
    CHECK(decode(nb.data(), nb.size(), s) && s.pose.chassis.p.x == 0.0f);
}

TEST("net: player names are cleaned up")
{
    CHECK(sanitizeName("rishi") == "RISHI");
    CHECK(sanitizeName("  big   al ") == "BIG AL");
    CHECK(sanitizeName("chloy.c@simplify") == "CHLOYCSIMPLI");
    CHECK(sanitizeName("") == "PLAYER");
    CHECK(sanitizeName("@@@") == "PLAYER");
    CHECK(sanitizeName("ab-cd") == "AB-CD");
    CHECK(sanitizeName("a very long player name").size() <= kMaxNameLength);
}

TEST("net: race order is finishers by time, then distance, leavers last")
{
    std::vector<Player> ps(5);
    for (size_t i = 0; i < ps.size(); ++i) ps[i].id = static_cast<uint8_t>(i + 1);
    ps[0].state = RacerState::Crashed;
    ps[0].distance = 700.0f;
    ps[1].state = RacerState::Finished;
    ps[1].finishTime = 80.0f;
    ps[1].distance = 1000.0f;
    ps[2].state = RacerState::Finished;
    ps[2].finishTime = 75.0f;
    ps[2].distance = 1000.0f;
    ps[3].state = RacerState::Left;
    ps[3].distance = 950.0f;
    ps[4].state = RacerState::Driving;
    ps[4].distance = 850.0f;
    const std::vector<size_t> order = standings(ps);
    CHECK(order.size() == 5);
    CHECK(order[0] == 2 && order[1] == 1 && order[2] == 4 && order[3] == 0 && order[4] == 3);
    CHECK(!nobodyDriving(ps));
    ps[4].state = RacerState::OutOfFuel;
    CHECK(nobodyDriving(ps));
}

TEST("net: snapshot buffer interpolates and holds the ends")
{
    SnapshotBuffer buf;
    CarSnapshot out;
    CHECK(!buf.sample(1.0f, out));
    buf.push(snapshotAt(1.0f, 10.0f));
    buf.push(snapshotAt(1.1f, 11.0f));
    buf.push(snapshotAt(1.05f, 99.0f));  // out of order: dropped
    CHECK(buf.sample(1.05f, out));
    CHECK_NEAR(out.pose.chassis.p.x, 10.5f, 1e-3);
    CHECK_NEAR(out.distance, 10.5f, 1e-3);
    CHECK(buf.sample(0.5f, out) && out.pose.chassis.p.x == 10.0f);
    // Past the newest snapshot the car glides on at its last speed (8 m/s), briefly.
    CHECK(buf.sample(1.15f, out));
    CHECK_NEAR(out.pose.chassis.p.x, 11.4f, 1e-3);
    CHECK_NEAR(out.pose.wheels[0].p.x, 11.0f - 1.2f + 0.4f, 1e-3);
    CHECK(buf.sample(5.0f, out));
    CHECK_NEAR(out.pose.chassis.p.x, 11.0f + 8.0f * SnapshotBuffer::kMaxGlide, 1e-3);
    // A crashed car doesn't glide.
    CarSnapshot wreck = snapshotAt(1.2f, 12.0f);
    wreck.state = RacerState::Crashed;
    buf.push(wreck);
    CHECK(buf.sample(3.0f, out) && out.pose.chassis.p.x == 12.0f);
    // Old snapshots are dropped after a couple of seconds.
    for (int i = 0; i < 300; ++i) buf.push(snapshotAt(2.0f + static_cast<float>(i) * 0.05f, 20.0f + static_cast<float>(i)));
    CHECK(buf.sample(0.0f, out) && out.time > 10.0f);
}

TEST("net: guests join a hosted room and see the same players")
{
    Room room = makeRoom(testPort(0), {"BEN", "CARA"});
    if (room.guests.size() != 2) return;
    Online& host = *room.host;
    CHECK(host.isHost() && host.myId() == 1);
    CHECK(host.roomName() == "ANNA'S ROOM");
    for (const auto& g : room.guests) {
        CHECK(g->myId() != 0 && g->myId() != host.myId());
        CHECK(g->roomName() == "ANNA'S ROOM");
        CHECK(g->me() && g->me()->vehicle == "bike");
    }
    CHECK(room.guests[0]->myId() != room.guests[1]->myId());
    // Everyone gets a different colour.
    const auto& ps = host.room().players;
    CHECK(ps.size() == 3 && ps[0].color != ps[1].color && ps[1].color != ps[2].color && ps[0].color != ps[2].color);
    CHECK(host.notices().size() == 2);
}

TEST("net: lobby choices reach everyone and starting needs everyone ready")
{
    Room room = makeRoom(testPort(1), {"BEN", "CARA"});
    if (room.guests.size() != 2) return;
    Online& host = *room.host;
    Online& ben = *room.guests[0];
    Online& cara = *room.guests[1];
    CHECK(!host.canStart());
    ben.setVehicle("monster");
    ben.setReady(true);
    host.setSettings({"arctic", Difficulty::Medium, 500});
    const bool synced = pump(room.all(), [&] {
        const Player* b = cara.player(ben.myId());
        return b && b->ready && b->vehicle == "monster" && cara.room().settings.stage == "arctic" &&
               host.player(ben.myId())->ready;
    });
    CHECK(synced);
    CHECK(cara.room().settings.goal == 500 && cara.room().settings.difficulty == Difficulty::Medium);
    CHECK(!host.canStart());  // Cara isn't ready yet
    // Guests can't change the settings.
    cara.setSettings({"moon", Difficulty::Hard, 100});
    cara.setReady(true);
    CHECK(pump(room.all(), [&] { return host.canStart(); }));
    CHECK(host.room().settings.stage == "arctic");
    // Unknown vehicles fall back to the jeep.
    cara.setVehicle("spaceship");
    CHECK(pump(room.all(), [&] { const Player* c = host.player(cara.myId()); return c && c->vehicle == "jeep"; }));
}

TEST("net: a full race with countdown, snapshots, finishes and results")
{
    Room room = makeRoom(testPort(2), {"BEN", "CARA"});
    if (room.guests.size() != 2) return;
    Online& host = *room.host;
    Online& ben = *room.guests[0];
    Online& cara = *room.guests[1];
    host.setSettings({"countryside", Difficulty::Easy, 1000});
    ben.setReady(true);
    cara.setReady(true);
    CHECK(pump(room.all(), [&] { return host.canStart(); }));
    host.startRace();
    CHECK(pump(room.all(), [&] { return ben.room().phase == Phase::Countdown && cara.room().phase == Phase::Countdown; }));
    CHECK(ben.countdown() > 2.0f && ben.phase() == Phase::Countdown);
    // Count down (bigger steps to save time).
    CHECK(pump(room.all(), [&] { return host.phase() == Phase::Racing && ben.phase() == Phase::Racing && cara.phase() == Phase::Racing; }, 0.1f));

    // Cars reach the other players through the host.
    for (int i = 1; i <= 5; ++i) {
        const float t = static_cast<float>(i) * 0.05f;
        ben.sendSnapshot(snapshotAt(t, 10.0f * static_cast<float>(i)));
        host.sendSnapshot(snapshotAt(t, 9.0f * static_cast<float>(i)));
    }
    CHECK(pump(room.all(), [&] {
        const SnapshotBuffer* benOnCara = cara.car(ben.myId());
        const SnapshotBuffer* hostOnCara = cara.car(host.myId());
        const SnapshotBuffer* benOnHost = host.car(ben.myId());
        return benOnCara && hostOnCara && benOnHost && benOnCara->latest().distance == 50.0f &&
               hostOnCara->latest().distance == 45.0f;
    }));
    CHECK(cara.car(cara.myId()) == nullptr);  // nobody echoes your own car back
    CHECK_NEAR(cara.distanceOf(ben.myId()), 50.0f, 1e-3);

    // Seventy seconds in, Ben finishes, which starts the clock for everyone else.
    advance(room.all(), 70.0f);
    ben.reportRunEnded(RacerState::Finished, 1000.5f, 70.0f);
    CHECK(pump(room.all(), [&] { return cara.deadline() > 0.0f; }));
    CHECK(cara.deadline() <= kFinishGraceSeconds);
    // A finish short of the line doesn't count.
    host.reportRunEnded(RacerState::Finished, 600.0f, 71.0f);
    CHECK(host.me()->state == RacerState::Driving);
    host.reportRunEnded(RacerState::OutOfFuel, 640.0f, 72.0f);
    cara.reportRunEnded(RacerState::Crashed, 720.0f, 73.0f);
    CHECK(pump(room.all(), [&] { return ben.room().phase == Phase::Results && cara.room().phase == Phase::Results; }));
    const std::vector<uint8_t> order = ben.raceOrder();
    CHECK(order.size() == 3);
    CHECK(order[0] == ben.myId() && order[1] == cara.myId() && order[2] == host.myId());
    CHECK(ben.player(ben.myId())->finishTime == 70.0f);
    CHECK(ben.player(cara.myId())->state == RacerState::Crashed);

    // Back to the room for a rematch.
    host.backToRoom();
    CHECK(pump(room.all(), [&] { return ben.room().phase == Phase::Lobby && cara.room().phase == Phase::Lobby; }));
    CHECK(ben.player(cara.myId())->state == RacerState::Driving && ben.player(cara.myId())->distance == 0.0f);
    CHECK(host.canStart());  // everyone stays ready
}

TEST("net: the race is called when the finish grace time runs out")
{
    Room room = makeRoom(testPort(3), {"BEN"});
    if (room.guests.size() != 1) return;
    Online& host = *room.host;
    Online& ben = *room.guests[0];
    ben.setReady(true);
    CHECK(pump(room.all(), [&] { return host.canStart(); }));
    host.startRace();
    CHECK(pump(room.all(), [&] { return ben.phase() == Phase::Racing && host.phase() == Phase::Racing; }, 0.1f));
    host.reportRunEnded(RacerState::Finished, 1000.0f, 0.5f);
    ben.sendSnapshot(snapshotAt(0.6f, 777.0f));
    // Ben keeps driving; the clock runs out.
    CHECK(pump(room.all(), [&] { return ben.room().phase == Phase::Results; }, 0.25f));
    const Player* b = ben.player(ben.myId());
    CHECK(b && b->state == RacerState::Driving);
    CHECK_NEAR(ben.distanceOf(ben.myId()), 777.0f, 1e-3);
    CHECK(ben.raceOrder().front() == host.myId());
}

TEST("net: rooms turn players away when full, mid-race or on another version")
{
    Room room = makeRoom(testPort(4), {"BEN", "CARA", "DAN"});
    if (room.guests.size() != 3) return;
    std::string error;
    auto extra = Online::join("127.0.0.1:" + std::to_string(testPort(4)), config("EVE", testPort(4)), error);
    CHECK(extra != nullptr);
    std::vector<Online*> all = room.all();
    all.push_back(extra.get());
    CHECK(pump(all, [&] { return extra->status() == Online::Status::Closed; }));
    CHECK(extra->closeReason().find("full") != std::string::npos);
    CHECK(room.host->room().players.size() == 4);

    // Different protocol version.
    auto oldGame = [&] {
        Online::Config c = config("OLD", testPort(4));
        c.protocol = kProtocolVersion + 1;
        return Online::join("127.0.0.1:" + std::to_string(testPort(4)), c, error);
    }();
    room.guests[2].reset();  // DAN leaves, making room
    all = {room.host.get(), room.guests[0].get(), room.guests[1].get(), oldGame.get()};
    CHECK(pump(all, [&] { return oldGame->status() == Online::Status::Closed; }));
    CHECK(oldGame->closeReason().find("version") != std::string::npos);

    // Mid-race.
    room.guests[0]->setReady(true);
    room.guests[1]->setReady(true);
    all = {room.host.get(), room.guests[0].get(), room.guests[1].get()};
    CHECK(pump(all, [&] { return room.host->canStart(); }));
    room.host->startRace();
    auto late = Online::join("127.0.0.1:" + std::to_string(testPort(4)), config("LATE", testPort(4)), error);
    all.push_back(late.get());
    CHECK(pump(all, [&] { return late->status() == Online::Status::Closed; }));
    CHECK(late->closeReason().find("under way") != std::string::npos);
}

TEST("net: leaving mid-race is recorded; a closing host tells everyone")
{
    Room room = makeRoom(testPort(5), {"BEN", "CARA"});
    if (room.guests.size() != 2) return;
    Online& host = *room.host;
    Online& cara = *room.guests[1];
    const uint8_t benId = room.guests[0]->myId();
    room.guests[0]->setReady(true);
    cara.setReady(true);
    CHECK(pump(room.all(), [&] { return host.canStart(); }));
    host.startRace();
    CHECK(pump(room.all(), [&] { return cara.phase() == Phase::Racing; }, 0.1f));
    room.guests[0].reset();  // Ben quits the game mid-race
    std::vector<Online*> left = {&host, &cara};
    CHECK(pump(left, [&] { const Player* b = cara.player(benId); return b && b->state == RacerState::Left; }));
    CHECK(host.room().phase == Phase::Racing);  // the race goes on

    room.host.reset();  // the host quits
    CHECK(pump({&cara}, [&] { return cara.status() == Online::Status::Closed; }));
    CHECK(cara.closeReason().find("closed") != std::string::npos);
}

TEST("net: joining a room that isn't there fails cleanly")
{
    std::string error;
    CHECK(Online::join("", config("BEN", testPort(6)), error) == nullptr && !error.empty());
    CHECK(Online::join("127.0.0.1:99999", config("BEN", testPort(6)), error) == nullptr);
    auto nobody = Online::join("127.0.0.1:" + std::to_string(testPort(6)), config("BEN", testPort(6)), error);
    CHECK(nobody != nullptr);
    // Nothing listens there: the attempt gives up after a few seconds.
    CHECK(pump({nobody.get()}, [&] { return nobody->status() == Online::Status::Closed; }, 0.25f));
    CHECK(!nobody->closeReason().empty());
    // Two rooms can't share a port.
    auto first = Online::host(config("ANNA", testPort(7)), error);
    auto second = Online::host(config("BEN", testPort(7)), error);
    CHECK(first != nullptr && second == nullptr && !error.empty());
}

TEST("net: hostile numbers are clamped when decoding")
{
    RoomState room;
    room.countdown = 1.0e9f;
    room.deadline = 1.0e9f;
    Player p;
    p.finishTime = 1.0e12f;
    room.players = {p};
    const std::vector<uint8_t> bytes = encode(room);
    RoomState back;
    CHECK(decode(bytes.data(), bytes.size(), back));
    CHECK(back.countdown == kCountdownSeconds && back.deadline == kFinishGraceSeconds);
    CHECK(back.players[0].finishTime <= kMaxRaceSeconds + 60.0f);
    CarSnapshot s = snapshotAt(1.0e12f, 5.0f);
    const std::vector<uint8_t> sb = encode(s);
    CarSnapshot sback;
    CHECK(decode(sb.data(), sb.size(), sback) && sback.time <= kMaxRaceSeconds + 60.0f);
}

TEST("net: snapshot buffers stay small and recover from a restarted clock")
{
    SnapshotBuffer buf;
    for (int i = 0; i < 5000; ++i) buf.push(snapshotAt(1.0f + static_cast<float>(i) * 1e-4f, 1.0f));
    CHECK(buf.size() <= 128);
    buf.push(snapshotAt(100.0f, 50.0f));
    buf.push(snapshotAt(2.0f, 3.0f));  // the sender started over
    CHECK(buf.size() == 1 && buf.latest().time == 2.0f);
}

TEST("net: a late snapshot can't spill into the next race")
{
    Room room = makeRoom(testPort(9), {"BEN"});
    if (room.guests.size() != 1) return;
    Online& host = *room.host;
    Online& ben = *room.guests[0];
    ben.setReady(true);
    CHECK(pump(room.all(), [&] { return host.canStart(); }));
    host.startRace();
    CHECK(pump(room.all(), [&] { return ben.phase() == Phase::Racing && host.phase() == Phase::Racing; }, 0.1f));
    host.reportRunEnded(RacerState::Crashed, 10.0f, 1.0f);
    ben.reportRunEnded(RacerState::Crashed, 12.0f, 1.0f);
    CHECK(pump(room.all(), [&] { return ben.room().phase == Phase::Results; }));
    host.backToRoom();
    CHECK(pump(room.all(), [&] { return ben.room().phase == Phase::Lobby; }));
    host.startRace();
    // Ben hasn't heard about race 2 yet: whatever he sends now belongs to race 1.
    ben.sendSnapshot(snapshotAt(30.0f, 700.0f));
    CHECK(pump(room.all(), [&] { return ben.room().phase == Phase::Countdown; }));
    CHECK(pump(room.all(), [&] { return host.phase() == Phase::Racing; }, 0.1f));
    CHECK(host.car(ben.myId()) == nullptr);
    CHECK(host.distanceOf(ben.myId()) == 0.0f);
}

TEST("net: finish times far from the host's race clock are pulled back")
{
    Room room = makeRoom(testPort(12), {"BEN"});
    if (room.guests.size() != 1) return;
    Online& host = *room.host;
    Online& ben = *room.guests[0];
    ben.setReady(true);
    CHECK(pump(room.all(), [&] { return host.canStart(); }));
    host.startRace();
    CHECK(pump(room.all(), [&] { return ben.phase() == Phase::Racing && host.phase() == Phase::Racing; }, 0.1f));
    advance(room.all(), 40.0f);
    // "I finished in 3 seconds": the host knows the race has run for 40.
    ben.reportRunEnded(RacerState::Finished, 1000.0f, 3.0f);
    CHECK(pump(room.all(), [&] { const Player* b = host.player(ben.myId()); return b && b->state == RacerState::Finished; }));
    CHECK(host.player(ben.myId())->finishTime >= 35.0f);
}

TEST("net: leaving after the race doesn't change the results")
{
    Room room = makeRoom(testPort(10), {"BEN", "CARA"});
    if (room.guests.size() != 2) return;
    Online& host = *room.host;
    Online& ben = *room.guests[0];
    Online& cara = *room.guests[1];
    ben.setReady(true);
    cara.setReady(true);
    CHECK(pump(room.all(), [&] { return host.canStart(); }));
    host.startRace();
    CHECK(pump(room.all(), [&] { return host.phase() == Phase::Racing && ben.phase() == Phase::Racing && cara.phase() == Phase::Racing; }, 0.1f));
    ben.sendSnapshot(snapshotAt(1.0f, 800.0f));
    cara.reportRunEnded(RacerState::Crashed, 200.0f, 2.0f);
    host.reportRunEnded(RacerState::Finished, 1000.0f, 60.0f);
    // Ben is still driving when time runs out: second, ahead of Cara's crash.
    CHECK(pump(room.all(), [&] { return cara.room().phase == Phase::Results; }, 0.25f));
    const uint8_t benId = ben.myId();
    CHECK(cara.raceOrder().size() == 3 && cara.raceOrder()[1] == benId);
    room.guests[0].reset();  // Ben leaves from the results screen
    std::vector<Online*> left = {&host, &cara};
    CHECK(pump(left, [&] { return host.notices().size() >= 3 && host.notices().back() == "BEN LEFT"; }));
    pump(left, [&] { return false; });  // let the room state travel
    const std::vector<uint8_t> order = cara.raceOrder();
    CHECK(order.size() == 3 && order[1] == benId);
    CHECK(cara.player(benId) && cara.player(benId)->state == RacerState::Driving);
}

TEST("net: a car change the host ignores doesn't stick")
{
    Room room = makeRoom(testPort(11), {"BEN"});
    if (room.guests.size() != 1) return;
    Online& host = *room.host;
    Online& ben = *room.guests[0];
    ben.setReady(true);
    CHECK(pump(room.all(), [&] { return host.canStart(); }));
    // Ben picks another car just as the host presses START.
    ben.setVehicle("monster");
    host.startRace();
    CHECK(pump(room.all(), [&] { return ben.room().phase == Phase::Countdown && host.player(ben.myId())->ack != 0; }));
    pump(room.all(), [&] { return false; });  // let the room state settle
    CHECK(host.player(ben.myId())->vehicle == "bike");
    CHECK(ben.me() && ben.me()->vehicle == "bike");  // everyone agrees on what Ben drives
}

TEST("net: rooms on this computer are found by discovery")
{
    std::string error;
    Online::Config c = config("ANNA", testPort(8));
    c.discoveryPort = kTestDiscoveryPort;
    auto host = Online::host(c, error);
    CHECK(host != nullptr);
    if (!host) return;
    RoomFinder finder(kTestDiscoveryPort);
    CHECK(finder.working());
    bool found = false;
    for (int i = 0; i < 300 && !found; ++i) {
        host->update(1.0f / 60.0f);
        finder.update(1.0f / 60.0f);
        for (const RoomFinder::Room& r : finder.rooms()) {
            found = found || (r.ad.name == "ANNA'S ROOM" && r.ad.port == testPort(8) && r.ad.players == 1);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(2));
    }
    CHECK(found);
    CHECK(finder.rooms().size() == 1);  // answers from several addresses count once
    CHECK(isPrivateAddress("192.168.1.20") && isPrivateAddress("10.0.0.5") && isPrivateAddress("100.101.102.103"));
    CHECK(!isPrivateAddress("8.8.8.8") && !isPrivateAddress("not an address"));
}
