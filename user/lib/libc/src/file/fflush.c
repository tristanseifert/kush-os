#include <stdio.h>

#include "file_private.h"

/**
 * Flushes all output buffers in the given file pointer.
 */
int fflush(FILE *file) {
    // if null, we should flush ALL streams
    if(!file) {
        return -1;
    }

    // otherwise, just flush this stream
    return file->flush(file);
}

/**
 * Purges all input and output buffers of the given file, discarding any unsent/unread data.
 */
int fpurge(FILE *file) {
    return file->purge(file);
}
