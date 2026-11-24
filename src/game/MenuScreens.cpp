#include "core/Math.hpp"
#include "core/Version.hpp"
#include "game/DemoWorld.hpp"
#include "game/Draw.hpp"
#include "game/Hud.hpp"
#include "game/Particles.hpp"
#include "game/Screens.hpp"
#include "game/VehicleArt.hpp"
#include "game/WorldView.hpp"
#include "sim/Bot.hpp"
#include "sim/Session.hpp"

#include <cmath>
#include <cstdio>

namespace hr {

namespace {

const Color kInk{30, 26, 38, 255};
const Color kPanelFill{44, 52, 74, 235};

void dimBackground(Ui& ui, float alpha)
{
    DrawRectangle(0, 0, static_cast<int>(ui.width()) + 1, static_cast<int>(ui.height()) + 1, draw::withAlpha(BLACK, alpha));
}

void drawLock(Vector2 c, float s)
{
    DrawRectangleRounded({c.x - 18 * s, c.y - 4 * s, 36 * s, 30 * s}, 0.25f, 6, kInk);
    DrawRectangleRounded({c.x - 15 * s, c.y - 1 * s, 30 * s, 24 * s}, 0.25f, 6, {250, 204, 60, 255});
    DrawRing({c.x, c.y - 4 * s}, 9 * s, 15 * s, 180.0f, 360.0f, 16, kInk);
    DrawCircleV({c.x, c.y + 10 * s}, 4 * s, kInk);
}

// ---------------------------------------------------------------------------

class TitleScreen : public Screen {
public:
    explicit TitleScreen(App& app) : Screen(app) { app_.audio().setMusicOn(true); }

    const char* name() const override { return "title"; }

    void update(float dt) override
    {
        time_ += dt;
        demo_.update(dt);
        if (app_.input().pressed(Action::Back)) app_.quit();
    }

