#include "core/Log.hpp"
#include "core/Math.hpp"
#include "game/App.hpp"
#include "game/Draw.hpp"
#include "game/Hud.hpp"
#include "game/LiveRun.hpp"
#include "game/OnlineUi.hpp"
#include "game/Screens.hpp"
#include "net/Online.hpp"
#include "sim/Bot.hpp"

#include <cmath>
#include <cstdio>

namespace hr {

namespace {

constexpr float kViewDelay = 0.1f;   // other cars are drawn this far in the past, between two snapshots
constexpr int kSnapshotEvery = 3;    // physics steps per snapshot: 20 a second
constexpr int kNoLevels = 1000000;   // online races pay no level rewards
constexpr float kWatchAfter = 2.5f;  // after your run ends, the camera moves on to a friend still driving
const Color kInk{30, 26, 38, 255};
const Color kSoftText{200, 210, 230, 255};

const char* stateWord(net::RacerState s)
{
    switch (s) {
    case net::RacerState::Crashed: return "DRIVER DOWN";
    case net::RacerState::OutOfFuel: return "OUT OF FUEL";
    case net::RacerState::Left: return "LEFT";
    default: return "";
    }
}

class OnlineRaceScreen : public Screen {
public:
    explicit OnlineRaceScreen(App& app);
    ~OnlineRaceScreen() override;

    const char* name() const override
    {
        if (results_) return "online-results";
        return paused_ ? "online-pause" : "online-race";
    }

    void update(float dt) override;
    void fixedUpdate() override;
    void draw() override;
    std::string telemetry() const override;

private:
    // Another player's car as this player sees it right now. Copies, not
    // pointers into the room: a room update replaces the players list.
    struct Remote {
        uint8_t id;
        std::string name;
        uint8_t color;
        net::RacerState state;
        const VehicleDef* def;
        net::CarSnapshot snap;
    };

    net::RacerState myState() const;
    void sendSnapshot(net::Online& online);
    void checkRunEnd(net::Online& online);
    void bank();
    void gatherRemotes(const net::Online& online);
    void drawTags(Ui& ui, const net::Online& online);
    void drawStandings(Ui& ui, const net::Online& online);
    void drawConnection(Ui& ui, const net::Online& online);
    void drawRaceText(Ui& ui, const net::Online& online);
    void drawPause(Ui& ui, net::Online& online);
    void drawResults(Ui& ui, net::Online& online);
    // Stops everything this screen does for the rest of the frame (the switch happens next frame).
    void leaving();

