#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>
#include <tuple>
#include <thread>
#include <utility>

class CursorHandler;

namespace DriverSupport::gfx {
class Display;
}

namespace gui::gfx {
class Context;
class Surface;
}

/**
 * The compositor handles drawing windows on an internal back buffer, which is copied to the output
 * framebuffer as regions of it are dirtied.
 *
 * @note Currently, we only support 32bpp back buffers.
 */
class Compositor {
    friend class CursorHandler;

    constexpr static const uintptr_t kUpdateBufferBit{1 << 0};
    constexpr static const uintptr_t kCursorUpdateBit{1 << 1};
    constexpr static const uintptr_t kShutdownBit{1 << 16};

    public:
        /// Create a compositor instance for the given display
        Compositor(const std::shared_ptr<DriverSupport::gfx::Display> &display);
        /// Cleans up all internal buffers.
        ~Compositor();

        /// Handles a mouse movement event
        void handleMouseEvent(const std::tuple<int, int, int> &move, const uintptr_t buttons);
        /// Handles a keyboard event
        void handleKeyEvent(const uint32_t scancode, const bool release);

    private:
        /// Resizes the buffer in response to the display size changing
        void updateBuffer();

        /// Main loop for the rendering thread
        void workerMain();
        /// Sends a notification to the worker thread.
        void notifyWorker(const uintptr_t bits);

        /// Redraws the parts of the display that have changed
        void draw(const uintptr_t what);
        /// Redraws windows under the current clip region
        void drawWindows();

    private:
        /// Display for which we're responsible
        std::shared_ptr<DriverSupport::gfx::Display> display;

        /// Dimensions of back buffer (width, height)
        std::pair<uint32_t, uint32_t> bufferDimensions{0, 0};

        /// graphics context
        std::unique_ptr<gui::gfx::Context> context;
        /// bitmap surface backed by the framebuffer
        std::shared_ptr<gui::gfx::Surface> surface;

        /// cursor drawing
        std::unique_ptr<CursorHandler> cursor;

        /// render thread
        std::unique_ptr<std::thread> worker;
        /// whether the render thread shall execute
        std::atomic_bool run{true};
};
