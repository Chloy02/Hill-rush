#pragma once

#include "game/App.hpp"

#include <memory>
#include <string>

namespace hr {

std::unique_ptr<Screen> makeTitleScreen(App& app);
std::unique_ptr<Screen> makeStageSelectScreen(App& app);
// `backToTitle`: where the Garage's BACK button goes (title or stage select).
std::unique_ptr<Screen> makeGarageScreen(App& app, bool backToTitle = false);
std::unique_ptr<Screen> makeSettingsScreen(App& app);
std::unique_ptr<Screen> makePlayScreen(App& app, const std::string& stageId, const std::string& vehicleId);

// Online races: find / host a room, the room itself, and the race.
std::unique_ptr<Screen> makeOnlineMenuScreen(App& app, const std::string& message);
std::unique_ptr<Screen> makeRoomScreen(App& app);
std::unique_ptr<Screen> makeOnlineRaceScreen(App& app);

// Shared decoration for menu screens -------------------------------------------

// Coin balance badge in the top-right corner.
void drawCoinBalance(App& app);
// Screen title with the cartoon outline.
void drawScreenTitle(App& app, const char* title);
// "DIFFICULTY: HARD" under the screen title.
void drawDifficultyChip(App& app);

} // namespace hr
