#ifndef FBCONS_BITMAPFONTS_H
#define FBCONS_BITMAPFONTS_H

#include <stddef.h>
#include <stdint.h>

namespace Platform::Shared::FbCons {
/**
 * @brief Data on a single bitmap font
 *
 * Bitmap fonts are stored as contiguous arrays of 1bpp data. Each character cell is expected to be
 * the same size, as described here.
 */
struct BitmapFont {
    /**
     * @brief Descriptive font name
     *
     * This is currently not used for anything beyond embedding in the binary.
     */
    const char *name{nullptr};
    /// Width of each character, in pixels
    const size_t width;
    /// Height of each character, in pixels
    const size_t height;
    /// Bytes between glyphs in the data array
    const size_t stride;
    /// Maximum glyph value
    const size_t maxGlyph;

    /**
     * @brief Character data
     *
     * Pointer to an array that contains the actual raw bitmap font data.
     */
    const uint8_t *data{nullptr};
};

/**
 * @brief Helper providing access to different fonts
 */
struct BitmapFontHelper {
    /// Total number of fonts installed
    constexpr static const size_t kNumFonts{1};
    /// Info for each font
    static const BitmapFont gFonts[kNumFonts];

    BitmapFontHelper() = delete;
};
}

#endif
