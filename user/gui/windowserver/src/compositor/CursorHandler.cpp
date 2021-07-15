#include "CursorHandler.h"
#include "Compositor.h"

#include "Log.h"

#include <gfx/Image.h>
#include <gfx/Context.h>
#include <gfx/Pattern.h>
#include <gfx/Surface.h>

#include <toml++/toml.h>

#include <algorithm>

using namespace std::string_view_literals;

/**
 * This map contains a list of lower case cursor names to the corresponding cursor enum.
 */
const std::unordered_map<std::string_view, CursorHandler::SystemCursor> CursorHandler::gCursorNameMap{
    {"pointer.normal"sv,        SystemCursor::PointerNormal},
    {"pointer.help"sv,          SystemCursor::PointerHelp},
    {"pointer.prohibited"sv,    SystemCursor::PointerProhibited},
    {"pointer.add"sv,           SystemCursor::PointerAdd},
    {"pointer.menu"sv,          SystemCursor::PointerMenu},
    {"pointer.link"sv,          SystemCursor::PointerLink},
    {"pointer.progress"sv,      SystemCursor::PointerProgress},

    {"hand.pointer"sv,          SystemCursor::HandPointer},
    {"hand.closed"sv,           SystemCursor::HandClosed},
    {"hand.open"sv,             SystemCursor::HandOpen},

    {"text.ibar"sv,             SystemCursor::Caret},

    {"move"sv,                  SystemCursor::Move},

    {"resize.col"sv,            SystemCursor::ResizeColumn},
    {"resize.row"sv,            SystemCursor::ResizeRow},

    {"resize.e"sv,              SystemCursor::ResizeEast},
    {"resize.ew"sv,             SystemCursor::ResizeEastWest},
    {"resize.n"sv,              SystemCursor::ResizeNorth},
    {"resize.ne"sv,             SystemCursor::ResizeNorthEast},
    {"resize.nesw"sv,           SystemCursor::ResizeNorthEastSouthWest},
    {"resize.ns"sv,             SystemCursor::ResizeNorthSouth},
    {"resize.nw"sv,             SystemCursor::ResizeNorthWest},
    {"resize.nwse"sv,           SystemCursor::ResizeNorthWestSouthEast},
    {"resize.s"sv,              SystemCursor::ResizeSouth},
    {"resize.se"sv,             SystemCursor::ResizeSouthEast},
    {"resize.sw"sv,             SystemCursor::ResizeSouthWest},
    {"resize.w"sv,              SystemCursor::ResizeWest},

    {"cross"sv,                 SystemCursor::Cross},
    {"crosshair"sv,             SystemCursor::Crosshair},
    {"colorpicker"sv,           SystemCursor::ColorPicker},

    {"zoom.in"sv,               SystemCursor::ZoomIn},
    {"zoom.out"sv,              SystemCursor::ZoomOut},

    {"wait"sv,                  SystemCursor::Wait},
};

/**
 * Initializes the cursor handler.
 */
CursorHandler::CursorHandler(Compositor *_comp) : comp(_comp) {
    // load system cursors and set the default cursor
    this->loadCursors();

    // set the default cursor
}

/**
 * Loads cursors from the filesystem. This is done by reading a TOML file, which in turn defines
 * the cursors to load, their hot spots, and so forth.
 */
void CursorHandler::loadCursors() {
    int err;
    toml::parse_result res = toml::parse_file(kConfigFile);

    if(!res) {
        const auto &err = res.error();
        auto &beg = err.source().begin;
        Abort("Failed to parse cursor config at %s %lu:%lu: %s", kConfigFile.data(), beg.line,
                beg.column, err.description());
    }

    // get the general configuration options
    const toml::table &tab = res;
    const std::string baseDir = tab["base"].value_or("/");

    // then load each of the cursors
    std::unordered_map<SystemCursor, CursorInfo> cursors;

    if(auto arr = tab["cursors"].as_array()) {
        for(auto &elem : *arr) {
            auto info = *elem.as_table();

            // get its required info
            const std::string filename = info["file"].value_or("");
            const std::string_view typeStr = info["name"].value_or(""sv);

            if(filename.empty() | typeStr.empty()) {
                Abort("Invalid entry: type %s filename %s", typeStr.data(), filename.c_str());
            }

            // convert type and build path
            auto path = baseDir + "/" + filename;

            SystemCursor type;
            if(!TranslateTypeName(typeStr, type)) {
                Warn("Failed to translate cursor type '%s'", typeStr.data());
                continue;
            }

            // try to read it into a surface
            CursorInfo inf;

            err = gui::gfx::LoadPng(path, inf.surface);
            if(err) {
                Warn("Failed to load '%s': %d", path.c_str(), err);
                continue;
            }

            inf.size = inf.surface->getSize();

            // read animation info (if provided)
            auto anim = info["animation"].as_table();
            if(anim) {
                auto &animInfo = *anim;
                inf.numFrames = animInfo["frames"].value_or(1);
                inf.frameDelay = animInfo["delay"].value_or(100);

                inf.size.width /= inf.numFrames;
            }

            // read optional info and store it in the map
            auto hotspot = info["hotspot"].as_array();
            if(hotspot && hotspot->size() == 2) {
                auto &arr = *hotspot;
                inf.hotspot = {arr[0].value_or(0.), arr[1].value_or(0.)};
            }

            if(kLogCursorLoad) Trace("Cursor %s: %.0f x %.0f, hotspot at (%.0f, %.0f) "
                    "with %lu frames (delay %u ms)",
                    filename.c_str(), inf.size.width, inf.size.height, inf.hotspot.x,
                    inf.hotspot.y, inf.numFrames, inf.frameDelay);
            cursors.emplace(type, std::move(inf));
        }
    }

    this->systemCursors = cursors;
    if(kLogCursorLoad) Trace("Loaded %lu cursors (total %lu kinds known)", cursors.size(),
            gCursorNameMap.size());
}

