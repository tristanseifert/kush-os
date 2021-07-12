#ifndef GUI_LIBGFX_CONTEXT
#define GUI_LIBGFX_CONTEXT

#include <memory>
#include <tuple>

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
        using RgbColor = std::tuple<double, double, double>;
        using RgbaColor = std::tuple<double, double, double, double>;

        using Point = std::tuple<double, double>;
        using Size = std::tuple<double, double>;

        /// Defines the type of data a group holds
        enum class GroupContent {
            Color, Alpha, ColorAlpha
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
         * Strokes the current path according to the current stroke settings (line width, join,
         * cap, and dash order) and then clears the path.
         */
        void stroke();
        /**
         * Strokes the current path, but does not clear it after stroking.
         */
        void strokePreserve();

        /**
         * Applies the current source pattern in the entire clip region.
         */
        void paint();
        /**
         * Applies the current source in the entire clip region, using a constant alpha value.
         */
        void paint(const double alpha);

        /**
         * Sets the current line width.
         */
        void setLineWidth(const double width);

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

    private:
        /// Cairo context that we're backed by
        struct _cairo *ctx{nullptr};
        /// Surface we're drawing on
        std::shared_ptr<Surface> backing;
};
}

#endif
