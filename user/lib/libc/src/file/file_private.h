#ifndef FILE_FILE_PRIVATE_H
#define FILE_FILE_PRIVATE_H

#include <sys/types.h>
#include <threads.h>

/**
 * Definition for the base file structure.
 *
 * Various types of output streams that present the FILE-style interface may "subclass"
 * this type, hence the length field is provided to determine the actual length of the
 * structure beyond this basic "header."
 */
typedef struct __libc_file_stream {
    /// length of the whole struct
    size_t length;

    /// lock over the stream
    mtx_t lock;

    /// writes a character to the stream
    int (*putc)(struct __libc_file_stream *, char ch);
    /// flushes any buffered output and input
    int (*flush)(struct __libc_file_stream *);
    /// purges any pending output and input
    int (*purge)(struct __libc_file_stream *);
} stream_t;

#endif
