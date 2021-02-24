#include "rpc_file_streams.h"
#include "file_private.h"

#include <assert.h>
#include <stdio.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <sys/syscalls.h>
#include <rpc/file.h>

/**
 * File stream backed by an RPC service; this is usually the system's global file IO service, but
 * the lookup can be configured.
 */
struct RpcFileStream {
    struct __libc_file_stream h;

    /// file handle on the remote server
    uintptr_t remoteHandle;

    /// current file offset
    uint64_t position;
    /// file size (at open time)
    uint64_t length;
};

/**
 * Retrieves the file position pointer.
 */
static int RpcFileGetPos(struct __libc_file_stream *_file, long *out) {
    struct RpcFileStream *file = (struct RpcFileStream *) _file;
    *out = file->position;
    return 0;
}

/**
 * Seeks the file stream.
 */
static int RpcFileSeek(struct __libc_file_stream *_file, const long off, const int whence) {
    struct RpcFileStream *file = (struct RpcFileStream *) _file;

    switch(whence) {
        case SEEK_SET:
            if(off < file->length) file->position = off;
            else file->position = file->length;
            return 0;

        case SEEK_CUR:
            file->position += off;
            if(off > file->length) file->position = file->length;
            return 0;

        case SEEK_END:
            if(off < file->length) file->position = file->length - off;
            else file->position = 0;
            return 0;

        // should never get this
        default:
            return -1;
    }
}

/**
 * Empty implementations of flush/purge since we do not cache anything.
 */
static int RpcFileFlushPurgeNoOp(struct __libc_file_stream *file) {
    return 0;
}

/**
 * Reads from the file.
 */
static int RpcFileRead(struct __libc_file_stream *_file, void *buf, const size_t toRead) {
    int err;
    struct RpcFileStream *file = (struct RpcFileStream *) _file;

    err = FileRead(file->remoteHandle, file->position, toRead, buf);
    if(err > 0) {
        file->position += err;
    }
    return err;
}

/**
 * Closes an RPC file.
 */
static int RpcFileClose(struct __libc_file_stream *_file) {
    struct RpcFileStream *file = (struct RpcFileStream *) _file;

    return FileClose(file->remoteHandle);
}

/**
 * Contacts the file server via RPC to attempt to open the file at the given path.
 */
LIBC_INTERNAL FILE *__libc_rpc_file_open(const char *path, const char *mode) {
    int err;

    // convert mode string
    uintptr_t flags = 0;
    const size_t len = strlen(mode);
    bool seekToEnd = false;

    if(memchr(mode, 'r', len)) {
        flags |= FILE_OPEN_READ;
    } else if(memchr(mode, 'w', len)) {
        flags |= FILE_OPEN_WRITE | FILE_OPEN_CREATE;
    } else if(memchr(mode, 'a', len)) {
        flags |= FILE_OPEN_WRITE | FILE_OPEN_CREATE;
        seekToEnd = true;
    }

    if(memchr(mode, '+', len)) {
        flags |= (FILE_OPEN_READ | FILE_OPEN_WRITE);
    } 


    // make the request
    uintptr_t handle;
    uint64_t length;
    err = FileOpen(path, flags, &handle, &length);

    if(err) return NULL;

    // create an RPC file object
    struct RpcFileStream *stream = (struct RpcFileStream *) calloc(sizeof(struct RpcFileStream), 1);

    stream->remoteHandle = handle;
    stream->length = length;
    stream->h.length = sizeof(struct RpcFileStream);

    stream->h.close = RpcFileClose;
    stream->h.flush = RpcFileFlushPurgeNoOp;
    stream->h.purge = RpcFileFlushPurgeNoOp;
    stream->h.seek = RpcFileSeek;
    stream->h.tell = RpcFileGetPos;
    stream->h.read = RpcFileRead;

    // seek it to end if needed
    if(seekToEnd) {
        stream->position = length;
    }

    return (FILE *) stream;
}
