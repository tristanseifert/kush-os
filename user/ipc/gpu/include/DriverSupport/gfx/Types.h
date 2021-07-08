#ifndef DRIVERSUPPORT_GFX_TYPES_H
#define DRIVERSUPPORT_GFX_TYPES_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <span>

namespace DriverSupport::gfx {
/**
 * Bits valid for the the display capability field.
 */
enum DisplayCapabilities: uint32_t {
    /**
     * The driver wishes to be informed of updated rectangles on screen.
     *
     * Clients must call `rectUpdated()` on the device if this capability is provided or the
     * display may not function correctly.
     */
    kUpdateRects                        = (1 << 0),
};

/**
 * Encapsulates all properties of a display mode.
 */
struct DisplayMode {
    enum class Bpp: uint8_t {
        Indexed8        = 8,
        RGB24           = 24,
        RGBA32          = 32,
    };

    /// Refresh rate, if supported by driver
    float refresh{0};
    /// Output pixel resolution
    std::pair<uint32_t, uint32_t> resolution;
    /// Bit depth
    Bpp bpp;
};
}

namespace rpc {
bool serialize(std::span<std::byte> &, const DriverSupport::gfx::DisplayMode &);
bool deserialize(const std::span<std::byte> &, DriverSupport::gfx::DisplayMode &);
size_t bytesFor(const DriverSupport::gfx::DisplayMode &);
}

#endif
