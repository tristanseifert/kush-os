#include "Pattern.h"
#include "Surface.h"

#include "Helpers.h"

#include <cairo/cairo.h>

#include <cassert>

using namespace gui::gfx;

Pattern::PatternPtr Pattern::Make(const RgbColor &color) {
    const auto [r, g, b] = color;
    auto cpat = cairo_pattern_create_rgb(r, g, b);
    if(cairo_pattern_status(cpat) != CAIRO_STATUS_SUCCESS) return nullptr;

    return std::shared_ptr<Pattern>(new Pattern(cpat, Type::Color));
}

Pattern::PatternPtr Pattern::Make(const RgbaColor &color) {
    const auto [r, g, b, a] = color;
    auto cpat = cairo_pattern_create_rgba(r, g, b, a);
    if(cairo_pattern_status(cpat) != CAIRO_STATUS_SUCCESS) return nullptr;

    return std::shared_ptr<Pattern>(new Pattern(cpat, Type::Color));
}

Pattern::PatternPtr Pattern::Make(const std::shared_ptr<Surface> &surface) {
    auto cpat = cairo_pattern_create_for_surface(cairo_surface_reference(surface->backing));
    if(cairo_pattern_status(cpat) != CAIRO_STATUS_SUCCESS) return nullptr;

    return std::shared_ptr<Pattern>(new Pattern(cpat, Type::Surface));
}

Pattern::PatternPtr Pattern::Make(const Point &p1, const Point &p2) {
    const auto [x1, y1] = p1;
    const auto [x2, y2] = p2;

    auto cpat = cairo_pattern_create_linear(x1, y1, x2, y2);
    if(cairo_pattern_status(cpat) != CAIRO_STATUS_SUCCESS) return nullptr;

    return std::shared_ptr<Pattern>(new Pattern(cpat, Type::LinearGradient));
}

Pattern::PatternPtr Pattern::Make(const Point &c1, const double r1,
        const Point &c2, const double r2) {
    const auto [x1, y1] = c1;
    const auto [x2, y2] = c2;

    auto cpat = cairo_pattern_create_radial(x1, y1, r1, x2, y2, r2);
    if(cairo_pattern_status(cpat) != CAIRO_STATUS_SUCCESS) return nullptr;

    return std::shared_ptr<Pattern>(new Pattern(cpat, Type::RadialGradient));
}

/**
 * Creates a pattern object from an existing Cairo pattern.
 */
Pattern::Pattern(struct _cairo_pattern *_pt, const Type _type) : type(_type), pt(_pt) {
    // TODO: anything?
}

/**
 * Release the pattern when deallocating.
 */
Pattern::~Pattern() {
    cairo_pattern_destroy(this->pt);
}



int Pattern::getNumGradientStops() const {
    int temp;
    const auto ret = cairo_pattern_get_color_stop_count(this->pt, &temp);

    return (ret == CAIRO_STATUS_SUCCESS) ? temp : -1;
}
void Pattern::addGradientStop(const double offset, const RgbColor &color) {
    assert(this->type == Type::LinearGradient || this->type == Type::RadialGradient);

    const auto [r, g, b] = color;
    cairo_pattern_add_color_stop_rgb(this->pt, offset, r, g, b);
}
void Pattern::addGradientStop(const double offset, const RgbaColor &color) {
    assert(this->type == Type::LinearGradient || this->type == Type::RadialGradient);

    const auto [r, g, b, a] = color;
    cairo_pattern_add_color_stop_rgba(this->pt, offset, r, g, b, a);
}

