#include "game/App.hpp"

#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "core/Version.hpp"
#include "game/Automation.hpp"
#include "game/Platform.hpp"
#include "game/Screens.hpp"
#include "net/Online.hpp"
#include "sim/Session.hpp"

#include <raylib.h>
#include <rlgl.h>

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <ctime>

namespace hr {

namespace {

void raylibLog(int level, const char* text, va_list args)
{
    char message[512];
    std::vsnprintf(message, sizeof(message), text, args);
    if (level >= LOG_ERROR) LOG_ERROR("raylib: %s", message);
    else if (level >= LOG_WARNING) LOG_WARN("raylib: %s", message);
    else LOG_DEBUG("raylib: %s", message);
}

std::string timestampName()
{
    const std::time_t now = std::time(nullptr);
    char buf[64];
    std::strftime(buf, sizeof(buf), "hillrush-%Y%m%d-%H%M%S", std::localtime(&now));
    return buf;
}

} // namespace

App::App(LaunchOptions options) : options_(std::move(options))
{
    if (!options_.dataDir.empty()) paths::setDataDir(options_.dataDir);
    log::setFile((paths::dataDir() + "/hillrush.log").c_str());
    savePath_ = paths::dataDir() + "/save.txt";
    if (!progress_.load(savePath_)) LOG_INFO("No save yet; starting fresh (%s)", savePath_.c_str());
    if (!options_.difficulty.empty()) progress_.settings.difficulty = difficultyFromKey(options_.difficulty);
    if (!options_.playerName.empty()) progress_.settings.playerName = net::sanitizeName(options_.playerName);
}

App::~App() = default;

void App::initWindow()
{
    SetTraceLogCallback(raylibLog);
    SetTraceLogLevel(LOG_WARNING);
    unsigned int flags = FLAG_MSAA_4X_HINT | FLAG_WINDOW_RESIZABLE;
    if (!options_.fixedStep) flags |= FLAG_VSYNC_HINT;
    SetConfigFlags(flags);

    const bool explicitSize = options_.width > 0 && options_.height > 0;
    InitWindow(explicitSize ? options_.width : 1280, explicitSize ? options_.height : 720, kGameName);
    if (!explicitSize) {
        // 80% of the monitor at 16:9, centred.
        const int monitor = GetCurrentMonitor();
        const int mw = GetMonitorWidth(monitor);
        const int mh = GetMonitorHeight(monitor);
        if (mw > 0 && mh > 0) {
            int w = static_cast<int>(static_cast<float>(mw) * 0.8f);
            int h = w * 9 / 16;
            if (h > static_cast<int>(static_cast<float>(mh) * 0.8f)) {
                h = static_cast<int>(static_cast<float>(mh) * 0.8f);
                w = h * 16 / 9;
            }
            w = std::max(w, 960);
            h = std::max(h, 540);
            SetWindowSize(w, h);
            const Vector2 origin = GetMonitorPosition(monitor);
            SetWindowPosition(static_cast<int>(origin.x) + (mw - w) / 2, static_cast<int>(origin.y) + (mh - h) / 2);
        }
    }
    SetWindowMinSize(800, 450);
    SetExitKey(KEY_NULL);  // Esc pauses / goes back instead of quitting

    if (options_.fixedStep) {
        SetTargetFPS(0);
    } else {
        const int refresh = GetMonitorRefreshRate(GetCurrentMonitor());
        SetTargetFPS(refresh >= 30 ? refresh : 60);
    }
}

int App::run()
{
    // Load the automation script first so a bad script fails before any
    // window or audio thread exists.
    if (!options_.script.empty() || !options_.telemetry.empty()) {
        automation_ = std::make_unique<Automation>(*this, options_.script, options_.telemetry);
        if (!automation_->ok()) return 2;
    }

    initWindow();
    if (!IsWindowReady()) {
        LOG_ERROR("Could not create the game window (needs OpenGL 3.3)");
        platform::showFatalError(kGameName,
                                 "Hill Rush could not open its window.\n\n"
                                 "It needs a graphics driver with OpenGL 3.3 support. "
                                 "Please update your graphics drivers and try again.");
        return 4;
    }
    assets_.load();
#ifndef _WIN32
    SetWindowIcons(assets_.windowIcons.data(), static_cast<int>(assets_.windowIcons.size()));
#endif
    // (On Windows the window class already has the .exe's multi-size GLFW_ICON
    // resource, which the taskbar, Alt+Tab and title bar use, as in native apps.)
    audio_.init(options_.mute);
    applySettings();
    ui_.onClick = [this] { audio_.play(Sfx::Click); };

    const bool wantFullscreen = options_.fullscreen || (progress_.settings.fullscreen && !options_.windowed);
    if (wantFullscreen) toggleFullscreen();

    if (options_.hostRoom || !options_.joinAddress.empty()) {
        audio_.setMusicOn(true);
        std::string error;
        if (options_.hostRoom && hostOnline(error)) showRoom();
        else if (!options_.hostRoom && joinOnline(options_.joinAddress, error)) showOnlineMenu();
        else showOnlineMenu(error);
    } else if (!options_.playStage.empty() || options_.autoplay) {
        const std::string stage = options_.playStage.empty() ? progress_.selectedStage : options_.playStage;
        const std::string vehicle = options_.playVehicle.empty() ? progress_.selectedVehicle : options_.playVehicle;
        audio_.setMusicOn(true);
        switchTo(makePlayScreen(*this, stage, vehicle));
    } else {
        showTitle();
    }

    LOG_INFO("%s %s running (%dx%d)", kGameName, kVersion, GetScreenWidth(), GetScreenHeight());
    while (!quit_ && !WindowShouldClose()) {
        frameStep();
        if (options_.maxFrames > 0 && frame_ >= options_.maxFrames) break;
    }

    // Screens bank unfinished runs when destroyed, so drop them before saving.
    pending_.reset();
    screen_.reset();
    online_.reset();  // say goodbye to the room
    saveProgress();
    const int failures = automation_ ? automation_->failures() : 0;
    automation_.reset();
    audio_.shutdown();
    assets_.unload();
    CloseWindow();
    if (failures > 0) LOG_ERROR("Automation finished with %d failure(s)", failures);
    return failures > 0 ? 3 : 0;
}

void App::frameStep()
{
    if (pending_) {
        screen_ = std::move(pending_);
        fade_ = 1.0f;
        accumulator_ = 0.0f;
    }
    frame_++;
    syncWindowSize();
    if (automation_) automation_->beforeFrame();
    input_.update();

    if (input_.pressed(Action::Fullscreen)) toggleFullscreen();
    if (input_.pressed(Action::Screenshot)) requestScreenshot("");

    const float dt = options_.fixedStep ? kFixedDt : std::min(GetFrameTime(), 0.1f);
    time_ += dt;
    if (online_) online_->update(dt);
    ui_.beginFrame(input_, assets_);
    screen_->update(dt);

    if (options_.fixedStep) {
        screen_->fixedUpdate();
        alpha_ = 1.0f;
    } else {
        accumulator_ += dt;
        int steps = 0;
        while (accumulator_ >= kFixedDt && steps < 6) {
            screen_->fixedUpdate();
            accumulator_ -= kFixedDt;
            steps++;
        }
        if (steps == 6) accumulator_ = 0.0f;  // too slow to keep up: drop time rather than spiral
        alpha_ = accumulator_ / kFixedDt;
    }
    audio_.update();

    BeginDrawing();
    ClearBackground(BLACK);
    rlDisableBackfaceCulling();
    screen_->draw();
    if (progress_.settings.showFps) {
        char fps[32];
        std::snprintf(fps, sizeof(fps), "%d FPS", GetFPS());
        ui_.text(fps, {ui_.width() - ui_.px(16), ui_.height() - ui_.px(34)}, ui_.px(24), WHITE, Align::Right);
    }
    if (fade_ > 0.0f) {
        DrawRectangle(0, 0, GetScreenWidth(), GetScreenHeight(), Fade(BLACK, fade_));
        fade_ = std::max(0.0f, fade_ - dt * 4.0f);
    }
    if (!pendingScreenshot_.empty()) captureScreenshot();
    EndDrawing();
    ui_.endFrame();
    if (automation_) automation_->afterFrame();
}

void App::switchTo(std::unique_ptr<Screen> next)
{
    // Deferred: the current screen may be mid-way through its own update/draw.
    pending_ = std::move(next);
    if (!screen_) {
        screen_ = std::move(pending_);
        fade_ = 1.0f;
    }
}

void App::saveProgress()
{
    if (!progress_.save(savePath_)) LOG_WARN("Could not save progress to %s", savePath_.c_str());
}

void App::applySettings()
{
    const Settings& s = progress_.settings;
    audio_.setVolumes(s.masterVolume, s.sfxVolume, s.musicVolume);
}

namespace {

// raylib's own window. (GetWindowHandle() returns the native X11/Win32 handle,
// but raylib keeps its GL context current, so GLFW can tell us which window it is.)
GLFWwindow* glfwWindow() { return glfwGetCurrentContext(); }

} // namespace

void App::toggleFullscreen()
{
    // Done with GLFW directly: raylib's borderless toggle also flips window
    // decorations, which on GNOME leaves the window stuck at full size.
    auto* window = glfwWindow();
    if (!fullscreen_) {
        glfwGetWindowPos(window, &windowedX_, &windowedY_);
        glfwGetWindowSize(window, &windowedW_, &windowedH_);
        int count = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&count);
        const int index = GetCurrentMonitor();
        GLFWmonitor* monitor = index >= 0 && index < count ? monitors[index] : glfwGetPrimaryMonitor();
        const GLFWvidmode* mode = monitor ? glfwGetVideoMode(monitor) : nullptr;
        if (!mode) {
            LOG_WARN("Fullscreen unavailable: no video mode for the current monitor");
            return;
        }
        glfwSetWindowMonitor(window, monitor, 0, 0, mode->width, mode->height, mode->refreshRate);
        fullscreen_ = true;
    } else {
        glfwSetWindowMonitor(window, nullptr, windowedX_, windowedY_, windowedW_, windowedH_, GLFW_DONT_CARE);
        fullscreen_ = false;
        restoreFrames_ = 15;
    }
    progress_.settings.fullscreen = fullscreen_;
    sizeMismatchFrames_ = 0;
    LOG_INFO("Fullscreen %s", fullscreen_ ? "on" : "off");
}

