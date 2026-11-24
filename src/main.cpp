#include "core/Log.hpp"
#include "core/Math.hpp"
#include "core/Version.hpp"
#include "game/App.hpp"
#include "game/Assets.hpp"
#include "game/Audio.hpp"
#include "sim/SimHarness.hpp"
#include "sim/Terrain.hpp"

#include <raylib.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace {

void printHelp()
{
    std::printf(
        "%s %s - a physics hill-climb driving game\n\n"
        "Usage: hillrush [options]\n\n"
        "Game options:\n"
        "  --windowed              start in a window (ignores the saved fullscreen setting)\n"
        "  --fullscreen            start fullscreen\n"
        "  --size WxH              window size, e.g. 1600x900\n"
        "  --mute                  no sound\n"
        "  --data-dir DIR          where to keep the save file and screenshots\n"
        "                          (default: ~/.local/share/hillrush)\n"
        "  --play STAGE[:VEHICLE]  skip the menus and start driving\n"
        "  --difficulty LEVEL      easy, medium or hard (also selectable in Settings)\n"
        "  --autoplay              let the autopilot drive (add --stunts for flips)\n\n"
        "Online races (see README):\n"
        "  --name NAME             your name in online races (saved)\n"
        "  --host                  open a room on this computer straight away\n"
        "  --join ADDRESS          join the room at ADDRESS (e.g. 192.168.1.20)\n\n"
        "Testing options:\n"
        "  --script FILE           run an automation script (see src/game/Automation.hpp)\n"
        "  --screenshot-dir DIR    where scripted screenshots go\n"
        "  --telemetry FILE        write JSON-lines telemetry\n"
        "  --fixed-step            one physics step per frame (deterministic)\n"
        "  --frames N              quit after N frames\n"
        "  --export-icon FILE      write the game icon as PNG (with --icon-size N)\n"
        "  --export-audio DIR      write every synthesised sound to WAV files\n\n"
        "Headless tools:\n"
        "  --sim                   bot-driven run without a window; prints JSON\n"
        "                          (--stage S --vehicle V --upgrades a,b,c,d --seconds N --verbose)\n"
        "  --terrain-stats STAGE   slope statistics of a stage\n"
        "  --climb-test            steepest ramp a vehicle can climb (--vehicle, --upgrades)\n\n"
        "Controls: Right/D = gas, Left/A = brake, Esc/P = pause, R = restart,\n"
        "          F11 = fullscreen, F12 = screenshot. Gamepads and mouse work too.\n",
        hr::kGameName, hr::kVersion);
}

void terrainStats(const std::string& stageId)
{
    const hr::StageDef* stage = hr::findStage(stageId);
    if (!stage) {
        std::fprintf(stderr, "unknown stage %s\n", stageId.c_str());
        return;
    }
    hr::TerrainGenerator gen(*stage);
    std::printf("stage %s: slope (deg) percentiles and height range per 500 m band\n", stageId.c_str());
    for (int band = 0; band < 10; ++band) {
        std::vector<float> slopes;
        float lo = 1e9f, hi = -1e9f;
        for (float x = static_cast<float>(band) * 500.0f; x < static_cast<float>(band + 1) * 500.0f; x += 0.5f) {
            const float h0 = gen.height(x), h1 = gen.height(x + 0.5f);
            slopes.push_back(std::fabs(hr::radToDeg(std::atan2(h1 - h0, 0.5f))));
            lo = std::min(lo, h0);
            hi = std::max(hi, h0);
        }
        std::sort(slopes.begin(), slopes.end());
        auto pct = [&](float p) { return slopes[static_cast<size_t>(p * static_cast<float>(slopes.size() - 1))]; };
        std::printf("  %5d-%5dm  p50=%4.1f p90=%4.1f p99=%4.1f max=%4.1f  height %6.1f..%6.1f\n", band * 500,
                    (band + 1) * 500, pct(0.5f), pct(0.9f), pct(0.99f), slopes.back(), lo, hi);
    }
}

