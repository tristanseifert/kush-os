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
#define __MATH_BUILTIN_RELOPS
#include <openlibm/openlibm_math.h>

/*
 * Now, declare a bunch of functions openlibm doesn't quite export right.
 */
/*#define signbit(x) __signbit(x)
#define isfinite(x) __isfinite(x)
#define isnormal(x) __isnormal(x)*/

#endif
