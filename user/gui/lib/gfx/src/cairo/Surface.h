#ifndef GUI_LIBGFX_SURFACE_H
#define GUI_LIBGFX_SURFACE_H

#include <cstddef>
#include <cstdint>
#include <utility>
#include <span>

struct _cairo_surface;

namespace gui::gfx {
/**
 * Surfaces represent regions of memory that can be drawn in.
 */
class Surface {
    friend class Context;
    friend class Pattern;

    public:
        using Point = std::pair<int, int>;
        using Size = std::pair<int, int>;

        /// Formats that a surface may take on
        enum class Format {
            /// Invalid or unsupported format
            Invalid,

            /// 32-bit ARGB, stored in native byte order with premultiplied alpha
            ARGB32,
            /// 24-bit RGB, stored in native byte order without an alpha channel
            RGB24,
            /// 8 bit, single channel, holding an alpha value
            A8,
            /// 1 bit, single channel, holding an alpha value; pixels are matched into 32-bit words
            A1,
            /// 16-bit RGB pixels, where red and blue take 5 bits and green takes the middle 6
            RGB565,
            /// 32-bit RGB with 10 bits per color component (similar to RGB24)
            RGB30,
        };

    public:
        /**
         * Determine the most optimal pitch (bytes per row) for the given dimensions and pixel
         * format. This pitch is guaranteed to meet all internal alignment constraints of the
         * graphics library to allow the most optimal rendering paths.
         *
         * @param dimensions Size of the surface
         * @param format Pixel format of the surface
         *
         * @return Pitch to use for a surface with the given size and format
         */
        static size_t GetOptimalPitch(const Size &dimensions, const Format format);

    public:
        /**
         * Create a surface with the given dimensions; its backing store is allocated on the heap
         * automatically.
         *
         * @param dimensions The width and height of the surface, in pixels
         * @param format Pixel format to use for the allocated surface
         */
        Surface(const Size &dimensions, const Format format = Format::ARGB32);

        /**
         * Create a surface with the given dimensions and pixel format, but with an already
         * allocated pixel buffer.
         *
         * @param buffer Pixel buffer to use for the surface
         * @param pitch Pitch of the pixel buffer (bytes per row)
         * @param format Pixel format to use for the surface
         * @param dimensions The width and height of the surface, in pixels
         *
         * @note The provided memory buffer _must_ exist for as long as the surface is being used
         * in any draw calls.
         */
        Surface(std::span<std::byte> &buffer, const size_t pitch, const Format format,
                const Size &dimensions);

        ~Surface();

        /**
         * Force all pending draw calls to be performed and written to the surface's underlying
         * backing store.
         */
        void flush();
        /**
         * Indicates that the underlying backing store of the surface has been modified outside of
         * the graphics library, and that any internal caches of the region should be invalidated.
         */
        void markDirty();
        /**
         * Indicate that the specified rectangular region of the surface's backing store has been
         * modified outside of the graphics library.
         */
        void markDirty(const Point &point, const Size &size);

        /// Get a pointer to the buffer that this surface is backing
        const void *data() const;
        /// Get a pointer to the buffer that this surface is backing
        void *data();

        /// Get the width of the surface
        size_t getWidth() const;
        /// Get the height of the surface
        size_t getHeight() const;
        /// Get the pitch (bytes per row) of the surface
        size_t getPitch() const;
        /// Get the pixel format of this surface
        Format getFormat() const;

    private:
        /// Cairo surface we're representing
        struct _cairo_surface *backing{nullptr};
};
} // namespace gui::gfx

#endif
