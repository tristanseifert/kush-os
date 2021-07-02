#ifndef _LIBSYSTEM_SYSCALLS_MISC_H
#define _LIBSYSTEM_SYSCALLS_MISC_H

#include <_libsystem.h>
#include <stddef.h>
#include <stdint.h>

LIBSYSTEM_EXPORT int GetEntropy(void * _Nonnull outBuf, const size_t outBufSize);

#endif

