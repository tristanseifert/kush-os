/*
 * Provides an userspace mapping to emulate file descriptor numbers.
 *
 * This maps file descriptor numbers to a libc file structure, on which the actual IO is performed.
 * All calls that take file descriptors are thunks that look up the appropriate file object and
 * invoke the actual IO routines on it.
 */
#ifndef LIBC_FILE_FD_MAP_H
#define LIBC_FILE_FD_MAP_H

#include <stdbool.h>
#include <_libc.h>
#include "../file_private.h"

/**
 * Performs one-time initialization of the file descriptor map.
 */
LIBC_INTERNAL void InitFdToStreamMap();

/**
 * Converts a file descriptor to file structure.
 */
LIBC_INTERNAL stream_t * _Nullable ConvertFdToStream(int fd);

/**
 * Registers a stream with the file descriptor map. A fake file descriptor number is allocated for
 * the stream and stored in the structure.
 *
 * @param allocateFd Whether a file descriptor number should be allocated
 *
 * @return 0 on success, error code otherwise.
 */
LIBC_INTERNAL int RegisterFdStream(stream_t * _Nonnull stream, bool allocateFd);

/**
 * Removes an existing stream from the file descriptor map.
 */
LIBC_INTERNAL int UnregisterFdStream(stream_t * _Nonnull stream);

#endif
