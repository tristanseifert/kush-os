#include <cairo/cairo.h>

#include "Surface.h"
#include "Helpers.h"

#include <cassert>
#include <cstdio>

using namespace gui::gfx;

/**
 * Creates a new surface with the given dimensions and pixel format. We'll allocate memory for it
 * as part of the call.
 */
Surface::Surface(const Size &dimensions, const Format format) {
    const auto [w, h] = dimensions;
    this->backing = cairo_image_surface_create(util::ConvertFormat(format), w, h);

    auto status = cairo_surface_status(this->backing);
    if(status != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "%s failed: %d\n", "cairo_image_surface_create_for_data", status);
        abort();
    }
}

/**
 * Creates a new surface with an existing pixel buffer.
 */
Surface::Surface(std::span<std::byte> &buffer, const size_t pitch, const Format format,
        const Size &dimensions) {
    // perform validation on inputs
    assert(pitch);
    assert(format != Format::Invalid);
    assert(!buffer.empty());

    // create the surface
    const auto [w, h] = dimensions;
    this->backing = cairo_image_surface_create_for_data(
            reinterpret_cast<unsigned char *>(buffer.data()), util::ConvertFormat(format), w, h, pitch);

    auto status = cairo_surface_status(this->backing);
    if(status != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "%s failed: %d\n", "cairo_image_surface_create_for_data", status);
        abort();
    }
}

/**
 * Cleans up the surface's resources.
 */
Surface::~Surface() {
    cairo_surface_destroy(this->backing);
}



void Surface::flush() {
    cairo_surface_flush(this->backing);
}
void Surface::markDirty() {
    cairo_surface_mark_dirty(this->backing);
}
void Surface::markDirty(const Point &origin, const Size &size) {
    const auto [x, y] = origin;
    const auto [w, h] = size;

    cairo_surface_mark_dirty_rectangle(this->backing, x, y, w, h);
}


size_t Surface::getWidth() const {
    return cairo_image_surface_get_width(this->backing);
}
size_t Surface::getHeight() const {
    return cairo_image_surface_get_height(this->backing);
}
size_t Surface::getPitch() const {
    return cairo_image_surface_get_stride(this->backing);
}
const void *Surface::data() const {
    return cairo_image_surface_get_data(this->backing);
}
void *Surface::data() {
    return cairo_image_surface_get_data(this->backing);
}
Surface::Format Surface::getFormat() const {
    return util::ConvertFormat(cairo_image_surface_get_format(this->backing));
}



/**
 * Get the most optimal pitch for the given sized surface.
 */
size_t Surface::GetOptimalPitch(const Size &dimensions, const Format format) {
    return cairo_format_stride_for_width(util::ConvertFormat(format), dimensions.first);
}

