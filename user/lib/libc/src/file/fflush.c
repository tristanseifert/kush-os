#include <stdio.h>

/**
 * Flushes all output buffers in the given file pointer.
 */
int fflush(FILE *file) {
    // if null, we should flush ALL streams
    if(!file) {
        return -1;
    }

    // otherwise, just flush this stream
    return -1;
}

/**
 * Purges all input and output buffers of the given file, discarding any unsent/unread data.
 */
int fpurge(FILE *stream) {
    return -1;
}
