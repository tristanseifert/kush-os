#include "Commands2D.h"
#include "FIFO.h"
#include "SVGA.h"

#include "Log.h"

#include <svga_reg.h>

using namespace svga;

/**
 * Initializes the 2D commands handler.
 */
Commands2D::Commands2D(SVGA *device) : s(device) {
}



/**
 * Notifies the GPU that the given region of the framebuffer has been updated and needs to be
 * redrawn.
 *
 * @param origin A pair of (x, y) coordinates defining the base of the rectangle that updated
 * @param size The dimensions (width, height) of the rectangle that was updated
 *
 * @return 0 on success or an error code
 */
int Commands2D::update(const Point &origin, const Size &size) {
    int err;
    std::span<std::byte> cmdBuf;

    // reserve command
    err = this->s->fifo->reserveCommand(SVGA_CMD_UPDATE, sizeof(SVGAFifoCmdUpdate), cmdBuf);
    if(err) return err;

    // fill out the command
    auto cmd = reinterpret_cast<SVGAFifoCmdUpdate *>(cmdBuf.data());
    std::tie(cmd->x, cmd->y) = origin;
    std::tie(cmd->width, cmd->height) = size;

    // submit
    return this->s->fifo->commitAll();
}

/**
 * Notifies the GPU that the entire framebuffer (that is, a rectangle at the origin with the size
 * of the framebuffer) needs updating.
 *
 * @return 0 on success or an error code
 */
int Commands2D::update() {
    auto size = this->s->getFramebufferDimensions();
    return this->update({0, 0}, size);
}

/**
 * Defines the cursor image.
 *
 * TODO: Implement this
 */
int Commands2D::defineCursor(const Point &hotspot, const Size &size,
        const std::span<uint32_t> &bitmap) {
    return -1;
}

/**
 * Updates the cursor's position and visibility state from the cached values.
 */
int Commands2D::updateCursor() {
    // use FIFO register mechanism if supported
    if(this->s->fifo->hasCapability(SVGA_FIFO_CAP_CURSOR_BYPASS_3)) {
        auto fm = this->s->fifo->fifo;

        fm[SVGA_FIFO_CURSOR_ON] = this->cursorVisible;
        std::tie(fm[SVGA_FIFO_CURSOR_X], fm[SVGA_FIFO_CURSOR_Y]) = this->cursorPos;
        fm[SVGA_FIFO_CURSOR_COUNT]++;
    }
    // otherwise, fall back to the legacy register based approach
    else {
        const auto [x, y] = this->cursorPos;

        this->s->regWrite(SVGA_REG_CURSOR_ID, 0);
        this->s->regWrite(SVGA_REG_CURSOR_ON, this->cursorVisible);
        this->s->regWrite(SVGA_REG_CURSOR_X, x);
        this->s->regWrite(SVGA_REG_CURSOR_Y, y);
    }

    return 0;
}