    void draw() override
    {
        Ui& ui = app_.ui();
        demo_.draw(app_, time_);
        dimBackground(ui, 0.18f);

        // Logo.
        const float bob = std::sin(time_ * 2.0f) * ui.px(6);
        const float cx = ui.width() * 0.5f;
        ui.text("HILL", {cx, ui.px(70) + bob}, ui.px(118), {255, 214, 60, 255}, Align::Center);
        ui.text("RUSH", {cx, ui.px(180) + bob}, ui.px(118), {255, 120, 60, 255}, Align::Center);
        ui.text("a hill-climb driving game", {cx, ui.px(300)}, ui.px(28), WHITE, Align::Center);

        ui.setDefaultFocus("play");
        if (ui.button("play", ui.centered(0, 56, 320, 80), "PLAY", ButtonKind::Go, true, 52)) {
            app_.showStageSelect();
            return;
        }
        if (ui.button("online", ui.centered(0, 144, 320, 64), "ONLINE RACE", ButtonKind::Info, true, 34)) {
            app_.showOnlineMenu();
            return;
        }
        if (ui.button("garage", ui.centered(-205, 228, 190, 60), "GARAGE", ButtonKind::Normal)) {
            app_.showGarage(true);
            return;
        }
        if (ui.button("settings", ui.centered(0, 228, 190, 60), "SETTINGS", ButtonKind::Normal)) {
            app_.showSettings();
            return;
        }
        if (ui.button("quit", ui.centered(205, 228, 190, 60), "QUIT", ButtonKind::Back)) app_.quit();

        drawCoinBalance(app_);
        ui.text("Gas: Right / D      Brake: Left / A      Pause: Esc", {ui.px(20), ui.height() - ui.px(40)}, ui.px(22),
                WHITE, Align::Left);
        char version[32];
        std::snprintf(version, sizeof(version), "v%s", kVersion);
        ui.text(version, {ui.width() - ui.px(20), ui.height() - ui.px(40)}, ui.px(22), WHITE, Align::Right);
    }

private:
    DemoWorld demo_;
    float time_ = 0.0f;
};

// ---------------------------------------------------------------------------

class StageSelectScreen : public Screen {
public:
    explicit StageSelectScreen(App& app) : Screen(app)
    {
        const auto& list = stages();
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].id == app_.progress().selectedStage) selected_ = static_cast<int>(i);
        }
        shown_ = static_cast<float>(selected_);
        app_.audio().setMusicOn(true);
    }

    const char* name() const override { return "stages"; }

    void update(float dt) override
    {
        time_ += dt;
        shown_ = approach(shown_, static_cast<float>(selected_), 10.0f, dt);
        Input& in = app_.input();
        if (in.pressed(Action::Back)) app_.showTitle();
    }

    void draw() override
    {
        Ui& ui = app_.ui();
        const auto& list = stages();
        const StageDef& stage = list[static_cast<size_t>(selected_)];
        Progress& p = app_.progress();
        const bool unlocked = p.stageUnlocked(stage.id);

        DrawRectangleGradientV(0, 0, static_cast<int>(ui.width()) + 1, static_cast<int>(ui.height()) + 1,
                               {54, 112, 190, 255}, {120, 190, 240, 255});
        drawScreenTitle(app_, "SELECT STAGE");
        drawCoinBalance(app_);
        drawDifficultyChip(app_);

        // Cards: the selected one in the middle, neighbours to the sides.
        // The arrows overlap the side cards, so a click on an arrow must not
        // also count as a click on the card underneath it.
        const Rectangle prevArrow = ui.centered(-560, -10, 70, 90);
        const Rectangle nextArrow = ui.centered(560, -10, 70, 90);
        const bool overArrow = ui.hovered(prevArrow) || ui.hovered(nextArrow);
        int clickedCard = -1;
        for (int i = 0; i < static_cast<int>(list.size()); ++i) {
            const float offset = static_cast<float>(i) - shown_;
            if (std::fabs(offset) > 2.2f) continue;
            const float scale = 1.0f - 0.22f * std::fmin(1.0f, std::fabs(offset));
            const Rectangle card = ui.centered(offset * 430.0f, -10, 400 * scale, 330 * scale);
            drawCard(list[static_cast<size_t>(i)], card, scale, i == selected_);
            if (i != selected_ && !overArrow && ui.hovered(card) && app_.input().mouseReleased()) clickedCard = i;
        }
        if (clickedCard >= 0) select(clickedCard);

        // Navigation: Left/Right always page the cards.
        ui.lockHorizontal();
        if (ui.arrowButton("prev", prevArrow, -1, selected_ > 0) || (app_.input().pressed(Action::Left) && selected_ > 0)) {
            select(selected_ - 1);
        }
        if (ui.arrowButton("next-stage", nextArrow, 1, selected_ + 1 < static_cast<int>(list.size())) ||
            (app_.input().pressed(Action::Right) && selected_ + 1 < static_cast<int>(list.size()))) {
            select(selected_ + 1);
        }

        if (ui.button("back", ui.anchored(0.0f, 1.0f, 30, -30, 220, 72), "BACK", ButtonKind::Back)) {
            app_.showTitle();
            return;
        }
        ui.setDefaultFocus(unlocked ? "next" : "unlock");
        if (unlocked) {
            if (ui.button("next", ui.anchored(1.0f, 1.0f, -30, -30, 260, 72), "GARAGE", ButtonKind::Go)) {
                p.selectedStage = stage.id;
                app_.saveProgress();
                app_.showGarage();
                return;
            }
        } else {
            char label[48];
            std::snprintf(label, sizeof(label), "UNLOCK %s", formatThousands(stage.price).c_str());
            const bool affordable = p.coins >= stage.price;
            if (ui.button("unlock", ui.centered(0, 248, 380, 70), label, ButtonKind::Buy, affordable, 32)) {
                if (p.spend(stage.price)) {
                    p.unlockStage(stage.id);
                    p.selectedStage = stage.id;
                    app_.saveProgress();
                    app_.audio().play(Sfx::Buy);
                }
            }
            if (!affordable) {
                std::snprintf(label, sizeof(label), "Need %s more coins", formatThousands(stage.price - p.coins).c_str());
                ui.text(label, {ui.width() * 0.5f, ui.height() * 0.5f + ui.px(292)}, ui.px(22), {255, 220, 200, 255},
                        Align::Center);
            }
        }
    }

