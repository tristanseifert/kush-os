/**
 * Several helper methods that allow us to stay independent of the C library.
 */
#ifndef LIBSYSTEM_HELPERS_H
#define LIBSYSTEM_HELPERS_H

#include <stddef.h>

#include "_libsystem.h"

void LIBSYSTEM_INTERNAL *InternalMemset(void *b, int c, size_t len);

size_t LIBSYSTEM_INTERNAL InternalStrlen(const char *s);

#endif
