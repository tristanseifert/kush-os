#ifndef GUI_LIBGFX_CONTEXT_H
#define GUI_LIBGFX_CONTEXT_H

#include <memory>
#include <span>

#include <gfx/Types.h>

struct _cairo;

namespace gui::gfx {
class Pattern;
class Surface;

/**
 * Encapsulates a drawing context's state; these are the basic objects on which all drawing is
 * performed. Each context maintains some internal state to use for all subsequent drawing
 * operations.
 */
class Context {
    public:
        /// Defines the type of data a group holds
        enum class GroupContent {
            Color, Alpha, ColorAlpha
        };

        /// Antialiasing mode hints
        enum class Antialiasing {
            /// Allow the device to select whether it performs antialiased rendering.
            Default,
            /// Do not use antialiasing
            None,
            /// Use single-color antialiasing
            Gray,
            /// Use subpixel antialiasing (taking advantage of LCD pixel orders)
            Subpixel,

            /// Perform antialiasing but prefer speed over quality
            Fast,
            /// Perform antialiasing while balancing performance and quality
            Good,
            /// Perform antialiasing while preferring quality over speed
            Best,
        };

        /**
         * Defines the operator to use when drawing pixels.
         *
         * See https://cairographics.org/operators/ for more information.
         */
        enum class Operator {
            Clear,
            Source,
            Over,
            In,
            Out,
            Atop,
            Dest,
            DestOver,
            DestIn,
            DestOut,
            DestAtop,
            Xor,
            Add,
            Saturate,
            Multiply,
            Screen,
            Overlay,
            Darken,
            Lighten,
            ColorDodge,
            ColorBurn,
            HardLight,
            SoftLight,
            Difference,
            Exclusion,
            /// Take the hue from the source, rest from destination
            HslHue,
            /// Take saturation from source and rest from destination
            HslSaturation,
            /// Take hue and saturation from source, luminosity from destination
            HslColor,
            /// Take luminosity from source, rest from destination
            HslLuminosity,
        };

        /**
         * How a path is filled.
         *
         * Conceptually, this can be thought of taking a ray from a point on the path and checking
         * for intersections.
         */
        enum class FillRule {
            /**
             * If the total count is nonzero, the point is filled; if the ray crosses left-to-right
             * the count is incremented, and if right-to-left, decremented.
             *
             * This is similar to the OpenGL concept of vertex winding orders.
             */
            Winding,
            /**
             * Counts the total number of intersections WITHOUT regard for the direction; if the
             * number of intersections is odd, the point is filled.
             */
            EvenOdd,
        };

        /// Defines how the endpoints of a path are rendered
        enum class LineCap {
            /// Endpoints are drawn as lines at exactly the given points
            Butt,
            /// Round endings; the center of the circle is the given point
            Round,
            /// Square endings; the center of the square is the given point
            Square,
        };

        /// Defines how two lines are joined when stroked
        enum class LineJoin {
            /// Use a sharp (angled) corner
            Miter,
            /// Use a rounded join (circle) whose center is the intersection point
            Round,
            /// use a cut off join, cut off at half the line width from the intersection point
            Bevel,
        };

    public:
        /**
         * Allocate a new graphics context whose output is directed to the given surface.
         *
         * @param destination Surface to render to
         * @param clear Whether the surface is cleared or not
         */
        Context(const std::shared_ptr<Surface> &destination, const bool clear = false);
        ~Context();

        /**
         * Check for context errors.
         */
        void checkErrors();

        /**
         * Makes a copy of the current context state and pushes it on an internal stack.
         */
        void pushState();
        /**
         * Restores the copy of the context state at the top of the internal state stack.
         */
        void popState();

        /**
         * Creates a new group, which is an intermediate surface that will receive the results of
         * all draw calls until the group is terminated. The result of the group can then be used
         * as a pattern.
         */
        void beginGroup();
        /**
         * Creates a new group that contains either a color or alpha component, or both.
         */
        void beginGroup(const GroupContent content);
        /**
         * Finishes the current group and returns a pattern object containing its results.
         */
        std::shared_ptr<Pattern> endGroup();
        /**
         * Finishes the current group and sets it as the source pattern of the context.
         */
        void endGroupAsSource();

        /**
         * Sets the antialiasing mode of the context. This is just a hint to the rendering backend
         * when performing draw calls; there is no guarantee the requested antialiasing method will
         * be used.
         */
        void setAntialiasingMode(const Antialiasing mode);

        /**
         * Sets the source pattern to an opaque color.
         */
        void setSource(const RgbColor &color);
        /**
         * Sets the source pattern to a translucent color.
         */
        void setSource(const RgbaColor &color);
        /**
         * Sets the source pattern.
         */
        void setSource(const std::shared_ptr<Pattern> &pattern);
        /**
         * Sets the source by creating a pattern that is backed by the provided surface.
         */
        void setSource(const std::shared_ptr<Surface> &surface,
                const Point &origin = {0, 0});

        /**
         * Fills the current path according to the current fill rule, then clears the path.
         */
        void fill();
        /**
         * Fills the current path according to the current fill rule, but leaves it on the context
         * state after.
         */
        void fillPreserve();
        /**
         * Determine what region of the context would be affected by a fill operation against the
         * current path. If none, an empty rect is returned.
         */
        Rectangle getFillExtents() const;
        /**
         * Determine whether the given point falls into the region to be filled.
         */
        bool isInFill(const Point &pt) const;

