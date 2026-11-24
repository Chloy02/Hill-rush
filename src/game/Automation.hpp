#pragma once

#include <cstdio>
#include <string>
#include <vector>

namespace hr {

class App;

// Drives the game from a text script, for automated play-testing. Inputs go
// through the same Input object as real devices, so menus, pedals and the
// pause key are exercised exactly as a player would.
//
// Script commands (one per line, '#' starts a comment):
//   wait <frames>           let the game run
//   press <action>          hold: gas brake up down left right confirm back pause restart
//   release <action>
//   tap <action>            press for 3 frames
//   click <x> <y>           click at logical coordinates (1280x720 space)
//   type <text>             type into the text field being edited
//   erase <count>           press Backspace <count> times
//   screenshot <name>       save <screenshot-dir>/<name>.png
//   expect <screen>         fail the run unless the named screen is showing
//   waitfor <screen> [n]    wait until the named screen shows (fail after n frames, default 900)
//   waitstart <players> [n] online host: wait until that many players are in and ready (default 1800 frames)
//   coins <amount>          set the coin balance (test setup)
//   log <text>
//   quit
class Automation {
public:
    Automation(App& app, const std::string& scriptPath, const std::string& telemetryPath);
    ~Automation();

    bool ok() const { return loaded_; }
    // Applies the commands due this frame. Call before input is polled.
    void beforeFrame();
    // Writes telemetry. Call after the frame is drawn.
    void afterFrame();
    int failures() const { return failures_; }

private:
    struct Command {
        std::string op;
        std::string arg;
        float x = 0.0f;
        float y = 0.0f;
        int n = 0;
        int line = 0;
    };

    App& app_;
    std::vector<Command> commands_;
    size_t next_ = 0;
    long waitUntil_ = 0;
    std::string waitScreen_;  // waitfor: the screen being waited for
    long waitScreenUntil_ = 0;
    bool waitStart_ = false;  // waitstart: until the online race can start
    size_t waitPlayers_ = 2;
    int waitLine_ = 0;
    int clickFrames_ = 0;
    std::vector<std::pair<std::string, int>> releases_;  // action, frames left
    bool loaded_ = false;
    int failures_ = 0;
    std::FILE* telemetry_ = nullptr;
};

} // namespace hr
