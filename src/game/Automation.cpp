#include "game/Automation.hpp"

#include "core/Log.hpp"
#include "core/Paths.hpp"
#include "game/App.hpp"
#include "net/Online.hpp"

#include <raylib.h>

#include <cstdlib>
#include <sstream>

namespace hr {

namespace {

bool actionFor(const std::string& name, Action& out)
{
    static const std::pair<const char*, Action> table[] = {
        {"gas", Action::Gas},         {"brake", Action::Brake},   {"up", Action::Up},
        {"down", Action::Down},       {"left", Action::Left},     {"right", Action::Right},
        {"confirm", Action::Confirm}, {"back", Action::Back},     {"pause", Action::Pause},
        {"restart", Action::Restart}, {"fullscreen", Action::Fullscreen},
        {"special", Action::Special}, {"boost", Action::Special},
    };
    for (const auto& [n, a] : table) {
        if (name == n) {
            out = a;
            return true;
        }
    }
    return false;
}

} // namespace

Automation::Automation(App& app, const std::string& scriptPath, const std::string& telemetryPath) : app_(app)
{
    if (!telemetryPath.empty()) telemetry_ = std::fopen(telemetryPath.c_str(), "w");
    if (scriptPath.empty()) {
        loaded_ = true;
        return;
    }
    std::string text;
    if (!paths::readFile(scriptPath, text)) {
        LOG_ERROR("Could not read automation script %s", scriptPath.c_str());
        return;
    }
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        lineNo++;
        const size_t hash = line.find('#');
        if (hash != std::string::npos) line = line.substr(0, hash);
        std::istringstream words(line);
        Command c;
        c.line = lineNo;
        if (!(words >> c.op)) continue;
        if (c.op == "wait") words >> c.n;
        else if (c.op == "click") words >> c.x >> c.y;
        else if (c.op == "coins" || c.op == "erase") words >> c.n;
        else if (c.op == "waitfor") {
            words >> c.arg;
            if (!(words >> c.n)) c.n = 900;
        } else if (c.op == "waitstart") {
            words >> c.x;  // players
            if (!(words >> c.n)) c.n = 1800;
        } else if (c.op == "log" || c.op == "type") {
            std::getline(words >> std::ws, c.arg);
        } else words >> c.arg;
        commands_.push_back(c);
    }
    loaded_ = true;
    LOG_INFO("Automation: %zu commands from %s", commands_.size(), scriptPath.c_str());
    app_.input().setExclusiveVirtual(true);
}

Automation::~Automation()
{
    if (telemetry_) std::fclose(telemetry_);
}

void Automation::beforeFrame()
{
    Input& input = app_.input();
    const long frame = app_.frame();

    // Timed releases from `tap`.
    for (auto it = releases_.begin(); it != releases_.end();) {
        if (--it->second <= 0) {
            Action a;
            if (actionFor(it->first, a)) input.setVirtual(a, false);
            it = releases_.erase(it);
        } else {
            ++it;
        }
    }
    if (clickFrames_ > 0 && --clickFrames_ == 0) {
        input.setVirtualMouse(input.mouse(), false);
    }

    if (waitStart_) {
        net::Online* online = app_.online();
        if (online && online->canStart() && online->room().players.size() >= waitPlayers_) {
            LOG_INFO("script line %d: everyone is ready", waitLine_);
            waitStart_ = false;
        } else if (frame >= waitScreenUntil_) {
            LOG_ERROR("script line %d: gave up waiting for the room to be ready", waitLine_);
            failures_++;
            waitStart_ = false;
        } else {
            return;
        }
    }
    if (!waitScreen_.empty()) {
        const char* current = app_.screen() ? app_.screen()->name() : "none";
        if (waitScreen_ == current) {
            LOG_INFO("script line %d: '%s' is showing", waitLine_, current);
            waitScreen_.clear();
        } else if (frame >= waitScreenUntil_) {
            LOG_ERROR("script line %d: gave up waiting for screen '%s' ('%s' is showing)", waitLine_,
                      waitScreen_.c_str(), current);
            failures_++;
            waitScreen_.clear();
        } else {
            return;
        }
    }

    while (next_ < commands_.size() && frame >= waitUntil_ && waitScreen_.empty() && !waitStart_) {
        const Command& c = commands_[next_++];
        Action action;
        if (c.op == "wait") {
            waitUntil_ = frame + c.n;
        } else if (c.op == "waitfor") {
            waitScreen_ = c.arg;
            waitScreenUntil_ = frame + c.n;
            waitLine_ = c.line;
        } else if (c.op == "waitstart") {
            waitStart_ = true;
            waitPlayers_ = c.x >= 2.0f ? static_cast<size_t>(c.x) : 2;
            waitScreenUntil_ = frame + c.n;
            waitLine_ = c.line;
        } else if (c.op == "type") {
            input.injectText(c.arg);
            waitUntil_ = frame + 1;
        } else if (c.op == "erase") {
            input.injectBackspaces(c.n);
            waitUntil_ = frame + 1;
        } else if (c.op == "press" || c.op == "release" || c.op == "tap") {
            if (!actionFor(c.arg, action)) {
                LOG_ERROR("script line %d: unknown action '%s'", c.line, c.arg.c_str());
                failures_++;
                continue;
            }
            input.setVirtual(action, c.op != "release");
            if (c.op == "tap") releases_.push_back({c.arg, 3});
        } else if (c.op == "click") {
            const Ui& ui = app_.ui();
            const float sx = ui.width() * 0.5f + (c.x - Ui::kLogicalW * 0.5f) * ui.scale();
            const float sy = ui.height() * 0.5f + (c.y - Ui::kLogicalH * 0.5f) * ui.scale();
            input.setVirtualMouse({sx, sy}, true);
            clickFrames_ = 3;
            waitUntil_ = frame + 4;  // let the press and release land before the next command
        } else if (c.op == "screenshot") {
            app_.requestScreenshot(c.arg);
            waitUntil_ = frame + 1;  // capture this frame before anything else changes
        } else if (c.op == "expect") {
            const char* current = app_.screen() ? app_.screen()->name() : "none";
            if (c.arg != current) {
                LOG_ERROR("script line %d: expected screen '%s' but '%s' is showing", c.line, c.arg.c_str(), current);
                failures_++;
            } else {
                LOG_INFO("script line %d: on screen '%s' as expected", c.line, current);
            }
        } else if (c.op == "coins") {
            app_.progress().coins = c.n;
        } else if (c.op == "log") {
            LOG_INFO("script: %s", c.arg.c_str());
        } else if (c.op == "quit") {
            app_.quit();
        } else {
            LOG_ERROR("script line %d: unknown command '%s'", c.line, c.op.c_str());
            failures_++;
        }
    }
}

void Automation::afterFrame()
{
    if (!telemetry_ || app_.frame() % 10 != 0) return;
    const Screen* s = app_.screen();
    int realW = 0, realH = 0;
    app_.realWindowSize(realW, realH);
    std::fprintf(telemetry_,
                 "{\"frame\":%ld,\"time\":%.2f,\"fps\":%d,\"window\":[%d,%d],\"real\":[%d,%d],\"fullscreen\":%s,"
                 "\"screen\":\"%s\",\"state\":%s}\n",
                 app_.frame(), app_.time(), GetFPS(), GetScreenWidth(), GetScreenHeight(), realW, realH,
                 app_.isFullscreen() ? "true" : "false", s ? s->name() : "none",
                 s ? s->telemetry().c_str() : "{}");
    std::fflush(telemetry_);
}

} // namespace hr
