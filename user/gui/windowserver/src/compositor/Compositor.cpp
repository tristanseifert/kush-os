#include "Compositor.h"
#include "Log.h"

#include <DriverSupport/gfx/Display.h>

/**
 * Instantiates a compositor instance for the given display.
 */
Compositor::Compositor(const std::shared_ptr<DriverSupport::gfx::Display> &_d) : display(_d) {
    this->updateBuffer();
}

/**
 * Cleans up our drawing context(s) and any other resources allocated.
 */
Compositor::~Compositor() {

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
    this->bufferStride = info.pitch;

    // resize our buffer
    this->buffer.resize(info.pitch * info.h);
    Trace("New back buffer size: %lu bytes (%u x %u)", this->buffer.size(), info.w, info.h);

    // clear it
    auto fb = this->display->getFramebuffer();
    Trace("Framebuffer is %lu bytes at $%p", fb.size(), fb.data());

    for(size_t y = 0; info.h; y++) {
        const auto offset = y * info.pitch;
        const auto bytesLeft = std::min(static_cast<size_t>(info.pitch), fb.size() - offset);
        auto line = fb.subspan(offset, bytesLeft);

        if(line.empty()) break;
        memset(line.data(), 0, line.size());
    }

    // and mark it as updated
    this->display->RegionUpdated({0, 0}, this->bufferDimensions);
}
