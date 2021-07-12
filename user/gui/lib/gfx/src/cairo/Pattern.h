#ifndef GUI_LIBGFX_PATTERN_H
#define GUI_LIBGFX_PATTERN_H

#include <memory>

#include <gfx/Types.h>

struct _cairo_pattern;

namespace gui::gfx {
class Surface;

class Pattern {
    using PatternPtr = std::shared_ptr<Pattern>;

    friend class Context;

    public:
        /**
         * Defines the type of pattern. This affects which operations are valid on it.
         */
        enum class Type {
            Color, Surface, LinearGradient, RadialGradient
        };

    public:
        /**
         * Creates a new pattern that will paint with the provided solid color.
         */
        static PatternPtr Make(const RgbColor &color);
        /**
         * Creates a new pattern that will paint with the provided translucent color.
         */
        static PatternPtr Make(const RgbaColor &color);

        /**
         * Creates a pattern that uses the provided surface as its source.
         */
        static PatternPtr Make(const std::shared_ptr<Surface> &surface);

        /**
         * Creates a pattern that will draw a linear gradient, along the line defined by the two
         * specified points.
         *
         * @note You must add color stops before the pattern can be used.
         */
        static PatternPtr Make(const Point &p1, const Point &p2);

        /**
         * Creates a pattern that will draw a radial gradient, between the two circles defined by
         * the associated centers and radii.
         *
         * @note You must add color stops before the pattern can be used.
         *
         * @param c1 Center of the start circle
         * @param r1 Radius of the start circle
         * @param c2 Center of the end circle
         * @param r2 Radius of the end circle
         */
        static PatternPtr Make(const Point &c1, const double r1, const Point &c2, const double r2);

        ~Pattern();

        /// Get the number of color stops, if this is a gradient pattern
        int getNumGradientStops() const;
        /**
         * Adds a color stop at the given offset.
         *
         * @param offset along the gradient vector [0, 1]
         * @param color Color to add to the gradient
         */
        void addGradientStop(const double offset, const RgbColor &color);
        /**
         * Adds a color stop at the given offset.
         *
         * @param offset along the gradient vector [0, 1]
         * @param color Color to add to the gradient
         */
        void addGradientStop(const double offset, const RgbaColor &color);

    private:
        Pattern(struct _cairo_pattern *pattern, const Type type);

    private:
        /// Pattern type
        Type type;
        /// Cairo pattern that we're backed by
        struct _cairo_pattern *pt{nullptr};
};
} // namespace gui::gfx

#endif
