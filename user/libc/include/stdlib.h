#ifndef LIBC_STDLIB_H
#define LIBC_STDLIB_H

#include <stdint.h>
#include <stddef.h>
#include <_libc.h>

#ifdef __cplusplus
extern "C" {
#endif

/// Aborts program execution
LIBC_EXPORT void abort() __attribute__((noreturn));

/// Allocates zeroed memory
LIBC_EXPORT void *calloc(const size_t count, const size_t size);
/// Allocates memory
LIBC_EXPORT void *malloc(const size_t numBytes);
/// Frees previously allocated memory
LIBC_EXPORT void free(void *ptr);
/// Resizes the previously made allocation.
LIBC_EXPORT void *realloc(void *ptr, const size_t size);

#ifdef __cplusplus
}
#endif

#endif