    std::unique_ptr<LiveRun> run_;
    Bot bot_;
    InputState input_;
    uint32_t race_ = 0;
    int goal_ = 0;
    float raceTime_ = 0.0f;  // seconds since GO
    int waitTicks_ = 0;      // physics ticks spent on the start line
    bool reported_ = false;  // my run is over and the room knows
    bool finished_ = false;
    int finishPlace_ = 0;
    float outFor_ = 0.0f;  // seconds since my run ended
    bool paused_ = false;
    bool results_ = false;
    float resultsTimer_ = 0.0f;
    bool banked_ = false;
    long long earned_ = 0;
    bool testDrive_ = false;
    float time_ = 0.0f;
    int lastCount_ = -1;
    uint8_t watching_ = 0;
    bool leaving_ = false;
    std::vector<Remote> remotes_;
    Toasts toasts_;
};

OnlineRaceScreen::OnlineRaceScreen(App& app) : Screen(app)
{
    const net::Online* online = app_.online();
    const net::RoomState room = online ? online->room() : net::RoomState{};
    race_ = room.race;
    goal_ = room.settings.goal;
    const StageDef* stage = findStage(room.settings.stage);
    if (!stage) stage = &stages().front();
    const net::Player* me = online ? online->me() : nullptr;
    const VehicleDef* vehicle = findVehicle(me ? me->vehicle : "");
    if (!vehicle) vehicle = &vehicles().front();
    const Progress& p = app_.progress();
    run_ = std::make_unique<LiveRun>(app_, *stage, room.settings.difficulty, *vehicle, p.upgrades(vehicle->id), 0.0f,
                                     kNoLevels);
    run_->hud().setGoalLine(goalText(goal_));
    // The autopilot (testing) races too, but whatever it earns isn't kept.
    testDrive_ = app_.options().autoplay;
    app_.audio().setMusicDuck(0.45f);
    LOG_INFO("Online race %u: %s, %s, %s", race_, stage->id.c_str(), vehicle->id.c_str(), goalText(goal_).c_str());
}

OnlineRaceScreen::~OnlineRaceScreen()
{
    bank();
    app_.audio().setEngine(false, 0.0f, 0.0f);
    app_.audio().setMusicDuck(1.0f);
}

void OnlineRaceScreen::leaving()
{
    leaving_ = true;
    remotes_.clear();
    watching_ = 0;
}

void OnlineRaceScreen::update(float dt)
{
    if (leaving_) return;
    time_ += dt;
    net::Online* online = app_.online();
    if (!online || online->status() == net::Online::Status::Closed) {
        bank();
        leaving();
        app_.leaveOnline(online ? online->closeReason() : "The room closed.");
        return;
    }
    const net::Phase phase = online->phase();
    if (phase == net::Phase::Lobby) {
        leaving();
        app_.showRoom();
        return;
    }
    if (online->room().race != race_) {  // the host started another race straight away
        leaving();
        app_.showOnlineRace();
        return;
    }
    app_.audio().setMusicDuck(0.45f);  // (every frame: a previous race screen resets it as it closes)
    toasts_.take(online->notices());
    toasts_.update(dt);

    Input& in = app_.input();
    Ui& ui = app_.ui();
    if (!results_) {
        const bool pauseClicked = in.mousePressed() && CheckCollisionPointRec(in.mouse(), Hud::pauseButton(ui));
        if (in.pressed(Action::Pause) || pauseClicked) {
            paused_ = !paused_;
            app_.audio().play(Sfx::Click);
        }
    }
    float gas = in.gas();
    float brake = in.brake();
    float special = in.special();
    if (in.mouseDown() && !paused_) {
        if (CheckCollisionPointRec(in.mouse(), Hud::gasPedal(ui))) gas = 1.0f;
        if (CheckCollisionPointRec(in.mouse(), Hud::brakePedal(ui))) brake = 1.0f;
        if ((run_->session().hasThrusters() || run_->session().hasNitro()) &&
            CheckCollisionPointRec(in.mouse(), Hud::specialPedal(ui))) {
            special = 1.0f;
        }
    }
    // The race doesn't stop for the pause menu: the car just lets go of the pedals.
    input_ = phase == net::Phase::Racing && !paused_ && !reported_ ? InputState{gas, brake, special} : InputState{};

    // 3, 2, 1, GO.
    if (phase == net::Phase::Countdown) {
        const int count = static_cast<int>(std::ceil(online->countdown()));
        if (count != lastCount_ && count > 0) app_.audio().play(Sfx::Click, 0.8f + 0.1f * static_cast<float>(3 - count));
        lastCount_ = count;
    } else if (lastCount_ != 0) {
        lastCount_ = 0;
        app_.audio().play(Sfx::Buy, 1.2f);
    }

    if (phase == net::Phase::Results && !results_) {
        results_ = true;
        paused_ = false;
        resultsTimer_ = 0.0f;
        bank();
        const std::vector<uint8_t> order = online->raceOrder();
        const bool won = !order.empty() && order.front() == online->myId();
        app_.audio().play(won ? Sfx::Fanfare : Sfx::GameOver);
        ui.setFocus(online->isHost() ? "again" : "leave");
    }
    if (results_) resultsTimer_ += dt;

    // The camera follows our car; once our run is over, whoever leads among those still driving.
    gatherRemotes(*online);
    if (reported_) outFor_ += dt;
    watching_ = 0;
    if (reported_ && outFor_ > kWatchAfter && !results_) {
        float best = -1.0f;
        for (const Remote& r : remotes_) {
            if (r.state == net::RacerState::Driving && r.snap.distance > best) {
                best = r.snap.distance;
                watching_ = r.id;
            }
        }
    }
    const Remote* watched = nullptr;
    for (const Remote& r : remotes_) {
        if (r.id == watching_) watched = &r;
    }
    if (watched) run_->update(dt, &watched->snap.pose.chassis.p, watched->snap.velocity);
    else run_->update(dt);

    const Session& s = run_->session();
    const bool engineOn = !results_ && !s.isOver() && s.state() == RunState::Running && !finished_;
    app_.audio().setEngine(engineOn, s.engineRpm(), s.throttle(), engineVoiceFor(run_->vehicle().id),
                           s.boosting() ? 1.0f : 0.0f, s.thrusting());
}

void OnlineRaceScreen::fixedUpdate()
{
    net::Online* online = app_.online();
    if (leaving_ || !online || results_ || online->status() != net::Online::Status::InRoom) return;
    if (online->room().race != race_) return;  // another race has begun; this screen is on its way out
    const net::Phase phase = online->phase();
    if (phase == net::Phase::Countdown) {
        // Everyone waits on the start line (no physics, so no fuel burns), but
        // shows up there for the others.
        if (waitTicks_++ % kSnapshotEvery == 0) sendSnapshot(*online);
        return;
    }
    if (phase != net::Phase::Racing) return;
    raceTime_ += kFixedDt;
    InputState input = input_;
    if (app_.options().autoplay && !reported_ && !run_->session().isOver()) input = bot_.think(run_->session());
    if (finished_) input = {};
    run_->step(input);
    checkRunEnd(*online);
    if (run_->steps() % kSnapshotEvery == 0) sendSnapshot(*online);
}

net::RacerState OnlineRaceScreen::myState() const
{
    if (finished_) return net::RacerState::Finished;
    const Session& s = run_->session();
    if (s.state() == RunState::Crashed) return net::RacerState::Crashed;
    if (s.isOver()) return net::RacerState::OutOfFuel;
    return net::RacerState::Driving;
}

void OnlineRaceScreen::sendSnapshot(net::Online& online)
{
    const Session& s = run_->session();
    net::CarSnapshot snap;
    snap.time = raceTime_;
    snap.distance = s.distance();
    snap.state = myState();
    snap.velocity = s.vehicle().velocity();
    snap.pose = poseOf(s);
    online.sendSnapshot(snap);
}

void OnlineRaceScreen::checkRunEnd(net::Online& online)
{
    if (reported_) return;
    const Session& s = run_->session();
    // A wreck sliding over the line doesn't count.
    if (goal_ > 0 && s.distance() >= static_cast<float>(goal_) && s.state() != RunState::Crashed) {
        reported_ = true;
        finished_ = true;
        online.reportRunEnded(net::RacerState::Finished, s.distance(), raceTime_);
        finishPlace_ = 1;
        for (const net::Player& p : online.room().players) {
            if (p.id != online.myId() && p.state == net::RacerState::Finished) finishPlace_++;
        }
        char text[48];
        std::snprintf(text, sizeof(text), "FINISHED %s!", placeText(finishPlace_));
        run_->hud().shout(text, placeColor(finishPlace_), 3.5f);
        app_.audio().play(Sfx::Fanfare);
    } else if (s.isOver()) {
        reported_ = true;
        const bool crashed = s.result().reason == EndReason::DriverDown;
        online.reportRunEnded(crashed ? net::RacerState::Crashed : net::RacerState::OutOfFuel, s.distance(), raceTime_);
    }
}

void OnlineRaceScreen::bank()
{
    if (banked_) return;
    banked_ = true;
    const RunResult& r = run_->session().result();
    earned_ = r.pickupCoins + r.bonusCoins;
    if (testDrive_ || earned_ <= 0) return;
    Progress& p = app_.progress();
    p.coins += earned_;
    p.stats.totalCoins += earned_;
    app_.saveProgress();
    LOG_INFO("Online race: +%lld coins (balance %lld)", earned_, p.coins);
}

void OnlineRaceScreen::gatherRemotes(const net::Online& online)
{
    remotes_.clear();
    const float t = raceTime_ - kViewDelay;
    for (const net::Player& p : online.room().players) {
        if (p.id == online.myId() || p.state == net::RacerState::Left) continue;
        const net::SnapshotBuffer* buffer = online.car(p.id);
        const VehicleDef* def = findVehicle(p.vehicle);
        Remote r{p.id, p.name, p.color, p.state, def, {}};
        if (!buffer || !def || !buffer->sample(t, r.snap)) continue;
        if (r.snap.pose.wheels.size() != def->wheels.size()) continue;  // not that car (yet)
        remotes_.push_back(r);
    }
}

void OnlineRaceScreen::draw()
{
    Ui& ui = app_.ui();
    if (leaving_) {
        run_->drawWorld(time_, true, false);
        return;
    }
    std::vector<OtherCar> others;
    for (const Remote& r : remotes_) others.push_back({r.def, r.snap.pose});
    run_->drawWorld(time_, results_, false, others);
    net::Online* online = app_.online();
    if (!online) return;
    if (!results_) {
        drawTags(ui, *online);
        run_->hud().draw(run_->session(), ui, input_.gas > 0.0f, input_.brake > 0.0f, true, input_.special > 0.0f);
        drawStandings(ui, *online);
        drawConnection(ui, *online);
        drawRaceText(ui, *online);
        const Session& s = run_->session();
        const float top = 134.0f + (s.hasNitro() ? 34.0f : 0.0f) + (s.vehicle().hasRollCage() ? 32.0f : 0.0f);
        // Under the leaderboard, clear of the trick banners in the middle.
        toasts_.draw(ui, top + 16.0f + 38.0f * static_cast<float>(online->room().players.size()) + 56.0f, 20.0f);
    }
    if (results_) drawResults(ui, *online);
    else if (paused_) drawPause(ui, *online);
}

void OnlineRaceScreen::drawTags(Ui& ui, const net::Online& online)
{
    const WorldView& view = run_->view();
    const float margin = ui.px(70);
    for (const Remote& r : remotes_) {
        const b2Vec2 above{r.snap.pose.chassis.p.x, std::fmax(r.snap.pose.head.p.y, r.snap.pose.chassis.p.y) + 0.9f};
        const Vector2 at = view.toScreen(above);
        const Color color = playerColor(r.color);
        if (at.x > margin && at.x < ui.width() - margin) {
            // Kept on screen when the car is up near the top edge (or above it, mid-jump).
            drawNameTag(ui, {at.x, std::fmax(at.y, ui.px(56))}, r.name.c_str(), color);
            continue;
        }
        // Off screen: an arrow at the edge, pointing the way.
        const bool ahead = at.x >= ui.width() - margin;
        char line[48];
        std::snprintf(line, sizeof(line), "%s  %sm", r.name.c_str(),
                      formatThousands(static_cast<long long>(online.distanceOf(r.id))).c_str());
        const float size = ui.px(24);
        const Vector2 dim = measureText(ui.assets(), line, size);
        const float y = clampf(at.y, ui.px(250), ui.height() - ui.px(250));
        const float w = dim.x + ui.px(28);
        const float h = ui.px(40);
        const float tipX = ahead ? ui.width() - ui.px(16) : ui.px(16);
        const float x = ahead ? tipX - ui.px(34) - w : tipX + ui.px(34);
        const Rectangle pill{x, y - h * 0.5f, w, h};
        const float edge = ahead ? pill.x + pill.width - ui.px(4) : pill.x + ui.px(4);
        DrawTriangle({edge, y - ui.px(20)}, {edge, y + ui.px(20)}, {tipX, y}, kInk);
        outlinedPill(ui, pill, color);
        const float inner = ahead ? edge - ui.px(4) : edge + ui.px(4);
        DrawTriangle({inner, y - ui.px(14)}, {inner, y + ui.px(14)}, {tipX + (ahead ? -ui.px(6) : ui.px(6)), y}, color);
        ui.text(line, {pill.x + ui.px(14), y - dim.y * 0.5f}, size, WHITE, Align::Left);
    }
    // And a tag on our own car, so it's clear which one is yours.
    const VehiclePose me = run_->pose();
    const Vector2 at = view.toScreen({me.chassis.p.x, std::fmax(me.head.p.y, me.chassis.p.y) + 0.9f});
    const net::Player* p = online.me();
    drawNameTag(ui, at, "YOU", playerColor(p ? p->color : 0));
}

void OnlineRaceScreen::drawStandings(Ui& ui, const net::Online& online)
{
    const std::vector<uint8_t> order = online.raceOrder();
    const Session& s = run_->session();
    const float top = 134.0f + (s.hasNitro() ? 34.0f : 0.0f) + (s.vehicle().hasRollCage() ? 32.0f : 0.0f);
    const Rectangle panel{ui.px(20), ui.px(top), ui.px(280), ui.px(56.0f + 38.0f * static_cast<float>(order.size()))};
    DrawRectangleRounded({panel.x - ui.px(3), panel.y - ui.px(3), panel.width + ui.px(6), panel.height + ui.px(6)}, 0.18f,
                         10, kInk);
    DrawRectangleRounded(panel, 0.18f, 10, {36, 42, 62, 225});
    ui.text(goalText(goal_).c_str(), {panel.x + ui.px(16), panel.y + ui.px(10)}, ui.px(24), {255, 230, 140, 255});
    for (size_t i = 0; i < order.size(); ++i) {
        const net::Player* p = online.player(order[i]);
        if (!p) continue;
        const bool me = p->id == online.myId();
        const float y = panel.y + ui.px(48.0f + 38.0f * static_cast<float>(i));
        const Color color = playerColor(p->color);
        if (me) {
            DrawRectangleRounded({panel.x + ui.px(6), y - ui.px(3), panel.width - ui.px(12), ui.px(36)}, 0.5f, 8,
                                 draw::withAlpha(color, 0.45f));
        }
        char text[32];
        std::snprintf(text, sizeof(text), "%zu", i + 1);
        ui.text(text, {panel.x + ui.px(26), y}, ui.px(26), WHITE, Align::Center);
        DrawCircleV({panel.x + ui.px(56), y + ui.px(15)}, ui.px(10), kInk);
        DrawCircleV({panel.x + ui.px(56), y + ui.px(15)}, ui.px(8), color);
        ui.text(me ? "YOU" : p->name.c_str(), {panel.x + ui.px(76), y}, ui.px(26), WHITE);
        std::string value;
        Color valueColor{220, 226, 240, 255};
        if (p->state == net::RacerState::Finished) {
            value = raceTimeText(p->finishTime);
            valueColor = {255, 214, 60, 255};
        } else if (p->state == net::RacerState::Left) {
            value = "LEFT";
            valueColor = {150, 158, 180, 255};
        } else {
            value = formatThousands(static_cast<long long>(online.distanceOf(p->id))) + "m";
            if (p->state != net::RacerState::Driving) valueColor = {150, 158, 180, 255};
        }
        ui.text(value.c_str(), {panel.x + panel.width - ui.px(16), y}, ui.px(26), valueColor, Align::Right);
    }
}

void OnlineRaceScreen::drawConnection(Ui& ui, const net::Online& online)
{
    // Left of the pause button, as in the HUD's top-right corner.
    const int ping = online.pingMs();
    char text[32];
    std::snprintf(text, sizeof(text), "%s   %d ms", online.isHost() ? "HOST" : online.onLan() ? "LAN" : "NET", ping);
    const float size = ui.px(24);
    const Vector2 dim = measureText(ui.assets(), text, size);
    const Rectangle chip{ui.width() - ui.px(104) - dim.x - ui.px(50), ui.px(30), dim.x + ui.px(62), ui.px(40)};
    outlinedPill(ui, chip, {44, 52, 74, 235});
    const Color dot = ping < 80 ? Color{120, 230, 100, 255} : ping < 200 ? Color{250, 210, 60, 255} : Color{240, 90, 70, 255};
    DrawCircleV({chip.x + ui.px(24), chip.y + chip.height * 0.5f}, ui.px(8), dot);
    ui.text(text, {chip.x + ui.px(42), chip.y + (chip.height - dim.y) * 0.5f}, size, WHITE, Align::Left);
}

void OnlineRaceScreen::drawRaceText(Ui& ui, const net::Online& online)
{
    const float cx = ui.width() * 0.5f;
    if (online.phase() == net::Phase::Countdown) {
        const float left = online.countdown();
        const int count = std::max(1, static_cast<int>(std::ceil(left)));
        const float frac = left - std::floor(left);
        const float pop = 1.0f + 0.35f * frac;
        char text[16];
        std::snprintf(text, sizeof(text), "%d", count);
        ui.text(text, {cx, ui.height() * 0.30f - ui.px(40) * pop}, ui.px(150) * pop, {255, 214, 60, 255}, Align::Center);
        ui.text("GET READY", {cx, ui.height() * 0.30f + ui.px(110)}, ui.px(36), WHITE, Align::Center);
    } else if (raceTime_ < 1.2f) {
        const float alpha = clampf((1.2f - raceTime_) * 2.0f, 0.0f, 1.0f);
        ui.text("GO!", {cx, ui.height() * 0.30f - ui.px(50)}, ui.px(150), Fade({120, 230, 100, 255}, alpha), Align::Center);
    }

    float y = 128.0f;
    if (watching_) {
        if (const net::Player* p = online.player(watching_)) {
            const std::string text = "WATCHING " + p->name;
            ui.text(text.c_str(), {cx, ui.px(y)}, ui.px(30), playerColor(p->color), Align::Center);
            y += 36.0f;
        }
    }
    if (online.deadline() > 0.0f) {
        char text[64];
        std::snprintf(text, sizeof(text), "%sRACE ENDS IN %ds", reported_ ? "" : "HURRY!  ",
                      static_cast<int>(std::ceil(online.deadline())));
        ui.text(text, {cx, ui.px(y)}, ui.px(28), {255, 170, 70, 255}, Align::Center);
    } else if (reported_ && !finished_ && !watching_ && outFor_ > kWatchAfter) {
        ui.text("Waiting for the others to finish...", {cx, ui.px(y)}, ui.px(26), WHITE, Align::Center);
    }
}

void OnlineRaceScreen::drawPause(Ui& ui, net::Online& online)
{
    DrawRectangle(0, 0, static_cast<int>(ui.width()) + 1, static_cast<int>(ui.height()) + 1, draw::withAlpha(BLACK, 0.45f));
    const Rectangle panel = ui.centered(0, 0, 460, 360);
    ui.panel(panel, {44, 52, 74, 245}, {30, 26, 38, 255});
    ui.text("PAUSED", {panel.x + panel.width * 0.5f, panel.y + ui.px(24)}, ui.px(54), WHITE, Align::Center);
    ui.text("The race keeps going!", {panel.x + panel.width * 0.5f, panel.y + ui.px(92)}, ui.px(24), {255, 200, 120, 255},
            Align::Center);
    ui.setDefaultFocus("resume");
    if (ui.button("resume", ui.centered(0, 0, 320, 64), "RESUME", ButtonKind::Go)) paused_ = false;
    if (ui.button("leave", ui.centered(0, 90, 320, 64), online.isHost() ? "CLOSE ROOM" : "LEAVE RACE", ButtonKind::Back)) {
        bank();
        leaving();
        app_.leaveOnline();
    }
}

void OnlineRaceScreen::drawResults(Ui& ui, net::Online& online)
{
    const float fadeIn = clampf(resultsTimer_ * 3.0f, 0.0f, 1.0f);
    DrawTexturePro(app_.assets().vignette, {0, 0, 256, 256}, {0, 0, ui.width(), ui.height()}, {0, 0}, 0.0f,
                   draw::withAlpha(WHITE, fadeIn));
    DrawRectangle(0, 0, static_cast<int>(ui.width()) + 1, static_cast<int>(ui.height()) + 1,
                  draw::withAlpha(BLACK, 0.35f * fadeIn));

    const net::RoomState& room = online.room();
    const Rectangle panel = ui.centered(0, -16, 760, 560);
    ui.panel(panel, {44, 52, 74, 245}, {30, 26, 38, 255});
    const float cx = panel.x + panel.width * 0.5f;
    ui.text("RACE RESULTS", {cx, panel.y + ui.px(18)}, ui.px(54), WHITE, Align::Center);
    const StageDef* stage = findStage(room.settings.stage);
    char line[128];
    std::snprintf(line, sizeof(line), "%s  -  %s  -  %s", stage ? stage->name.c_str() : "?",
                  difficultyDef(room.settings.difficulty).name, goalText(room.settings.goal).c_str());
    ui.text(line, {cx, panel.y + ui.px(80)}, ui.px(24), kSoftText, Align::Center);

    const std::vector<uint8_t> order = online.raceOrder();
    for (size_t i = 0; i < order.size(); ++i) {
        const net::Player* p = online.player(order[i]);
        if (!p) continue;
        const int place = static_cast<int>(i) + 1;
        const bool me = p->id == online.myId();
        const Rectangle row{panel.x + ui.px(24), panel.y + ui.px(122.0f + 72.0f * static_cast<float>(i)),
                            panel.width - ui.px(48), ui.px(62)};
        DrawRectangleRounded(row, 0.3f, 10, me ? Color{64, 84, 124, 255} : Color{56, 64, 90, 255});
        ui.text(placeText(place), {row.x + ui.px(46), row.y + ui.px(12)}, ui.px(34), placeColor(place), Align::Center);
        drawAvatar(ui, {row.x + ui.px(122), row.y + row.height * 0.5f}, ui.px(20), p->name, playerColor(p->color));
        ui.text(me ? (p->name + " (YOU)").c_str() : p->name.c_str(), {row.x + ui.px(156), row.y + ui.px(4)}, ui.px(30), WHITE);
        if (const VehicleDef* def = findVehicle(p->vehicle)) {
            ui.text(def->name.c_str(), {row.x + ui.px(156), row.y + ui.px(36)}, ui.px(18), kSoftText);
        }
        const float right = row.x + row.width - ui.px(20);
        if (p->state == net::RacerState::Finished) {
            ui.text(raceTimeText(p->finishTime).c_str(), {right, row.y + ui.px(4)}, ui.px(30), {255, 214, 60, 255},
                    Align::Right);
            ui.text("FINISHED", {right, row.y + ui.px(36)}, ui.px(18), kSoftText, Align::Right);
        } else {
            const std::string metres = formatThousands(static_cast<long long>(online.distanceOf(p->id))) + "m";
            ui.text(p->state == net::RacerState::Left ? "--" : metres.c_str(), {right, row.y + ui.px(4)}, ui.px(30), WHITE,
                    Align::Right);
            const char* word = stateWord(p->state);
            ui.text(word[0] ? word : "TIME'S UP", {right, row.y + ui.px(36)}, ui.px(18), kSoftText, Align::Right);
        }
    }

    const float bottom = panel.y + panel.height;
    if (testDrive_) {
        ui.text("AUTOPILOT - coins not saved", {cx, bottom - ui.px(150)}, ui.px(26), {255, 200, 120, 255}, Align::Center);
    } else {
        std::snprintf(line, sizeof(line), "You earned %s coins", formatThousands(earned_).c_str());
        ui.text(line, {cx, bottom - ui.px(150)}, ui.px(28), {255, 230, 120, 255}, Align::Center);
    }
    if (resultsTimer_ < 0.4f) return;  // ignore inputs held over from driving
    const bool host = online.isHost();
    if (host) {
        ui.setDefaultFocus("again");
        if (ui.button("again", ui.centered(-130, 196, 240, 66), "RACE AGAIN", ButtonKind::Go)) {
            online.backToRoom();
            return;
        }
        if (ui.button("leave", ui.centered(130, 196, 240, 66), "CLOSE ROOM", ButtonKind::Back)) {
            leaving();
            app_.leaveOnline();
        }
    } else {
        const net::Player* h = nullptr;
        for (const net::Player& p : room.players) {
            if (p.host) h = &p;
        }
        const std::string wait = "Waiting for " + (h ? h->name : std::string("the host")) + " to go again...";
        ui.text(wait.c_str(), {cx, bottom - ui.px(114)}, ui.px(22), kSoftText, Align::Center);
        ui.setDefaultFocus("leave");
        if (ui.button("leave", ui.centered(0, 214, 240, 62), "LEAVE", ButtonKind::Back)) {
            leaving();
            app_.leaveOnline();
        }
    }
}

std::string OnlineRaceScreen::telemetry() const
{
    const net::Online* online = app_.online();
    if (!online) return "{}";
    std::string players;
    for (uint8_t id : online->raceOrder()) {
        const net::Player* p = online->player(id);
        if (!p) continue;
        char item[160];
        std::snprintf(item, sizeof(item), "%s{\"id\":%u,\"name\":\"%s\",\"state\":%d,\"distance\":%.1f,\"time\":%.2f}",
                      players.empty() ? "" : ",", p->id, p->name.c_str(), static_cast<int>(p->state),
                      online->distanceOf(p->id), p->finishTime);
        players += item;
    }
    char buf[256];
    std::snprintf(buf, sizeof(buf),
                  "{\"phase\":%d,\"race\":%u,\"raceTime\":%.2f,\"me\":%u,\"distance\":%.1f,\"state\":%d,\"remotes\":%zu,"
                  "\"ping\":%d,\"order\":[",
                  static_cast<int>(online->phase()), race_, raceTime_, online->myId(), run_->session().distance(),
                  static_cast<int>(myState()), remotes_.size(), online->pingMs());
    return std::string(buf) + players + "]}";
}

} // namespace

std::unique_ptr<Screen> makeOnlineRaceScreen(App& app) { return std::make_unique<OnlineRaceScreen>(app); }

} // namespace hr
