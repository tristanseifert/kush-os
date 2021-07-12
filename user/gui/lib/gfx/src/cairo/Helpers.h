/**
 * Helpers to convert various internal types to the corresponding Cairo types.
 */
#pragma once

#include "Context.h"
#include "Surface.h"

#include <cairo/cairo.h>

namespace gui::gfx::util {
/// Converts group content enum to correct value
cairo_content_t ConvertGroupContent(const Context::GroupContent c);
/// Converts our antialiasing mode enum to the Cairo type
cairo_antialias_t ConvertAliasingMode(const Context::Antialiasing a);
/// Converts our fill rule to the Cairo type
cairo_fill_rule_t ConvertFillRule(const Context::FillRule rule);
/// Converts our line cap mode to the Cairo type
cairo_line_cap_t ConvertLineCap(const Context::LineCap mode);
/// Converts our line join mode to the Cairo type
cairo_line_join_t ConvertLineJoin(const Context::LineJoin mode);
/// Converts our operator enum to the Cairo type
cairo_operator_t ConvertOperator(const Context::Operator op);



/// Convert from Cairo surface format
Surface::Format ConvertFormat(const cairo_format_t f);
/// Convert to Cairo surface format
cairo_format_t ConvertFormat(const Surface::Format f);

}

