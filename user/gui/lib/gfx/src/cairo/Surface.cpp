#include <cairo/cairo.h>

#include "Surface.h"

#include <cassert>
#include <cstdio>

using namespace gui::gfx;

static Surface::Format ConvertFormat(const cairo_format_t f);
static cairo_format_t ConvertFormat(const Surface::Format f);

/**
 * Creates a new surface with the given dimensions and pixel format. We'll allocate memory for it
 * as part of the call.
 */
Surface::Surface(const Size &dimensions, const Format format) {
    const auto [w, h] = dimensions;
    this->backing = cairo_image_surface_create(ConvertFormat(format), w, h);

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
            reinterpret_cast<unsigned char *>(buffer.data()), ConvertFormat(format), w, h, pitch);

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
    return ConvertFormat(cairo_image_surface_get_format(this->backing));
}



/**
 * Get the most optimal pitch for the given sized surface.
 */
size_t Surface::GetOptimalPitch(const Size &dimensions, const Format format) {
    return cairo_format_stride_for_width(ConvertFormat(format), dimensions.first);
}



/**
 * Convert from the Cairo pixel format enum
 */
static Surface::Format ConvertFormat(const cairo_format_t f) {
    using Format = Surface::Format;

    switch(f) {
        case CAIRO_FORMAT_ARGB32:
            return Format::ARGB32;
        case CAIRO_FORMAT_RGB24:
            return Format::RGB24;
        case CAIRO_FORMAT_A8:
            return Format::A8;
        case CAIRO_FORMAT_A1:
            return Format::A1;
        case CAIRO_FORMAT_RGB16_565:
            return Format::RGB565;
        case CAIRO_FORMAT_RGB30:
            return Format::RGB30;

        case CAIRO_FORMAT_INVALID:
            return Format::Invalid;

        default:
            return Format::Invalid;
    }
}
/**
 * Convert to the Cairo pixel format enum.
 */
static cairo_format_t ConvertFormat(const Surface::Format f) {
    using Format = Surface::Format;

    switch(f) {
        case Format::ARGB32:
            return CAIRO_FORMAT_ARGB32;
        case Format::RGB24:
            return CAIRO_FORMAT_RGB24;
        case Format::A8:
            return CAIRO_FORMAT_A8;
        case Format::A1:
            return CAIRO_FORMAT_A1;
        case Format::RGB565:
            return CAIRO_FORMAT_RGB16_565;
        case Format::RGB30:
            return CAIRO_FORMAT_RGB30;

        case Format::Invalid:
            return CAIRO_FORMAT_INVALID;
    }
}
