#include "Context.h"
#include "Surface.h"

#include <cairo/cairo.h>

#include <cassert>
#include <cstdio>
#include <cstdlib>

using namespace gui::gfx;

static cairo_content_t ConvertGroupContent(const Context::GroupContent c);

/**
 * Creates a new context.
 */
Context::Context(const std::shared_ptr<Surface> &dest, const bool clear) : backing(dest) {
    assert(dest && dest->backing);

    this->ctx = cairo_create(dest->backing);
    this->checkErrors();

    if(clear) {
        cairo_set_source_rgb(this->ctx, 0, 0, 0);
        cairo_paint(this->ctx);
    }
}

/**
 * Releases the context.
 */
Context::~Context() {
    cairo_destroy(this->ctx);
}

/**
 * Checks if the context is in an error state.
 */
void Context::checkErrors() {
    const auto err = cairo_status(this->ctx);
    if(err != CAIRO_STATUS_SUCCESS) {
        fprintf(stderr, "Context %p has invalid state %d\n", this, err);
        abort();
    }
}


void Context::pushState() {
    cairo_save(this->ctx);
}
void Context::popState() {
    cairo_restore(this->ctx);
}

void Context::beginGroup() {
    cairo_push_group(this->ctx);
}
void Context::beginGroup(const GroupContent c) {
    cairo_push_group_with_content(this->ctx, ConvertGroupContent(c));
}
void Context::endGroupAsSource() {
    cairo_pop_group_to_source(this->ctx);
}

void Context::setSource(const RgbColor &color) {
    const auto [r, g, b] = color;
    cairo_set_source_rgb(this->ctx, r, g, b);
}
void Context::setSource(const RgbaColor &color) {
    const auto [r, g, b, a] = color;
    cairo_set_source_rgba(this->ctx, r, g, b, a);
}
void Context::setSource(const std::shared_ptr<Surface> &surface,
        const std::tuple<double, double> &origin) {
    const auto [x, y] = origin;
    cairo_set_source_surface(this->ctx, surface->backing, x, y);
}

void Context::fill() {
    cairo_fill(this->ctx);
}
void Context::fillPreserve() {
    cairo_fill_preserve(this->ctx);
}

void Context::stroke() {
    cairo_stroke(this->ctx);
}
void Context::strokePreserve() {
    cairo_stroke_preserve(this->ctx);
}

void Context::paint() {
    cairo_paint(this->ctx);
}
void Context::paint(const double alpha) {
    cairo_paint_with_alpha(this->ctx, alpha);
}

void Context::setLineWidth(const double width) {
    cairo_set_line_width(this->ctx, width);
}

void Context::newPath() {
    cairo_new_path(this->ctx);
}
void Context::newSubpath() {
    cairo_new_sub_path(this->ctx);
}
void Context::closePath() {
    cairo_close_path(this->ctx);
}
void Context::arc(const Point &center, const double radius,
        const std::tuple<double, double> &angles) {
    const auto [xc, yc] = center;
    const auto [a1, a2] = angles;
    cairo_arc(this->ctx, xc, yc, radius, a1, a2);
}
void Context::arcNegative(const Point &center, const double radius,
        const std::tuple<double, double> &angles) {
    const auto [xc, yc] = center;
    const auto [a1, a2] = angles;
    cairo_arc_negative(this->ctx, xc, yc, radius, a1, a2);
}
void Context::curveTo(const Point &c1, const Point &c2, const Point &end) {
    const auto [x1, y1] = c1;
    const auto [x2, y2] = c2;
    const auto [x3, y3] = end;
    cairo_curve_to(this->ctx, x1, y1, x2, y2, x3, y3);
}
void Context::lineTo(const Point &point) {
    const auto [x, y] = point;
    cairo_line_to(this->ctx, x, y);
}
void Context::moveTo(const Point &point) {
    const auto [x, y] = point;
    cairo_move_to(this->ctx, x, y);
}
void Context::rectangle(const Point &origin, const Size &size) {
    const auto [x, y] = origin;
    const auto [w, h] = size;
    cairo_rectangle(this->ctx, x, y, w, h);
}

/**
 * Converts the group content value to the Cairo type.
 */
static cairo_content_t ConvertGroupContent(const Context::GroupContent c) {
    switch(c) {
        case Context::GroupContent::Color:
            return CAIRO_CONTENT_COLOR;
        case Context::GroupContent::Alpha:
            return CAIRO_CONTENT_ALPHA;
        case Context::GroupContent::ColorAlpha:
            return CAIRO_CONTENT_COLOR_ALPHA;
    }
}

