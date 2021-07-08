#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>

class SVGA;

namespace svga {
/**
 * Provides wrapper for 2D commands
 */
class Commands2D {
    public:
        using Point = std::pair<size_t, size_t>;
        using Size = std::pair<size_t, size_t>;

    public:
        Commands2D(::SVGA *device);

        /// Marks a rectangular region of the screen as needing to be redrawn.
        [[nodiscard]] int update(const Point &origin, const Size &size);
        /// Marks the entire framebuffer as needing to be redrawn.
        [[nodiscard]] int update();

        /// Defines the 32-bit BGRA image used for the cursor.
        [[nodiscard]] int defineCursor(const Point &hotspot, const Size &size,
                const std::span<uint32_t> &bitmap);

        /// Sets the visibility of the mouse cursor without changing its position.
        [[nodiscard]] int setCursorVisible(const bool visible) {
            this->cursorVisible = visible;
            return this->updateCursor();
        }
        /// Sets the position of the mouse cursor.
        [[nodiscard]] int setCursorPos(const Point &origin) {
            this->cursorPos = origin;
            return this->updateCursor();
        }
        /// Sets the position of the mouse cursor and updates its visibility state.
        [[nodiscard]] int setCursorPos(const Point &origin, const bool visible) {
            this->cursorVisible = visible;
            this->cursorPos = origin;
            return this->updateCursor();
        }

    private:
        /// Updates the device registers/FIFO registers for the cursor.
        int updateCursor();

    private:
        SVGA *s;

        /// last cursor position
        Point cursorPos{0, 0};
        /// whether the cursor is visible
        bool cursorVisible{false};
};
}
