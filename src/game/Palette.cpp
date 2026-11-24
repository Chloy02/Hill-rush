#include "game/Palette.hpp"

namespace hr {

namespace {

Palette countryside()
{
    Palette p{};
    p.skyTop = {74, 158, 230, 255};
    p.skyBottom = {189, 228, 250, 255};
    p.farHills = {137, 186, 170, 255};
    p.nearHills = {96, 160, 92, 255};
    p.ground = {133, 89, 52, 255};
    p.groundDark = {104, 67, 38, 255};
    p.surface = {108, 186, 60, 255};
    p.surfaceEdge = {70, 133, 38, 255};
    p.ice = p.surface;
    p.dust = {150, 112, 70, 255};
    p.sun = {255, 236, 140, 255};
    p.cloud = {255, 255, 255, 235};
    p.scenery = Scenery::Trees;
    return p;
}

Palette desert()
{
    Palette p{};
    p.skyTop = {242, 158, 82, 255};
    p.skyBottom = {255, 223, 160, 255};
    p.farHills = {222, 160, 104, 255};
    p.nearHills = {204, 140, 82, 255};
    p.ground = {214, 166, 96, 255};
    p.groundDark = {186, 136, 74, 255};
    p.surface = {240, 202, 128, 255};
    p.surfaceEdge = {205, 160, 92, 255};
    p.ice = p.surface;
    p.dust = {232, 196, 130, 255};
    p.sun = {255, 246, 200, 255};
    p.cloud = {255, 242, 225, 200};
    p.scenery = Scenery::Cacti;
    return p;
}

Palette arctic()
{
    Palette p{};
    p.skyTop = {120, 170, 220, 255};
    p.skyBottom = {222, 238, 250, 255};
    p.farHills = {190, 210, 232, 255};
    p.nearHills = {160, 188, 214, 255};
    p.ground = {120, 140, 165, 255};
    p.groundDark = {96, 114, 138, 255};
    p.surface = {246, 250, 255, 255};
    p.surfaceEdge = {196, 214, 234, 255};
    p.ice = {150, 214, 245, 255};
    p.dust = {240, 246, 255, 255};
    p.sun = {255, 250, 220, 255};
    p.cloud = {255, 255, 255, 220};
    p.scenery = Scenery::Pines;
    p.snowfall = true;
    return p;
}

Palette highlands()
{
    Palette p{};
    p.skyTop = {84, 120, 190, 255};
    p.skyBottom = {255, 186, 150, 255};
    p.farHills = {110, 116, 168, 255};
    p.nearHills = {72, 112, 104, 255};
    p.ground = {98, 82, 70, 255};
    p.groundDark = {76, 62, 54, 255};
    p.surface = {92, 156, 76, 255};
    p.surfaceEdge = {58, 108, 52, 255};
    p.ice = p.surface;
    p.dust = {120, 100, 84, 255};
    p.sun = {255, 210, 160, 255};
    p.cloud = {255, 222, 214, 210};
    p.scenery = Scenery::Spires;
    return p;
}

Palette mars()
{
    Palette p{};
    p.skyTop = {170, 92, 66, 255};
    p.skyBottom = {236, 170, 130, 255};
    p.farHills = {176, 96, 70, 255};
    p.nearHills = {150, 74, 52, 255};
    p.ground = {150, 66, 42, 255};
    p.groundDark = {120, 50, 32, 255};
    p.surface = {206, 104, 62, 255};
    p.surfaceEdge = {160, 74, 44, 255};
    p.ice = p.surface;
    p.dust = {214, 120, 80, 255};
    p.sun = {255, 230, 200, 255};
    p.cloud = {240, 190, 160, 120};
    p.scenery = Scenery::Rocks;
    return p;
}

Palette moon()
{
    Palette p{};
    p.skyTop = {6, 8, 20, 255};
    p.skyBottom = {26, 30, 54, 255};
    p.farHills = {70, 72, 84, 255};
    p.nearHills = {94, 96, 108, 255};
    p.ground = {120, 120, 126, 255};
    p.groundDark = {96, 96, 104, 255};
    p.surface = {176, 176, 182, 255};
    p.surfaceEdge = {136, 136, 144, 255};
    p.ice = p.surface;
    p.dust = {190, 190, 196, 255};
    p.sun = {255, 255, 240, 255};
    p.cloud = {0, 0, 0, 0};
    p.scenery = Scenery::Boulders;
    p.stars = true;
    p.planet = true;
    return p;
}

} // namespace

const Palette& paletteFor(const std::string& stageId)
{
    static const Palette kCountryside = countryside();
    static const Palette kDesert = desert();
    static const Palette kArctic = arctic();
    static const Palette kHighlands = highlands();
    static const Palette kMars = mars();
    static const Palette kMoon = moon();
    if (stageId == "desert") return kDesert;
    if (stageId == "arctic") return kArctic;
    if (stageId == "highlands") return kHighlands;
    if (stageId == "mars") return kMars;
    if (stageId == "moon") return kMoon;
    return kCountryside;
}

} // namespace hr