        /**
         * Strokes the current path according to the current stroke settings (line width, join,
         * cap, and dash order) and then clears the path.
         */
        void stroke();
        /**
         * Strokes the current path, but does not clear it after stroking.
         */
        void strokePreserve();
        /**
         * Determine what region of the context would be stroked with the current path; if none, an
         * empty rect is returned.
         */
        Rectangle getStrokeExtents() const;
        /**
         * Determine whether the given point falls into the region to be stroked.
         */
        bool isInStroke(const Point &pt) const;

        /**
         * Applies the current source pattern in the entire clip region.
         */
        void paint();
        /**
         * Applies the current source in the entire clip region, using a constant alpha value.
         */
        void paint(const double alpha);

        /**
         * Sets the rule used for filling a path.
         */
        void setFillRule(const FillRule rule);

        /**
         * Sets the dash pattern used for stroking paths.
         *
         * @param dashes An array of positive values, in context user space distance units, between
         * consecutive on and off portions of the stroke.
         * @param offset Offset into the pattern (in units) at which the stroking begins
         */
        void setDash(const std::span<double> dashes, const double offset = 0);
        /**
         * Sets the dash pattern used for stroking paths to be a single symmetric pattern with the
         * given dash length.
         *
         * @param length Length of the stroked and unstroked sections of the path
         */
        void setDash(double length) {
            this->setDash(std::span<double>(&length, 1));
        }
        /**
         * Sets the line cap mode, which defines how the ends of strokes are drawn.
         */
        void setLineCap(const LineCap mode);
        /**
         * Sets the line join mode, which defines how two adjacent lines in a path are joined
         * together.
         */
        void setLineJoin(const LineJoin mode);
        /**
         * Sets the current line width.
         */
        void setLineWidth(const double width);
        /**
         * Gets the current line width.
         */
        double getLineWidth() const;
        /**
         * Sets the miter limit, which determines whether a line is joined with a bevel instead of
         * a rounded mitered joint.
         *
         * @param limit Angle at which to use bevels, in radians.
         */
        void setMiterLimit(const double angle);

        /**
         * Sets the operator to use for subsequent drawing operations.
         */
        void setOperator(const Operator op);

        /**
         * Sets the rasterization tolerance that's used when converting paths to trapezoids to be
         * drawn on screen.
         */
        void setTolerance(const double tolerance);
        /**
         * Returns the current rasterization tolerance.
         */
        double getTolerance() const;

        /**
         * Updates the clipping region of the context by intersecting the current clip region (if
         * there is one) with the current path, then clear the path.
         *
         * @note To embiggen the clip region, it needs to be reset; but the region is a part of
         * context state saved via `pushState()` and restored by `popState()`.
         */
        void clip();
        /**
         * Updates the clipping region of the context in the same manner as `clip()` but does not
         * clear the context's path after.
         */
        void clipPreserve();
        /**
         * Clears the current clipping region.
         */
        void clipReset();
        /**
         * Gets the bounding box covering the current clipping area.
         */
        Rectangle getClipExtents() const;
        /**
         * Test if the given point is clipped or not.
         */
        bool isInClip(const Point &pt) const;

        /**
         * Clears the current path.
         */
        void newPath();
        /**
         * Begins a new subpath.
         */
        void newSubpath();
        /**
         * Closes the current subpath; a line is added from the current point to the beginning of
         * the current subpath.
         */
        void closePath();
        /**
         * Adds a circular arc to the current path. The arc will begin at angles[0] and go in the
         * positive (counterclockwise) direction until angles[1].
         *
         * @param center Point at which the arc is centered
         * @param radius Radius of the path, in points
         * @param angles Starting and ending angle, in radians.
         */
        void arc(const Point &center, const double radius,
                const std::tuple<double, double> &angles);
        /**
         * Adds a circular arc to the current path. The arc will begin at angles[0] and go in the
         * negative (clockwise) direction until angles[1].
         *
         * @param center Point at which the arc is centered
         * @param radius Radius of the path, in points
         * @param angles Starting and ending angle, in radians.
         */
        void arcNegative(const Point &center, const double radius,
                const std::tuple<double, double> &angles);
        /**
         * Adds a cubic Bézier spline to the path, from the current point to the given destination
         * position. Two control points are specified.
         *
         * @note If there is no current point, it will implicitly be set as the first control
         * point.
         *
         * @param c1 First control point
         * @param c2 Second control point
         * @param end Ending point
         */
        void curveTo(const Point &c1, const Point &c2, const Point &end);
        /**
         * Adds a line on the path from the current point to the given point.
         *
         * @param to Destination of the line
         */
        void lineTo(const Point &to);
        /**
         * Begins a new subpath, and sets its current point.
         *
         * @param start Starting position of this subpath
         */
        void moveTo(const Point &start);
        /**
         * Creates a closed subpath representing the given rectangle and adds it to the current
         * path.
         *
         * @param origin Top left corner of the rectangle
         * @param size Size of the rectangle
         */
        void rectangle(const Point &origin, const Size &size);
        /**
         * Adds the given rectangle to the path of the context.
         */
        inline void rectangle(const Rectangle &rect) {
            this->rectangle(rect.origin, rect.size);
        }

        /**
         * Computes the bounding box of the current path.
         */
        Rectangle getPathExtents() const;

    private:
        /// Cairo context that we're backed by
        struct _cairo *ctx{nullptr};
        /// Surface we're drawing on
        std::shared_ptr<Surface> backing;
};
} // namespace gui::gfx

#endif
