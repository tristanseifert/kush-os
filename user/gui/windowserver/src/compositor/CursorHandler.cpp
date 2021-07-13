#include "CursorHandler.h"
#include "Compositor.h"

#include "Log.h"

#include <gfx/Image.h>
#include <gfx/Context.h>
#include <gfx/Surface.h>

#include <toml++/toml.h>

#include <algorithm>

using namespace std::string_view_literals;

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

            // read optional info and store it in the map
            auto hotspot = info["hotspot"].as_array();
            if(hotspot && hotspot->size() == 2) {
                auto &arr = *hotspot;
                inf.hotspot = {arr[0].value_or(0.), arr[1].value_or(0.)};
            }

            if(kLogCursorLoad) Trace("Cursor %s: %.0f x %.0f, hotspot at (%.0f, %.0f)",
                    filename.c_str(), inf.size.width, inf.size.height, inf.hotspot.x,
                    inf.hotspot.y);
            cursors.emplace(type, std::move(inf));
        }
    }

    this->systemCursors = cursors;
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
    const auto &cursor = this->systemCursors.at(this->cursor);
    gui::gfx::Point origin = this->position;
    origin -= cursor.hotspot;

    // draw it
    ctx->pushState();

    ctx->setSource(cursor.surface, origin);
    ctx->rectangle(origin, cursor.size);
    ctx->fill();

    ctx->popState();

    this->cursorRect = {origin, cursor.size};
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

    // compare them
    if(lower == "normal") {
        outType = SystemCursor::Normal;
        return true;
    } else if(lower == "pointer") {
        outType = SystemCursor::Pointer;
        return true;
    } else if(lower == "move") {
        outType = SystemCursor::Move;
        return true;
    } else if(lower == "caret") {
        outType = SystemCursor::Caret;
        return true;
    }

    // if we get here, the name isn't known
    return false;
}