void App::realWindowSize(int& width, int& height) const
{
    glfwGetWindowSize(glfwWindow(), &width, &height);
}

void App::syncWindowSize()
{
    // Some window managers (GNOME) apply the restored size to the frame
    // including its title bar, so the window shrinks a little each time.
    // Once things have settled, ask for the saved client size again.
    if (restoreFrames_ > 0 && --restoreFrames_ == 0 && !fullscreen_) {
        int w = 0, h = 0;
        realWindowSize(w, h);
        if (w != windowedW_ || h != windowedH_) {
            glfwSetWindowSize(glfwWindow(), windowedW_, windowedH_);
            glfwSetWindowPos(glfwWindow(), windowedX_, windowedY_);
            LOG_INFO("Restored window to %dx%d", windowedW_, windowedH_);
        }
    }

    // Safety net: if the window manager resized the window without raylib
    // noticing (or ignored a resize), adopt the real size so the game always
    // fills its window instead of drawing into a corner.
    int w = 0, h = 0;
    realWindowSize(w, h);
    if (w <= 0 || h <= 0 || (w == GetScreenWidth() && h == GetScreenHeight())) {
        sizeMismatchFrames_ = 0;
        return;
    }
    if (++sizeMismatchFrames_ < 10) return;  // give pending window events time to arrive
    LOG_INFO("Window is %dx%d but the game drew at %dx%d; resyncing", w, h, GetScreenWidth(), GetScreenHeight());
    SetWindowSize(w, h);
    sizeMismatchFrames_ = 0;
}

