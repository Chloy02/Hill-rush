#pragma once

#include "sim/Defs.hpp"

#include <map>
#include <string>

namespace hr {

struct Settings {
    float masterVolume = 0.8f;
    float sfxVolume = 1.0f;
    float musicVolume = 0.55f;
    bool fullscreen = false;
    bool showFps = false;
    Difficulty difficulty = Difficulty::Easy;
    std::string playerName;  // online races; empty = not chosen yet
};

struct Stats {
    int runs = 0;
    double totalDistance = 0.0;
    long long totalCoins = 0;
    int flips = 0;
};

// Everything that persists between sessions. Stored as a simple
// `key=value` text file so it is easy to inspect and forward compatible:
// unknown keys are ignored, missing keys take defaults.
class Progress {
public:
    static constexpr int kStartingCoins = 0;

    Progress();

    bool load(const std::string& path);
    bool save(const std::string& path) const;
    // Serialisation without touching the filesystem (used by tests).
    std::string serialize() const;
    void deserialize(const std::string& text);

    long long coins = kStartingCoins;
    std::string selectedStage = "countryside";
    std::string selectedVehicle = "jeep";
    Settings settings;
    Stats stats;

    bool vehicleUnlocked(const std::string& id) const;
    bool stageUnlocked(const std::string& id) const;
    void unlockVehicle(const std::string& id) { vehicles_[id] = true; }
    void unlockStage(const std::string& id) { stages_[id] = true; }

    // Upgrade levels are 1-based: 1 is the stock part.
    UpgradeLevels upgrades(const std::string& vehicleId) const;
    void setUpgrade(const std::string& vehicleId, int slot, int level);

    // Records and level checkpoints are kept separately for each difficulty,
    // so an Easy record never stands in for a Hard one.

    // Level checkpoints already paid out on a stage.
    int levelsCompleted(const std::string& stageId, Difficulty d = Difficulty::Easy) const;
    void setLevelsCompleted(const std::string& stageId, int levels, Difficulty d = Difficulty::Easy);

    float best(const std::string& stageId, const std::string& vehicleId, Difficulty d = Difficulty::Easy) const;
    // Best over all vehicles on a stage.
    float bestOnStage(const std::string& stageId, Difficulty d = Difficulty::Easy) const;
    void recordDistance(const std::string& stageId, const std::string& vehicleId, float distance,
                        Difficulty d = Difficulty::Easy);

    // Spends coins if affordable. Returns false (and changes nothing) otherwise.
    bool spend(long long amount);

private:
    std::map<std::string, bool> vehicles_;
    std::map<std::string, bool> stages_;
    std::map<std::string, UpgradeLevels> upgrades_;
    std::map<std::string, float> best_;  // key: stage|vehicle (Easy) or stage|vehicle|<difficulty>
    std::map<std::string, int> levels_;  // key: stage (Easy) or stage@<difficulty>
};

} // namespace hr
