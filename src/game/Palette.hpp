#pragma once

#include <raylib.h>

#include <string>

namespace hr {

enum class Scenery { Trees, Cacti, Pines, Spires, Rocks, Boulders };

// Colours and decoration style for one stage.
struct Palette {
    Color skyTop;
    Color skyBottom;
    Color farHills;
    Color nearHills;
    Color ground;        // fill below the surface
    Color groundDark;    // texture speckle
    Color surface;       // grass / sand / snow strip
    Color surfaceEdge;   // darker underside of the strip
    Color ice;           // ice patches (arctic)
    Color dust;          // wheel dust particles
    Color sun;
    Color cloud;
    Scenery scenery;
    bool stars;          // night sky with stars
    bool planet;         // big planet in the sky (moon stage)
    bool snowfall;
};

const Palette& paletteFor(const std::string& stageId);

} // namespace hr
