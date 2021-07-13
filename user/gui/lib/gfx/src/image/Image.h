#ifndef GUI_LIBGFX_IMAGE_H
#define GUI_LIBGFX_IMAGE_H

#include <memory>
#include <string_view>

namespace gui::gfx {
class Surface;

/**
 * Loads a PNG image, allocates a surface to contain it, and copies its pixels into the surface.
 *
 * @param path Path to the PNG file
 * @param outSurface On success, the pointer that will contain the allocated surface
 *
 * @return 0 on success, error code otherwise
 */
int LoadPng(const std::string_view &path, std::shared_ptr<Surface> &outSurface);
} // namespace gui::gfx

#endif
