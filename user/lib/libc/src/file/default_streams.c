#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <sys/syscalls.h>

#include "file_private.h"

/**
 * Define the default streams (stdin, stdout, and stderr) and provide the code to connect them at
 * library startup.
 */
FILE *__stdinp = NULL;
FILE *__stdoutp = NULL;
FILE *__stderrp = NULL;

/**
 * Debug output stream; this implements some line buffering that will output a complete
 * line via the DbgOut() syscall.
 */
#define kBufLength (200)

struct DebugOutStream {
    struct __libc_file_stream header;

    /// total number of bytes written to the output stream
    uint64_t bytesWritten;

    /// number of bytes of buffer space used so far
    size_t bufUsed;
    /// buffer area
    char buf[kBufLength];
};

/**
 * Debug streams are always at the end of the stream.
 */
static int DebugTell(struct __libc_file_stream *_file, long *out) {
    struct DebugOutStream *file = (struct DebugOutStream *) _file;
    *out = file->bytesWritten;

    return 0;
}

/**
 * Flushes the buffered contents of the output stream.
 */
static int DebugOutFlush(struct __libc_file_stream *_file) {
    struct DebugOutStream *file = (struct DebugOutStream *) _file;

    if(file->bufUsed) {
        DbgOut(file->buf, file->bufUsed);

        memset(file->buf, 0, file->bufUsed);
        file->bufUsed = 0;
    }

    return 0;
}

/**
 * Writes a new character to the debug out stream. If it's a newline (or if we reach the
 * maximum bounds of the buffer) we flush the stream.
 */
static int DebugOutPutc(struct __libc_file_stream *_file, char c) {
    struct DebugOutStream *file = (struct DebugOutStream *) _file;

    int err = mtx_lock(&file->header.lock);
    if(err != thrd_success) {
        return EOF;
    }

    file->bytesWritten++;

    // if buffer is full, flush and try again
    if(file->bufUsed == kBufLength) {
        DebugOutFlush(_file);
    }
    // flush if newline
    if(c == '\n') {
        DebugOutFlush(_file);
    }
    // otherwise, add it into the buffer
    else {
        file->buf[file->bufUsed++] = c;
    }

    mtx_unlock(&file->header.lock);
    return c;
}

/**
 * Discards all data in the write buffer of the stream.
 */
static int DebugOutPurge(struct __libc_file_stream *_file) {
    struct DebugOutStream *file = (struct DebugOutStream *) _file;

    if(file->bufUsed) {
        memset(file->buf, 0, file->bufUsed);
        file->bufUsed = 0;
    }

    return 0;
}

/**
 * Connect the standard streams to the requisite consoles if they're set up. Otherwise, we create
 * null descriptors (basically the same as those pointing to /dev/null on UNIX-like systems) for
 * each of them.
 */
void __stdstream_init() {
    // allocate a stream
    struct DebugOutStream *stream = calloc(1, sizeof(struct DebugOutStream));
    if(!stream) abort();

    stream->header.length = sizeof(struct DebugOutStream);

    stream->header.putc = DebugOutPutc;
    stream->header.flush = DebugOutFlush;
    stream->header.purge = DebugOutPurge;
    stream->header.tell = DebugTell;

    // configure it as the standard output and error out
    __stderrp = (FILE *) stream;
    __stdoutp = (FILE *) stream;
}

