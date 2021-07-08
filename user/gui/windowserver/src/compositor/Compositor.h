#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <utility>

namespace DriverSupport::gfx {
class Display;
}

/**
 * The compositor handles drawing windows on an internal back buffer, which is copied to the output
 * framebuffer as regions of it are dirtied.
 *
 * @note Currently, we only support 32bpp back buffers.
 */
class Compositor {
    public:
        /// Create a compositor instance for the given display
        Compositor(const std::shared_ptr<DriverSupport::gfx::Display> &display);
        /// Cleans up all internal buffers.
        ~Compositor();

    private:
        /// Resizes the buffer in response to the display size changing
        void updateBuffer();

    private:
        /// Display for which we're responsible
        std::shared_ptr<DriverSupport::gfx::Display> display;

        /// Dimensions of back buffer (width, height)
        std::pair<uint32_t, uint32_t> bufferDimensions{0, 0};
        /// stride (bytes per line) in buffer
        size_t bufferStride{0};
        /// Back buffer
        std::vector<std::byte> buffer;
};
