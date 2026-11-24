#include "core/Math.hpp"
#include "game/App.hpp"
#include "game/DemoWorld.hpp"
#include "game/Draw.hpp"
#include "game/Hud.hpp"
#include "game/OnlineUi.hpp"
#include "game/Screens.hpp"
#include "game/WorldView.hpp"
#include "net/Discovery.hpp"
#include "net/Online.hpp"

#include <cmath>
#include <cstdio>
#include <iterator>

namespace hr {

namespace {

const Color kPanelFill{44, 52, 74, 238};
const Color kPanelEdge{30, 26, 38, 255};
const Color kSoftText{200, 210, 230, 255};
const Color kWarnText{255, 160, 130, 255};
const Color kGold{255, 230, 140, 255};

// What was last typed into "join by address", kept for this session.
std::string g_lastAddress;

void dimScreen(Ui& ui, float alpha)
{
    DrawRectangle(0, 0, static_cast<int>(ui.width()) + 1, static_cast<int>(ui.height()) + 1, draw::withAlpha(BLACK, alpha));
}

std::string stageName(const std::string& id)
{
    const StageDef* s = findStage(id);
    return s ? s->name : id;
}

// Index of the next (dir = 1) or previous (dir = -1) of n entries, wrapping round.
size_t step(size_t index, int dir, size_t n) { return (index + (dir < 0 ? n - 1 : 1)) % n; }

// ---------------------------------------------------------------------------
// Online menu: rooms found on the network, hosting, joining by address.

class OnlineMenuScreen : public Screen {
public:
    OnlineMenuScreen(App& app, std::string message) : Screen(app), message_(std::move(message))
    {
        name_ = app_.playerName();
        address_ = g_lastAddress;
        app_.audio().setMusicOn(true);
    }

    const char* name() const override { return "online-menu"; }

    void update(float dt) override
    {
        time_ += dt;
        demo_.update(dt);
        finder_.update(dt);
        if (net::Online* online = app_.online()) {
            if (online->status() == net::Online::Status::InRoom && online->me()) {
                app_.showRoom();
                return;
            }
            if (online->status() == net::Online::Status::Closed) {
                app_.leaveOnline(online->closeReason());
                return;
            }
        }
        if (!app_.ui().editing() && app_.input().pressed(Action::Back)) goBack();
    }

    void draw() override
    {
        Ui& ui = app_.ui();
        demo_.draw(app_, time_);
        dimScreen(ui, 0.5f);
        ui.text("ONLINE RACE", {ui.width() * 0.5f, ui.px(22)}, ui.px(64), WHITE, Align::Center);
        drawCoinBalance(app_);
        const net::Online* online = app_.online();
        const bool connecting = online != nullptr;

        drawRoomList(ui, connecting);

        // Name, hosting and joining by address.
        const Rectangle side = ui.centered(408, 20, 424, 440);
        ui.panel(side, kPanelFill, kPanelEdge, 0.08f);
        const float x = side.x + ui.px(20);
        const float w = side.width - ui.px(40);
        float y = side.y + ui.px(16);
        ui.text("YOUR NAME", {x + ui.px(2), y}, ui.px(24), kSoftText);
        if (ui.textField("name", {x, y + ui.px(34), w, ui.px(52)}, name_, net::kMaxNameLength, TextKind::Name, "YOUR NAME")) {
            saveName();
        }
        ui.setDefaultFocus("host");
        if (ui.button("host", {x, y + ui.px(108), w, ui.px(72)}, "HOST A RACE", ButtonKind::Go, !connecting, 38)) host();
        ui.text("Friends on your Wi-Fi will see it", {side.x + side.width * 0.5f, y + ui.px(188)}, ui.px(20), kSoftText,
                Align::Center);
        DrawLineEx({x, y + ui.px(226)}, {x + w, y + ui.px(226)}, ui.px(2), {90, 100, 130, 255});
        ui.text("JOIN BY ADDRESS", {x + ui.px(2), y + ui.px(240)}, ui.px(24), kSoftText);
        ui.textField("address", {x, y + ui.px(274), w, ui.px(52)}, address_, 40, TextKind::Address, "e.g. 192.168.1.20");
        if (ui.button("join", {x, y + ui.px(344), w, ui.px(64)}, "JOIN", ButtonKind::Info, !connecting && !address_.empty())) {
            g_lastAddress = address_;
            join(address_);
        }

        // Status: connecting, or what went wrong.
        if (online) {
            char line[96];
            const int dots = static_cast<int>(time_ * 3.0f) % 4;
            std::snprintf(line, sizeof(line), "Connecting to %s%.*s", online->address().c_str(), dots, "...");
            ui.text(line, {ui.width() * 0.5f, ui.px(606)}, ui.px(26), WHITE, Align::Center);
        } else if (!message_.empty()) {
            drawWrapped(ui, message_, 600, 760, 22, kWarnText);
        }

        if (ui.button("back", ui.anchored(0.0f, 1.0f, 30, -24, 200, 70), connecting ? "CANCEL" : "BACK", ButtonKind::Back)) {
            goBack();
        }
    }

