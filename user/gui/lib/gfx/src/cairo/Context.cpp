#include "Context.h"
#include "Pattern.h"
#include "Surface.h"

#include "Helpers.h"

#include <cairo/cairo.h>

#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace gui::gfx;


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
    cairo_push_group_with_content(this->ctx, util::ConvertGroupContent(c));
}
std::shared_ptr<Pattern> Context::endGroup() {
    // get the pattern and ensure it's valid
    auto cpt = cairo_pop_group(this->ctx);
    if(cairo_pattern_status(cpt) != CAIRO_STATUS_SUCCESS) return nullptr;

    // create the pattern object from it
    std::shared_ptr<Pattern> pt(new Pattern(cpt, Pattern::Type::Surface));
    return pt;
}
void Context::endGroupAsSource() {
    cairo_pop_group_to_source(this->ctx);
}

void Context::setAntialiasingMode(const Antialiasing mode) {
    cairo_set_antialias(this->ctx, util::ConvertAliasingMode(mode));
}

void Context::setSource(const RgbColor &color) {
    const auto [r, g, b] = color;
    cairo_set_source_rgb(this->ctx, r, g, b);
}
void Context::setSource(const RgbaColor &color) {
    const auto [r, g, b, a] = color;
    cairo_set_source_rgba(this->ctx, r, g, b, a);
}
void Context::setSource(const std::shared_ptr<Pattern> &pattern) {
    cairo_set_source(this->ctx, pattern->pt);
}
void Context::setSource(const std::shared_ptr<Surface> &surface, const Point &origin) {
    const auto [x, y] = origin;
    cairo_set_source_surface(this->ctx, surface->backing, x, y);
}

void Context::fill() {
    cairo_fill(this->ctx);
}
void Context::fillPreserve() {
    cairo_fill_preserve(this->ctx);
}
Rectangle Context::getFillExtents() const {
    double x1, y1, x2, y2;
    cairo_fill_extents(this->ctx, &x1, &y1, &x2, &y2);

    Rectangle r{
        .origin = {x1, y1},
        .size   = {x2-x1, y2-y1}
    };
    return r;
}
bool Context::isInFill(const Point &pt) const {
    const auto [x, y] = pt;
    return cairo_in_fill(this->ctx, x, y);
}

void Context::stroke() {
    cairo_stroke(this->ctx);
}
void Context::strokePreserve() {
    cairo_stroke_preserve(this->ctx);
}
Rectangle Context::getStrokeExtents() const {
    double x1, y1, x2, y2;
    cairo_stroke_extents(this->ctx, &x1, &y1, &x2, &y2);

    Rectangle r{
        .origin = {x1, y1},
        .size   = {x2-x1, y2-y1}
    };
    return r;
}
bool Context::isInStroke(const Point &pt) const {
    const auto [x, y] = pt;
    return cairo_in_stroke(this->ctx, x, y);
}

void Context::paint() {
    cairo_paint(this->ctx);
}
void Context::paint(const double alpha) {
    cairo_paint_with_alpha(this->ctx, alpha);
}

void Context::setDash(const std::span<double> dashes, const double offset) {
    cairo_set_dash(this->ctx, dashes.data(), dashes.size(), offset);
}
void Context::setFillRule(const FillRule rule) {
    cairo_set_fill_rule(this->ctx, util::ConvertFillRule(rule));
}
void Context::setLineCap(const LineCap mode) {
    cairo_set_line_cap(this->ctx, util::ConvertLineCap(mode));
}
void Context::setLineJoin(const LineJoin mode) {
    cairo_set_line_join(this->ctx, util::ConvertLineJoin(mode));
}
void Context::setLineWidth(const double width) {
    cairo_set_line_width(this->ctx, width);
}
double Context::getLineWidth() const {
    return cairo_get_line_width(this->ctx);
}

void Context::setMiterLimit(const double angle) {
    const auto lim = 1. / sin(angle / 2.);
    cairo_set_miter_limit(this->ctx, lim);
}

void Context::setOperator(const Operator op) {
    cairo_set_operator(this->ctx, util::ConvertOperator(op));
}

void Context::setTolerance(const double tolerance) {
    cairo_set_tolerance(this->ctx, tolerance);
}
double Context::getTolerance() const {
    return cairo_get_tolerance(this->ctx);
}

void Context::clip() {
    cairo_clip(this->ctx);
}
void Context::clipPreserve() {
    cairo_clip_preserve(this->ctx);
}
void Context::clipReset() {
    cairo_reset_clip(this->ctx);
}
Rectangle Context::getClipExtents() const {
    double x1, y1, x2, y2;
    cairo_clip_extents(this->ctx, &x1, &y1, &x2, &y2);

    Rectangle r{
        .origin = {x1, y1},
        .size   = {x2-x1, y2-y1}
    };
    return r;
}
bool Context::isInClip(const Point &pt) const {
    const auto [x, y] = pt;
    return cairo_in_clip(this->ctx, x, y);
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

Rectangle Context::getPathExtents() const {
    double x1, y1, x2, y2;
    cairo_path_extents(this->ctx, &x1, &y1, &x2, &y2);

    Rectangle r{
        .origin = {x1, y1},
        .size   = {x2-x1, y2-y1}
    };
    return r;
}