private:
    void select(int index)
    {
        selected_ = index;
        app_.audio().play(Sfx::Click);
        const StageDef& stage = stages()[static_cast<size_t>(index)];
        if (app_.progress().stageUnlocked(stage.id)) app_.progress().selectedStage = stage.id;
    }

    void drawCard(const StageDef& stage, Rectangle card, float scale, bool selected)
    {
        Ui& ui = app_.ui();
        Progress& p = app_.progress();
        const bool unlocked = p.stageUnlocked(stage.id);
        ui.panel(card, kPanelFill, selected ? Color{255, 214, 60, 255} : kInk, 0.12f);
        const Rectangle preview{card.x + ui.px(14) * scale, card.y + ui.px(14) * scale, card.width - ui.px(28) * scale,
                                card.height * 0.58f};
        drawStagePreview(stage, preview, time_);
        if (!unlocked) {
            DrawRectangleRec(preview, draw::withAlpha(BLACK, 0.45f));
            drawLock({preview.x + preview.width * 0.5f, preview.y + preview.height * 0.45f}, ui.scale() * 1.6f * scale);
        }
        const float cx = card.x + card.width * 0.5f;
        float y = preview.y + preview.height + ui.px(8) * scale;
        ui.text(stage.name.c_str(), {cx, y}, ui.px(40) * scale, WHITE, Align::Center);
        y += ui.px(46) * scale;
        char line[96];
        if (unlocked) {
            const Difficulty d = p.settings.difficulty;
            std::snprintf(line, sizeof(line), "BEST %dm   LEVEL %d", static_cast<int>(p.bestOnStage(stage.id, d)),
                          p.levelsCompleted(stage.id, d) + 1);
            ui.text(line, {cx, y}, ui.px(24) * scale, {255, 230, 140, 255}, Align::Center);
        } else {
            std::snprintf(line, sizeof(line), "%s coins", formatThousands(stage.price).c_str());
            ui.text(line, {cx, y}, ui.px(26) * scale, {255, 214, 60, 255}, Align::Center);
        }
        if (selected) {
            ui.text(stage.blurb.c_str(), {ui.width() * 0.5f, card.y + card.height + ui.px(14)}, ui.px(24), WHITE,
                    Align::Center);
        }
    }

    int selected_ = 0;
    float shown_ = 0.0f;
    float time_ = 0.0f;
};

// ---------------------------------------------------------------------------

class GarageScreen : public Screen {
public:
    GarageScreen(App& app, bool backToTitle) : Screen(app), backToTitle_(backToTitle)
    {
        const auto& list = vehicles();
        for (size_t i = 0; i < list.size(); ++i) {
            if (list[i].id == app_.progress().selectedVehicle) selected_ = static_cast<int>(i);
        }
        app_.audio().setMusicOn(true);
    }

    const char* name() const override { return "garage"; }

    void update(float dt) override
    {
        time_ += dt;
        flash_ = std::fmax(0.0f, flash_ - dt * 2.0f);
        if (app_.input().pressed(Action::Back)) goBack();
    }