    std::string telemetry() const override
    {
        char buf[128];
        std::snprintf(buf, sizeof(buf), "{\"rooms\":%zu,\"connecting\":%s,\"name\":\"%s\"}", finder_.rooms().size(),
                      app_.online() ? "true" : "false", name_.c_str());
        return buf;
    }

private:
    void drawRoomList(Ui& ui, bool connecting)
    {
        const Rectangle list = ui.centered(-218, 20, 760, 440);
        ui.panel(list, kPanelFill, kPanelEdge, 0.08f);
        ui.text("RACES ON YOUR NETWORK", {list.x + ui.px(24), list.y + ui.px(14)}, ui.px(32), WHITE);
        const auto& rooms = finder_.rooms();
        if (rooms.empty()) {
            const float cx = list.x + list.width * 0.5f;
            char line[64];
            const int dots = static_cast<int>(time_ * 2.0f) % 4;
            std::snprintf(line, sizeof(line), "Looking for races%.*s", dots, "...");
            ui.text(finder_.working() ? line : "Can't search this network", {cx, list.y + ui.px(170)}, ui.px(32), WHITE,
                    Align::Center);
            ui.text("When a friend presses HOST A RACE,", {cx, list.y + ui.px(226)}, ui.px(22), kSoftText, Align::Center);
            ui.text("their room shows up here.", {cx, list.y + ui.px(256)}, ui.px(22), kSoftText, Align::Center);
            ui.text("Not showing up? Join with the address in their room.", {cx, list.y + ui.px(330)}, ui.px(20),
                    kSoftText, Align::Center);
            return;
        }
        for (size_t i = 0; i < rooms.size() && i < 4; ++i) {
            const net::RoomFinder::Room& r = rooms[i];
            const Rectangle row{list.x + ui.px(18), list.y + ui.px(64.0f + 88.0f * static_cast<float>(i)),
                                list.width - ui.px(36), ui.px(78)};
            DrawRectangleRounded(row, 0.3f, 10, {56, 64, 90, 255});
            drawAvatar(ui, {row.x + ui.px(40), row.y + row.height * 0.5f}, ui.px(24), r.ad.name, playerColor(0));
            ui.text(r.ad.name.c_str(), {row.x + ui.px(80), row.y + ui.px(8)}, ui.px(32), WHITE);
            char line[96];
            std::snprintf(line, sizeof(line), "%d / %d players  -  %s  -  %s", r.ad.players, r.ad.maxPlayers,
                          stageName(r.ad.stage).c_str(), r.address.c_str());
            ui.text(line, {row.x + ui.px(80), row.y + ui.px(44)}, ui.px(20), kSoftText);
            const bool busy = r.ad.phase != net::Phase::Lobby;
            const bool full = r.ad.players >= r.ad.maxPlayers;
            const Rectangle action{row.x + row.width - ui.px(146), row.y + ui.px(13), ui.px(132), ui.px(52)};
            if (busy || full) {
                outlinedPill(ui, {action.x, action.y + ui.px(8), action.width, ui.px(36)}, {120, 124, 140, 255});
                ui.text(busy ? "RACING" : "FULL", {action.x + action.width * 0.5f, action.y + ui.px(13)}, ui.px(22), WHITE,
                        Align::Center);
            } else {
                char id[16];
                std::snprintf(id, sizeof(id), "room%zu", i);
                if (ui.button(id, action, "JOIN", ButtonKind::Go, !connecting, 28)) {
                    join(r.address + ":" + std::to_string(r.ad.port));
                }
            }
        }
    }

    void saveName()
    {
        name_ = net::sanitizeName(name_);
        if (app_.progress().settings.playerName != name_) {
            app_.progress().settings.playerName = name_;
            app_.saveProgress();
        }
    }

