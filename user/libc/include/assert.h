#ifndef LIBC_ASSERT_H
#define LIBC_ASSERT_H

#include <stdint.h>
#include <stddef.h>
#include <_libc.h>

#ifdef __cplusplus
extern "C" {
#endif

#undef assert
#undef __assert

#ifdef NDEBUG
#define assert(e) ((void)0)
#else
LIBC_EXPORT void __assert(const char *func, const char *file, int line, const char *expr);

#define assert(e)                                                              \
  ((e) ? ((void)0) : (__assert(__func__, __FILE__, __LINE__, #e)))
#endif

#ifdef __cplusplus
}
#endif

#endif
