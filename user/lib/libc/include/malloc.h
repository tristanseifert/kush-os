#ifndef LIBC_MALLOC_H
#define LIBC_MALLOC_H

#include <stdint.h>
#include <stddef.h>
#include <_libc.h>

// TODO: expand all declarations to have nullability specifiers
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wnullability-completeness"

// define our supported malloc options set
#define M_TRIM_THRESHOLD     (-1)
#define M_GRANULARITY        (-2)
#define M_MMAP_THRESHOLD     (-3)

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Allocates page-aligned memory.
 */
LIBC_EXPORT void* valloc(const size_t numBytes);

/**
 * Allocates page-aligned memory, with page granularity. If the provided size is not an exact multiple of
 * the page size, it's roumded up.
 */
LIBC_EXPORT void* pvalloc(const size_t numBytes);

/**
 * Returns the total number of bytes acquired by the malloc implementation from the system; this includes
 * internal bookkeeping done by the implementation.
 */
LIBC_EXPORT size_t malloc_footprint(void);

/**
 * Returns the highest amount of memory the malloc implementation had allocated from the system at
 * some point in the past.
 */
LIBC_EXPORT size_t malloc_max_footprint(void);

/**
 * Returns the current heap size limit.
 */
LIBC_EXPORT size_t malloc_footprint_limit();

/**
 * Adjusts the malloc footprint limit; allocations beyond this limit will fail, even if the system has sufficient
 * memory to provide.
 *
 * The maximum size_t value will disable footprint limit checking.
 *
 * @note Existing allocations beyond the limit will NOT be retroactively deallocated.
 */
LIBC_EXPORT size_t malloc_set_footprint_limit(const size_t newLimit);

/**
 * Adjusts malloc options.
 *
 * Supported options are:
 *   Symbol            param #  default    allowed param values
 * M_TRIM_THRESHOLD     -1   2*1024*1024   any   (-1 disables)
 * M_GRANULARITY        -2     page size   any power of 2 >= page size
 * M_MMAP_THRESHOLD     -3      256*1024   any   (or 0 if no MMAP support)
 */
LIBC_EXPORT int mallopt(const int option, const int value);

/**
 * Trims the malloc region, leaving at most `trim` bytes of extra space.
 */
LIBC_EXPORT int malloc_trim(const size_t trim);

#ifdef __cplusplus
}
#endif

#pragma clang diagnostic pop

#endif