    void host()
    {
        saveName();
        std::string error;
        if (app_.hostOnline(error)) app_.showRoom();
        else message_ = error;
    }

    void join(const std::string& address)
    {
        saveName();
        std::string error;
        if (app_.joinOnline(address, error)) message_.clear();
        else message_ = error;
    }

    void goBack()
    {
        if (app_.online()) app_.leaveOnline();
        else app_.showTitle();
    }

    DemoWorld demo_;
    net::RoomFinder finder_;
    std::string name_;
    std::string address_;
    std::string message_;
    float time_ = 0.0f;
};

// ---------------------------------------------------------------------------
// The room: who's in, what they drive, the race settings, and the start.

class RoomScreen : public Screen {
public:
    explicit RoomScreen(App& app) : Screen(app) { app_.audio().setMusicOn(true); }

    const char* name() const override { return "room"; }

    void update(float dt) override
    {
        time_ += dt;
        demo_.update(dt);
        net::Online* online = app_.online();
        if (!online || online->status() == net::Online::Status::Closed) {
            app_.leaveOnline(online ? online->closeReason() : "");
            return;
        }
        const net::Phase phase = online->phase();
        if (phase == net::Phase::Countdown || phase == net::Phase::Racing) {
            app_.showOnlineRace();
            return;
        }
        toasts_.take(online->notices());
        toasts_.update(dt);
        if (app_.input().pressed(Action::Back)) app_.leaveOnline();
    }

    void draw() override
    {
        Ui& ui = app_.ui();
        demo_.draw(app_, time_);
        dimScreen(ui, 0.5f);
        net::Online* online = app_.online();
        if (!online) return;
        const net::RoomState& room = online->room();
        const bool host = online->isHost();

        ui.text(online->roomName().c_str(), {ui.width() * 0.5f, ui.px(16)}, ui.px(60), WHITE, Align::Center);
        std::string subtitle;
        if (host) {
            subtitle = "Friends on your Wi-Fi see this room.";
            if (!online->shareAddress().empty()) subtitle += " Others can join with " + online->shareAddress();
        } else {
            const net::Player* h = nullptr;
            for (const net::Player& p : room.players) {
                if (p.host) h = &p;
            }
            subtitle = "Waiting for " + (h ? h->name : std::string("the host")) + " to start the race";
        }
        ui.text(subtitle.c_str(), {ui.width() * 0.5f, ui.px(84)}, ui.px(22), kGold, Align::Center);
        drawCoinBalance(app_);
        toasts_.draw(ui, 118);

        drawPlayers(ui, *online);
        drawSettings(ui, *online);

        if (ui.button("leave", ui.anchored(0.0f, 1.0f, 30, -24, 200, 70), host ? "CLOSE" : "LEAVE", ButtonKind::Back)) {
            app_.leaveOnline();
            return;
        }
        if (host) {
            const bool canStart = online->canStart();
            if (!canStart) {
                const char* why = room.players.size() < 2 ? "Waiting for friends to join..." : "Waiting for everyone to be ready...";
                ui.text(why, {ui.width() * 0.5f, ui.px(612)}, ui.px(24), WHITE, Align::Center);
            }
            ui.setDefaultFocus("start");
            if (ui.button("start", ui.anchored(1.0f, 1.0f, -30, -24, 300, 70), "START RACE", ButtonKind::Go, canStart, 38)) {
                online->startRace();
            }
        } else {
            const net::Player* me = online->me();
            const bool ready = me && me->ready;
            char line[64];
            std::snprintf(line, sizeof(line), "Connected  -  ping %d ms", online->pingMs());
            ui.text(line, {ui.width() * 0.5f, ui.px(612)}, ui.px(22), kSoftText, Align::Center);
            ui.setDefaultFocus("ready");
            if (ui.button("ready", ui.anchored(1.0f, 1.0f, -30, -24, 300, 70), ready ? "NOT READY" : "READY!",
                          ready ? ButtonKind::Normal : ButtonKind::Go, true, 38)) {
                online->setReady(!ready);
            }
        }
    }

