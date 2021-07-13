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

    Size() = default;
    Size(const double _width, const double _height) : width(_width), height(_height) {}

    template<typename T>
    Size(const std::pair<T, T> &p) : width(p.first), height(p.second) {}

    auto operator<=> (const Size &) const = default;
};

/**
 * Represents a location in a drawing context, in points.
 */
struct Point {
    double x{0};
    double y{0};

    Point() = default;
    Point(const double _x, const double _y) : x(_x), y(_y) {}
    template<typename T>
    Point(const std::pair<T, T> &p) : x(p.first), y(p.second) {}
    template<typename T>
    Point(const std::tuple<T, T> &p) : x(std::get<0>(p)), y(std::get<1>(p)) {}

    inline Point& operator -=(const Point& p) {
        this->x -= p.x;
        this->y -= p.y;
        return *this;
    }
    inline Point& operator +=(const Point& p) {
        this->x += p.x;
        this->y += p.y;
        return *this;
    }

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