/**
 * Handles a mouse movement event.
 */
void CursorHandler::handleEvent(const std::tuple<int, int, int> &move, const uintptr_t buttons) {
    const auto [dX, dY, dZ] = move;
    bool redrawCursor{false};

    // handle mouse movement
    if(dX || dY) {
        const auto [screenWidth, screenHeight] = this->comp->bufferDimensions;

        // get new position
        auto pos = this->position;
        int newX = std::get<0>(pos) + dX;
        int newY = std::get<1>(pos) + dY;

        // then clamp it
        std::get<0>(pos) = std::clamp(newX, 0, (int) screenWidth);
        std::get<1>(pos) = std::clamp(newY, 0, (int) screenHeight);

        // update cursor if it moved
        if(pos != this->position) {
            redrawCursor = true;
            this->position = pos;

            this->distributeMoveEvent();
        }
    }

    // handle scroll wheel
    if(dZ) {
        this->distributeScrollEvent(dZ);
    }

    // handle buttons
    if(buttons != this->buttonState) {
        this->buttonState = buttons;
        redrawCursor = true;

        this->distributeButtonEvent();
    }

    // redraw cursor
    if(redrawCursor) {
        this->comp->notifyWorker(Compositor::kCursorUpdateBit);
    }
}

/**
 * Sends a mouse movement event to the key application, if it wants them. Applications must opt in
 * to receiving the unsolicited mouse movement events.
 */
void CursorHandler::distributeMoveEvent() {
    //const auto [x, y] = this->position;
    //Trace("Mouse moved: (%4u, %4u)", x, y);
}

/**
 * Sends a mouse button event to the key application. This includes the screen absolute position of
 * the cursor.
 */
void CursorHandler::distributeButtonEvent() {
    const auto [x, y] = this->position;
    Trace("Mouse clicked: %08x (%4u, %4u)", this->buttonState, x, y);
}

/**
 * Sends a scroll wheel event to the key application. We don't keep track of the state of the
 * scroll wheel as that's a rather meaningless value, and we're more interested in the relative
 * movement of the wheel.
 */
void CursorHandler::distributeScrollEvent(const int delta) {
    Trace("Scroll wheel: %d", delta);
}



/**
 * Draws the current mouse cursor.
 */
void CursorHandler::draw(const std::unique_ptr<gui::gfx::Context> &ctx) {
    // calculate the cursor's origin
    auto &cursor = this->systemCursors.at(this->cursor);
    gui::gfx::Point origin = this->position;
    //origin -= cursor.hotspot;

    // if the cursor is animated, shift the surface's origin
    auto imageOrigin = origin;

    if(cursor.numFrames) {
        imageOrigin.x -= (cursor.size.width * cursor.currentFrame);
    }

    // draw it
    ctx->pushState();

    ctx->setSource(cursor.surface, imageOrigin);
    ctx->rectangle(origin, cursor.size);
    ctx->fill();

    ctx->popState();

    this->cursorRect = {origin, cursor.size};
}

/**
 * Increments the animation frame for the current cursor.
 *
 * @return Whether the cursor image has changed and needs to be redrawn
 */
bool CursorHandler::tick() {
    auto &cursor = this->systemCursors.at(this->cursor);
    cursor.currentFrame = (cursor.currentFrame + 1) % cursor.numFrames;

    return true;
}



/**
 * Translates a string cursor type to the appropriate enum value.
 *
 * @return Whether the name was known or not
 */
bool CursorHandler::TranslateTypeName(const std::string_view &name, SystemCursor &outType) {
    // convert string to lowercase
    std::string lower(name);
    std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);

    // look up in map
    if(gCursorNameMap.contains(name)) {
        outType = gCursorNameMap.at(name);
        return true;
    }

    // if we get here, the name isn't known
    return false;
}