    std::string telemetry() const override
    {
        const net::Online* online = app_.online();
        if (!online) return "{}";
        char buf[256];
        std::snprintf(buf, sizeof(buf), "{\"players\":%zu,\"host\":%s,\"canStart\":%s,\"stage\":\"%s\",\"goal\":%d}",
                      online->room().players.size(), online->isHost() ? "true" : "false",
                      online->canStart() ? "true" : "false", online->room().settings.stage.c_str(),
                      online->room().settings.goal);
        return buf;
    }

private:
    void drawPlayers(Ui& ui, net::Online& online)
    {
        const net::RoomState& room = online.room();
        const Rectangle panel = ui.centered(-218, 30, 760, 404);
        ui.panel(panel, kPanelFill, kPanelEdge, 0.08f);
        ui.text("PLAYERS", {panel.x + ui.px(24), panel.y + ui.px(14)}, ui.px(34), WHITE);
        char count[32];
        std::snprintf(count, sizeof(count), "%zu / %d", room.players.size(), net::kMaxPlayers);
        ui.text(count, {panel.x + panel.width - ui.px(24), panel.y + ui.px(22)}, ui.px(24), kSoftText, Align::Right);

        for (int i = 0; i < net::kMaxPlayers; ++i) {
            const Rectangle row{panel.x + ui.px(18), panel.y + ui.px(66.0f + 82.0f * static_cast<float>(i)),
                                panel.width - ui.px(36), ui.px(72)};
            if (static_cast<size_t>(i) >= room.players.size()) {
                DrawRectangleRounded(row, 0.3f, 10, {56, 64, 90, 110});
                ui.text("open seat", {row.x + row.width * 0.5f, row.y + ui.px(22)}, ui.px(24), {150, 160, 185, 255},
                        Align::Center);
                continue;
            }
            const net::Player& p = room.players[static_cast<size_t>(i)];
            const bool me = p.id == online.myId();
            DrawRectangleRounded(row, 0.3f, 10, me ? Color{64, 84, 124, 255} : Color{56, 64, 90, 255});
            drawAvatar(ui, {row.x + ui.px(40), row.y + row.height * 0.5f}, ui.px(23), p.name, playerColor(p.color));
            ui.text(p.name.c_str(), {row.x + ui.px(80), row.y + ui.px(6)}, ui.px(32), WHITE);
            const char* tag = p.host && me ? "you  -  host" : p.host ? "host" : me ? "you" : "";
            ui.text(tag, {row.x + ui.px(80), row.y + ui.px(42)}, ui.px(18), kSoftText);

            if (const VehicleDef* def = findVehicle(p.vehicle)) {
                drawVehicleThumb(ui, *def, {row.x + ui.px(330), row.y + row.height * 0.62f}, 21.0f, time_);
                ui.text(def->name.c_str(), {row.x + ui.px(420), row.y + ui.px(22)}, ui.px(24), {225, 230, 245, 255});
            }
            if (me) {
                if (ui.arrowButton("car-prev", {row.x + ui.px(254), row.y + ui.px(14), ui.px(30), ui.px(44)}, -1)) cycleVehicle(online, -1);
                if (ui.arrowButton("car-next", {row.x + ui.px(376), row.y + ui.px(14), ui.px(30), ui.px(44)}, 1)) cycleVehicle(online, 1);
            }
            const Rectangle chip{row.x + row.width - ui.px(150), row.y + ui.px(18), ui.px(134), ui.px(36)};
            outlinedPill(ui, chip, p.ready ? Color{94, 186, 52, 255} : Color{230, 150, 40, 255});
            ui.text(p.ready ? "READY" : "NOT READY", {chip.x + chip.width * 0.5f, chip.y + ui.px(6)}, ui.px(20), WHITE,
                    Align::Center);
        }
    }