    void draw() override
    {
        Ui& ui = app_.ui();
        Progress& p = app_.progress();
        const auto& list = vehicles();
        const VehicleDef& v = list[static_cast<size_t>(selected_)];
        const bool owned = p.vehicleUnlocked(v.id);
        const StageDef* stage = findStage(p.selectedStage);

        DrawRectangleGradientV(0, 0, static_cast<int>(ui.width()) + 1, static_cast<int>(ui.height()) + 1,
                               {60, 66, 92, 255}, {120, 128, 160, 255});
        drawScreenTitle(app_, "GARAGE");
        drawCoinBalance(app_);
        if (stage) {
            char line[64];
            std::snprintf(line, sizeof(line), "Stage: %s (%s)", stage->name.c_str(),
                          difficultyDef(p.settings.difficulty).name);
            ui.text(line, {ui.px(24), ui.px(24)}, ui.px(26), {255, 230, 140, 255}, Align::Left);
        }

        // Vehicle preview on a turntable.
        const Rectangle stand = ui.centered(0, -60, 560, 250);
        DrawEllipse(static_cast<int>(stand.x + stand.width * 0.5f), static_cast<int>(stand.y + stand.height * 0.86f),
                    stand.width * 0.42f, ui.px(26), draw::withAlpha(BLACK, 0.25f));
        Camera2D cam{};
        cam.target = {0.0f, -0.55f};
        cam.offset = {stand.x + stand.width * 0.5f, stand.y + stand.height * 0.5f};
        cam.zoom = ui.px(78.0f);
        BeginMode2D(cam);
        VehiclePose pose = restPose(v, {0.0f, 0.0f}, 0.0f);
        // Gentle idle bob so the preview feels alive.
        pose.chassis.p.y += std::sin(time_ * 3.0f) * 0.015f;
        pose.head.p.y += std::sin(time_ * 3.0f - 0.6f) * 0.03f;
        drawVehicle(v, pose, time_);
        EndMode2D();
        if (!owned) {
            drawLock({stand.x + stand.width * 0.5f, stand.y + stand.height * 0.40f}, ui.scale() * 2.2f);
        }

        ui.text(v.name.c_str(), {ui.width() * 0.5f, stand.y - ui.px(66)}, ui.px(50), WHITE, Align::Center);
        ui.text(v.blurb.c_str(), {ui.width() * 0.5f, stand.y + stand.height - ui.px(14)}, ui.px(24),
                {220, 226, 240, 255}, Align::Center);

        if (ui.arrowButton("prev", ui.centered(-380, -60, 70, 90), -1, selected_ > 0)) select(selected_ - 1);
        if (ui.arrowButton("next-vehicle", ui.centered(380, -60, 70, 90), 1, selected_ + 1 < static_cast<int>(list.size()))) {
            select(selected_ + 1);
        }
        // Keyboard: Left/Right page through vehicles unless an upgrade is focused.
        if (!upgradesFocused()) {
            ui.lockHorizontal();
            if (app_.input().pressed(Action::Left) && selected_ > 0) select(selected_ - 1);
            if (app_.input().pressed(Action::Right) && selected_ + 1 < static_cast<int>(list.size())) select(selected_ + 1);
        }

        if (owned) drawUpgrades(v);
        else {
            char label[48];
            std::snprintf(label, sizeof(label), "BUY %s", formatThousands(v.price).c_str());
            ui.setDefaultFocus("buy");
            const bool affordable = p.coins >= v.price;
            if (ui.button("buy", ui.centered(0, 180, 360, 76), label, ButtonKind::Buy, affordable, 34)) {
                if (p.spend(v.price)) {
                    p.unlockVehicle(v.id);
                    p.selectedVehicle = v.id;
                    app_.saveProgress();
                    app_.audio().play(Sfx::Buy);
                    flash_ = 1.0f;
                }
            }
            if (!affordable) {
                std::snprintf(label, sizeof(label), "Need %s more coins", formatThousands(v.price - p.coins).c_str());
                ui.text(label, {ui.width() * 0.5f, ui.height() * 0.5f + ui.px(226)}, ui.px(22), {255, 220, 200, 255},
                        Align::Center);
            }
        }

        if (ui.button("back", ui.anchored(0.0f, 1.0f, 30, -24, 200, 70), "BACK", ButtonKind::Back)) {
            goBack();
            return;
        }
        if (owned) {
            ui.setDefaultFocus("start");
            if (ui.button("start", ui.anchored(1.0f, 1.0f, -30, -24, 240, 70), "START", ButtonKind::Go, true, 40)) {
                p.selectedVehicle = v.id;
                app_.saveProgress();
                app_.startRun();
                return;
            }
        }
        if (flash_ > 0.0f) {
            DrawRectangle(0, 0, static_cast<int>(ui.width()) + 1, static_cast<int>(ui.height()) + 1,
                          draw::withAlpha(WHITE, flash_ * 0.4f));
        }
    }

private:
    void goBack()
    {
        if (backToTitle_) app_.showTitle();
        else app_.showStageSelect();
    }

    bool upgradesFocused() const
    {
        const Ui& ui = app_.ui();
        for (int i = 0; i < 4; ++i) {
            char id[16];
            std::snprintf(id, sizeof(id), "up%d", i);
            if (ui.focused(id)) return true;
        }
        return false;
    }

    void select(int index)
    {
        selected_ = index;
        app_.audio().play(Sfx::Click);
        const VehicleDef& v = vehicles()[static_cast<size_t>(index)];
        if (app_.progress().vehicleUnlocked(v.id)) app_.progress().selectedVehicle = v.id;
    }

