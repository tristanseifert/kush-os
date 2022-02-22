#ifndef KERNEL_RUNTIME_STRING_H
#define KERNEL_RUNTIME_STRING_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void *memcpy(void *dst0, const void *src0, size_t length);
void *memmove(void *dst0, const void *src0, size_t length);
void *memset(void *dst, int c, size_t n);

char *strncpy(char *dst, const char *src, size_t n);

#ifdef __cplusplus
}
#endif

#endif