    void drawSettings(Ui& ui, net::Online& online)
    {
        const net::RoomSettings& s = online.room().settings;
        const bool host = online.isHost();
        const Rectangle panel = ui.centered(408, 30, 424, 404);
        ui.panel(panel, kPanelFill, kPanelEdge, 0.08f);
        ui.text("RACE", {panel.x + ui.px(24), panel.y + ui.px(14)}, ui.px(34), WHITE);

        const StageDef* stage = findStage(s.stage);
        const Rectangle card{panel.x + ui.px(20), panel.y + ui.px(62), panel.width - ui.px(40), ui.px(150)};
        DrawRectangleRounded({card.x - ui.px(4), card.y - ui.px(4), card.width + ui.px(8), card.height + ui.px(8)}, 0.12f,
                             10, kPanelEdge);
        if (stage) drawStagePreview(*stage, card, time_);
        ui.text(stageName(s.stage).c_str(), {card.x + ui.px(14), card.y + card.height - ui.px(40)}, ui.px(30), WHITE);
        if (host) {
            if (ui.arrowButton("stage-prev", {card.x - ui.px(4), card.y + ui.px(46), ui.px(36), ui.px(56)}, -1)) cycleStage(online, -1);
            if (ui.arrowButton("stage-next", {card.x + card.width - ui.px(32), card.y + ui.px(46), ui.px(36), ui.px(56)}, 1)) {
                cycleStage(online, 1);
            }
        }

        auto row = [&](int i, const char* label) {
            const float y = card.y + card.height + ui.px(18.0f + 42.0f * static_cast<float>(i));
            ui.text(label, {panel.x + ui.px(26), y}, ui.px(24), kSoftText);
            return y;
        };
        const float right = panel.x + panel.width - ui.px(24);
        const std::string goal = s.goal > 0 ? "first to " + formatThousands(s.goal) + "m" : "furthest wins";
        float y = row(0, "Goal");
        if (host) {
            if (ui.button("goal", {right - ui.px(200), y - ui.px(6), ui.px(200), ui.px(40)}, goal.c_str(), ButtonKind::Normal,
                          true, 22)) {
                cycleGoal(online);
            }
        } else {
            ui.text(goal.c_str(), {right, y}, ui.px(24), WHITE, Align::Right);
        }
        y = row(1, "Difficulty");
        const char* difficulty = difficultyDef(s.difficulty).name;
        if (host) {
            if (ui.button("difficulty", {right - ui.px(200), y - ui.px(6), ui.px(200), ui.px(40)}, difficulty,
                          ButtonKind::Normal, true, 22)) {
                net::RoomSettings next = s;
                next.difficulty = static_cast<Difficulty>((static_cast<int>(s.difficulty) + 1) % 3);
                online.setSettings(next);
            }
        } else {
            ui.text(difficulty, {right, y}, ui.px(24), difficultyColor(s.difficulty), Align::Right);
        }
        y = row(2, "Car crashes");
        ui.text("off (ghost cars)", {right, y}, ui.px(24), WHITE, Align::Right);
        y = row(3, "Upgrades");
        ui.text("everyone's own", {right, y}, ui.px(24), WHITE, Align::Right);
    }

    void cycleVehicle(net::Online& online, int dir)
    {
        Progress& p = app_.progress();
        std::vector<std::string> owned;
        for (const VehicleDef& v : vehicles()) {
            if (p.vehicleUnlocked(v.id)) owned.push_back(v.id);
        }
        const net::Player* me = online.me();
        if (owned.size() < 2 || !me) return;
        size_t index = 0;
        for (size_t i = 0; i < owned.size(); ++i) {
            if (owned[i] == me->vehicle) index = i;
        }
        index = step(index, dir, owned.size());
        online.setVehicle(owned[index]);
        p.selectedVehicle = owned[index];
        app_.saveProgress();
    }

    void cycleStage(net::Online& online, int dir)
    {
        Progress& p = app_.progress();
        std::vector<std::string> owned;
        for (const StageDef& s : stages()) {
            if (p.stageUnlocked(s.id)) owned.push_back(s.id);
        }
        if (owned.size() < 2) return;
        net::RoomSettings next = online.room().settings;
        size_t index = 0;
        for (size_t i = 0; i < owned.size(); ++i) {
            if (owned[i] == next.stage) index = i;
        }
        index = step(index, dir, owned.size());
        next.stage = owned[index];
        online.setSettings(next);
    }

    void cycleGoal(net::Online& online)
    {
        static const int goals[] = {500, 1000, 2000, 5000, 0};
        net::RoomSettings next = online.room().settings;
        size_t index = 0;
        for (size_t i = 0; i < std::size(goals); ++i) {
            if (goals[i] == next.goal) index = i;
        }
        next.goal = goals[step(index, 1, std::size(goals))];
        online.setSettings(next);
    }

    DemoWorld demo_;
    Toasts toasts_;
    float time_ = 0.0f;
};

} // namespace

std::unique_ptr<Screen> makeOnlineMenuScreen(App& app, const std::string& message)
{
    return std::make_unique<OnlineMenuScreen>(app, message);
}

std::unique_ptr<Screen> makeRoomScreen(App& app) { return std::make_unique<RoomScreen>(app); }

} // namespace hr