void App::requestScreenshot(const std::string& name)
{
    pendingScreenshot_ = name.empty() ? timestampName() : name;
}

void App::captureScreenshot()
{
    std::string dir = options_.screenshotDir.empty() ? paths::dataDir() + "/screenshots" : options_.screenshotDir;
    paths::ensureDir(dir);
    const std::string path = dir + "/" + pendingScreenshot_ + ".png";
    pendingScreenshot_.clear();
    rlDrawRenderBatchActive();  // make sure everything queued is in the framebuffer
    Image img = LoadImageFromScreen();
    if (ExportImage(img, path.c_str())) LOG_INFO("Saved screenshot %s", path.c_str());
    else LOG_WARN("Could not save screenshot %s", path.c_str());
    UnloadImage(img);
}

void App::showTitle() { switchTo(makeTitleScreen(*this)); }
void App::showStageSelect() { switchTo(makeStageSelectScreen(*this)); }
void App::showGarage(bool backToTitle) { switchTo(makeGarageScreen(*this, backToTitle)); }
void App::showSettings() { switchTo(makeSettingsScreen(*this)); }

void App::startRun()
{
    switchTo(makePlayScreen(*this, progress_.selectedStage, progress_.selectedVehicle));
}

namespace {

net::Online::Config onlineConfig(const App& app, const Progress& progress)
{
    net::Online::Config config;
    config.name = app.playerName();
    config.vehicle = progress.selectedVehicle;
    config.version = kVersion;
    return config;
}

} // namespace

bool App::hostOnline(std::string& error)
{
    online_.reset();
    online_ = net::Online::host(onlineConfig(*this, progress_), error);
    if (!online_) return false;
    // The room starts on the host's own stage and difficulty.
    online_->setSettings({progress_.selectedStage, progress_.settings.difficulty, 1000});
    return true;
}

bool App::joinOnline(const std::string& address, std::string& error)
{
    online_.reset();
    online_ = net::Online::join(address, onlineConfig(*this, progress_), error);
    return online_ != nullptr;
}

void App::leaveOnline(std::string message)
{
    online_.reset();
    showOnlineMenu(message);
}

std::string App::playerName() const
{
    if (!progress_.settings.playerName.empty()) return net::sanitizeName(progress_.settings.playerName);
    // Default: the computer's user name, up to the first dot / @ ("chloy.c@corp" -> CHLOY).
    for (const char* var : {"USER", "USERNAME", "LOGNAME"}) {
        const char* value = std::getenv(var);
        if (!value || !value[0]) continue;
        std::string name = value;
        name = name.substr(0, name.find_first_of(".@_ "));
        if (!name.empty()) return net::sanitizeName(name);
    }
    return "PLAYER";
}

void App::showOnlineMenu(const std::string& message) { switchTo(makeOnlineMenuScreen(*this, message)); }
void App::showRoom() { switchTo(makeRoomScreen(*this)); }
void App::showOnlineRace() { switchTo(makeOnlineRaceScreen(*this)); }

} // namespace hr
