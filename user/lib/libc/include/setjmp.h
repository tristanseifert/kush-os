#ifndef LIBC_SETJMP_H
#define LIBC_SETJMP_H

#include <_libc.h>

#if defined(__amd64__)
#include <sys/amd64/setjmp.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

// buffer to hold a setjmp/longjmp return value
typedef long jmp_buf[_JBLEN];

__attribute__((returns_twice)) int setjmp(jmp_buf);
__attribute__((__noreturn__)) void longjmp(jmp_buf, int);

__attribute__((returns_twice)) int _setjmp(jmp_buf);
__attribute__((__noreturn__)) void _longjmp(jmp_buf, int);

#ifdef __cplusplus
}
#endif

#endif
