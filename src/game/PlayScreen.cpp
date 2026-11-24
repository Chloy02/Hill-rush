#include "core/Log.hpp"
#include "core/Math.hpp"
#include "game/Draw.hpp"
#include "game/Hud.hpp"
#include "game/LiveRun.hpp"
#include "game/Screens.hpp"
#include "sim/Bot.hpp"
#include "sim/Session.hpp"

#include <cmath>
#include <cstdio>

namespace hr {

namespace {

class PlayScreen : public Screen {
public:
    PlayScreen(App& app, const StageDef& stage, const VehicleDef& vehicle)
        : Screen(app), stage_(stage), vehicle_(vehicle)
    {
        // The autopilot, and --play with content the player hasn't bought, are
        // test drives: fully playable, but nothing is banked into the save.
        const Progress& p = app_.progress();
        testDrive_ = app_.options().autoplay || !p.stageUnlocked(stage.id) || !p.vehicleUnlocked(vehicle.id);
        restart();
        app_.audio().setMusicDuck(0.45f);
    }

    ~PlayScreen() override
    {
        bank();
        app_.audio().setEngine(false, 0.0f, 0.0f);
        app_.audio().setMusicDuck(1.0f);
    }

    const char* name() const override
    {
        if (showResults_) return "results";
        return paused_ ? "pause" : "play";
    }

    void update(float dt) override;
    void fixedUpdate() override;
    void draw() override;
    std::string telemetry() const override;

private:
    void restart();
    void bank();
    void drawPause();
    void drawResults();
    void drawTutorial();

