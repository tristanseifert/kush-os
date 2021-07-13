#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string_view>
#include <tuple>
#include <unordered_map>

#include <gfx/Types.h>
#include <gfx/Context.h>

class Compositor;

/**
 * Handles cursor acceleration, mouse event dispatching and other such fun stuff.
 */
class CursorHandler {
    /// Path of cursor configuration
#ifdef __Kush__
    constexpr static const std::string_view kConfigFile{"/System/Data/windowserver/cursors.toml"};
#else
    constexpr static const std::string_view kConfigFile{"../cursors.toml"};
#endif
    
    public:
        /// Defines the various types of system cursors
        enum class SystemCursor {
            /// Standard pointer
            Normal,
            /// Pointer (like for links)
            Pointer,
            /// Movement cursor
            Move,
            /// Text insertion bar (I-Bar)
            Caret,
        };

    public:
        CursorHandler(Compositor *comp);

        /// Draws the current mouse cursor on the given graphics context.
        void draw(const std::unique_ptr<gui::gfx::Context> &ctx);
        /// Handles a mouse movement/button event.
        void handleEvent(const std::tuple<int, int, int> &move, const uintptr_t buttons);

        /// Returns the rectangle in which the cursor was most recently drawn.
        constexpr auto &getCursorRect() const {
            return this->cursorRect;
        }

    private:
        /**
         * Contains information on a single loaded cursor, including its hotspot.
         */
        struct CursorInfo {
            /// Size of the actual cursor image
            gui::gfx::Size size;
            /// Cursor hotspot
            gui::gfx::Point hotspot;

            /// Number of frames for the cursor
            size_t numFrames{1};

            /// Surface holding the image(s) for the cursor
            std::shared_ptr<gui::gfx::Surface> surface;
        };

    private:
        void loadCursors();

        /// Generates a mouse movement event and sends it to all interested parties
        void distributeMoveEvent();
        /// Generates a mouse button event and sends it to all interested parties
        void distributeButtonEvent();
        /// Generates a mouse scroll wheel event and sends it to all interested parties
        void distributeScrollEvent(const int delta);

        /// Translates a cursor name string to enum type
        static bool TranslateTypeName(const std::string_view &name, SystemCursor &outType);

    private:
        /// Whether loading of cursors is logged
        constexpr static const bool kLogCursorLoad{false};

        /// Compositor instance for which we are responsible for handling the cursor
        Compositor *comp;

        /// Current screen absolute mouse position
        std::tuple<uint32_t, uint32_t> position{32, 32};
        /// Buttons that are currently pushed down
        uint32_t buttonState{0};

        /// rect at which the cursor was last drawn
        gui::gfx::Rectangle cursorRect;

        /// system cursor images
        std::unordered_map<SystemCursor, CursorInfo> systemCursors;
        /// current system cursor to use
        SystemCursor cursor{SystemCursor::Normal};
};
