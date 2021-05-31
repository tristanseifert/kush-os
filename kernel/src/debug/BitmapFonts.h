#ifndef DEBUG_BITMAPFONTS_H
#define DEBUG_BITMAPFONTS_H

#include <cstddef>
#include <cstdint>

namespace debug {
/**
 * Encapsulates a single font's data
 */
struct BitmapFont {
    /// descriptive name
    const char *name = nullptr;
    /// character size (in pixels)
    const size_t width, height;
    /// bytes between glyphs in the data array
    const size_t stride;
    /// maximum glyph value
    const size_t maxGlyph;

    /// character data
    const uint8_t *data = nullptr;
};

/**
 * Helper providing access to different fonts
 */
struct BitmapFontHelper {
    /// Total number of fonts installed
    constexpr static const size_t kNumFonts = 1;
    /// Info for each font
    static const BitmapFont gFonts[kNumFonts];

    BitmapFontHelper() = delete;
};
}

#endif
