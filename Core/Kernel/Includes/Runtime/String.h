#ifndef KERNEL_RUNTIME_STRING_H
#define KERNEL_RUNTIME_STRING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memset(void *dst, int c, size_t n);
void *memmove(void *dst0, const void *src0, size_t length);

#ifdef __cplusplus
}
#endif

#endif
