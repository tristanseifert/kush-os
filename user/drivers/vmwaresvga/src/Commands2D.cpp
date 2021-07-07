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
int Commands2D::update(const std::pair<uint32_t, uint32_t> &origin,
        const std::pair<uint32_t, uint32_t> &size) {
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
