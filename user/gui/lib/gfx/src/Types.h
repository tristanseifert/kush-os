#ifndef GUI_LIBGFX_TYPES
#define GUI_LIBGFX_TYPES

#include <compare>

namespace gui::gfx {
/**
 * A color value with no alpha component. Component values are [0, 1].
 */
struct RgbColor {
    double r{0};
    double g{0};
    double b{0};
};

/**
 * A color value with an alpha component. Component values are [0, 1].
 */
struct RgbaColor {
    double r{0};
    double g{0};
    double b{0};
    double a{1};
};

/**
 * Represents the size of an object, in points
 */
struct Size {
    double width{0};
    double height{0};

    auto operator<=> (const Size &) const = default;
};

/**
 * Represents a location in a drawing context, in points.
 */
struct Point {
    double x{0};
    double y{0};

    auto operator<=> (const Point &) const = default;
};

/**
 * Defines a rectangular region.
 */
struct Rectangle {
    /// Top left point of the rectangle
    Point origin;
    /// Size of the rectangle, in points
    Size size;

    /**
     * Test whether the rectangle is empty, e.g. whether its origin is zero and its size is zero
     * as well.
     */
    constexpr inline bool isEmpty() const {
        return origin.x == 0 && origin.y == 0 && size.width == 0 && size.height == 0;
    }

    auto operator<=> (const Rectangle &) const = default;
};

} // namespace gui::gfx

#endif
