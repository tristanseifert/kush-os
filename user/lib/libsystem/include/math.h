#ifndef LIBSYSTEM_MATH_H
#define LIBSYSTEM_MATH_H

/*
 * Declare the float_t and double_t types.
 */
typedef float float_t;
typedef double double_t;

/*
 * This is just a forward to include the OpenLibm headers, which is the actual libm implementation
 * on our system.
 *
 * We only include the basic math header here, since there's some complex number stuff that can
 * canse conflicts when compiled normally.
 */
#include <sys/cdefs.h>
#include <openlibm/openlibm_math.h>

/*
 * Redeclare some stuff as overloaded functions for C++.
 */
#ifdef __cplusplus

#undef signbit
inline bool signbit(float x) {
    return __signbitf(x);
}
inline bool signbit(double x) {
    return __signbit(x);
}
inline bool signbit(long double x) {
    return __signbitl(x);
}

#undef fpclassify
inline int fpclassify(float x) {
    return __fpclassifyf(x);
}
inline int fpclassify(double x) {
    return __fpclassifyd(x);
}
inline int fpclassify(long double x) {
    return __fpclassifyl(x);
}

#undef isfinite
inline bool isfinite(float x) {
    return __isfinitef(x);
}
inline bool isfinite(double x) {
    return __isfinite(x);
}
inline bool isfinite(long double x) {
    return __isfinitel(x);
}

#undef isinf
inline bool isinf(float x) {
    return __isinff(x);
}
inline bool isinf(long double x) {
    return __isinfl(x);
}

#undef isnan
inline bool isnan(float x) {
    return __isnanf(x);
}
inline bool isnan(long double x) {
    return __isnanl(x);
}

#undef isnormal
inline bool isnormal(float x) {
    return __isnormalf(x);
}
inline bool isnormal(double x) {
    return __isnormal(x);
}
inline bool isnormal(long double x) {
    return __isnormall(x);
}

#undef isgreater
inline bool isgreater(float x, float y) {
    return __builtin_isgreater((x), (y));
}
inline bool isgreater(double x, double y) {
    return __builtin_isgreater((x), (y));
}
inline bool isgreater(long double x, long double y) {
    return __builtin_isgreater((x), (y));
}
#undef isgreaterequal
inline bool isgreaterequal(float x, float y) {
    return __builtin_isgreaterequal((x), (y));
}
inline bool isgreaterequal(double x, double y) {
    return __builtin_isgreaterequal((x), (y));
}
inline bool isgreaterequal(long double x, long double y) {
    return __builtin_isgreaterequal((x), (y));
}
#undef isless
inline bool isless(float x, float y) {
    return __builtin_isless((x), (y));
}
inline bool isless(double x, double y) {
    return __builtin_isless((x), (y));
}
inline bool isless(long double x, long double y) {
    return __builtin_isless((x), (y));
}
#undef islessequal
inline bool islessequal(float x, float y) {
    return __builtin_islessequal((x), (y));
}
inline bool islessequal(double x, double y) {
    return __builtin_islessequal((x), (y));
}
inline bool islessequal(long double x, long double y) {
    return __builtin_islessequal((x), (y));
}
#undef islessgreater
inline bool islessgreater(float x, float y) {
    return __builtin_islessgreater((x), (y));
}
inline bool islessgreater(double x, double y) {
    return __builtin_islessgreater((x), (y));
}
inline bool islessgreater(long double x, long double y) {
    return __builtin_islessgreater((x), (y));
}
#undef isunordered
inline bool isunordered(float x, float y) {
    return __builtin_isunordered((x), (y));
}
inline bool isunordered(double x, double y) {
    return __builtin_isunordered((x), (y));
}
inline bool isunordered(long double x, long double y) {
    return __builtin_isunordered((x), (y));
}

#endif

#endif
