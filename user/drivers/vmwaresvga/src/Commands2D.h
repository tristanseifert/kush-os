#pragma once

#include <cstddef>
#include <cstdint>
#include <utility>

class SVGA;

namespace svga {
/**
 * Provides wrapper for 2D commands
 */
class Commands2D {
    public:
        Commands2D(::SVGA *device);

        /// Marks a rectangular region of the screen as needing to be redrawn.
        [[nodiscard]] int update(const std::pair<uint32_t, uint32_t> &origin,
                const std::pair<uint32_t, uint32_t> &size);
        /// Marks the entire framebuffer as needing to be redrawn.
        [[nodiscard]] int update();

    private:
        SVGA *s;
};
}
