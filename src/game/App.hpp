#pragma once

#include "game/Assets.hpp"
#include "game/Audio.hpp"
#include "game/Input.hpp"
#include "game/Ui.hpp"
#include "sim/Progress.hpp"

#include <memory>
#include <string>

namespace hr {

class Automation;
namespace net {
class Online;
}

struct LaunchOptions {
    std::string dataDir;        // save location override
    std::string playStage;      // skip the menus and start a run
    std::string playVehicle;
    bool autoplay = false;      // the bot drives (and restarts runs)
    bool stunts = false;        // ...and tries flips on big jumps
    std::string difficulty;     // overrides the saved difficulty setting ("easy"/"medium"/"hard")
    std::string script;         // automation script for scripted play-tests
    std::string screenshotDir;  // where scripted screenshots go
    std::string telemetry;      // JSON-lines telemetry output
    int width = 0;
    int height = 0;
    bool fullscreen = false;
    bool windowed = false;
    bool mute = false;
    bool fixedStep = false;     // exactly one physics step per frame (deterministic)
    int maxFrames = 0;          // quit after this many frames (0 = never)
    std::string playerName;     // online name (saved)
    bool hostRoom = false;      // start hosting an online room straight away
    std::string joinAddress;    // join the online room at this address straight away
};

class Screen {
public:
    explicit Screen(class App& app) : app_(app) {}
    virtual ~Screen() = default;

    virtual const char* name() const = 0;
    virtual void update(float dt) = 0;  // once per rendered frame
    virtual void fixedUpdate() {}       // once per physics step
    virtual void draw() = 0;
    // One line of JSON describing the screen's state (for telemetry).
    virtual std::string telemetry() const { return "{}"; }

protected:
    class App& app_;
};

class App {
public:
    explicit App(LaunchOptions options);
    ~App();
    App(const App&) = delete;
    App& operator=(const App&) = delete;

    int run();

    Assets& assets() { return assets_; }
    Audio& audio() { return audio_; }
    Input& input() { return input_; }
    Ui& ui() { return ui_; }
    Progress& progress() { return progress_; }
    const LaunchOptions& options() const { return options_; }

    void saveProgress();
    void applySettings();
    void switchTo(std::unique_ptr<Screen> next);
    void quit() { quit_ = true; }
    void toggleFullscreen();
    bool isFullscreen() const { return fullscreen_; }
    // The window's actual size as the window system reports it.
    void realWindowSize(int& width, int& height) const;

    // Saves a PNG of the current frame. Relative names go to the screenshot dir.
    void requestScreenshot(const std::string& name);

    float time() const { return time_; }
    float interpolation() const { return alpha_; }
    long frame() const { return frame_; }
    const Screen* screen() const { return screen_.get(); }

    // Navigation
    void showTitle();
    void showStageSelect();
    void showGarage(bool backToTitle = false);
    void showSettings();
    void startRun();

    // Online races ----------------------------------------------------------
    // The room this player is in (null when not online). Serviced every frame.
    net::Online* online() { return online_.get(); }
    // Opens a room on this computer, or starts joining one. False (with `error`) on failure.
    bool hostOnline(std::string& error);
    bool joinOnline(const std::string& address, std::string& error);
    // Leaves the room and shows the online menu, with `message` (e.g. why the room closed).
    void leaveOnline(std::string message = "");  // (a copy: it may come from the room being closed)
    // The name used online: the saved one, or one made from the computer's user name.
    std::string playerName() const;
    void showOnlineMenu(const std::string& message = "");
    void showRoom();
    void showOnlineRace();

private:
    void initWindow();
    void frameStep();
    void captureScreenshot();

    LaunchOptions options_;
    Assets assets_;
    Audio audio_;
    Input input_;
    Ui ui_;
    Progress progress_;
    std::unique_ptr<net::Online> online_;  // before the screens: they talk to it as they close
    std::unique_ptr<Screen> screen_;
    std::unique_ptr<Screen> pending_;
    std::unique_ptr<Automation> automation_;
    std::string savePath_;
    std::string pendingScreenshot_;
    bool quit_ = false;
    float time_ = 0.0f;
    float accumulator_ = 0.0f;
    float alpha_ = 0.0f;
    float fade_ = 1.0f;
    long frame_ = 0;
    void syncWindowSize();

    // Window geometry to restore when leaving fullscreen.
    bool fullscreen_ = false;
    int windowedX_ = 0;
    int windowedY_ = 0;
    int windowedW_ = 0;
    int windowedH_ = 0;
    int sizeMismatchFrames_ = 0;
    int restoreFrames_ = 0;  // countdown to re-asserting the windowed geometry
};

} // namespace hr