    void drawUpgrades(const VehicleDef& v)
    {
        Ui& ui = app_.ui();
        Progress& p = app_.progress();
        const UpgradeLevels levels = p.upgrades(v.id);
        for (int i = 0; i < 4; ++i) {
            const UpgradeSlot& slot = v.upgrades[static_cast<size_t>(i)];
            const int level = levels[static_cast<size_t>(i)];
            const Rectangle card = ui.centered(-411 + 274.0f * static_cast<float>(i), 186, 256, 150);
            ui.panel(card, kPanelFill, kInk, 0.15f);
            const float cx = card.x + card.width * 0.5f;
            ui.text(upgradeName(slot.kind), {cx, card.y + ui.px(8)}, ui.px(28), WHITE, Align::Center);
            // Level pips.
            const float pipW = (card.width - ui.px(30)) / static_cast<float>(slot.maxLevel);
            for (int l = 0; l < slot.maxLevel; ++l) {
                const Rectangle pip{card.x + ui.px(15) + pipW * static_cast<float>(l) + 1, card.y + ui.px(46),
                                    pipW - 2, ui.px(14)};
                DrawRectangleRec(pip, l < level ? Color{110, 210, 60, 255} : Color{70, 74, 92, 255});
            }
            char label[48];
            std::snprintf(label, sizeof(label), "LV %d/%d", level, slot.maxLevel);
            ui.text(label, {cx, card.y + ui.px(64)}, ui.px(22), {200, 210, 230, 255}, Align::Center);

            char id[16];
            std::snprintf(id, sizeof(id), "up%d", i);
            const Rectangle buy{card.x + ui.px(16), card.y + card.height - ui.px(52), card.width - ui.px(32), ui.px(44)};
            if (level >= slot.maxLevel) {
                ui.button(id, buy, "MAX", ButtonKind::Info, false, 26);
                continue;
            }
            const long long cost = upgradeCost(slot, level + 1);
            std::snprintf(label, sizeof(label), "%s", formatThousands(cost).c_str());
            const bool affordable = p.coins >= cost;
            if (ui.button(id, buy, label, ButtonKind::Buy, affordable, 26) && p.spend(cost)) {
                p.setUpgrade(v.id, i, level + 1);
                app_.saveProgress();
                app_.audio().play(Sfx::Buy);
            }
        }
    }

    int selected_ = 0;
    float time_ = 0.0f;
    float flash_ = 0.0f;
    bool backToTitle_ = false;
};

// ---------------------------------------------------------------------------

class SettingsScreen : public Screen {
public:
    explicit SettingsScreen(App& app) : Screen(app) {}

    const char* name() const override { return "settings"; }

    void update(float dt) override
    {
        resetArmed_ = std::fmax(0.0f, resetArmed_ - dt);
        if (app_.input().pressed(Action::Back)) close();
    }

