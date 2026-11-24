#pragma once

#include <raylib.h>

#include <vector>

namespace hr {

// Fonts and procedurally generated textures. Everything is created at
// start-up from data compiled into the executable; nothing is read from disk.
class Assets {
public:
    void load();
    void unload();

    // Picks the font atlas closest to the requested pixel size.
    const Font& font(float size) const { return size <= 44.0f ? fontSmall_ : fontLarge_; }

    Texture2D groundTexture{};   // tiling speckled dirt, white-ish so it can be tinted
    Texture2D surfaceTexture{};  // tiling grass/sand streaks along the surface strip
    Texture2D vignette{};        // soft dark corners for the results screen
    // The window icon at several sizes (CPU side): docks and task switchers
    // pick the one closest to what they draw.
    std::vector<Image> windowIcons;

private:
    Font fontSmall_{};
    Font fontLarge_{};
};

// Text helpers built on the asset fonts (screen space).
struct TextStyle {
    float size = 32.0f;
    Color color = WHITE;
    Color outline = {0, 0, 0, 0};  // alpha 0 = no outline
    float outlineWidth = 0.0f;
    Color shadow = {0, 0, 0, 0};  // alpha 0 = no shadow
    float spacing = 1.0f;
};

enum class Align { Left, Center, Right };

Vector2 measureText(const Assets& assets, const char* text, float size, float spacing = 1.0f);
// Draws text anchored at `pos` (top edge; x interpreted by `align`).
void drawText(const Assets& assets, const char* text, Vector2 pos, const TextStyle& style,
              Align align = Align::Left);

// Generates the game icon (also used by `--export-icon`).
Image makeIconImage(int size);

} // namespace hr