    const StageDef& stage_;
    const VehicleDef& vehicle_;
    std::unique_ptr<LiveRun> run_;
    Bot bot_;
    InputState input_;
    bool paused_ = false;
    bool banked_ = false;
    bool showResults_ = false;
    bool debug_ = false;
    float resultsTimer_ = 0.0f;  // time since the results panel appeared
    float endDelay_ = 0.0f;      // time since the run ended
    float time_ = 0.0f;
    RunResult final_;
    bool recordBroken_ = false;
    bool tutorial_ = false;  // first ever run: explain the controls
    Difficulty difficulty_ = Difficulty::Easy;  // fixed for the run, even if Settings change
    bool testDrive_ = false;  // nothing gets saved
};

void PlayScreen::restart()
{
    bank();
    Progress& p = app_.progress();
    tutorial_ = p.stats.runs == 0 && !app_.options().autoplay;
    difficulty_ = p.settings.difficulty;
    run_ = std::make_unique<LiveRun>(app_, stage_, difficulty_, vehicle_, p.upgrades(vehicle_.id),
                                     p.best(stage_.id, vehicle_.id, difficulty_),
                                     p.levelsCompleted(stage_.id, difficulty_));
    bot_ = Bot();
    bot_.setStunts(app_.options().stunts);
    input_ = {};
    paused_ = false;
    banked_ = false;
    showResults_ = false;
    resultsTimer_ = 0.0f;
    endDelay_ = 0.0f;
    recordBroken_ = false;
    char goal[64];
    std::snprintf(goal, sizeof(goal), "LEVEL %d: REACH %dm", run_->session().levelsCompleted() + 1,
                  static_cast<int>(run_->session().nextLevelDistance()));
    run_->hud().shout(goal, {255, 236, 140, 255}, 2.2f);
}

void PlayScreen::bank()
{
    if (banked_ || !run_) return;
    banked_ = true;
    const RunResult& r = run_->session().result();
    final_ = r;
    // A run abandoned on the start line isn't worth a stats entry.
    if (r.distance < 5.0f && r.totalCoins() == 0) return;
    if (testDrive_) {
        LOG_INFO("Test drive over: %.0fm, %lld coins (not saved)", r.distance, r.totalCoins());
        return;
    }
    Progress& p = app_.progress();
    const float previousBest = p.best(stage_.id, vehicle_.id, difficulty_);
    p.coins += r.totalCoins();
    p.recordDistance(stage_.id, vehicle_.id, r.distance, difficulty_);
    p.setLevelsCompleted(stage_.id, run_->session().levelsCompleted(), difficulty_);
    p.stats.runs++;
    p.stats.totalDistance += r.distance;
    p.stats.totalCoins += r.totalCoins();
    p.stats.flips += r.flips();
    app_.saveProgress();
    final_ = r;
    recordBroken_ = r.distance > previousBest && previousBest > 0.0f;
    LOG_INFO("Banked run: %.0fm, +%lld coins (balance %lld)", r.distance, r.totalCoins(), p.coins);
}

void PlayScreen::update(float dt)
{
    Input& in = app_.input();
    Ui& ui = app_.ui();
    time_ += dt;

    if (in.pressed(Action::Debug)) debug_ = !debug_;

    // Auto-pause when the window loses focus (not once the run is over:
    // the results screen takes over from there).
    const Session& session = run_->session();
    const bool runOver = session.isOver();
    if (!IsWindowFocused() && !runOver && !app_.options().autoplay && app_.options().script.empty()) paused_ = true;

    if (!runOver) {
        const bool pauseClicked = in.mousePressed() && CheckCollisionPointRec(in.mouse(), Hud::pauseButton(ui));
        if (in.pressed(Action::Pause) || pauseClicked) {
            paused_ = !paused_;
            app_.audio().play(Sfx::Click);
        }
    }

    // Pedals: keyboard/gamepad plus the on-screen pedals (mouse/touch).
    float gas = in.gas();
    float brake = in.brake();
    if (in.mouseDown() && !paused_) {
        if (CheckCollisionPointRec(in.mouse(), Hud::gasPedal(ui))) gas = 1.0f;
        if (CheckCollisionPointRec(in.mouse(), Hud::brakePedal(ui))) brake = 1.0f;
    }
    if (!app_.options().autoplay) input_ = {gas, brake};

    if (!paused_) run_->update(dt);

    const bool engineRunning = !paused_ && !session.isOver() && session.state() == RunState::Running;
    app_.audio().setEngine(engineRunning, session.engineRpm(), session.throttle());

    if (runOver && !showResults_) {
        endDelay_ += dt;
        if (endDelay_ > 0.5f) {
            showResults_ = true;
            paused_ = false;
            resultsTimer_ = 0.0f;  // drives the fade-in and the input guard below
            bank();
            app_.audio().play(recordBroken_ ? Sfx::Fanfare : Sfx::GameOver);
            ui.setFocus("retry");
        }
    }

    if (showResults_) {
        resultsTimer_ += dt;
        if (in.pressed(Action::Restart)) restart();
        if (app_.options().autoplay && resultsTimer_ > 3.0f) restart();
    } else if (paused_ && in.pressed(Action::Restart)) {
        restart();
    }
}

void PlayScreen::fixedUpdate()
{
    if (paused_ || showResults_ || run_->session().isOver()) return;
    if (app_.options().autoplay) input_ = bot_.think(run_->session());
    run_->step(input_);
}

void PlayScreen::draw()
{
    Ui& ui = app_.ui();
    run_->drawWorld(time_, paused_ || showResults_, debug_);

    if (!showResults_) {
        Input& in = app_.input();
        const bool gasDown = input_.gas > 0.0f;
        const bool brakeDown = input_.brake > 0.0f;
        (void)in;
        run_->hud().draw(run_->session(), ui, gasDown, brakeDown, true);
        if (tutorial_) drawTutorial();
        if (testDrive_) {
            ui.text(app_.options().autoplay ? "AUTOPILOT - TEST DRIVE" : "TEST DRIVE - progress not saved",
                    {ui.px(20), ui.px(118)}, ui.px(22), {255, 200, 120, 255}, Align::Left);
        }
    }
    if (debug_) {
        char line[160];
        const Vehicle& v = run_->session().vehicle();
        std::snprintf(line, sizeof(line), "x=%.1f y=%.1f v=%.1fm/s ang=%.2f spin=%.2f rpm=%.2f particles=%zu",
                      v.position().x, v.position().y, v.speed(), v.angle(), v.angularVelocity(),
                      run_->session().engineRpm(), run_->particles().count());
        ui.text(line, {ui.px(16), ui.height() - ui.px(200)}, ui.px(20), WHITE, Align::Left);
    }
    if (showResults_) drawResults();
    else if (paused_) drawPause();
}

void PlayScreen::drawTutorial()
{
    // Fades out after the first stretch of road.
    const float t = run_->session().time();
    const float alpha = clampf(std::fmin((14.0f - t) / 2.0f, (70.0f - run_->session().distance()) / 15.0f), 0.0f, 1.0f);
    if (alpha <= 0.0f) return;
    Ui& ui = app_.ui();
    const Rectangle r = ui.anchored(0.5f, 1.0f, 0, -150, 760, 112);
    DrawRectangleRounded(r, 0.3f, 8, draw::withAlpha({20, 22, 34, 255}, 0.75f * alpha));
    const Color white = draw::withAlpha(WHITE, alpha);
    const Color gold = draw::withAlpha({255, 214, 60, 255}, alpha);
    ui.text("Hold RIGHT (or D) to drive  -  LEFT (or A) brakes and reverses", {r.x + r.width * 0.5f, r.y + ui.px(14)},
            ui.px(26), white, Align::Center);
    ui.text("In the air: RIGHT tips you back, LEFT tips you forward", {r.x + r.width * 0.5f, r.y + ui.px(46)},
            ui.px(24), gold, Align::Center);
    ui.text("Grab fuel cans and never land on your head!", {r.x + r.width * 0.5f, r.y + ui.px(76)}, ui.px(22), white,
            Align::Center);
}

void PlayScreen::drawPause()
{
    Ui& ui = app_.ui();
    DrawRectangle(0, 0, static_cast<int>(ui.width()) + 1, static_cast<int>(ui.height()) + 1, draw::withAlpha(BLACK, 0.5f));
    const Rectangle panel = ui.centered(0, 0, 420, 430);
    ui.panel(panel, {44, 52, 74, 245}, {30, 26, 38, 255});
    ui.text("PAUSED", {panel.x + panel.width * 0.5f, panel.y + ui.px(24)}, ui.px(54), WHITE, Align::Center);
    ui.setDefaultFocus("resume");
    if (ui.button("resume", ui.centered(0, -60, 300, 64), "RESUME", ButtonKind::Go)) paused_ = false;
    if (ui.button("restart", ui.centered(0, 16, 300, 64), "RESTART", ButtonKind::Normal)) restart();
    if (ui.button("garage", ui.centered(0, 92, 300, 64), "GARAGE", ButtonKind::Info)) {
        app_.showGarage();
        return;
    }
    if (ui.button("menu", ui.centered(0, 168, 300, 64), "MAIN MENU", ButtonKind::Back)) app_.showTitle();
}

void PlayScreen::drawResults()
{
    Ui& ui = app_.ui();
    const float fadeIn = clampf(resultsTimer_ * 3.0f, 0.0f, 1.0f);
    DrawTexturePro(app_.assets().vignette, {0, 0, 256, 256}, {0, 0, ui.width(), ui.height()}, {0, 0}, 0.0f,
                   draw::withAlpha(WHITE, fadeIn));
    DrawRectangle(0, 0, static_cast<int>(ui.width()) + 1, static_cast<int>(ui.height()) + 1,
                  draw::withAlpha(BLACK, 0.35f * fadeIn));

    const Rectangle panel = ui.centered(0, -10, 620, 560);
    ui.panel(panel, {44, 52, 74, 245}, {30, 26, 38, 255});
    const float cx = panel.x + panel.width * 0.5f;
    const RunResult& r = final_;
    const bool crashed = r.reason == EndReason::DriverDown;
    ui.text(crashed ? "DRIVER DOWN" : "OUT OF FUEL", {cx, panel.y + ui.px(18)}, ui.px(56),
            crashed ? Color{255, 96, 80, 255} : Color{255, 170, 70, 255}, Align::Center);

    char line[128];
    std::snprintf(line, sizeof(line), "%dm", static_cast<int>(r.distance));
    ui.text(line, {cx, panel.y + ui.px(84)}, ui.px(72), WHITE, Align::Center);
    std::snprintf(line, sizeof(line), "%s  -  %s  -  %s", stage_.name.c_str(), vehicle_.name.c_str(),
                  difficultyDef(difficulty_).name);
    ui.text(line, {cx, panel.y + ui.px(160)}, ui.px(24), {200, 210, 230, 255}, Align::Center);
    if (testDrive_) {
        ui.text("TEST DRIVE - progress not saved", {cx, panel.y + ui.px(194)}, ui.px(26), {255, 200, 120, 255},
                Align::Center);
    } else if (recordBroken_) {
        const float pulse = 1.0f + 0.06f * std::sin(time_ * 6.0f);
        ui.text("NEW RECORD!", {cx, panel.y + ui.px(190)}, ui.px(36) * pulse, {255, 214, 60, 255}, Align::Center);
    } else {
        std::snprintf(line, sizeof(line), "BEST %dm",
                      static_cast<int>(app_.progress().best(stage_.id, vehicle_.id, difficulty_)));
        ui.text(line, {cx, panel.y + ui.px(194)}, ui.px(28), {255, 230, 140, 255}, Align::Center);
    }

    float y = panel.y + ui.px(244);
    auto row = [&](const char* label, const std::string& value, Color color) {
        ui.text(label, {panel.x + ui.px(60), y}, ui.px(28), {220, 226, 240, 255}, Align::Left);
        ui.text(value.c_str(), {panel.x + panel.width - ui.px(60), y}, ui.px(28), color, Align::Right);
        y += ui.px(38);
    };
    row("Coins collected", "+" + formatThousands(r.pickupCoins), {255, 230, 120, 255});
    std::snprintf(line, sizeof(line), "Tricks  (air x%d  flips x%d  neck x%d)", r.airTimes, r.flips(), r.neckFlips);
    row(line, "+" + formatThousands(r.bonusCoins), {150, 220, 255, 255});
    if (r.levelCoins > 0) {
        std::snprintf(line, sizeof(line), "Level rewards  (x%d)", r.levelsReached);
        row(line, "+" + formatThousands(r.levelCoins), {150, 240, 130, 255});
    }
    DrawLineEx({panel.x + ui.px(50), y + ui.px(4)}, {panel.x + panel.width - ui.px(50), y + ui.px(4)}, ui.px(3),
               {90, 100, 130, 255});
    y += ui.px(14);
    ui.text("TOTAL", {panel.x + ui.px(60), y}, ui.px(36), WHITE, Align::Left);
    ui.text(("+" + formatThousands(r.totalCoins())).c_str(), {panel.x + panel.width - ui.px(60), y}, ui.px(36),
            {255, 214, 60, 255}, Align::Right);

    if (resultsTimer_ < 0.4f) return;  // ignore inputs held over from driving
    ui.setDefaultFocus("retry");
    if (ui.button("garage", ui.centered(-200, 222, 180, 66), "GARAGE", ButtonKind::Info)) {
        app_.showGarage();
        return;
    }
    if (ui.button("retry", ui.centered(0, 222, 180, 66), "RETRY", ButtonKind::Go)) {
        restart();
        return;
    }
    if (ui.button("menu", ui.centered(200, 222, 180, 66), "MENU", ButtonKind::Back)) app_.showTitle();
}

std::string PlayScreen::telemetry() const
{
    const Session& session = run_->session();
    const Vehicle& v = session.vehicle();
    char buf[320];
    const char* state = session.state() == RunState::Running ? "running"
                        : session.state() == RunState::Crashed ? "crashed" : "outoffuel";
    std::snprintf(buf, sizeof(buf),
                  "{\"stage\":\"%s\",\"vehicle\":\"%s\",\"x\":%.1f,\"y\":%.1f,\"speed\":%.1f,\"angle\":%.2f,"
                  "\"fuel\":%.2f,\"coins\":%lld,\"distance\":%.1f,\"run\":\"%s\",\"airborne\":%s,\"paused\":%s,"
                  "\"particles\":%zu}",
                  stage_.id.c_str(), vehicle_.id.c_str(), v.position().x, v.position().y, v.speed(), v.angle(),
                  session.fuel(), session.coins(), session.distance(), state, session.airborne() ? "true" : "false",
                  paused_ ? "true" : "false", run_->particles().count());
    return buf;
}

} // namespace

std::unique_ptr<Screen> makePlayScreen(App& app, const std::string& stageId, const std::string& vehicleId)
{
    const StageDef* stage = findStage(stageId);
    const VehicleDef* vehicle = findVehicle(vehicleId);
    if (!stage) stage = &stages().front();
    if (!vehicle) vehicle = &vehicles().front();
    return std::make_unique<PlayScreen>(app, *stage, *vehicle);
}

} // namespace hr
