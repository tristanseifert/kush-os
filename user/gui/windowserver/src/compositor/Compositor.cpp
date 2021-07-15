#include "Compositor.h"
#include "CursorHandler.h"

#include "Log.h"

#include <DriverSupport/gfx/Display.h>

#ifdef __Kush__
#include <threads.h>
#endif

#include <gfx/Context.h>
#include <gfx/Surface.h>
#include <gfx/Pattern.h>

/**
 * Instantiates a compositor instance for the given display.
 */
Compositor::Compositor(const std::shared_ptr<DriverSupport::gfx::Display> &_d) : display(_d) {
    this->cursor = std::make_unique<CursorHandler>(this);

#ifdef __Kush__
    this->worker = std::make_unique<std::thread>(&Compositor::workerMain, this);
#else
    this->updateBuffer();
#endif
}

/**
 * Cleans up our drawing context(s) and any other resources allocated.
 */
Compositor::~Compositor() {
    // shut down the worker
    this->notifyWorker(kShutdownBit);
}

/**
 * Updates the dimensions of the buffer as well as its stride and other information, then
 * reallocates the internal back buffer to match this.
 */
void Compositor::updateBuffer() {
    // update framebuffer info
    auto info = this->display->GetFramebufferInfo();
    REQUIRE(!info.status, "Failed to get framebuffer info: %d", info);

    this->bufferDimensions = {info.w, info.h};

    // update the backing surface and recreate context
    auto fb = this->display->getFramebuffer();
    this->surface = std::make_shared<gui::gfx::Surface>(fb, info.pitch,
            gui::gfx::Surface::Format::ARGB32, this->bufferDimensions);

    this->context = std::make_unique<gui::gfx::Context>(this->surface);

    // clear it
    this->context->setSource(gui::gfx::RgbColor{0, 0, 0});
    this->context->paint();

    this->draw(kDrawEverything);

    // and mark it as updated
    this->surface->flush();
    this->display->RegionUpdated({0, 0}, this->bufferDimensions);
}



#ifdef __Kush__
/**
 * Main loop for the worker thread. We'll wait to receive notifications forever and redraw the
 * display in response to them.
 */
void Compositor::workerMain() {
    uintptr_t note;

    // set up buffer initially
    this->updateBuffer();

    // handle events
    while(this->run) {
        bool needsDraw{false};
        uintptr_t drawWhat{0};

        // receive notifications
        note = NotificationReceive(UINTPTR_MAX, 16666);
        if(!note) {
            // animate cursor
            const bool cursorUpdated = this->cursor->tick();
            if(cursorUpdated) {
                needsDraw = true;
                drawWhat |= kCursorUpdateBit;
            }

            goto draw;
        }

        // system events
        if(note & kShutdownBit) {
            this->run = false;
            continue;
        }
        if(note & kUpdateBufferBit) {
            this->updateBuffer();
        }

        // redraw events
        if(note & kCursorUpdateBit) {
            needsDraw = true;
            drawWhat |= kCursorUpdateBit;
        }

draw:;
        // perform the drawing if needed
        if(needsDraw) {
            this->draw(drawWhat);
        }
    }

    // clean up
    this->context.reset();
    this->surface.reset();
}
#endif

/**
 * Sends a notification to the worker thread.
 */
void Compositor::notifyWorker(const uintptr_t bits) {
#ifdef __Kush__
    if(!bits) return;
    if(!this->run) return;

    auto workLoopThrd = reinterpret_cast<thrd_t>(this->worker->native_handle());
    const auto thread = thrd_get_handle_np(workLoopThrd);

    auto err = NotificationSend(thread, bits);
    if(err) {
        Warn("%s failed: %d", "NotificationSend", err);
    }
#endif
}


/**
 * Redraws the output display and updates the framebuffer.
 *
 * We try to be smart about this and only update the regions of the display that actually changed;
 * for example, when the cursor moves, we only redraw the part of the display where the old cursor
 * was, and then draw the new cursor on top; this is done via clever use of clip rects.
 */
void Compositor::draw(const uintptr_t what) {
    // redraw windows
    if(what & kUpdateBufferBit) {
        this->drawWindows();
    }

    // add a clipping rect for the last cursor position if it changed
    if(what & kCursorUpdateBit) {
        auto rect = this->cursor->getCursorRect();
        if(!rect.isEmpty()) {
            // truncate rect to positive coordinates only
            if(rect.origin.x < 0) {
                rect.size.width += rect.origin.x;
                rect.origin.x = 0;
            }
            if(rect.origin.y < 0) {
                rect.size.height += rect.origin.y;
                rect.origin.y = 0;
            }

            // then set up the clipping rectangle
            this->context->pushState();

            this->context->clipReset();
            this->context->rectangle(rect);
            this->context->clip();

            // and draw the windows in it
            this->drawWindows();
            this->context->popState();
        }
    }

    // draw cursor
    this->cursor->draw(this->context);

    // TODO: use more granular update region
    this->display->RegionUpdated({0, 0}, this->bufferDimensions);
}

/**
 * Draws all application windows.
 *
 * You should have the clipping rects for the context configured appropriately to redraw only the
 * parts of the screen that are desired.
 */
void Compositor::drawWindows() {
    // set up state
    this->context->pushState();
    this->context->setSource(gui::gfx::RgbColor{0.2, 0, 0});

    this->context->paint();

    // restore state
    this->context->popState();
}



/**
 * Handles a mouse event. This is pushed into the cursor handler, which will then request a redraw
 * if needed.
 */
void Compositor::handleMouseEvent(const std::tuple<int, int, int> &move, const uintptr_t buttons) {
    this->cursor->handleEvent(move, buttons);
}

/**
 * Handles a keyboard event. The event is simply dispatched to the application that's currently
 * the key window.
 */
void Compositor::handleKeyEvent(const uint32_t scancode, const bool release) {
    Trace("Key event: %5s %08x", release ? "break" : "make", scancode);
}
