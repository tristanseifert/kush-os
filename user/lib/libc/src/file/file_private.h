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
 *
 * Any functions that aren't implemented can be left as NULL; they will fail with an ENODEV error.
 */
typedef struct __libc_file_stream {
    /// length of the whole struct
    size_t length;

    /// lock over the stream
    mtx_t lock;

    /// Performs any actions necessary to close the stream.
    int (*close)(struct __libc_file_stream *);

    /// writes a character to the stream
    int (*putc)(struct __libc_file_stream *, char ch);
    /// flushes any buffered output and input
    int (*flush)(struct __libc_file_stream *);
    /// purges any pending output and input
    int (*purge)(struct __libc_file_stream *);

    /// seeks to the given position in the file (plus whence mode)
    int (*seek)(struct __libc_file_stream *, const long, const int);
    /// gets the current file position
    int (*tell)(struct __libc_file_stream *, long *);

    /// writes a blob of data to the file
    int (*write)(struct __libc_file_stream *, const void *, const size_t);
    /// reads up to the given number of bytes from the file
    int (*read)(struct __libc_file_stream *, void *, const size_t);
} stream_t;

#endif