    void draw() override
    {
        Ui& ui = app_.ui();
        Settings& s = app_.progress().settings;
        DrawRectangleGradientV(0, 0, static_cast<int>(ui.width()) + 1, static_cast<int>(ui.height()) + 1,
                               {54, 112, 190, 255}, {120, 190, 240, 255});
        drawScreenTitle(app_, "SETTINGS");
        const Rectangle panel = ui.centered(0, 30, 800, 560);
        ui.panel(panel, kPanelFill, kInk, 0.08f);

        float y = -205.0f;
        auto label = [&](const char* text) {
            ui.text(text, {panel.x + ui.px(50), ui.height() * 0.5f + ui.px(y - 18)}, ui.px(32), WHITE, Align::Left);
        };

        // Difficulty: three buttons, the active one in green.
        label("Difficulty");
        for (const DifficultyDef& d : difficulties()) {
            const float x = 30.0f + 150.0f * static_cast<float>(static_cast<int>(d.id));
            const bool active = s.difficulty == d.id;
            if (ui.button(d.key, ui.centered(x, y, 140, 52), d.name, active ? ButtonKind::Go : ButtonKind::Info, true,
                          26) && !active) {
                s.difficulty = d.id;
                app_.saveProgress();
            }
        }
        ui.text(difficultyDef(s.difficulty).blurb, {panel.x + ui.px(50), ui.height() * 0.5f + ui.px(y + 34)},
                ui.px(22), difficultyColor(s.difficulty), Align::Left);
        y += 100.0f;

        ui.setDefaultFocus("master");
        label("Master volume");
        const float master = ui.slider("master", ui.centered(170, y, 300, 34), s.masterVolume);
        y += 64.0f;
        label("Sound effects");
        const float sfx = ui.slider("sfx", ui.centered(170, y, 300, 34), s.sfxVolume);
        y += 64.0f;
        label("Music");
        const float music = ui.slider("music", ui.centered(170, y, 300, 34), s.musicVolume);
        y += 64.0f;
        if (master != s.masterVolume || sfx != s.sfxVolume || music != s.musicVolume) {
            s.masterVolume = master;
            s.sfxVolume = sfx;
            s.musicVolume = music;
            app_.applySettings();
        }
        label("Fullscreen");
        // Show the real window state (a --windowed launch can differ from the saved setting).
        const bool isFull = app_.isFullscreen();
        if (ui.toggle("fullscreen", ui.centered(60, y, 70, 38), isFull) != isFull) app_.toggleFullscreen();
        y += 64.0f;
        label("Show FPS");
        s.showFps = ui.toggle("fps", ui.centered(60, y, 70, 38), s.showFps);
        y += 78.0f;

        const char* resetLabel = resetArmed_ > 0.0f ? "REALLY RESET?" : "RESET PROGRESS";
        if (ui.button("reset", ui.centered(-170, y, 320, 60), resetLabel, ButtonKind::Back, true, 28)) {
            if (resetArmed_ > 0.0f) {
                const Settings keep = s;
                app_.progress() = Progress();
                app_.progress().settings = keep;
                app_.saveProgress();
                resetArmed_ = 0.0f;
                app_.audio().play(Sfx::GameOver);
            } else {
                resetArmed_ = 3.0f;
            }
        }
        if (ui.button("done", ui.centered(170, y, 260, 60), "DONE", ButtonKind::Go)) close();
    }

private:
    void close()
    {
        app_.saveProgress();
        app_.showTitle();
    }

    float resetArmed_ = 0.0f;
};

} // namespace

void drawCoinBalance(App& app)
{
    Ui& ui = app.ui();
    const std::string text = formatThousands(app.progress().coins);
    const float size = ui.px(36);
    const Vector2 dim = measureText(app.assets(), text.c_str(), size);
    const float right = ui.width() - ui.px(24);
    const Rectangle pill{right - dim.x - ui.px(70), ui.px(18), dim.x + ui.px(70), ui.px(54)};
    DrawRectangleRounded({pill.x - ui.px(3), pill.y - ui.px(3), pill.width + ui.px(6), pill.height + ui.px(6)}, 1.0f, 12, kInk);
    DrawRectangleRounded(pill, 1.0f, 12, {40, 44, 62, 230});
    const Vector2 c{pill.x + ui.px(28), pill.y + pill.height * 0.5f};
    DrawCircleV(c, ui.px(17), kInk);
    DrawCircleV(c, ui.px(15), {214, 150, 20, 255});
    DrawCircleV(c, ui.px(11), {255, 204, 40, 255});
    ui.text(text.c_str(), {right - ui.px(10), pill.y + (pill.height - dim.y) * 0.5f}, size, {255, 230, 120, 255},
            Align::Right);
}

void drawDifficultyChip(App& app)
{
    Ui& ui = app.ui();
    const Difficulty d = app.progress().settings.difficulty;
    char text[48];
    std::snprintf(text, sizeof(text), "DIFFICULTY: %s", difficultyDef(d).name);
    ui.text(text, {ui.width() * 0.5f, ui.px(96)}, ui.px(24), difficultyColor(d), Align::Center);
}

void drawScreenTitle(App& app, const char* title)
{
    Ui& ui = app.ui();
    ui.text(title, {ui.width() * 0.5f, ui.px(26)}, ui.px(64), WHITE, Align::Center);
}

std::unique_ptr<Screen> makeTitleScreen(App& app) { return std::make_unique<TitleScreen>(app); }
std::unique_ptr<Screen> makeStageSelectScreen(App& app) { return std::make_unique<StageSelectScreen>(app); }
std::unique_ptr<Screen> makeGarageScreen(App& app, bool backToTitle)
{
    return std::make_unique<GarageScreen>(app, backToTitle);
}
std::unique_ptr<Screen> makeSettingsScreen(App& app) { return std::make_unique<SettingsScreen>(app); }

} // namespace hr
