#include "Compositor.h"
#include "Log.h"

#include <DriverSupport/gfx/Display.h>

#include <gfx/Context.h>
#include <gfx/Surface.h>

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

    // update the backing surface and recreate context
    auto fb = this->display->getFramebuffer();
    this->surface = std::make_shared<gui::gfx::Surface>(fb, info.pitch,
            gui::gfx::Surface::Format::ARGB32, this->bufferDimensions);

    this->context = std::make_unique<gui::gfx::Context>(this->surface);

    // clear it
    this->context->setSource({1, 0, 1, 1});
    this->context->rectangle({8, 64}, {64, 64});
    this->context->fill();

    this->context->setLineWidth(4);
    this->context->arc({256, 256}, 100, {0, 2});
    this->context->stroke();

    this->surface->flush();

    // and mark it as updated
    this->display->RegionUpdated({0, 0}, this->bufferDimensions);
}

