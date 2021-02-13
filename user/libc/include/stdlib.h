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

#ifdef __cplusplus
}
#endif

#endif
