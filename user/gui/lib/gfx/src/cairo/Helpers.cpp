#include "Helpers.h"

using namespace gui::gfx;

cairo_content_t util::ConvertGroupContent(const Context::GroupContent c) {
    switch(c) {
        case Context::GroupContent::Color:
            return CAIRO_CONTENT_COLOR;
        case Context::GroupContent::Alpha:
            return CAIRO_CONTENT_ALPHA;
        case Context::GroupContent::ColorAlpha:
            return CAIRO_CONTENT_COLOR_ALPHA;
    }
}

cairo_antialias_t util::ConvertAliasingMode(const Context::Antialiasing a) {
    using Antialiasing = Context::Antialiasing;
    switch(a) {
        case Antialiasing::Default:
            return CAIRO_ANTIALIAS_DEFAULT;
        case Antialiasing::None:
            return CAIRO_ANTIALIAS_NONE;
        case Antialiasing::Gray:
            return CAIRO_ANTIALIAS_GRAY;
        case Antialiasing::Subpixel:
            return CAIRO_ANTIALIAS_SUBPIXEL;
        case Antialiasing::Fast:
            return CAIRO_ANTIALIAS_FAST;
        case Antialiasing::Good:
            return CAIRO_ANTIALIAS_GOOD;
        case Antialiasing::Best:
            return CAIRO_ANTIALIAS_BEST;
    }
}

cairo_fill_rule_t util::ConvertFillRule(const Context::FillRule rule) {
    using FillRule = Context::FillRule;
    switch(rule) {
        case FillRule::Winding:
            return CAIRO_FILL_RULE_WINDING;
        case FillRule::EvenOdd:
            return CAIRO_FILL_RULE_EVEN_ODD;
    }
}

cairo_line_cap_t util::ConvertLineCap(const Context::LineCap mode) {
    using LineCap = Context::LineCap;
    switch(mode) {
        case LineCap::Butt:
            return CAIRO_LINE_CAP_BUTT;
        case LineCap::Round:
            return CAIRO_LINE_CAP_ROUND;
        case LineCap::Square:
            return CAIRO_LINE_CAP_SQUARE;
    }
}

cairo_line_join_t util::ConvertLineJoin(const Context::LineJoin mode) {
    using LineJoin = Context::LineJoin;
    switch(mode) {
        case LineJoin::Miter:
            return CAIRO_LINE_JOIN_MITER;
        case LineJoin::Round:
            return CAIRO_LINE_JOIN_ROUND;
        case LineJoin::Bevel:
            return CAIRO_LINE_JOIN_BEVEL;
    }
}

cairo_operator_t util::ConvertOperator(const Context::Operator op) {
    switch(op) {
        case Context::Operator::Clear:
            return CAIRO_OPERATOR_CLEAR;
        case Context::Operator::Source:
            return CAIRO_OPERATOR_SOURCE;
        case Context::Operator::Over:
            return CAIRO_OPERATOR_OVER;
        case Context::Operator::In:
            return CAIRO_OPERATOR_IN;
        case Context::Operator::Out:
            return CAIRO_OPERATOR_OUT;
        case Context::Operator::Atop:
            return CAIRO_OPERATOR_ATOP;
        case Context::Operator::Dest:
            return CAIRO_OPERATOR_DEST;
        case Context::Operator::DestOver:
            return CAIRO_OPERATOR_DEST_OVER;
        case Context::Operator::DestIn:
            return CAIRO_OPERATOR_DEST_IN;
        case Context::Operator::DestOut:
            return CAIRO_OPERATOR_DEST_OUT;
        case Context::Operator::DestAtop:
            return CAIRO_OPERATOR_DEST_ATOP;
        case Context::Operator::Xor:
            return CAIRO_OPERATOR_XOR;
        case Context::Operator::Add:
            return CAIRO_OPERATOR_ADD;
        case Context::Operator::Saturate:
            return CAIRO_OPERATOR_SATURATE;
        case Context::Operator::Multiply:
            return CAIRO_OPERATOR_MULTIPLY;
        case Context::Operator::Screen:
            return CAIRO_OPERATOR_SCREEN;
        case Context::Operator::Overlay:
            return CAIRO_OPERATOR_OVERLAY;
        case Context::Operator::Darken:
            return CAIRO_OPERATOR_DARKEN;
        case Context::Operator::Lighten:
            return CAIRO_OPERATOR_LIGHTEN;
        case Context::Operator::ColorDodge:
            return CAIRO_OPERATOR_COLOR_DODGE;
        case Context::Operator::ColorBurn:
            return CAIRO_OPERATOR_COLOR_BURN;
        case Context::Operator::HardLight:
            return CAIRO_OPERATOR_HARD_LIGHT;
        case Context::Operator::SoftLight:
            return CAIRO_OPERATOR_SOFT_LIGHT;
        case Context::Operator::Difference:
            return CAIRO_OPERATOR_DIFFERENCE;
        case Context::Operator::Exclusion:
            return CAIRO_OPERATOR_EXCLUSION;
        case Context::Operator::HslHue:
            return CAIRO_OPERATOR_HSL_HUE;
        case Context::Operator::HslSaturation:
            return CAIRO_OPERATOR_HSL_SATURATION;
        case Context::Operator::HslColor:
            return CAIRO_OPERATOR_HSL_COLOR;
        case Context::Operator::HslLuminosity:
            return CAIRO_OPERATOR_HSL_LUMINOSITY;
    }
}



Surface::Format util::ConvertFormat(const cairo_format_t f) {
    using Format = Surface::Format;
    switch(f) {
        case CAIRO_FORMAT_ARGB32:
            return Format::ARGB32;
        case CAIRO_FORMAT_RGB24:
            return Format::RGB24;
        case CAIRO_FORMAT_A8:
            return Format::A8;
        case CAIRO_FORMAT_A1:
            return Format::A1;
        case CAIRO_FORMAT_RGB16_565:
            return Format::RGB565;
        case CAIRO_FORMAT_RGB30:
            return Format::RGB30;
        case CAIRO_FORMAT_INVALID:
            return Format::Invalid;
        default:
            return Format::Invalid;
    }
}
cairo_format_t util::ConvertFormat(const Surface::Format f) {
    using Format = Surface::Format;
    switch(f) {
        case Format::ARGB32:
            return CAIRO_FORMAT_ARGB32;
        case Format::RGB24:
            return CAIRO_FORMAT_RGB24;
        case Format::A8:
            return CAIRO_FORMAT_A8;
        case Format::A1:
            return CAIRO_FORMAT_A1;
        case Format::RGB565:
            return CAIRO_FORMAT_RGB16_565;
        case Format::RGB30:
            return CAIRO_FORMAT_RGB30;
        case Format::Invalid:
            return CAIRO_FORMAT_INVALID;
    }
}
