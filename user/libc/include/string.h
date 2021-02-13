#ifndef LIBC_STRING_H
#define LIBC_STRING_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// string functions: memory
const void *memchr(const void *ptr, const uint8_t value, const size_t num);
int memcmp(const void *ptr1, const void *ptr2, const size_t num);
void *memcpy(void *destination, const void *source, const size_t num);
void *memset(void *ptr, const uint8_t value, const size_t num);
void *memclr(void *start, const size_t count);
void *memmove(void *dest, const void *src, const size_t n);

// string functions
char *strncat(char * __restrict dst, const char * __restrict src, size_t n);
int strncmp(const char * s1, const char * s2, size_t n);
char *strncpy(char *dest, const char *src, size_t n);
size_t strlen(const char *str);
size_t strnlen(const char *s, size_t maxlen);
const char *strnstr(const char *s, const char *find, size_t slen);


#ifdef __cplusplus
}
#endif

#endif
