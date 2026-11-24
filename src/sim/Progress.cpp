#include "sim/Progress.hpp"

#include "core/Log.hpp"
#include "core/Paths.hpp"

#include <algorithm>
#include <cstdlib>
#include <sstream>

namespace hr {

namespace {

constexpr int kFormatVersion = 1;

// Easy keeps the original key formats, so saves from before difficulties
// existed load as Easy progress.
std::string bestKey(const std::string& stage, const std::string& vehicle, Difficulty d)
{
    std::string key = stage + "|" + vehicle;
    if (d != Difficulty::Easy) key += std::string("|") + difficultyDef(d).key;
    return key;
}

std::string levelsKey(const std::string& stage, Difficulty d)
{
    return d == Difficulty::Easy ? stage : stage + "@" + difficultyDef(d).key;
}

bool parseBool(const std::string& v) { return v == "1" || v == "true"; }

float parseFloat(const std::string& v, float fallback)
{
    char* end = nullptr;
    const float f = std::strtof(v.c_str(), &end);
    return end != v.c_str() ? f : fallback;
}

long long parseInt(const std::string& v, long long fallback)
{
    char* end = nullptr;
    const long long n = std::strtoll(v.c_str(), &end, 10);
    return end != v.c_str() ? n : fallback;
}

float clampVolume(float v) { return std::clamp(v, 0.0f, 1.0f); }

} // namespace

Progress::Progress()
{
    vehicles_["jeep"] = true;
    stages_["countryside"] = true;
}

bool Progress::vehicleUnlocked(const std::string& id) const
{
    const VehicleDef* def = findVehicle(id);
    if (def && def->price == 0) return true;
    auto it = vehicles_.find(id);
    return it != vehicles_.end() && it->second;
}

bool Progress::stageUnlocked(const std::string& id) const
{
    const StageDef* def = findStage(id);
    if (def && def->price == 0) return true;
    auto it = stages_.find(id);
    return it != stages_.end() && it->second;
}

UpgradeLevels Progress::upgrades(const std::string& vehicleId) const
{
    auto it = upgrades_.find(vehicleId);
    return it != upgrades_.end() ? it->second : kStockLevels;
}

void Progress::setUpgrade(const std::string& vehicleId, int slot, int level)
{
    if (slot < 0 || slot >= 4) return;
    UpgradeLevels levels = upgrades(vehicleId);
    int maxLevel = level;
    if (const VehicleDef* def = findVehicle(vehicleId)) maxLevel = def->upgrades[static_cast<size_t>(slot)].maxLevel;
    levels[static_cast<size_t>(slot)] = std::clamp(level, 1, std::max(1, maxLevel));
    upgrades_[vehicleId] = levels;
}

int Progress::levelsCompleted(const std::string& stageId, Difficulty d) const
{
    auto it = levels_.find(levelsKey(stageId, d));
    return it != levels_.end() ? it->second : 0;
}

void Progress::setLevelsCompleted(const std::string& stageId, int levels, Difficulty d)
{
    levels_[levelsKey(stageId, d)] = std::max(levelsCompleted(stageId, d), std::max(0, levels));
}

float Progress::best(const std::string& stageId, const std::string& vehicleId, Difficulty d) const
{
    auto it = best_.find(bestKey(stageId, vehicleId, d));
    return it != best_.end() ? it->second : 0.0f;
}

float Progress::bestOnStage(const std::string& stageId, Difficulty d) const
{
    float best = 0.0f;
    for (const VehicleDef& v : vehicles()) best = std::max(best, this->best(stageId, v.id, d));
    return best;
}

void Progress::recordDistance(const std::string& stageId, const std::string& vehicleId, float distance, Difficulty d)
{
    float& slot = best_[bestKey(stageId, vehicleId, d)];
    slot = std::max(slot, distance);
}

bool Progress::spend(long long amount)
{
    if (amount < 0 || coins < amount) return false;
    coins -= amount;
    return true;
}

std::string Progress::serialize() const
{
    std::ostringstream out;
    out << "# Hill Rush save file\n";
    out << "version=" << kFormatVersion << "\n";
    out << "coins=" << coins << "\n";
    out << "selected.stage=" << selectedStage << "\n";
    out << "selected.vehicle=" << selectedVehicle << "\n";
    for (const auto& [id, unlocked] : vehicles_) out << "vehicle." << id << ".unlocked=" << (unlocked ? 1 : 0) << "\n";
    for (const auto& [id, unlocked] : stages_) out << "stage." << id << ".unlocked=" << (unlocked ? 1 : 0) << "\n";
    for (const auto& [id, levels] : upgrades_) {
        for (size_t i = 0; i < levels.size(); ++i) {
            out << "vehicle." << id << ".upgrade." << i << "=" << levels[i] << "\n";
        }
    }
    for (const auto& [key, distance] : best_) out << "best." << key << "=" << distance << "\n";
    for (const auto& [key, levels] : levels_) {
        const size_t at = key.find('@');
        if (at == std::string::npos) out << "stage." << key << ".levels=" << levels << "\n";
        else out << "stage." << key.substr(0, at) << ".levels." << key.substr(at + 1) << "=" << levels << "\n";
    }
    out << "settings.master=" << settings.masterVolume << "\n";
    out << "settings.sfx=" << settings.sfxVolume << "\n";
    out << "settings.music=" << settings.musicVolume << "\n";
    out << "settings.fullscreen=" << (settings.fullscreen ? 1 : 0) << "\n";
    out << "settings.showfps=" << (settings.showFps ? 1 : 0) << "\n";
    out << "settings.difficulty=" << difficultyDef(settings.difficulty).key << "\n";
    if (!settings.playerName.empty()) out << "settings.name=" << settings.playerName << "\n";
    out << "stats.runs=" << stats.runs << "\n";
    out << "stats.distance=" << static_cast<long long>(stats.totalDistance) << "\n";
    out << "stats.coins=" << stats.totalCoins << "\n";
    out << "stats.flips=" << stats.flips << "\n";
    return out.str();
}

void Progress::deserialize(const std::string& text)
{
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        const std::string key = line.substr(0, eq);
        const std::string value = line.substr(eq + 1);

        if (key == "coins") coins = std::max(0LL, parseInt(value, coins));
        else if (key == "selected.stage") selectedStage = value;
        else if (key == "selected.vehicle") selectedVehicle = value;
        else if (key == "settings.master") settings.masterVolume = clampVolume(parseFloat(value, settings.masterVolume));
        else if (key == "settings.sfx") settings.sfxVolume = clampVolume(parseFloat(value, settings.sfxVolume));
        else if (key == "settings.music") settings.musicVolume = clampVolume(parseFloat(value, settings.musicVolume));
        else if (key == "settings.fullscreen") settings.fullscreen = parseBool(value);
        else if (key == "settings.showfps") settings.showFps = parseBool(value);
        else if (key == "settings.difficulty") settings.difficulty = difficultyFromKey(value);
        else if (key == "settings.name") settings.playerName = value.substr(0, 32);
        else if (key == "stats.runs") stats.runs = static_cast<int>(parseInt(value, 0));
        else if (key == "stats.distance") stats.totalDistance = static_cast<double>(parseInt(value, 0));
        else if (key == "stats.coins") stats.totalCoins = parseInt(value, 0);
        else if (key == "stats.flips") stats.flips = static_cast<int>(parseInt(value, 0));
        else if (key.rfind("best.", 0) == 0) {
            best_[key.substr(5)] = std::max(0.0f, parseFloat(value, 0.0f));
        } else if (key.rfind("vehicle.", 0) == 0 || key.rfind("stage.", 0) == 0) {
            const bool isVehicle = key[0] == 'v';
            const std::string rest = key.substr(isVehicle ? 8 : 6);
            const size_t dot = rest.find('.');
            if (dot == std::string::npos) continue;
            const std::string id = rest.substr(0, dot);
            const std::string field = rest.substr(dot + 1);
            if (field == "unlocked") {
                (isVehicle ? vehicles_ : stages_)[id] = parseBool(value);
            } else if (!isVehicle && (field == "levels" || field.rfind("levels.", 0) == 0)) {
                const Difficulty d = field == "levels" ? Difficulty::Easy : difficultyFromKey(field.substr(7));
                if (d == Difficulty::Easy && field != "levels") continue;  // unknown difficulty name
                setLevelsCompleted(id, static_cast<int>(std::clamp(parseInt(value, 0), 0LL, 100000LL)), d);
            } else if (isVehicle && field.rfind("upgrade.", 0) == 0) {
                const int slot = static_cast<int>(parseInt(field.substr(8), -1));
                if (findVehicle(id)) setUpgrade(id, slot, static_cast<int>(std::clamp(parseInt(value, 1), 1LL, 1000LL)));
            }
        }
    }
    if (!findStage(selectedStage) || !stageUnlocked(selectedStage)) selectedStage = "countryside";
    if (!findVehicle(selectedVehicle) || !vehicleUnlocked(selectedVehicle)) selectedVehicle = "jeep";
}

bool Progress::load(const std::string& path)
{
    std::string text;
    if (!paths::readFile(path, text)) return false;
    deserialize(text);
    LOG_INFO("Loaded progress from %s (%lld coins)", path.c_str(), coins);
    return true;
}

bool Progress::save(const std::string& path) const
{
    return paths::writeFileAtomic(path, serialize());
}

} // namespace hr
