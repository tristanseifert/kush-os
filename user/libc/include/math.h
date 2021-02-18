#ifndef LIBC_MATH_H
#define LIBC_MATH_H

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
#include <openlibm/openlibm_math.h>

#endif
