#include "Image.h"

#include <png.h>

#include <gfx/Surface.h>

#include <cassert>
#include <cerrno>
#include <cstdio>
#include <vector>

using namespace gui::gfx;

static uint32_t MultiplyAlpha(const uint32_t color);

/// Error codes for image loading
enum Errors: int {
    /// The image type is not what the routine expected.
    InvalidImageType                    = -101000,
    /// Failed to initialize image loader
    InitializationError                 = -101001,
    /// Image dimensions are invalid
    InvalidDimensions                   = -101002,
    /// Pixel format of image is not supported
    UnsupportedPixelFormat              = -101003,
    /// Unknown error while decoding image
    UnknownError                        = -101004,
};

/**
 * Loads a PNG image using libpng.
 *
 * This can currently handle 8 bit RGB and RGBA images. This should be expanded to handle the
 * full range of image formats that PNG supports; though Cairo only supports 10 bits per component
 * at most, so 16 bit formats are probably not worth bothering with.
 */
int gui::gfx::LoadPng(const std::string_view &path, std::shared_ptr<Surface> &outSurface) {
    int ret{0};

    png_structp png{nullptr};
    png_infop info{nullptr};
    uint32_t width, height;
    uint8_t colorType, bitDepth;
    bool convertAlpha{false};
    std::shared_ptr<Surface> sfc;

    std::vector<png_bytep> rows;
    std::byte *base;
    size_t pitch;

    // open file and ensure it's a PNG
    auto fp = fopen(path.data(), "rb");
    if(!fp) {
        return errno;
    }

    // create libpng buffers
    png = png_create_read_struct(PNG_LIBPNG_VER_STRING, nullptr, nullptr, nullptr);
    if(!png) {
        ret = Errors::InitializationError;
        goto beach;
    }

    info = png_create_info_struct(png);
    if(!info) {
        ret = Errors::InitializationError;
        goto beach;
    }

    // read file and get info
    png_init_io(png, fp);
    png_read_info(png, info);

    width = png_get_image_width(png, info);
    height = png_get_image_height(png, info);
    colorType = png_get_color_type(png, info);
    bitDepth = png_get_bit_depth(png, info);

    if(!width || !height) {
        ret = Errors::InvalidDimensions;
        goto beach;
    }

    // convert paletted images to truecolor; also expand greyscale to 8 bits
    if(colorType == PNG_COLOR_TYPE_PALETTE) {
        png_set_palette_to_rgb(png);
    } else if(colorType == PNG_COLOR_TYPE_GRAY && bitDepth < 8) {
        png_set_expand_gray_1_2_4_to_8(png);
    }

    // determine the surface format to use
    if(bitDepth != 8) {
        ret = Errors::UnsupportedPixelFormat;
        goto beach;
    }

    Surface::Format fmt;
    switch(colorType) {
        case PNG_COLOR_TYPE_GRAY:
            fmt = Surface::Format::A8;
            break;
        case PNG_COLOR_TYPE_RGB:
            fmt = Surface::Format::RGB24;
            break;
        // PNG reads as RGBA by default; flip to read as ARGB instead
        case PNG_COLOR_TYPE_RGBA:
            fmt = Surface::Format::ARGB32;
            convertAlpha = true;
            //png_set_swap_alpha(png);
            break;
        default:
            ret = Errors::UnsupportedPixelFormat;
            goto beach;
    }

    png_read_update_info(png, info);

    // allocate the surface
    sfc = std::make_shared<Surface>(std::make_pair(width, height), fmt);
    if(!sfc) {
        ret = Errors::UnknownError;
        goto beach;
    }

    // allocate and populate the row pointers array
    sfc->flush();

    base = reinterpret_cast<std::byte *>(sfc->data());
    pitch = sfc->getPitch();

    rows.reserve(height);

    for(size_t y = 0; y < height; y++) {
        rows.emplace_back(reinterpret_cast<png_bytep>(base) + (pitch * y));
    }

    // read the image
    assert(rows.size() == height);
    png_read_image(png, rows.data());

    // convert alpha channel from straight to premultiplied if needed
    if(convertAlpha) {
        for(size_t y = 0; y < height; y++) {
            auto bytes = reinterpret_cast<uint32_t *>(rows[y]);

            for(size_t x = 0; x < width; x++) {
                bytes[x] = MultiplyAlpha(bytes[x]);
            }
        }
    }

    // invalidate surface caches and output it
    sfc->markDirty();
    outSurface = sfc;

beach:;
    // clean up
    fclose(fp);

    png_destroy_read_struct(&png, &info, nullptr);
    return ret;
}



/**
 * Converts a pixel (in AARRGGBB order) from straight alpha to premultiplied.
 */
static uint32_t MultiplyAlpha(const uint32_t color) {
    const uint32_t a = color & 0xFF000000;

    // Better behavior at a=255 by adding 1 to it. See http://stereopsis.com/doubleblend.html
    const uint32_t a1 = (a >> 24) + 1;

    uint32_t rb = color & 0xFF00FF;
    uint32_t g  = color & 0xFF00;
    rb *= a1;
    g  *= a1;
    rb >>= 8;
    g  >>= 8;
    rb &= 0xFF00FF;
    g &= 0xFF00;

    return rb | g | a;
}