void climbTable(const hr::SimOptions& sim)
{
    const hr::VehicleDef* v = hr::findVehicle(sim.vehicle);
    if (!v) return;
    std::printf("%s upgrades %d,%d,%d,%d: steepest ramp climbed (deg)\n", v->id.c_str(), sim.upgrades[0],
                sim.upgrades[1], sim.upgrades[2], sim.upgrades[3]);
    for (float runUp : {0.0f, 40.0f}) {
        float best = 0.0f;
        for (float angle = 10.0f; angle <= 70.0f; angle += 2.5f) {
            if (hr::climbTest(*v, sim.upgrades, angle, runUp)) best = angle;
        }
        std::printf("  run-up %2.0f m: %.1f\n", runUp, best);
    }
}

} // namespace

int main(int argc, char** argv)
{
    hr::LaunchOptions game;
    hr::SimOptions sim;
    std::string mode = "game";
    std::string terrainStage;
    std::string iconPath;
    int iconSize = 256;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&]() -> std::string {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "missing value for %s\n", arg.c_str());
                std::exit(2);
            }
            return argv[++i];
        };
        if (arg == "--help" || arg == "-h") {
            printHelp();
            return 0;
        } else if (arg == "--version") {
            std::printf("%s %s\n", hr::kGameName, hr::kVersion);
            return 0;
        } else if (arg == "--windowed") game.windowed = true;
        else if (arg == "--fullscreen") game.fullscreen = true;
        else if (arg == "--size") {
            const std::string v = next();
            if (std::sscanf(v.c_str(), "%dx%d", &game.width, &game.height) != 2) {
                std::fprintf(stderr, "bad --size '%s' (expected WxH)\n", v.c_str());
                return 2;
            }
        } else if (arg == "--mute") game.mute = true;
        else if (arg == "--data-dir") game.dataDir = next();
        else if (arg == "--play") {
            const std::string v = next();
            const size_t colon = v.find(':');
            game.playStage = v.substr(0, colon);
            if (colon != std::string::npos) game.playVehicle = v.substr(colon + 1);
        } else if (arg == "--autoplay") game.autoplay = true;
        else if (arg == "--stunts") {
            game.stunts = true;
            sim.stunts = true;
        }
        else if (arg == "--name") game.playerName = next();
        else if (arg == "--host") game.hostRoom = true;
        else if (arg == "--join") game.joinAddress = next();
        else if (arg == "--script") game.script = next();
        else if (arg == "--screenshot-dir") game.screenshotDir = next();
        else if (arg == "--telemetry") game.telemetry = next();
        else if (arg == "--fixed-step") game.fixedStep = true;
        else if (arg == "--frames") game.maxFrames = std::atoi(next().c_str());
        else if (arg == "--export-icon") {
            mode = "icon";
            iconPath = next();
        } else if (arg == "--icon-size") iconSize = std::atoi(next().c_str());
        else if (arg == "--export-audio") {
            mode = "audio";
            iconPath = next();
        }
        else if (arg == "--sim") mode = "sim";
        else if (arg == "--terrain-stats") {
            mode = "terrain";
            terrainStage = next();
        } else if (arg == "--climb-test") mode = "climb";
        else if (arg == "--difficulty") {
            game.difficulty = next();
            sim.difficulty = hr::difficultyFromKey(game.difficulty);
        } else if (arg == "--stage") sim.stage = next();
        else if (arg == "--vehicle") sim.vehicle = next();
        else if (arg == "--seconds") sim.maxSeconds = std::strtof(next().c_str(), nullptr);
        else if (arg == "--verbose") sim.verbose = true;
        else if (arg == "--upgrades") {
            const std::string list = next();
            std::sscanf(list.c_str(), "%d,%d,%d,%d", &sim.upgrades[0], &sim.upgrades[1], &sim.upgrades[2],
                        &sim.upgrades[3]);
        } else {
            std::fprintf(stderr, "unknown option %s (see --help)\n", arg.c_str());
            return 2;
        }
    }

    if (mode == "sim") {
        const hr::SimReport report = hr::runHeadless(sim);
        std::printf("%s\n", hr::toJson(sim, report).c_str());
        return report.finite ? 0 : 1;
    }
    if (mode == "terrain") {
        terrainStats(terrainStage);
        return 0;
    }
    if (mode == "climb") {
        climbTable(sim);
        return 0;
    }
    if (mode == "icon") {
        Image icon = hr::makeIconImage(iconSize);
        const bool ok = ExportImage(icon, iconPath.c_str());
        UnloadImage(icon);
        return ok ? 0 : 1;
    }

    if (mode == "audio") return hr::exportAudio(iconPath) ? 0 : 1;

    hr::App app(game);
    return app.run();
}
